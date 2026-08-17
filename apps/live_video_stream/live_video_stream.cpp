// live_video_stream: the end-to-end proof of ndl's viewport-driven live
// streaming design (ring_buffer.h + imageIO/video_io.h's
// VideoStreamReader + viewport.h + net/websocket_server.h + net/json.h).
// Streams a local .mp4 file as a stand-in for a real-time sensor: reads it
// one frame at a time (never materializing the whole clip in memory,
// VideoStreamReader's whole point), retains a short sliding window in a
// RingBufferImage (no memmove, ever), and serves it to any number of
// connected browsers over a hand-rolled WebSocket. Unlike a naive "push
// every frame to everyone" server, what actually gets sent to each client
// is driven BY that client: each one creates one or more independent
// "viewports" (crop region, resolution, window/level, live-updated as new
// frames arrive) via small JSON control messages, and the server renders
// and sends back exactly what was asked for -- the same "client's current
// view is a request, not a hint" model Google Earth/Neuroglancer/
// BigDataViewer use, just for a live video feed instead of a static
// pre-tiled dataset.
//
// This program is deliberately an "app" (apps/), not a "demo" (demo/): a
// live client/server program can't be captured into a static tutorial
// page the way this project's other demos are (there's no faithful way
// to "capture stdout" from something that requires an actual running
// server and a browser connecting to it over time) -- see this project's
// own apps/ vs demo/ documentation for the full rationale.
//
// Usage: live_video_stream [--video <path>] [--port <port>] [--help] --
// videoPath defaults to unitTests/data/waves.mp4, port defaults to 8901;
// the file's length doesn't matter (see VideoStreamReader's own comment),
// so this works the same for a multi-hour recording as a multi-second
// clip. Prints the URL to open in a browser once it's listening.
#include <ndl/imageIO/video_io.h>
#include <ndl/viewer/ring_buffer.h>
#include <ndl/viewer/viewport.h>
#include <ndl/net/websocket_server.h>
#include <ndl/net/json.h>
#include "../appHelpers.h"

#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <vector>
#include <algorithm>

using namespace ndl;
using namespace ndl::net;

namespace
{
	// {channel, x, y, time} -- the same {3,width,height,frameCount}
	// convention every other video/image loader in this library uses;
	// time (axis 3) is the ring/sliding axis.
	constexpr int RING_AXIS = 3;

	std::atomic<bool> g_running{ true };
	void handleSigint(int) { g_running = false; }

	// Every other demo in this project is smoke-tested by just running it
	// to completion and checking the exit code -- but this app, unlike
	// those, is SUPPOSED to run forever until stopped. Rather than have
	// ctest juggle external process signaling (timeout/kill, with its own
	// portability and exit-code subtleties), self-test mode makes the
	// process behave like an ordinary demo for exactly this purpose: it
	// still does everything a real run does (opens the file, builds the
	// ring buffer, starts the WebSocket server, streams a few frames), it
	// just also stops itself after a short, fixed duration -- so `ctest`
	// can smoke-test it with the exact same "run it, expect exit 0"
	// pattern as every other demo.
	bool selfTestMode() { return std::getenv("NDL_LIVE_VIDEO_STREAM_SELFTEST") != nullptr; }

	// Wire format for one rendered viewport update, sent as a single
	// binary WS frame:
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
	// id<->id mapping, is simpler for no real bandwidth cost at this
	// scale.
	std::vector<uint8_t> encodeWireFrame(const std::string& viewportId, long long globalIndex, const RenderedFrame& frame)
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

	// Every viewport currently open for one connected client.
	struct ClientViewports { std::map<std::string, std::shared_ptr<Viewport>> viewports; };
}

int main(int argc, char** argv)
{
	std::signal(SIGINT, handleSigint);

	// videoPath defaults to the same waves.mp4 clip demo/color_video
	// already uses, purely so this app (and its self-test mode) still
	// runs with zero arguments; any real use should pass its own file via
	// --video -- readFrame()'s own incremental design (imageIO/
	// video_io.h) means the file's length doesn't matter, a multi-hour
	// recording streams with exactly the same bounded memory footprint as
	// a multi-second clip, since nothing here ever holds more than one
	// frame plus the ring buffer's own fixed-size window at once.
	apps::CliParser cli("live_video_stream",
		"Streams a local video file as a stand-in for a real-time sensor, over a hand-rolled\n"
		"WebSocket, with the client's own current view (crop, resolution, window/level) driving\n"
		"what the server actually renders. See this file's own top comment for the full design.");
	cli.addOption("video", "<path>", "video file to stream", std::string(NDL_TEST_DATA_DIR) + "/waves.mp4");
	cli.addOption("port", "<port>", "TCP port to listen on", "8901");
	cli.parse(argc, argv);

	std::string path = cli.get("video");
	int port = cli.getInt("port");
	const double targetFps = 10.0;
	const int windowSeconds = 5;

	image_io::VideoStreamReader reader(path, /*targetWidth*/ 400, /*targetHeight*/ 0, targetFps);
	std::cout << "live_video_stream: " << path << " (" << reader.width() << "x" << reader.height()
		<< " @ " << reader.fps() << "fps)" << std::endl;

	int capacity = std::max(1, (int)(reader.fps() * windowSeconds));
	RingBufferImage<uint8_t, 4> ring({ 3, reader.width(), reader.height(), capacity }, RING_AXIS);

	RendererRegistry<RingBufferImage<uint8_t, 4>> registry;
	registry.registerOp("slice", [](const RingBufferImage<uint8_t, 4>& src, const JsonValue& p) { return renderSlice(src, p); });
	registry.registerOp("volume", [](const RingBufferImage<uint8_t, 4>& src, const JsonValue& p) { return renderVolume(src, p); });

	std::mutex clientsMutex;
	std::map<WebSocketServer::ClientId, ClientViewports> clientViewports;

	// Protects `ring` itself from a genuine cross-thread race: the main
	// loop below is the only thing that ever WRITES to it
	// (readFrame()+commitWrite()), but queryValue() (Component 11) now
	// also READS it directly from onMessage's own thread (a WebSocketServer
	// per-client reader thread, not the main thread) -- without this,
	// that read could race a concurrent write to the exact same physical
	// ring slot. The render loop's own reads of `ring` further down don't
	// need this lock: they only ever happen on the SAME (main) thread as
	// the writes, sequentially, and a read racing another read (this
	// thread's render() vs. the other thread's queryValue()) is never
	// unsafe on its own.
	std::mutex ringMutex;

	// Assigned right after the WebSocketServer below is actually
	// constructed -- onMessage's own closure needs to call sendText() on
	// it (for queryValue's own reply), but the server object doesn't
	// exist yet at the point onMessage itself is defined (it's an
	// argument to the very constructor call that creates it); a plain
	// `[&]` reference capture can't reach a name that isn't in scope yet
	// either way, so this pointer is what's actually captured instead.
	WebSocketServer* serverPtr = nullptr;

	auto onMessage = [&](WebSocketServer::ClientId client, const std::string& text) {
		JsonValue msg;
		try { msg = parseJson(text); }
		catch (const std::exception& e) { std::cerr << "live_video_stream: ignoring malformed JSON from client " << client << ": " << e.what() << std::endl; return; }

		std::string type = msg.stringOr("type", "");
		std::string id = msg.stringOr("id", "");
		if (id.empty()) return;

		if (type == "queryValue")
		{
			// Native-value hover: doesn't touch any Viewport at all (no
			// windowing, no channel reduction -- see queryValue()'s own
			// comment in viewer/viewport.h), so this doesn't need the
			// clientsMutex lock every other message type below takes to
			// touch clientViewports.
			try
			{
				double value;
				{
					std::lock_guard<std::mutex> ringLock(ringMutex);
					value = queryValue(ring, msg);
				}
				JsonValue response = JsonValue::makeObject();
				response["type"] = "valueResult";
				response["id"] = id;
				response["value"] = value;
				if (serverPtr) serverPtr->sendText(client, response.toString());
			}
			catch (const std::exception& e)
			{
				std::cerr << "live_video_stream: queryValue failed for client " << client << ": " << e.what() << std::endl;
			}
			return;
		}

		std::lock_guard<std::mutex> lock(clientsMutex);
		auto& cv = clientViewports[client];
		if (type == "createViewport")
		{
			std::string op = msg.stringOr("op", "slice");
			auto vp = std::make_shared<Viewport>(id, op);
			if (msg.has("params")) vp->setParams(msg["params"]);
			cv.viewports[id] = vp;
			std::cout << "live_video_stream: client " << client << " created viewport \"" << id << "\" (op=" << op << ")" << std::endl;
		}
		else if (type == "updateViewport")
		{
			auto it = cv.viewports.find(id);
			if (it != cv.viewports.end() && msg.has("params")) it->second->setParams(msg["params"]);
		}
		else if (type == "closeViewport")
		{
			cv.viewports.erase(id);
		}
	};
	auto onConnect = [&](WebSocketServer::ClientId client) {
		std::lock_guard<std::mutex> lock(clientsMutex);
		clientViewports[client];
		std::cout << "live_video_stream: client " << client << " connected" << std::endl;
	};
	auto onDisconnect = [&](WebSocketServer::ClientId client) {
		std::lock_guard<std::mutex> lock(clientsMutex);
		clientViewports.erase(client);
		std::cout << "live_video_stream: client " << client << " disconnected" << std::endl;
	};

	WebSocketServer server(port, onMessage, onConnect, onDisconnect, NDL_REPO_ROOT_DIR);
	serverPtr = &server;
	std::cout << "live_video_stream: open http://localhost:" << port << "/apps/live_video_stream/live_video_stream.html" << std::endl;

	std::thread selfTestWatchdog;
	if (selfTestMode())
		selfTestWatchdog = std::thread([] { std::this_thread::sleep_for(std::chrono::milliseconds(800)); g_running = false; });

	// Writer thread: drains every viewport's pending rendered output and
	// sends it -- decoupled from the render loop below specifically so a
	// slow or stalled client's blocking socket write can never hold up
	// producing the NEXT frame for everyone else. A viewport's own
	// setPendingOutput() ("latest wins", viewport.h) is what actually
	// implements "drop frames a client couldn't keep up with": if this
	// thread falls behind, it simply finds a newer frame waiting the next
	// time it checks, never a backlog.
	std::thread writer([&] {
		while (g_running)
		{
			{
				std::lock_guard<std::mutex> lock(clientsMutex);
				for (auto& clientEntry : clientViewports)
					for (auto& vpEntry : clientEntry.second.viewports)
					{
						RenderedFrame frame;
						if (vpEntry.second->takePendingOutput(frame))
						{
							auto wire = encodeWireFrame(vpEntry.second->id(), ring.totalWritten() - 1, frame);
							server.sendBinary(clientEntry.first, wire.data(), wire.size());
						}
					}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(4));
		}
	});

	// Main loop: read one frame at a time (VideoStreamReader never holds
	// the whole clip in memory), push it into the ring (O(1), no
	// shifting), then re-render every currently active viewport against
	// the new data. Every viewport here always tracks the newest sample
	// (v1 scope is live-only, no historical scrubbing yet -- see
	// viewport.h's own comment on "fixed" for how a future client could
	// request a specific past index instead).
	auto frameInterval = std::chrono::duration<double>(1.0 / reader.fps());
	auto nextFrameTime = std::chrono::steady_clock::now();
	while (g_running)
	{
		bool gotFrame;
		{
			std::lock_guard<std::mutex> ringLock(ringMutex);
			gotFrame = reader.readFrame(ring.nextWriteSlot());
			if (gotFrame) ring.commitWrite();
		}
		if (!gotFrame)
		{
			// Loops the finite demo file to simulate a continuous live
			// source -- move-assigning a freshly-opened reader rather
			// than needing a separate "reset" method (VideoStreamReader's
			// own top comment in imageIO/video_io.h).
			reader = image_io::VideoStreamReader(path, reader.width(), reader.height(), reader.fps());
			continue;
		}

		{
			std::lock_guard<std::mutex> lock(clientsMutex);
			long long newest = ring.count() - 1;
			for (auto& clientEntry : clientViewports)
				for (auto& vpEntry : clientEntry.second.viewports)
				{
					auto vp = vpEntry.second;
					JsonValue params = vp->params();
					JsonValue fixed = params.has("fixed") ? params["fixed"] : JsonValue::makeObject();
					fixed[std::to_string(RING_AXIS)] = (int)newest;
					params["fixed"] = fixed;
					try
					{
						RenderedFrame frame = registry.render(vp->op(), ring, params);
						vp->setPendingOutput(std::move(frame));
					}
					catch (const std::exception& e)
					{
						std::cerr << "live_video_stream: render failed for viewport \"" << vp->id() << "\": " << e.what() << std::endl;
					}
				}
		}

		nextFrameTime += std::chrono::duration_cast<std::chrono::steady_clock::duration>(frameInterval);
		std::this_thread::sleep_until(nextFrameTime);
	}

	writer.join();
	if (selfTestWatchdog.joinable()) selfTestWatchdog.join();
	std::cout << "live_video_stream: shutting down" << std::endl;
	return 0;
}
