// live_video_stream: the end-to-end proof of ndl's viewport-driven live
// streaming design (ring_buffer.h + imageIO/video_io.h's
// VideoStreamReader + viewer/live_server.h + net/websocket_server.h +
// net/json.h). Streams a local .mp4 file as a stand-in for a real-time
// sensor: reads it one frame at a time (never materializing the whole
// clip in memory, VideoStreamReader's whole point), retains a short
// sliding window in a RingBufferImage (no memmove, ever), and serves it
// to any number of connected browsers over a hand-rolled WebSocket.
// Unlike a naive "push every frame to everyone" server, what actually
// gets sent to each client is driven BY that client: each one creates one
// or more independent "viewports" (crop region, resolution, window/level,
// live-updated as new frames arrive) via small JSON control messages, and
// the server renders and sends back exactly what was asked for -- the
// same "client's current view is a request, not a hint" model Google
// Earth/Neuroglancer/BigDataViewer use, just for a live video feed
// instead of a static pre-tiled dataset. All of the actual server-side
// plumbing (the createViewport/updateViewport/queryValue dispatch, the
// writer thread, wire-frame encoding) lives in viewer/live_server.h's own
// LiveStreamServer -- shared with apps/bouncing_donut.cpp, since the two
// turned out to need nearly identical plumbing around genuinely different
// "how does a new sample get produced" steps.
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
#include <ndl/viewer/live_server.h>
#include "../appHelpers.h"

#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <algorithm>

using namespace ndl;
using namespace ndl::net;

namespace
{
	// {channel, x, y, time} -- the same {3,width,height,frameCount}
	// convention every other video/image loader in this library uses;
	// time (axis 3) is the ring/sliding axis.
	constexpr int RING_AXIS = 3;
}

int main(int argc, char** argv)
{
	std::atomic<bool> running{ true };
	apps::installSigintHandler(running);

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
	// A short window is plenty here -- v1 scope is live-only (no
	// historical scrubbing, see viewport.h's own "fixed" comment), so this
	// exists purely to absorb a brief network hiccup and give queryValue()
	// some slack against a momentarily-stale client request, not to hold
	// meaningful playback history. Kept short deliberately: at a large
	// source resolution the ring buffer's own memory scales with both
	// this and the frame size, and a live source has no reason to hold
	// more than a couple seconds of it either way.
	const int windowSeconds = 2;

	// 0,0,0: decode at the SOURCE's own native resolution and native
	// frame rate (VideoStreamReader's own auto-detect default, imageIO/
	// video_io.h) -- no artificial downscale or fps cap baked into the
	// decode step itself. What actually controls bandwidth/render cost is
	// each CLIENT's own requested outputWidth/outputHeight (renderSlice()'s
	// own param, viewer/viewport.h) -- exactly the same on-demand-
	// resolution model bouncing_donut's own Size slider demonstrates, just
	// for a real video instead of a procedural one. An earlier version
	// hardcoded targetWidth=400/targetFps=10 here, which both cropped
	// every source down to a fixed size regardless of its own true
	// resolution AND capped playback well below a source's own real frame
	// rate -- neither was a genuine performance limit, just an
	// unnecessary artificial one baked into the wrong layer.
	image_io::VideoStreamReader reader(path, 0, 0, 0);
	std::cout << "live_video_stream: " << path << " (" << reader.width() << "x" << reader.height()
		<< " @ " << reader.fps() << "fps)" << std::endl;

	int capacity = std::max(1, (int)(reader.fps() * windowSeconds));
	RingBufferImage<uint8_t, 4> ring({ 3, reader.width(), reader.height(), capacity }, RING_AXIS);

	RendererRegistry<RingBufferImage<uint8_t, 4>> registry;
	registry.registerOp("slice", [](const RingBufferImage<uint8_t, 4>& src, const JsonValue& p) { return renderSlice(src, p); });
	registry.registerOp("volume", [](const RingBufferImage<uint8_t, 4>& src, const JsonValue& p) { return renderVolume(src, p); });

	LiveStreamServer<RingBufferImage<uint8_t, 4>> liveServer("live_video_stream", ring, std::move(registry), RING_AXIS, "slice", port, NDL_REPO_ROOT_DIR);
	std::cout << "live_video_stream: open " << liveServer.url("/apps/live_video_stream/live_video_stream.html") << std::endl;

	std::thread selfTestWatchdog = apps::selfTestWatchdog("NDL_LIVE_VIDEO_STREAM_SELFTEST", running);

	// Main loop: read one frame at a time (VideoStreamReader never holds
	// the whole clip in memory) and push it into the ring (O(1), no
	// shifting) -- LiveStreamServer's own background thread notices
	// (via source's own totalWritten()) and re-renders every currently
	// active viewport on its own, decoupled from this loop's own pacing
	// (see live_server.h's own top comment). Every viewport here always
	// tracks the newest sample (v1 scope is live-only, no historical
	// scrubbing yet -- see viewport.h's own comment on "fixed" for how a
	// future client could request a specific past index instead).
	auto frameInterval = std::chrono::duration<double>(1.0 / reader.fps());
	auto nextFrameTime = std::chrono::steady_clock::now();
	while (running)
	{
		bool gotFrame;
		{
			std::lock_guard<std::mutex> lock(liveServer.sourceMutex());
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

		nextFrameTime += std::chrono::duration_cast<std::chrono::steady_clock::duration>(frameInterval);
		std::this_thread::sleep_until(nextFrameTime);
	}

	if (selfTestWatchdog.joinable()) selfTestWatchdog.join();
	std::cout << "live_video_stream: shutting down" << std::endl;
	return 0;
}
