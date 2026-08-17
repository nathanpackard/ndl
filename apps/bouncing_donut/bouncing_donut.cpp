// bouncing_donut: a second, non-video proof of ndl's viewport-driven live
// streaming design (ring_buffer.h + viewer/live_server.h), and the
// concrete end-to-end demonstration of renderVolume() (viewer/viewport.h)
// actually streaming live. Unlike live_video_stream, there is no source
// file at all: each tick, a torus signed-distance field is evaluated
// directly on a GRID x GRID x GRID grid and written straight into a
// RingBufferImage's own nextWriteSlot() -- proving the ring buffer/
// viewport pipeline is genuinely generic (any source that can produce one
// "frame" at a time), not video-specific. The torus bounces vertically
// like a dropped ball (centerY = floor + amplitude*|sin(t*bounceSpeed)|)
// while slowly precessing around its own vertical axis at a fixed tilt,
// so a live "volume"-op viewport actually has something worth rotating to
// look at. Registers both "volume" (the primary op -- this is the whole
// point) and "slice" (a plain 2D cross-section, useful for comparing
// against/debugging the volume render) from viewer/viewport.h's own
// RendererRegistry. All of the actual server-side plumbing lives in
// viewer/live_server.h's own LiveStreamServer -- shared with
// apps/live_video_stream.cpp, since the two turned out to need nearly
// identical plumbing around genuinely different "how does a new sample
// get produced" steps.
//
// Usage: bouncing_donut [--port <port>] [--help] -- prints the URL to open
// once listening (see net/websocket_server.h's own staticRoot support).
#include <ndl/viewer/ring_buffer.h>
#include <ndl/viewer/live_server.h>
#include <ndl/processing/matrix/transform.h>
#include "../appHelpers.h"

#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <cmath>
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
	// mat3TransposeMultiplyVec3() uses for click-to-navigate. Both
	// rotations are built via ndl::make_rotate_matrix() (processing/
	// matrix/transform.h) -- the already-general "rotate in the plane
	// spanned by axis1/axis2" builder every N-D rotation decomposes into
	// -- rather than hand-rolling a 3x3 sine/cosine layout a second time:
	// axes (1,2) is the X-axis rotation (rotates the Y-Z plane), axes
	// (2,0) is the Y-axis one (rotates the Z-X plane), the same
	// correspondence make_rotate_matrix()'s own default axes (0,1)
	// documents for a Z-axis rotation.
	void generateFrame(uint8_t* frame, double t)
	{
		const double majorR = 0.35, minorR = 0.14, edge = 0.12;
		const double tilt = 0.6, precessSpeed = 0.4;
		// floorY/amplitude: chosen so the torus's own CENTER travels from
		// near the cube's actual floor up to mid-height, not just wobbling
		// in the middle. floorY is POSITIVE (near +1, not -1) and "bounce
		// up" SUBTRACTS from it -- this looks backwards next to a bare
		// physics formula, but it's required by this project's own
		// established rendering convention (see renderVolume()'s own
		// "vsY: -1 at screen-top, +1 at screen-bottom" comment,
		// viewer/viewport.h): under the identity rotation this app starts
		// at, a LOW raw index along axisB (near ny=-1) renders at
		// SCREEN-TOP, a HIGH one (near ny=+1) at SCREEN-BOTTOM -- so
		// "resting on the floor" (screen-bottom, visually down) has to
		// mean HIGH centerY, and "bouncing up" (screen-top) means
		// DECREASING centerY, the exact opposite of the first version's
		// own (buggy) floorY=-0.75/`+amplitude*|sin|` formula, which
		// rendered the bounce upside-down. floorY=0.62 leaves a margin of
		// roughly majorR+minorR (~0.49, the torus's own worst-case radius
		// from its center at a steep tilt) before the +1 wall, so it
		// reads as genuinely touching down rather than floating OR
		// clipping through the cube's own boundary (an earlier,
		// insufficient margin produced a visible flat-cut artifact right
		// at the bottom of each bounce).
		const double floorY = 0.62, amplitude = 0.45, bounceSpeed = 1.3;

		double centerY = floorY - amplitude * std::fabs(std::sin(t * bounceSpeed));
		Matrix<double, 3> Ry, Rx;
		make_rotate_matrix(Ry, t * precessSpeed, 2, 0);
		make_rotate_matrix(Rx, tilt, 1, 2);
		Matrix<double, 3> Rt = (Ry * Rx).transpose();

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
	std::atomic<bool> running{ true };
	apps::installSigintHandler(running);

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

	LiveStreamServer<RingBufferImage<uint8_t, 4>> liveServer("bouncing_donut", volume, std::move(registry), RING_AXIS, "volume", port, NDL_REPO_ROOT_DIR);
	std::cout << "bouncing_donut: open " << liveServer.url("/apps/bouncing_donut/bouncing_donut.html") << std::endl;

	std::thread selfTestWatchdog = apps::selfTestWatchdog("NDL_BOUNCING_DONUT_SELFTEST", running);

	// Main loop: generate one new density frame per tick (no file, no
	// reader -- generateFrame() IS the source) and push it into the ring
	// -- LiveStreamServer's own background thread notices and re-renders
	// every currently active viewport on its own, decoupled from this
	// loop's own pacing (see live_server.h's own top comment: this matters
	// MORE here than in live_video_stream, since renderVolume()'s own CPU
	// ray-march is genuinely expensive -- decoupling means a slow render
	// no longer throttles the torus's own animation clock `t`, only how
	// often a freshly-rendered frame actually reaches a client). Every
	// viewport always tracks the newest sample, same as live_video_stream.
	auto frameInterval = std::chrono::duration<double>(1.0 / targetFps);
	auto nextFrameTime = std::chrono::steady_clock::now();
	auto startTime = std::chrono::steady_clock::now();
	while (running)
	{
		double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
		{
			std::lock_guard<std::mutex> lock(liveServer.sourceMutex());
			generateFrame(volume.nextWriteSlot(), t);
			volume.commitWrite();
		}

		nextFrameTime += std::chrono::duration_cast<std::chrono::steady_clock::duration>(frameInterval);
		std::this_thread::sleep_until(nextFrameTime);
	}

	if (selfTestWatchdog.joinable()) selfTestWatchdog.join();
	std::cout << "bouncing_donut: shutting down" << std::endl;
	return 0;
}
