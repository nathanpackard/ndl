#include <gtest/gtest.h>
#include <array>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/viewer/ring_buffer.h>
#include <ndl/viewer/viewport.h>

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

namespace
{
	// A solid sphere (density 255 inside, 0 outside) in a {32,32,32}
	// volume, centered in the grid -- a shape simple enough to predict
	// renderVolume()'s own opacity/silhouette behavior by hand: a ray
	// through the center should saturate to (near-)opaque, a ray that
	// misses the sphere entirely should stay exactly black.
	RingBufferImage<uint8_t, 3> makeSphereVolume(int extentSize, double radius)
	{
		RingBufferImage<uint8_t, 3> ring({ extentSize, extentSize, extentSize }, 2);
		std::vector<uint8_t> frame((std::size_t)extentSize * extentSize * extentSize);
		double c = (extentSize - 1) / 2.0;
		std::size_t p = 0;
		for (int z = 0; z < extentSize; z++)
			for (int y = 0; y < extentSize; y++)
				for (int x = 0; x < extentSize; x++, p++)
				{
					double dx = x - c, dy = y - c, dz = z - c;
					frame[p] = (dx * dx + dy * dy + dz * dz <= radius * radius) ? 255 : 0;
				}
		ring.push(frame.data());
		return ring;
	}
}

TEST(Viewport, RenderVolumeSilhouetteMatchesSolidSphere) {
	std::stringstream passfail;
	std::cout << std::endl << "VIEWPORT -- renderVolume() SILHOUETTE (SOLID SPHERE)" << std::endl;

	auto sphere = makeSphereVolume(32, 10.0);

	JsonValue params = JsonValue::makeObject();
	params["axisA"] = 0; params["axisB"] = 1; params["axisC"] = 2;
	params["alphaScale"] = 5.0; // saturates quickly through a 20-voxel-diameter sphere
	params["windowMin"] = 0; params["windowMax"] = 255;
	params["outputWidth"] = 64; params["outputHeight"] = 64;

	RenderedFrame out = renderVolume(sphere, params);
	bool sizeOk = out.width == 64 && out.height == 64 && out.channels == 1;
	passfail << "output size matches the requested resolution, single channel (no colorAxis): " << (sizeOk ? "Pass" : "Fail") << std::endl;

	// Center ray: passes straight through the sphere's own diameter under
	// the default identity rotation -- should saturate close to opaque.
	int centerIdx = 32 * 64 + 32;
	bool centerBright = out.pixels[centerIdx] > 200;
	passfail << "a ray through the sphere's center reads bright (opaque, > 200/255): " << (centerBright ? "Pass" : "Fail") << " (got " << (int)out.pixels[centerIdx] << ")" << std::endl;

	// Corner ray: at vsX/vsY near -0.94, the ray's own closest approach to
	// the volume center is far outside the sphere's radius-10 silhouette
	// -- should never accumulate any opacity at all.
	int cornerIdx = 2 * 64 + 2;
	bool cornerBlack = out.pixels[cornerIdx] == 0;
	passfail << "a ray that misses the sphere entirely reads exactly black: " << (cornerBlack ? "Pass" : "Fail") << " (got " << (int)out.pixels[cornerIdx] << ")" << std::endl;

	reportPassFail(passfail);
}

TEST(Viewport, RenderVolumeRotationChangesWhatTheRaySees) {
	std::stringstream passfail;
	std::cout << std::endl << "VIEWPORT -- renderVolume() ROTATION AFFECTS THE RENDER" << std::endl;

	// A small solid cube OFF-CENTER along axis 0 only -- unlike a sphere,
	// this shape isn't rotation-symmetric, so rotating the volume should
	// visibly move where the center ray does/doesn't hit it, directly
	// proving the `rotation` param's column-major convention is wired
	// through correctly rather than just coincidentally passing a
	// symmetric-shape test.
	const int N = 32;
	RingBufferImage<uint8_t, 3> ring({ N, N, N }, 2);
	std::vector<uint8_t> frame((std::size_t)N * N * N, 0);
	for (int z = 10; z < 22; z++)
		for (int y = 10; y < 22; y++)
			for (int x = 22; x < 30; x++) // offset toward the +x edge
				frame[(std::size_t)z * N * N + y * N + x] = 255;
	ring.push(frame.data());

	JsonValue baseParams = JsonValue::makeObject();
	baseParams["axisA"] = 0; baseParams["axisB"] = 1; baseParams["axisC"] = 2;
	baseParams["alphaScale"] = 5.0;
	baseParams["windowMin"] = 0; baseParams["windowMax"] = 255;
	baseParams["outputWidth"] = 64; baseParams["outputHeight"] = 64;

	// Identity rotation: the center ray (straight down local Z) passes
	// through the volume's own center, x=16 -- OUTSIDE the cube (x in
	// [22,30)) -- so it should stay black.
	RenderedFrame identityRender = renderVolume(ring, baseParams);
	int centerIdx = 32 * 64 + 32;
	bool identityMisses = identityRender.pixels[centerIdx] == 0;
	passfail << "identity rotation: center ray passes through x=16, outside the offset cube, reads black: " << (identityMisses ? "Pass" : "Fail") << " (got " << (int)identityRender.pixels[centerIdx] << ")" << std::endl;

	// A 90-degree rotation about Y (column-major, matching web/
	// ndlviewer.js's own mat3RotationY(): [c,0,-s, 0,1,0, s,0,c]) turns
	// the local +Z ray direction into local +X -- so the SAME straight-
	// down-the-screen center ray now marches along what WAS the volume's
	// own x-axis, sweeping straight through the cube's own x in [22,30)
	// region instead of missing it.
	JsonValue rotatedParams = baseParams;
	JsonValue rotation = JsonValue::makeArray();
	double rot90Y[9] = { 0, 0, -1, 0, 1, 0, 1, 0, 0 }; // c=cos(90)=0, s=sin(90)=1
	for (double v : rot90Y) rotation.push_back(v);
	rotatedParams["rotation"] = rotation;

	RenderedFrame rotatedRender = renderVolume(ring, rotatedParams);
	bool rotatedHits = rotatedRender.pixels[centerIdx] > 200;
	passfail << "90-degree rotation about Y: the SAME center ray now sweeps through the cube, reads bright: " << (rotatedHits ? "Pass" : "Fail") << " (got " << (int)rotatedRender.pixels[centerIdx] << ")" << std::endl;

	reportPassFail(passfail);
}

TEST(Viewport, RenderVolumeColorAxisCompositesTrueColor) {
	std::stringstream passfail;
	std::cout << std::endl << "VIEWPORT -- renderVolume() colorAxis TRUE-COLOR COMPOSITING" << std::endl;

	// {channel=3, x=16, y=16, z=16}: a solid red sphere (channel 0 = 255,
	// channels 1/2 = 0, inside the sphere; all 0 outside).
	const int N = 16;
	RingBufferImage<uint8_t, 4> ring({ 3, N, N, N }, 3);
	std::vector<uint8_t> frame((std::size_t)3 * N * N * N, 0);
	double c = (N - 1) / 2.0, radius = 5.0;
	for (int z = 0; z < N; z++)
		for (int y = 0; y < N; y++)
			for (int x = 0; x < N; x++)
			{
				double dx = x - c, dy = y - c, dz = z - c;
				if (dx * dx + dy * dy + dz * dz <= radius * radius)
				{
					std::size_t base = ((std::size_t)z * N * N + (std::size_t)y * N + x) * 3;
					frame[base + 0] = 255; // R
				}
			}
	ring.push(frame.data());

	JsonValue params = JsonValue::makeObject();
	params["axisA"] = 1; params["axisB"] = 2; params["axisC"] = 3;
	params["colorAxis"] = 0;
	params["alphaScale"] = 5.0;
	params["windowMin"] = 0; params["windowMax"] = 255;
	params["outputWidth"] = 32; params["outputHeight"] = 32;

	RenderedFrame out = renderVolume(ring, params);
	bool sizeOk = out.width == 32 && out.height == 32 && out.channels == 3;
	passfail << "colorAxis given: output is 3-channel: " << (sizeOk ? "Pass" : "Fail") << std::endl;

	int centerIdx = (16 * 32 + 16) * 3;
	int red = out.pixels[centerIdx], green = out.pixels[centerIdx + 1], blue = out.pixels[centerIdx + 2];
	bool redDominates = red > 100 && red > green * 2 && red > blue * 2;
	passfail << "a ray through the red sphere's center reads predominantly red (R > 2*G, R > 2*B): " << (redDominates ? "Pass" : "Fail") << " (got R=" << red << " G=" << green << " B=" << blue << ")" << std::endl;

	reportPassFail(passfail);
}

namespace
{
	// A uniform {channel=N, x=4, y=4} source -- every spatial position has
	// the SAME channel values, so any pixel in a renderSlice() result
	// reflects reduceChannels()'s own output directly, with no crop/
	// position bookkeeping to get right first.
	OwnedImage<uint8_t, 3> makeUniformChannelSource(const std::vector<uint8_t>& channelValues)
	{
		int n = (int)channelValues.size();
		OwnedImage<uint8_t, 3> src({ n, 4, 4 });
		for (const auto& c : src.coordinates()) src.at(c) = channelValues[c[0]];
		return src;
	}

	JsonValue sliceParamsWithReduction(const std::string& mode, double windowMax)
	{
		JsonValue params = JsonValue::makeObject();
		params["axisI"] = 1; params["axisJ"] = 2;
		params["colorAxis"] = 0;
		params["channelReduction"] = mode;
		params["windowMin"] = 0; params["windowMax"] = windowMax;
		return params;
	}
}

TEST(Viewport, ChannelReductionMagnitude) {
	std::stringstream passfail;
	std::cout << std::endl << "VIEWPORT -- channelReduction \"magnitude\"" << std::endl;

	auto src = makeUniformChannelSource({ 3, 4 }); // magnitude = sqrt(9+16) = 5
	RenderedFrame out = renderSlice(src, sliceParamsWithReduction("magnitude", 255));
	bool ok = out.channels == 1 && out.pixels[0] == 5;
	passfail << "magnitude of (3,4) reduces to a single-channel value of 5: " << (ok ? "Pass" : "Fail") << " (channels=" << out.channels << ", got " << (int)out.pixels[0] << ")" << std::endl;

	reportPassFail(passfail);
}

TEST(Viewport, ChannelReductionPhase) {
	std::stringstream passfail;
	std::cout << std::endl << "VIEWPORT -- channelReduction \"phase\"" << std::endl;

	auto src = makeUniformChannelSource({ 1, 1 }); // atan2(1,1) = pi/4
	RenderedFrame out = renderSlice(src, sliceParamsWithReduction("phase", 3.14159265358979));
	// (pi/4) / pi * 255 = 63.75 -> rounds to 64.
	bool ok = out.channels == 1 && out.pixels[0] == 64;
	passfail << "phase of (real=1,imag=1) reduces to atan2(1,1)=pi/4, windowed [0,pi] -> 64: " << (ok ? "Pass" : "Fail") << " (channels=" << out.channels << ", got " << (int)out.pixels[0] << ")" << std::endl;

	reportPassFail(passfail);
}

TEST(Viewport, ChannelReductionSumMeanMax) {
	std::stringstream passfail;
	std::cout << std::endl << "VIEWPORT -- channelReduction \"sum\"/\"mean\"/\"max\"" << std::endl;

	auto src = makeUniformChannelSource({ 10, 20, 30 });

	RenderedFrame sumOut = renderSlice(src, sliceParamsWithReduction("sum", 255));
	bool sumOk = sumOut.channels == 1 && sumOut.pixels[0] == 60;
	passfail << "sum of (10,20,30) = 60: " << (sumOk ? "Pass" : "Fail") << " (got " << (int)sumOut.pixels[0] << ")" << std::endl;

	RenderedFrame meanOut = renderSlice(src, sliceParamsWithReduction("mean", 255));
	bool meanOk = meanOut.channels == 1 && meanOut.pixels[0] == 20;
	passfail << "mean of (10,20,30) = 20: " << (meanOk ? "Pass" : "Fail") << " (got " << (int)meanOut.pixels[0] << ")" << std::endl;

	RenderedFrame maxOut = renderSlice(src, sliceParamsWithReduction("max", 255));
	bool maxOk = maxOut.channels == 1 && maxOut.pixels[0] == 30;
	passfail << "max of (10,20,30) = 30: " << (maxOk ? "Pass" : "Fail") << " (got " << (int)maxOut.pixels[0] << ")" << std::endl;

	reportPassFail(passfail);
}

TEST(Viewport, ChannelReductionRejectsWrongChannelCountsAndUnknownModes) {
	std::stringstream passfail;
	std::cout << std::endl << "VIEWPORT -- channelReduction VALIDATION" << std::endl;

	auto src2 = makeUniformChannelSource({ 1, 2 });    // N=2
	auto src3 = makeUniformChannelSource({ 1, 2, 3 }); // N=3

	auto throwsInvalidArgument = [](const OwnedImage<uint8_t, 3>& s, const JsonValue& p) {
		try { renderSlice(s, p); return false; }
		catch (const std::invalid_argument&) { return true; }
	};

	bool rgbWrongCount = throwsInvalidArgument(src2, sliceParamsWithReduction("rgb", 255));
	passfail << "\"rgb\" mode with a 2-channel colorAxis throws: " << (rgbWrongCount ? "Pass" : "Fail") << std::endl;

	bool phaseWrongCount = throwsInvalidArgument(src3, sliceParamsWithReduction("phase", 255));
	passfail << "\"phase\" mode with a 3-channel colorAxis throws: " << (phaseWrongCount ? "Pass" : "Fail") << std::endl;

	bool unknownMode = throwsInvalidArgument(src3, sliceParamsWithReduction("not_a_real_mode", 255));
	passfail << "an unrecognized channelReduction mode throws: " << (unknownMode ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Viewport, RenderVolumeHonorsChannelReduction) {
	std::stringstream passfail;
	std::cout << std::endl << "VIEWPORT -- renderVolume() channelReduction" << std::endl;

	// A small uniform-density 2-channel {channel=2,x,y,z} volume -- just a
	// structural check (right channel COUNT out) rather than an exact
	// pixel value, since predicting renderVolume()'s own multi-sample
	// Beer-Lambert accumulation by hand is a much bigger derivation than
	// reduceChannels() itself already gets directly via renderSlice()
	// above.
	const int N = 8;
	RingBufferImage<uint8_t, 4> ring({ 2, N, N, N }, 3);
	std::vector<uint8_t> frame((std::size_t)2 * N * N * N, 100);
	ring.push(frame.data());

	JsonValue params = JsonValue::makeObject();
	params["axisA"] = 1; params["axisB"] = 2; params["axisC"] = 3;
	params["colorAxis"] = 0;
	params["channelReduction"] = "magnitude";
	params["alphaScale"] = 5.0;
	params["windowMin"] = 0; params["windowMax"] = 255;
	params["outputWidth"] = 16; params["outputHeight"] = 16;

	RenderedFrame out = renderVolume(ring, params);
	bool ok = out.channels == 1;
	passfail << "renderVolume() with channelReduction=\"magnitude\" produces a single-channel result: " << (ok ? "Pass" : "Fail") << " (got channels=" << out.channels << ")" << std::endl;

	reportPassFail(passfail);
}

TEST(Viewport, QueryValueReturnsRawUnwindowedValue) {
	std::stringstream passfail;
	std::cout << std::endl << "VIEWPORT -- queryValue() (native-value hover)" << std::endl;

	auto source = makeTestSource(); // {x=6,y=4,t=2}, value = 10 + t*50 + x + y*W

	// std::array overload: exact coordinate, well within range.
	double v1 = queryValue(source, std::array<int, 3>{ 2, 1, 1 });
	bool v1Ok = v1 == (10 + 50 + 2 + 1 * 6);
	passfail << "std::array overload returns the exact raw source value (no windowing/quantization): " << (v1Ok ? "Pass" : "Fail") << " (got " << v1 << ")" << std::endl;

	// Out-of-range coordinate clamps rather than throwing/crashing: x=999
	// clamps to 5 (extent 6, valid range [0,5]), y=-5 clamps to 0, t=0
	// stays 0 -> 10 + 0*50 + 5 + 0*6 = 15.
	double v2 = queryValue(source, std::array<int, 3>{ 999, -5, 0 });
	double expectedClamped = 15.0;
	bool v2Ok = v2 == expectedClamped;
	passfail << "an out-of-range coordinate clamps into the source's own extent rather than throwing: " << (v2Ok ? "Pass" : "Fail") << " (got " << v2 << ", expected " << expectedClamped << ")" << std::endl;

	// JsonValue overload, mirroring the actual queryValue wire message shape.
	JsonValue message = JsonValue::makeObject();
	JsonValue coord = JsonValue::makeArray();
	coord.push_back(2); coord.push_back(1); coord.push_back(1);
	message["coord"] = coord;
	double v3 = queryValue(source, message);
	bool v3Ok = v3 == v1;
	passfail << "JsonValue overload (reading \"coord\" the way the real wire message does) matches the std::array overload: " << (v3Ok ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}
