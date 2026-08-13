#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/visualize.h>

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

	reportPassFail(passfail);
}
