// bouncing_donut: a second, non-video proof of ndl's viewport-driven live
// streaming design (ring_buffer.h + viewer/viewport.h + net/
// websocket_server.h + net/json.h), and the concrete end-to-end
// demonstration of renderVolume() (viewer/viewport.h) actually streaming
// live. Unlike live_video_stream, there is no source file at all: each
// tick, a torus signed-distance field is evaluated directly on a
// GRID x GRID x GRID grid and written straight into a RingBufferImage's own
// nextWriteSlot() -- proving the ring buffer/viewport pipeline is genuinely
// generic (any source that can produce one "frame" at a time), not
// video-specific. The torus bounces vertically like a dropped ball
// (centerY = floor + amplitude*|sin(t*bounceSpeed)|) while slowly
// precessing around its own vertical axis at a fixed tilt, so a live
// "volume"-op viewport actually has something worth rotating to look at.
// Registers both "volume" (the primary op -- this is the whole point) and
// "slice" (a plain 2D cross-section, useful for comparing against/
// debugging the volume render) from viewer/viewport.h's own
// RendererRegistry.
//
// Usage: bouncing_donut [--port <port>] [--help] -- prints the URL to open
// once listening (see net/websocket_server.h's own staticRoot support,
// Component 13).
#include <ndl/viewer/ring_buffer.h>
#include <ndl/viewer/viewport.h>
#include <ndl/net/websocket_server.h>
#include <ndl/net/json.h>
#include <ndl/processing/matrix/core.h>
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
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>

using namespace ndl;
using namespace ndl::net;

namespace
{
	// {x, y, z, time} -- time (axis 3) is the ring/sliding axis; unlike
	// live_video_stream's {channel,x,y,time}, there's no channel axis here
	// (a plain scalar density field, not RGB), so the 3 spatial axes for
	// renderVolume()'s own axisA/axisB/axisC are simply 0, 1, 2.
	constexpr int RING_AXIS = 3;
	// Spatial resolution per axis. 48^3 density evaluations/tick (each a
	// handful of trig calls) is trivial CPU cost at this app's own target
	// frame rate -- chosen for a visually smooth torus, not pushed higher
	// since nothing here needs it.
	constexpr int GRID = 48;

	std::atomic<bool> g_running{ true };
	void handleSigint(int) { g_running = false; }

	// Same self-test-mode purpose as live_video_stream's own
	// NDL_LIVE_VIDEO_STREAM_SELFTEST (see that app's own comment): lets
	// ctest smoke-test a program that's otherwise supposed to run forever
	// with the ordinary "run it, expect exit 0" pattern.
	bool selfTestMode() { return std::getenv("NDL_BOUNCING_DONUT_SELFTEST") != nullptr; }

	// Identical wire format to live_video_stream's own encodeWireFrame()
	// (see that file's own comment for the exact byte layout) -- not
	// shared via a common header since it's a handful of lines and each
	// app is otherwise self-contained, matching this project's existing
	// apps/ style.
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

	// Small rotation-matrix builders, dogfooding ndl::Matrix<double,3>
	// (processing/matrix/core.h) rather than hand-rolling the 3x3 entries
	// inline -- same reasoning viewport.h's own matrix3FromColumnMajor()
	// comment gives. Matrix's own default constructor is already the
	// identity, so only the entries that differ from it need setting.
	Matrix<double, 3> rotationX(double a)
	{
		Matrix<double, 3> m;
		double c = std::cos(a), s = std::sin(a);
		m(1, 1) = c; m(1, 2) = -s;
		m(2, 1) = s; m(2, 2) = c;
		return m;
	}
	Matrix<double, 3> rotationY(double a)
	{
		Matrix<double, 3> m;
		double c = std::cos(a), s = std::sin(a);
		m(0, 0) = c; m(0, 2) = s;
		m(2, 0) = -s; m(2, 2) = c;
		return m;
	}

	// Fills one GRID^3 density frame (x fastest-varying, then y, then z --
	// matching RingBufferImage's own stride convention for axes 0,1,2,
	// storage_[0]=1, storage_[1]=GRID, storage_[2]=GRID*GRID) with a torus
	// signed-distance field at time `t` (seconds since start): a fixed-tilt
	// torus, centered at (0, centerY(t), 0) in a [-1,1]^3 cube, bouncing
	// vertically like a dropped ball and slowly precessing around the
	// world Y axis. The torus's own local frame is recovered from a world
	// point by applying the INVERSE of the world rotation (a rotation
	// matrix's inverse is its own transpose -- see Matrix::transpose()),
	// the same view->local trick web/ndlviewer.js's own
	// mat3TransposeMultiplyVec3() uses for click-to-navigate.
	void generateFrame(uint8_t* frame, double t)
	{
		const double majorR = 0.35, minorR = 0.14, edge = 0.12;
		const double tilt = 0.6, precessSpeed = 0.4;
		const double floorY = -0.5, amplitude = 0.35, bounceSpeed = 1.3;

		double centerY = floorY + amplitude * std::fabs(std::sin(t * bounceSpeed));
		Matrix<double, 3> R = rotationY(t * precessSpeed) * rotationX(tilt);
		Matrix<double, 3> Rt = R.transpose();

		std::size_t idx = 0;
		for (int iz = 0; iz < GRID; iz++)
		{
			double nz = ((iz + 0.5) / GRID) * 2.0 - 1.0;
			for (int iy = 0; iy < GRID; iy++)
			{
				double ny = ((iy + 0.5) / GRID) * 2.0 - 1.0 - centerY;
				for (int ix = 0; ix < GRID; ix++, idx++)
				{
					double nx = ((ix + 0.5) / GRID) * 2.0 - 1.0;
					std::array<double, 3> local = Rt * std::array<double, 3>{ nx, ny, nz };
					double qx = std::sqrt(local[0] * local[0] + local[2] * local[2]) - majorR;
					double qy = local[1];
					double dist = std::sqrt(qx * qx + qy * qy) - minorR;
					double f = 1.0 - dist / edge;
					f = f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
					frame[idx] = (uint8_t)(f * f * 255.0);
				}
			}
		}
	}
}

int main(int argc, char** argv)
{
	std::signal(SIGINT, handleSigint);

	apps::CliParser cli("bouncing_donut",
		"Streams a procedurally-animated bouncing, precessing torus density field -- no source\n"
		"file, no VideoStreamReader, just a signed-distance field evaluated fresh into a\n"
		"RingBufferImage each tick -- over the same viewport-driven WebSocket protocol\n"
		"live_video_stream uses. See this file's own top comment for the full design.");
	cli.addOption("port", "<port>", "TCP port to listen on", "8902");
	cli.parse(argc, argv);

	int port = cli.getInt("port");
	const double targetFps = 15.0;
	const int windowSeconds = 3;
	int capacity = std::max(1, (int)(targetFps * windowSeconds));

	RingBufferImage<uint8_t, 4> volume({ GRID, GRID, GRID, capacity }, RING_AXIS);
	std::cout << "bouncing_donut: " << GRID << "x" << GRID << "x" << GRID << " volume @ " << targetFps << "fps" << std::endl;

	RendererRegistry<RingBufferImage<uint8_t, 4>> registry;
	registry.registerOp("volume", [](const RingBufferImage<uint8_t, 4>& src, const JsonValue& p) { return renderVolume(src, p); });
	registry.registerOp("slice", [](const RingBufferImage<uint8_t, 4>& src, const JsonValue& p) { return renderSlice(src, p); });

	std::mutex clientsMutex;
	std::map<WebSocketServer::ClientId, ClientViewports> clientViewports;

	// Same cross-thread reasoning as live_video_stream's own ringMutex:
	// the main loop is the only writer (generateFrame()+commitWrite()),
	// but queryValue() now also reads `volume` directly from onMessage's
	// own thread (a WebSocketServer per-client reader thread).
	std::mutex volumeMutex;

	// Same reasoning as live_video_stream's own serverPtr: onMessage's
	// closure needs to call sendText() on the server for queryValue's own
	// reply, but the server object doesn't exist yet at the point
	// onMessage itself is defined.
	WebSocketServer* serverPtr = nullptr;

	auto onMessage = [&](WebSocketServer::ClientId client, const std::string& text) {
		JsonValue msg;
		try { msg = parseJson(text); }
		catch (const std::exception& e) { std::cerr << "bouncing_donut: ignoring malformed JSON from client " << client << ": " << e.what() << std::endl; return; }

		std::string type = msg.stringOr("type", "");
		std::string id = msg.stringOr("id", "");
		if (id.empty()) return;

		if (type == "queryValue")
		{
			try
			{
				double value;
				{
					std::lock_guard<std::mutex> volumeLock(volumeMutex);
					value = queryValue(volume, msg);
				}
				JsonValue response = JsonValue::makeObject();
				response["type"] = "valueResult";
				response["id"] = id;
				response["value"] = value;
				if (serverPtr) serverPtr->sendText(client, response.toString());
			}
			catch (const std::exception& e)
			{
				std::cerr << "bouncing_donut: queryValue failed for client " << client << ": " << e.what() << std::endl;
			}
			return;
		}

		std::lock_guard<std::mutex> lock(clientsMutex);
		auto& cv = clientViewports[client];
		if (type == "createViewport")
		{
			std::string op = msg.stringOr("op", "volume");
			auto vp = std::make_shared<Viewport>(id, op);
			if (msg.has("params")) vp->setParams(msg["params"]);
			cv.viewports[id] = vp;
			std::cout << "bouncing_donut: client " << client << " created viewport \"" << id << "\" (op=" << op << ")" << std::endl;
		}
		else if (type == "updateViewport")
		{
			// Merge onto the existing params rather than replacing them
			// outright -- same bug/fix as live_video_stream.cpp's own
			// identical code (see that file's own comment here): a bare
			// setParams(msg["params"]) wiped out axisA/axisB/axisC and
			// every other key the first time the rotation slider sent its
			// own {rotation:[...]}-only update, breaking every subsequent
			// render for that viewport.
			auto it = cv.viewports.find(id);
			if (it != cv.viewports.end() && msg.has("params"))
			{
				JsonValue merged = it->second->params();
				for (const auto& kv : msg["params"].asObject()) merged[kv.first] = kv.second;
				it->second->setParams(merged);
			}
		}
		else if (type == "closeViewport")
		{
			cv.viewports.erase(id);
		}
	};
	auto onConnect = [&](WebSocketServer::ClientId client) {
		std::lock_guard<std::mutex> lock(clientsMutex);
		clientViewports[client];
		std::cout << "bouncing_donut: client " << client << " connected" << std::endl;
	};
	auto onDisconnect = [&](WebSocketServer::ClientId client) {
		std::lock_guard<std::mutex> lock(clientsMutex);
		clientViewports.erase(client);
		std::cout << "bouncing_donut: client " << client << " disconnected" << std::endl;
	};

	WebSocketServer server(port, onMessage, onConnect, onDisconnect, NDL_REPO_ROOT_DIR);
	serverPtr = &server;
	std::cout << "bouncing_donut: open http://localhost:" << port << "/apps/bouncing_donut/bouncing_donut.html" << std::endl;

	std::thread selfTestWatchdog;
	if (selfTestMode())
		selfTestWatchdog = std::thread([] { std::this_thread::sleep_for(std::chrono::milliseconds(800)); g_running = false; });

	// Writer thread: same decoupling reasoning as live_video_stream's own
	// (a slow client's blocking write can never hold up the next tick's
	// generation for everyone else).
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
							auto wire = encodeWireFrame(vpEntry.second->id(), volume.totalWritten() - 1, frame);
							server.sendBinary(clientEntry.first, wire.data(), wire.size());
						}
					}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(4));
		}
	});

	// Main loop: generate one new density frame per tick (no file, no
	// reader -- generateFrame() IS the source), push it into the ring,
	// then re-render every currently active viewport against it. Every
	// viewport always tracks the newest sample, same as live_video_stream.
	auto frameInterval = std::chrono::duration<double>(1.0 / targetFps);
	auto nextFrameTime = std::chrono::steady_clock::now();
	auto startTime = std::chrono::steady_clock::now();
	while (g_running)
	{
		double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
		{
			std::lock_guard<std::mutex> volumeLock(volumeMutex);
			generateFrame(volume.nextWriteSlot(), t);
			volume.commitWrite();
		}

		{
			std::lock_guard<std::mutex> lock(clientsMutex);
			long long newest = volume.count() - 1;
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
						RenderedFrame frame = registry.render(vp->op(), volume, params);
						vp->setPendingOutput(std::move(frame));
					}
					catch (const std::exception& e)
					{
						std::cerr << "bouncing_donut: render failed for viewport \"" << vp->id() << "\": " << e.what() << std::endl;
					}
				}
		}

		nextFrameTime += std::chrono::duration_cast<std::chrono::steady_clock::duration>(frameInterval);
		std::this_thread::sleep_until(nextFrameTime);
	}

	writer.join();
	if (selfTestWatchdog.joinable()) selfTestWatchdog.join();
	std::cout << "bouncing_donut: shutting down" << std::endl;
	return 0;
}
