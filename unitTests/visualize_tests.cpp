#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iostream>
#include <array>

#include <ndl/image.h>
#include <ndl/processing/visualize.h>

#include "testHelpers.h"

using namespace ndl;

TEST(Visualize, BarChartMultiChannel) {
	std::stringstream passfail;
	std::cout << std::endl << "BAR_CHART (MULTI-CHANNEL DESTINATION)" << std::endl;

	// 4 bins: 1,2,4,4 over an 8x8 canvas -> bin i spans columns [2i,2i+1];
	// heights scale to the max (4), so bin 0 (1/4) reaches row 6 (of 8),
	// bin 2/3 (4/4, the max) reach the very top row.
	std::vector<int> srcData = { 1, 2, 4, 4 };
	Image<int, 1> src(srcData.data(), { 4 });
	std::vector<uint8_t> dstData(3 * 8 * 8);
	Image<uint8_t, 3> dst(dstData.data(), { 3, 8, 8 });
	bar_chart(src, dst, (uint8_t)255, (uint8_t)0);

	bool shortBinCorrect = true, tallBinCorrect = true, allChannelsMatch = true;
	for (int x = 0; x <= 1; x++) // bin 0: height = 1/4 * 8 = 2
		for (int y = 0; y < 8; y++)
		{
			uint8_t expected = (y >= 6) ? 255 : 0;
			for (int c = 0; c < 3; c++)
			{
				if (dst(c, x, y) != expected) shortBinCorrect = false;
				if (dst(c, x, y) != dst(0, x, y)) allChannelsMatch = false;
			}
		}
	for (int x = 4; x <= 5; x++) // bin 2: height = 4/4 * 8 = 8 (full column)
		for (int y = 0; y < 8; y++)
			for (int c = 0; c < 3; c++)
				if (dst(c, x, y) != 255) tallBinCorrect = false;

	passfail << "a short bin's column is background above its bar and barValue below it: " << (shortBinCorrect ? "Pass" : "Fail") << std::endl;
	passfail << "the tallest bin (matching the max) fills its column completely: " << (tallBinCorrect ? "Pass" : "Fail") << std::endl;
	passfail << "every channel gets the same value at a given (x,y) -- no single-channel tinting bug: " << (allChannelsMatch ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Visualize, BarChartSingleChannelAndEdgeCases) {
	std::stringstream passfail;
	std::cout << std::endl << "BAR_CHART (SINGLE-CHANNEL, EDGE CASES)" << std::endl;

	// DstDIM==2 (no channel axis at all) works the same way as DstDIM==3.
	std::vector<int> srcData = { 1, 3 };
	Image<int, 1> src(srcData.data(), { 2 });
	std::vector<uint8_t> dstData(4 * 6);
	Image<uint8_t, 2> dst(dstData.data(), { 4, 6 });
	bar_chart(src, dst, (uint8_t)9, (uint8_t)0);
	bool fullColumnCorrect = true;
	for (int x = 2; x <= 3; x++) // bin 1: 3/3 == max, full height
		for (int y = 0; y < 6; y++)
			if (dst(x, y) != 9) fullColumnCorrect = false;
	passfail << "single-channel (DstDIM==2) destination works the same way: " << (fullColumnCorrect ? "Pass" : "Fail") << std::endl;

	// All-zero source: no crash, stays entirely background.
	std::vector<int> zeroData = { 0, 0, 0 };
	Image<int, 1> zeroSrc(zeroData.data(), { 3 });
	std::vector<uint8_t> zeroDstData(3 * 6 * 6, 0xFF); // pre-fill with a sentinel so we can tell it was actually written
	Image<uint8_t, 3> zeroDst(zeroDstData.data(), { 3, 6, 6 });
	bar_chart(zeroSrc, zeroDst, (uint8_t)255, (uint8_t)7);
	bool allBackground = true;
	for (auto v : zeroDstData) if (v != 7) allBackground = false;
	passfail << "an all-zero source draws no bars at all (stays entirely background, no crash): " << (allBackground ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Visualize, HeatmapMultiChannel) {
	std::stringstream passfail;
	std::cout << std::endl << "HEATMAP (MULTI-CHANNEL DESTINATION)" << std::endl;

	std::vector<int> srcData = { 0, 10, 20, 40 }; // 2x2: (0,0)=0 (1,0)=10 (0,1)=20 (1,1)=40
	Image<int, 2> src(srcData.data(), { 2, 2 });
	std::vector<uint8_t> dstData(3 * 2 * 2);
	Image<uint8_t, 3> dst(dstData.data(), { 3, 2, 2 });
	heatmap(src, dst, (uint8_t)200);

	bool scaledCorrectly = true, allChannelsMatch = true;
	auto check = [&](int x, int y, uint8_t expected) {
		for (int c = 0; c < 3; c++)
		{
			if (dst(c, x, y) != expected) scaledCorrectly = false;
			if (dst(c, x, y) != dst(0, x, y)) allChannelsMatch = false;
		}
	};
	check(0, 0, 0);   // 0/40 * 200 = 0
	check(1, 0, 50);  // 10/40 * 200 = 50
	check(0, 1, 100); // 20/40 * 200 = 100
	check(1, 1, 200); // 40/40 * 200 = 200 (the max)

	passfail << "each pixel scales linearly to the source's own max, mapped onto [0,peakValue]: " << (scaledCorrectly ? "Pass" : "Fail") << std::endl;
	passfail << "every channel gets the same value at a given (x,y) -- no single-channel tinting bug: " << (allChannelsMatch ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Visualize, FlowMagnitude) {
	std::stringstream passfail;
	std::cout << std::endl << "FLOW_MAGNITUDE" << std::endl;

	// A 2D {2,W,H} field: a 3-4-5 triangle (magnitude exactly 5) at one
	// position, zero everywhere else -- checked against an exact value,
	// not just "some positive number came out".
	const int W = 3, H = 2;
	std::vector<double> flowData(2 * W * H, 0.0);
	Image<double, 3> flow(flowData.data(), { 2, W, H });
	flow(0, 1, 0) = 3.0;
	flow(1, 1, 0) = 4.0;

	std::vector<double> magData((std::size_t)W * H);
	Image<double, 2> mag(magData.data(), { W, H });
	flow_magnitude(flow, mag);

	passfail << "3-4-5 triangle position has magnitude exactly 5: " << (mag(1, 0) == 5.0 ? "Pass" : "Fail") << std::endl;
	passfail << "zero-flow position has magnitude exactly 0: " << (mag(0, 0) == 0.0 ? "Pass" : "Fail") << std::endl;

	// A 3-component field (not just 2D flow -- flow_magnitude() is
	// deliberately general over component count, unlike flow_to_color()/
	// flow_to_arrows()): a 3D vector {2,3,6} has magnitude 7 (2^2+3^2+6^2=49).
	std::vector<double> flow3Data(3 * 4, 0.0);
	Image<double, 2> flow3(flow3Data.data(), { 3, 4 }); // {component, spatial}
	flow3(0, 2) = 2.0; flow3(1, 2) = 3.0; flow3(2, 2) = 6.0;

	std::vector<double> mag3Data(4);
	Image<double, 1> mag3(mag3Data.data(), { 4 });
	flow_magnitude(flow3, mag3);
	passfail << "3-component vector field: {2,3,6} has magnitude exactly 7: " << (mag3(2) == 7.0 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Visualize, FlowToColor) {
	std::stringstream passfail;
	std::cout << std::endl << "FLOW_TO_COLOR" << std::endl;

	const int W = 4, H = 4;
	std::vector<double> flowData(2 * W * H, 0.0);
	Image<double, 3> flow(flowData.data(), { 2, W, H });
	// (0,0): pure rightward (angle 0), at the field's own max magnitude.
	flow(0, 0, 0) = 5.0;
	flow(1, 0, 0) = 0.0;
	// (1,1): zero flow -- should render as black regardless of hue.

	OwnedImage<uint8_t, 3> color({ 3, W, H });
	flow_to_color(flow, color, (uint8_t)255);

	bool zeroIsBlack = color(0, 1, 1) == 0 && color(1, 1, 1) == 0 && color(2, 1, 1) == 0;
	passfail << "zero-magnitude flow renders as black regardless of hue: " << (zeroIsBlack ? "Pass" : "Fail") << std::endl;

	int maxChannel = std::max({ (int)color(0, 0, 0), (int)color(1, 0, 0), (int)color(2, 0, 0) });
	passfail << "the field's own max-magnitude position reaches full brightness: " << (maxChannel >= 250 ? "Pass" : "Fail") << std::endl;

	// Rightward flow (angle 0) maps to hue 0.5 (cyan: R=0, G=B=max) under
	// this function's own (angle+pi)/(2*pi) convention -- checked directly
	// rather than just "some color came out", so a hue-mapping regression
	// (e.g. an accidental sign flip) would actually be caught.
	bool correctHue = color(0, 0, 0) == 0 && color(1, 0, 0) > 250 && color(2, 0, 0) > 250;
	passfail << "rightward flow (angle 0) maps to cyan (hue 0.5), matching this function's own angle convention: " << (correctHue ? "Pass" : "Fail") << std::endl;

	// A single outlier-magnitude vector shouldn't wash out every other
	// vector's brightness when an explicit magnitudeCap sidesteps it --
	// the same problem windowed_heatmap() (below) solves for a plain
	// scalar array.
	flow(0, 2, 2) = 1000.0; // one wild outlier vector at (2,2)
	flow(1, 2, 2) = 0.0;
	OwnedImage<uint8_t, 3> autoColor({ 3, W, H });
	flow_to_color(flow, autoColor, (uint8_t)255);
	OwnedImage<uint8_t, 3> cappedColor({ 3, W, H });
	flow_to_color(flow, cappedColor, (uint8_t)255, /*magnitudeCap=*/5.0);
	int autoBrightness = autoColor(1, 0, 0) + autoColor(2, 0, 0); // (0,0)'s own cyan channels
	int cappedBrightness = cappedColor(1, 0, 0) + cappedColor(2, 0, 0);
	passfail << "an outlier vector elsewhere in the field crushes auto-scaled brightness at (0,0) (" << autoBrightness << "): " << (autoBrightness < 20 ? "Pass" : "Fail") << std::endl;
	passfail << "an explicit magnitudeCap keeps (0,0) at full brightness (" << cappedBrightness << ") despite the outlier: " << (cappedBrightness > 500 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Visualize, FlowToArrows) {
	std::stringstream passfail;
	std::cout << std::endl << "FLOW_TO_ARROWS" << std::endl;

	// 3x3 grid of sample points at spacing=12 on a 36x36 field: (6,6),
	// (18,6), (30,6), (6,18), (18,18), (30,18), ... A normal-magnitude
	// vector at (6,6) and a wild outlier at (18,18), the same "one bad
	// vector shouldn't wreck everything else" setup FlowToColor uses above.
	const int W = 36, H = 36, spacing = 12;
	std::vector<double> flowData(2 * W * H, 0.0);
	Image<double, 3> flow(flowData.data(), { 2, W, H });
	flow(0, 6, 6) = 8.0;   // rightward, magnitude 8
	flow(1, 6, 6) = 0.0;
	flow(0, 18, 18) = 1000.0; // wild outlier, same direction
	flow(1, 18, 18) = 0.0;

	std::array<uint8_t, 3> white{ 255, 255, 255 };
	std::array<uint8_t, 3> background{ 100, 100, 100 };

	auto fillBackground = [&](OwnedImage<uint8_t, 3>& img) {
		for (const auto& c : img.coordinates()) img.at(c) = background[c[0]];
	};

	// Explicit magnitudeCap = the normal vector's own magnitude: its arrow
	// should reach full length (0.45*spacing rightward from (6,6), i.e. a
	// pixel partway along y=6 between x=6 and x=11.4 should be white), and
	// the outlier should SATURATE at that same length rather than
	// overrunning into the next cell.
	OwnedImage<uint8_t, 3> cappedImg({ 3, W, H });
	fillBackground(cappedImg);
	flow_to_arrows(flow, cappedImg, spacing, white, /*magnitudeCap=*/8.0);

	bool cappedNormalDrawn = cappedImg(0, 9, 6) == 255;
	passfail << "explicit magnitudeCap: normal vector's arrow reaches its full length: " << (cappedNormalDrawn ? "Pass" : "Fail") << std::endl;

	bool cappedOutlierDrawn = cappedImg(0, 21, 18) == 255;
	passfail << "explicit magnitudeCap: outlier's arrow reaches the same full length (saturated, not overrunning): " << (cappedOutlierDrawn ? "Pass" : "Fail") << std::endl;

	bool cappedOutlierContained = cappedImg(0, 29, 18) == background[0];
	passfail << "explicit magnitudeCap: outlier's arrow stays within its own cell, doesn't reach the neighboring grid point: " << (cappedOutlierContained ? "Pass" : "Fail") << std::endl;

	// Auto (magnitudeCap=-1, the default): the outlier's own true magnitude
	// (1000) becomes the scale, crushing the normal vector's arrow down to
	// a fraction of a pixel -- the same "auto-scaling gets wrecked by one
	// outlier" failure mode FlowToColor's own test checks, just for length
	// instead of brightness.
	OwnedImage<uint8_t, 3> autoImg({ 3, W, H });
	fillBackground(autoImg);
	flow_to_arrows(flow, autoImg, spacing, white);

	bool autoNormalCrushed = autoImg(0, 9, 6) == background[0];
	passfail << "auto-scaled: an outlier elsewhere crushes the normal vector's arrow down to invisible: " << (autoNormalCrushed ? "Pass" : "Fail") << std::endl;

	bool autoOutlierStillFullLength = autoImg(0, 21, 18) == 255;
	passfail << "auto-scaled: the outlier's own arrow still reaches full length (it set the scale): " << (autoOutlierStillFullLength ? "Pass" : "Fail") << std::endl;

	// Draws onto dst's existing content rather than clearing it -- a corner
	// pixel nowhere near any sample point or line should retain its
	// pre-filled background value untouched.
	bool backgroundPreserved = cappedImg(0, 0, 0) == background[0] && cappedImg(1, 0, 0) == background[1] && cappedImg(2, 0, 0) == background[2];
	passfail << "flow_to_arrows() draws onto dst without clearing unrelated background pixels: " << (backgroundPreserved ? "Pass" : "Fail") << std::endl;

	// dst is only required to have AT LEAST 3 channels, not exactly 3 --
	// unlike flow_to_color()'s own freshly-allocated {3,W,H} dst, a real
	// caller compositing onto a photo straight off image_io::load_owned()
	// (RGBA, 4 channels) needs this to work directly rather than requiring
	// an alpha-stripping copy first (demo/motion does exactly this).
	OwnedImage<uint8_t, 3> rgbaImg({ 4, W, H });
	for (const auto& c : rgbaImg.coordinates()) rgbaImg.at(c) = c[0] == 3 ? 42 : background[0]; // channel 3 = alpha, a distinct sentinel value
	flow_to_arrows(flow, rgbaImg, spacing, white, /*magnitudeCap=*/8.0);
	bool rgbaArrowDrawn = rgbaImg(0, 9, 6) == 255 && rgbaImg(1, 9, 6) == 255 && rgbaImg(2, 9, 6) == 255;
	passfail << "a 4-channel (RGBA) dst: the arrow is drawn into channels 0-2: " << (rgbaArrowDrawn ? "Pass" : "Fail") << std::endl;
	bool rgbaAlphaUntouched = rgbaImg(3, 9, 6) == 42 && rgbaImg(3, 0, 0) == 42;
	passfail << "a 4-channel (RGBA) dst: channel 3 (alpha) is left untouched: " << (rgbaAlphaUntouched ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Visualize, PercentileAndWindowedHeatmap) {
	std::stringstream passfail;
	std::cout << std::endl << "PERCENTILE / WINDOWED_HEATMAP" << std::endl;

	// percentile(): a known 0..99 distribution has unambiguous percentile
	// values (numpy's default "linear" interpolation convention).
	std::vector<double> rampData(100);
	for (int i = 0; i < 100; i++) rampData[i] = i;
	Image<double, 1> ramp(rampData.data(), { 100 });
	double p0 = percentile(ramp, 0.0), p50 = percentile(ramp, 50.0), p100 = percentile(ramp, 100.0);
	bool percentilesOk = std::abs(p0 - 0.0) < 1e-9 && std::abs(p50 - 49.5) < 1e-9 && std::abs(p100 - 99.0) < 1e-9;
	passfail << "percentile() matches known values on a 0..99 ramp (p0=" << p0 << ", p50=" << p50 << ", p100=" << p100 << "): " << (percentilesOk ? "Pass" : "Fail") << std::endl;

	// windowed_heatmap(): a mostly-uniform field with one huge outlier --
	// plain heatmap() crushes the uniform region to near-black (dominated
	// by the outlier's own max); the 5th-95th percentile window should
	// keep it clearly visible instead.
	const int W = 20, H = 20;
	std::vector<double> data(W * H, 10.0);
	data[0] = 100000.0; // one wild outlier
	Image<double, 2> img(data.data(), { W, H });

	OwnedImage<uint8_t, 2> plainHeat({ W, H });
	heatmap(img, plainHeat, (uint8_t)255);
	OwnedImage<uint8_t, 2> windowedHeat({ W, H });
	windowed_heatmap(img, windowedHeat, 5.0, 95.0, (uint8_t)255);

	passfail << "plain heatmap() crushes a normal pixel near-black under one outlier (value=" << (int)plainHeat(5, 5) << "): " << (plainHeat(5, 5) < 5 ? "Pass" : "Fail") << std::endl;
	passfail << "windowed_heatmap() keeps that same pixel clearly visible (value=" << (int)windowedHeat(5, 5) << "): " << (windowedHeat(5, 5) > 200 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}
