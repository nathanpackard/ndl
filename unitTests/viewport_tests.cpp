#include <gtest/gtest.h>
#include <array>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/ring_buffer.h>
#include <ndl/viewport.h>

#include "testHelpers.h"

using namespace ndl;
using namespace ndl::net;

namespace
{
	// A small {x=6, y=4, t=2} ring-buffered source (ring axis = 2), value
	// = 10 + t*50 + x + y*W -- distinct per frame (t) and per pixel, so a
	// test can tell apart "wrong axis," "wrong crop offset," and "wrong
	// fixed-axis selection" failures from each other rather than just
	// "some byte differs somewhere."
	RingBufferImage<uint8_t, 3> makeTestSource()
	{
		const int W = 6, H = 4, T = 2;
		RingBufferImage<uint8_t, 3> ring({ W, H, T }, 2);
		for (int t = 0; t < T; t++)
		{
			std::vector<uint8_t> frame((std::size_t)W * H);
			for (int y = 0; y < H; y++)
				for (int x = 0; x < W; x++)
					frame[(std::size_t)y * W + x] = (uint8_t)(10 + t * 50 + x + y * W);
			ring.push(frame.data());
		}
		return ring;
	}
}

TEST(Viewport, RenderSliceExtractsExactCropAndFixedAxis) {
	std::stringstream passfail;
	std::cout << std::endl << "VIEWPORT -- renderSlice() CROP REGION + FIXED AXIS" << std::endl;

	auto source = makeTestSource();
	const int W = 6;

	JsonValue params = JsonValue::makeObject();
	params["axisI"] = 0; params["axisJ"] = 1;
	JsonValue cropMin = JsonValue::makeArray(); cropMin.push_back(1); cropMin.push_back(1);
	JsonValue cropMax = JsonValue::makeArray(); cropMax.push_back(4); cropMax.push_back(3);
	params["cropMin"] = cropMin; params["cropMax"] = cropMax;
	JsonValue fixed = JsonValue::makeObject(); fixed["2"] = 1; // select frame t=1
	params["fixed"] = fixed;
	params["windowMin"] = 0; params["windowMax"] = 255;

	RenderedFrame out = renderSlice(source, params);
	bool sizeOk = out.width == 3 && out.height == 2 && out.channels == 1;
	passfail << "output size matches the requested crop region (3x2), single channel: " << (sizeOk ? "Pass" : "Fail") << std::endl;

	// Expected raw value at crop-local (dx,dy) is source's own formula at
	// (x=1+dx, y=1+dy, t=1): 10 + 50 + (1+dx) + (1+dy)*W. windowMin=0/
	// windowMax=255 is an identity mapping here since every value is well
	// under 255.
	bool valuesOk = true;
	for (int dy = 0; dy < 2 && valuesOk; dy++)
		for (int dx = 0; dx < 3 && valuesOk; dx++)
		{
			int expected = 10 + 50 + (1 + dx) + (1 + dy) * W;
			int got = out.pixels[(std::size_t)dy * 3 + dx];
			if (got != expected) valuesOk = false;
		}
	passfail << "every pixel matches the source's own value at (croppedX, croppedY, fixed t=1): " << (valuesOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Viewport, RenderSliceWindowMappingClampsAndScales) {
	std::stringstream passfail;
	std::cout << std::endl << "VIEWPORT -- renderSlice() WINDOW/LEVEL MAPPING" << std::endl;

	auto source = makeTestSource();

	// t=0, full W x H (values 10..10+5+3*6=33), windowed to [15,25]:
	// anything <=15 maps to 0, anything >=25 maps to 255, in between
	// scales linearly.
	JsonValue params = JsonValue::makeObject();
	params["axisI"] = 0; params["axisJ"] = 1;
	JsonValue fixed = JsonValue::makeObject(); fixed["2"] = 0;
	params["fixed"] = fixed;
	params["windowMin"] = 15; params["windowMax"] = 25;

	RenderedFrame out = renderSlice(source, params);
	// (x=0,y=0) -> raw value 10 -> below windowMin -> 0
	bool belowOk = out.pixels[0] == 0;
	// (x=5,y=3) -> raw value 10+5+3*6=33 -> above windowMax -> 255
	bool aboveOk = out.pixels[(std::size_t)3 * 6 + 5] == 255;
	// (x=5,y=0) -> raw value 15 -> exactly windowMin -> 0
	bool atMinOk = out.pixels[5] == 0;
	passfail << "a value below windowMin clamps to 0: " << (belowOk ? "Pass" : "Fail") << std::endl;
	passfail << "a value above windowMax clamps to 255: " << (aboveOk ? "Pass" : "Fail") << std::endl;
	passfail << "a value exactly at windowMin maps to 0: " << (atMinOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Viewport, RenderSliceNeverUpsamples) {
	std::stringstream passfail;
	std::cout << std::endl << "VIEWPORT -- renderSlice() NEVER UPSAMPLES" << std::endl;

	auto source = makeTestSource();
	JsonValue params = JsonValue::makeObject();
	params["axisI"] = 0; params["axisJ"] = 1;
	JsonValue fixed = JsonValue::makeObject(); fixed["2"] = 0;
	params["fixed"] = fixed;
	params["outputWidth"] = 999; params["outputHeight"] = 999; // far bigger than the 6x4 native crop

	RenderedFrame out = renderSlice(source, params);
	bool ok = out.width == 6 && out.height == 4;
	passfail << "requesting a larger output than the crop's native size returns the native size unchanged (no upsampling): " << (ok ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Viewport, RenderSliceDownsamplesAtNearestIntegerFactor) {
	std::stringstream passfail;
	std::cout << std::endl << "VIEWPORT -- renderSlice() DOWNSAMPLE-FACTOR RESOLUTION" << std::endl;

	// An 8x8 single-frame source; request output 3x3. factor =
	// max(1,min(8/3,8/3)) = 2 (integer division), and ndl::downsample()'s
	// own formula for the resulting extent is (extent-1)/factor+1 = 4 --
	// NOT an exact match to the requested 3x3, which is exactly the
	// documented "actual dimensions may not match the request" behavior.
	RingBufferImage<uint8_t, 3> ring({ 8, 8, 1 }, 2);
	std::vector<uint8_t> frame(64, 100);
	ring.push(frame.data());

	JsonValue params = JsonValue::makeObject();
	params["axisI"] = 0; params["axisJ"] = 1;
	JsonValue fixed = JsonValue::makeObject(); fixed["2"] = 0;
	params["fixed"] = fixed;
	params["outputWidth"] = 3; params["outputHeight"] = 3;

	RenderedFrame out = renderSlice(ring, params);
	bool ok = out.width == 4 && out.height == 4;
	passfail << "an 8x8 crop downsampled toward 3x3 lands at 4x4, matching ndl::downsample()'s own integer-factor formula: " << (ok ? "Pass" : "Fail") << " (got " << out.width << "x" << out.height << ")" << std::endl;

	reportPassFail(passfail);
}

TEST(Viewport, RendererRegistryDispatchesByOpName) {
	std::stringstream passfail;
	std::cout << std::endl << "VIEWPORT -- RendererRegistry DISPATCH" << std::endl;

	auto source = makeTestSource();
	RendererRegistry<RingBufferImage<uint8_t, 3>> registry;
	registry.registerOp("slice", [](const RingBufferImage<uint8_t, 3>& src, const JsonValue& p) { return renderSlice(src, p); });

	bool hasOk = registry.has("slice") && !registry.has("nonexistent");
	passfail << "has() correctly reports registered vs. unregistered op names: " << (hasOk ? "Pass" : "Fail") << std::endl;

	JsonValue params = JsonValue::makeObject();
	params["axisI"] = 0; params["axisJ"] = 1;
	JsonValue fixed = JsonValue::makeObject(); fixed["2"] = 0;
	params["fixed"] = fixed;
	RenderedFrame out = registry.render("slice", source, params);
	bool renderedOk = out.width == 6 && out.height == 4;
	passfail << "render() dispatches to the registered function and returns its result: " << (renderedOk ? "Pass" : "Fail") << std::endl;

	bool threwOnUnknown = false;
	try { registry.render("nonexistent", source, params); }
	catch (const std::invalid_argument&) { threwOnUnknown = true; }
	passfail << "render() throws for an unregistered op name: " << (threwOnUnknown ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Viewport, ParamsAndPendingOutputAreLatestWins) {
	std::stringstream passfail;
	std::cout << std::endl << "VIEWPORT -- Viewport PARAMS/PENDING-OUTPUT ARE \"LATEST WINS\"" << std::endl;

	Viewport vp("panelA", "slice");
	bool idOk = vp.id() == "panelA" && vp.op() == "slice";
	passfail << "id()/op() return what the Viewport was constructed with: " << (idOk ? "Pass" : "Fail") << std::endl;

	JsonValue p1 = JsonValue::makeObject(); p1["axisI"] = 0;
	JsonValue p2 = JsonValue::makeObject(); p2["axisI"] = 1;
	vp.setParams(p1);
	vp.setParams(p2); // overwrites p1 -- never queued
	bool paramsOk = vp.params()["axisI"].asInt() == 1;
	passfail << "setParams() called twice leaves only the LATEST params (no queueing): " << (paramsOk ? "Pass" : "Fail") << std::endl;

	RenderedFrame f1; f1.width = 10;
	RenderedFrame f2; f2.width = 20;
	vp.setPendingOutput(std::move(f1));
	vp.setPendingOutput(std::move(f2)); // overwrites the not-yet-taken f1

	RenderedFrame taken;
	bool tookLatest = vp.takePendingOutput(taken) && taken.width == 20;
	passfail << "takePendingOutput() after two overwriting setPendingOutput() calls returns only the LATEST frame: " << (tookLatest ? "Pass" : "Fail") << std::endl;

	bool secondTakeEmpty = !vp.takePendingOutput(taken);
	passfail << "a second takePendingOutput() with nothing new set returns false: " << (secondTakeEmpty ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}
