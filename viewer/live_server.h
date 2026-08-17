#pragma once
#include "viewport.h"
#include "../net/websocket_server.h"
#include "../net/json.h"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// The server-side plumbing every viewport-driven live-streaming apps/
// program needs -- factored out once apps/live_video_stream.cpp and
// apps/bouncing_donut.cpp turned out to share roughly 150 near-identical
// lines of it: the createViewport/updateViewport(merged)/closeViewport/
// queryValue dispatch, per-client bookkeeping, the writer thread, and
// wire-frame encoding. A sibling of viewport.h/ring_buffer.h/
// net/websocket_server.h, not part of image.h's core -- #include this
// directly if you use it.
//
// The ONE thing LiveStreamServer deliberately does NOT own is "how does a
// new sample actually get produced": decoding a video frame
// (VideoStreamReader) and evaluating a procedural density field
// (bouncing_donut's own generateFrame()) are genuinely different per app,
// and trying to unify them would just be forcing two different things
// into one shape for no real gain. The caller still drives its own main
// loop and its own pacing; it just calls sourceMutex() to protect writing
// a new sample, instead of hand-rolling the render/send fan-out inline.
//
// Rendering itself runs entirely on this class's own background thread,
// autonomously -- NOT synchronously inside the caller's own main loop (an
// earlier version had the caller call a `tick()` method right after each
// commit, which meant a slow render, e.g. renderVolume()'s own CPU ray-
// march at a high output resolution, directly throttled how fast new
// samples could even be GENERATED, since the main loop blocked on tick()
// before it could proceed to its own next pacing sleep). The background
// thread instead watches source's own totalWritten() counter and re-
// renders every active viewport whenever it changes, at whatever pace
// rendering itself can sustain -- generation and rendering are fully
// decoupled, the same "keep newest, drop stale" philosophy the mailbox
// pattern (Viewport::setPendingOutput()) already uses for OUTGOING
// frames, now applied to the render step itself: if rendering falls
// behind generation, it simply renders whatever's newest the next time it
// checks, rather than working through a backlog of stale samples.
//
// @tparam SourceT Any minimal-interface source (see RendererRegistry's
//         own comment) that ALSO exposes totalWritten() -- i.e. a
//         RingBufferImage<T,DIM>, this class's only intended source type.
namespace ndl
{
	// Wire format for one rendered viewport update, sent as a single
	// binary WS frame -- see this project's own apps/*.html pages
	// (decodeWireFrame() in web/ndlviewer.js) for the client-side mirror:
	//   byte 0      viewport id string length (uint8, max 255)
	//   N bytes     viewport id string (matches whatever the client itself chose in createViewport)
	//   8 bytes     globalIndex (uint64 LE) -- the source's own newest logical sample index at render time
	//   4 bytes     width  (uint32 LE)
	//   4 bytes     height (uint32 LE)
	//   1 byte      channels (uint8: 1 = grayscale, 3 = RGB)
	//   remaining   raw pixel bytes (width*height*channels)
	// No separate "createViewport ack" message or server-assigned numeric
	// id: the client's own string id is small enough (a handful of bytes)
	// that reusing it directly here, rather than maintaining a second
	// id<->id mapping, is simpler for no real bandwidth cost at this scale.
	/// @ingroup viewer
	inline std::vector<uint8_t> encodeWireFrame(const std::string& viewportId, long long globalIndex, const RenderedFrame& frame)
	{
		std::vector<uint8_t> wire;
		uint8_t idLen = (uint8_t)std::min<std::size_t>(viewportId.size(), 255);
		wire.push_back(idLen);
		wire.insert(wire.end(), viewportId.begin(), viewportId.begin() + idLen);
		auto put64 = [&](uint64_t v) { for (int i = 0; i < 8; i++) wire.push_back((uint8_t)(v >> (i * 8))); };
		auto put32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) wire.push_back((uint8_t)(v >> (i * 8))); };
		put64((uint64_t)globalIndex);
		put32((uint32_t)frame.width);
		put32((uint32_t)frame.height);
		wire.push_back((uint8_t)frame.channels);
		wire.insert(wire.end(), frame.pixels.begin(), frame.pixels.end());
		return wire;
	}

	/// Everything a viewport-driven live-streaming apps/ program needs server-side, besides "how a new
	/// sample gets produced" -- see this header's own top comment for the full rationale and scope.
	/// @tparam SourceT Any minimal-interface source that also exposes totalWritten() (RingBufferImage<T,DIM>).
	/// @ingroup viewer
	template<class SourceT>
	class LiveStreamServer
	{
	public:
		/// @param programName Log-line prefix (e.g. "live_video_stream") -- matches this project's existing apps/ convention of every stdout/stderr line naming which program it's from.
		/// @param source      The live data source; must outlive this LiveStreamServer. Written to by the caller under sourceMutex() (see this header's own top comment).
		/// @param registry    Op-name -> render-function dispatch (viewport.h's own RendererRegistry) -- moved in.
		/// @param ringAxis    Which axis of `source` is the ring/time axis -- injected as `fixed[ringAxis]` (viewer/viewport.h's own "fixed" convention) on every tick(), so a client never needs to (and never could, for a live source) request a specific historical index.
		/// @param defaultOp   The op a createViewport message uses if it doesn't specify one.
		/// @param port        TCP port to listen on.
		/// @param staticRoot  Passed straight to WebSocketServer's own staticRoot (net/websocket_server.h) -- serves this app's own HTML page (and web/ndlviewer.js) over the same port, empty disables it.
		LiveStreamServer(std::string programName, SourceT& source, RendererRegistry<SourceT> registry, int ringAxis, std::string defaultOp, int port, std::string staticRoot) :
			programName_(std::move(programName)), source_(source), registry_(std::move(registry)), ringAxis_(ringAxis), defaultOp_(std::move(defaultOp)), port_(port),
			server_(port,
				[this](net::WebSocketServer::ClientId c, const std::string& text) { onMessage(c, text); },
				[this](net::WebSocketServer::ClientId c) { onConnect(c); },
				[this](net::WebSocketServer::ClientId c) { onDisconnect(c); },
				std::move(staticRoot))
		{
			// server_ is declared LAST (see this class's own member-order
			// comment below) specifically so every other member above is
			// already fully constructed before WebSocketServer's own
			// constructor spawns its accept thread -- onMessage()/
			// onConnect()/onDisconnect() (captured as `this`-bound lambdas
			// just above) can genuinely be invoked from a client thread
			// the instant a connection arrives, so nothing they touch can
			// still be mid-construction.
			writerThread_ = std::thread([this] { workerLoop(); });
		}
		~LiveStreamServer()
		{
			writerRunning_ = false;
			if (writerThread_.joinable()) writerThread_.join();
		}
		LiveStreamServer(const LiveStreamServer&) = delete;
		LiveStreamServer& operator=(const LiveStreamServer&) = delete;

		/// The mutex to hold while writing a new sample into `source` (e.g. around VideoStreamReader::readFrame()+RingBufferImage::commitWrite(), or a procedural generateFrame()+commitWrite()) -- protects against the background render thread's own concurrent read of `source` (see this header's own top comment) and against queryValue()'s cross-thread read from a WebSocket client thread.
		std::mutex& sourceMutex() { return sourceMutex_; }

		/// The URL a browser should open -- printed by the caller right after construction (matches this project's existing apps/ convention of printing a clickable link once listening).
		std::string url(const std::string& htmlPath) const { return "http://localhost:" + std::to_string(port_) + htmlPath; }

	private:
		struct ClientViewports { std::map<std::string, std::shared_ptr<Viewport>> viewports; };

		void onMessage(net::WebSocketServer::ClientId client, const std::string& text)
		{
			net::JsonValue msg;
			try { msg = net::parseJson(text); }
			catch (const std::exception& e) { std::cerr << programName_ << ": ignoring malformed JSON from client " << client << ": " << e.what() << std::endl; return; }

			std::string type = msg.stringOr("type", "");
			std::string id = msg.stringOr("id", "");
			if (id.empty()) return;

			if (type == "queryValue")
			{
				// Native-value hover: doesn't touch any Viewport at all (no
				// windowing, no channel reduction -- see queryValue()'s own
				// comment in viewport.h), so this doesn't need clientsMutex_,
				// only sourceMutex_ (source_ is read from this thread, written
				// from the app's own main-loop thread, and also read from the
				// background render thread below).
				//
				// Two request shapes: `"coord"` (a single full N-D
				// coordinate) replies with a single `"value"`, unchanged
				// from Component 11's own original design; `"coords"` (an
				// ARRAY of full N-D coordinates -- e.g. the same spatial
				// position with only the channel axis varying, one entry
				// per RGB channel) replies with a parallel `"values"`
				// array instead. Deliberately just an array of already-
				// complete coordinates, not a "vary axis K over N values"
				// shorthand: this class has no idea which axis (if any) a
				// given source's own channel data lives on -- that's the
				// CLIENT's own convention (e.g. live_video_stream.html's
				// {channel,x,y,time}), so the client is what builds each
				// full coordinate, this only loops over however many it sent.
				try
				{
					net::JsonValue response = net::JsonValue::makeObject();
					response["type"] = "valueResult";
					response["id"] = id;
					std::lock_guard<std::mutex> sourceLock(sourceMutex_);
					if (msg.has("coords"))
					{
						net::JsonValue values = net::JsonValue::makeArray();
						for (const auto& coordJv : msg["coords"].asArray())
						{
							net::JsonValue coordMsg = net::JsonValue::makeObject();
							coordMsg["coord"] = coordJv;
							values.push_back(queryValue(source_, coordMsg));
						}
						response["values"] = values;
					}
					else
					{
						response["value"] = queryValue(source_, msg);
					}
					server_.sendText(client, response.toString());
				}
				catch (const std::exception& e)
				{
					std::cerr << programName_ << ": queryValue failed for client " << client << ": " << e.what() << std::endl;
				}
				return;
			}

			std::lock_guard<std::mutex> lock(clientsMutex_);
			auto& cv = clientViewports_[client];
			if (type == "createViewport")
			{
				std::string op = msg.stringOr("op", defaultOp_);
				auto vp = std::make_shared<Viewport>(id, op);
				if (msg.has("params")) vp->setParams(msg["params"]);
				cv.viewports[id] = vp;
				std::cout << programName_ << ": client " << client << " created viewport \"" << id << "\" (op=" << op << ")" << std::endl;
			}
			else if (type == "updateViewport")
			{
				// Merges onto the existing params rather than replacing them
				// outright -- Viewport::setParams()'s own doc comment says
				// the CALLER is responsible for this (an updateViewport
				// message is meant to be a partial patch, e.g. a pan/zoom
				// gesture sending only {cropMin,cropMax}, or a rotation
				// slider sending only {rotation:[...]}); a bare
				// setParams(msg["params"]) wiped out every other key the
				// very first time a client sent a partial update, breaking
				// that viewport's every subsequent render until a fresh
				// createViewport -- a real bug this project's own two apps
				// both hit before this class existed. Shallow merge only
				// (one level of object keys) -- the only nested object any
				// op's own params ever carries is "fixed", which no
				// updateViewport message here ever partially patches on its
				// own.
				auto it = cv.viewports.find(id);
				if (it != cv.viewports.end() && msg.has("params"))
				{
					net::JsonValue merged = it->second->params();
					for (const auto& kv : msg["params"].asObject()) merged[kv.first] = kv.second;
					it->second->setParams(merged);
				}
			}
			else if (type == "closeViewport")
			{
				if (cv.viewports.erase(id))
					std::cout << programName_ << ": client " << client << " closed viewport \"" << id << "\"" << std::endl;
			}
		}
		void onConnect(net::WebSocketServer::ClientId client)
		{
			std::lock_guard<std::mutex> lock(clientsMutex_);
			clientViewports_[client];
			std::cout << programName_ << ": client " << client << " connected" << std::endl;
		}
		void onDisconnect(net::WebSocketServer::ClientId client)
		{
			std::lock_guard<std::mutex> lock(clientsMutex_);
			clientViewports_.erase(client);
			std::cout << programName_ << ": client " << client << " disconnected" << std::endl;
		}
		// Re-renders every currently active viewport against `source`'s own
		// latest state -- called from workerLoop() below only when
		// source_.totalWritten() has actually advanced since the last call
		// (checked there, not here), under BOTH sourceMutex_ (source_ is
		// being read) and clientsMutex_ (iterating clientViewports_).
		// `newestGlobalIndex` becomes `fixed[ringAxis]` for every
		// viewport's own render call -- source_.count()-1, the highest
		// currently-valid LOGICAL ring index, is what workerLoop() passes.
		void renderActive(long long newestGlobalIndex)
		{
			std::lock_guard<std::mutex> lock(clientsMutex_);
			for (auto& clientEntry : clientViewports_)
				for (auto& vpEntry : clientEntry.second.viewports)
				{
					auto vp = vpEntry.second;
					net::JsonValue params = vp->params();
					net::JsonValue fixed = params.has("fixed") ? params["fixed"] : net::JsonValue::makeObject();
					fixed[std::to_string(ringAxis_)] = (int)newestGlobalIndex;
					params["fixed"] = fixed;
					try
					{
						RenderedFrame frame = registry_.render(vp->op(), source_, params);
						vp->setPendingOutput(std::move(frame));
					}
					catch (const std::exception& e)
					{
						std::cerr << programName_ << ": render failed for viewport \"" << vp->id() << "\": " << e.what() << std::endl;
					}
				}
		}
		// The one background thread doing both halves of "get a rendered
		// frame to each client": re-rendering (renderActive(), only when
		// source_ actually has new data -- lastRenderedTotal_ tracks this,
		// entirely on this one thread, no synchronization needed for it)
		// and draining+sending every viewport's own pending output.
		// Draining is unconditional every iteration (not just after a
		// render) since a slow or stalled client's own blocking socket
		// write shouldn't be gated on whether THIS particular tick
		// rendered anything new. A viewport's own setPendingOutput()
		// ("latest wins", viewport.h) is what actually implements "drop
		// frames a client couldn't keep up with": if sending falls behind,
		// it simply finds a newer frame waiting the next time it checks,
		// never a backlog -- and now, symmetrically, if RENDERING itself
		// is slow (renderVolume()'s own CPU ray-march cost, in
		// particular), this loop just renders whatever's newest the next
		// time it gets back around, rather than working through a queue of
		// stale samples.
		void workerLoop()
		{
			long long lastRenderedTotal = -1;
			while (writerRunning_)
			{
				// totalWritten() read here without sourceMutex_ -- an
				// aligned integer read/write racing the app's own writer
				// thread is benign in practice (this is purely a "did
				// anything change" poll, not a correctness-critical read);
				// the actual source_ CONTENTS are only ever read inside
				// renderActive(), which does hold sourceMutex_.
				long long total = source_.totalWritten();
				if (total > 0 && total != lastRenderedTotal)
				{
					lastRenderedTotal = total;
					std::lock_guard<std::mutex> sourceLock(sourceMutex_);
					renderActive(source_.count() - 1);
				}

				{
					std::lock_guard<std::mutex> lock(clientsMutex_);
					for (auto& clientEntry : clientViewports_)
						for (auto& vpEntry : clientEntry.second.viewports)
						{
							RenderedFrame frame;
							if (vpEntry.second->takePendingOutput(frame))
							{
								auto wire = encodeWireFrame(vpEntry.second->id(), source_.totalWritten() - 1, frame);
								server_.sendBinary(clientEntry.first, wire.data(), wire.size());
							}
						}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(4));
			}
		}

		std::string programName_;
		SourceT& source_;
		RendererRegistry<SourceT> registry_;
		int ringAxis_;
		std::string defaultOp_;
		int port_ = 0;

		// Protects `source_` itself from the cross-thread race between the
		// caller's own writes (readFrame()+commitWrite(), or a procedural
		// generateFrame()+commitWrite()) and queryValue()'s own reads from
		// a WebSocketServer per-client reader thread.
		std::mutex sourceMutex_;

		std::mutex clientsMutex_;
		std::map<net::WebSocketServer::ClientId, ClientViewports> clientViewports_;

		std::atomic<bool> writerRunning_{ true };
		std::thread writerThread_;

		// Declared LAST: see the constructor's own comment on why this
		// ordering is what makes capturing `this` in server_'s own
		// callbacks safe.
		net::WebSocketServer server_;
	};
}
