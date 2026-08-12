#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <cstdint>
#include <random>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/summed_area_table.h>

#include "testHelpers.h"

using namespace ndl;

TEST(SummedAreaTable, MatchesBruteForceRectangleSums) {
	std::stringstream passfail;
	std::cout << std::endl << "SUMMED-AREA TABLE VS BRUTE-FORCE RECTANGLE SUMS" << std::endl;

	std::mt19937 rng(7);
	const int W = 11, H = 9;
	std::vector<uint8_t> data(W * H);
	std::uniform_int_distribution<int> valueDist(0, 255);
	for (auto& v : data) v = (uint8_t)valueDist(rng);
	Image<uint8_t, 2> src(data.data(), { W, H });

	std::vector<double> tableData(W * H);
	Image<double, 2> table(tableData.data(), { W, H });
	summed_area_table(src, table);

	long fullSum = 0;
	for (const auto& c : src.coordinates()) fullSum += src.at(c);
	passfail << "table's own last corner equals the full-image sum: " << (table.at({ W - 1, H - 1 }) == (double)fullSum ? "Pass" : "Fail") << std::endl;

	std::uniform_int_distribution<int> xd(0, W - 1), yd(0, H - 1);
	bool allMatch = true;
	for (int trial = 0; trial < 200; trial++)
	{
		int x0 = xd(rng), x1 = xd(rng);
		int y0 = yd(rng), y1 = yd(rng);
		if (x0 > x1) std::swap(x0, x1);
		if (y0 > y1) std::swap(y0, y1);

		long brute = 0;
		for (int y = y0; y <= y1; y++)
			for (int x = x0; x <= x1; x++)
				brute += src(x, y);

		double q = rectangle_sum(table, std::array<int, 2>{x0, y0}, std::array<int, 2>{x1, y1});
		if (std::abs(q - (double)brute) > 1e-6) allMatch = false;
	}
	passfail << "200 random rectangle queries all match brute-force sums: " << (allMatch ? "Pass" : "Fail") << std::endl;

	double single = rectangle_sum(table, std::array<int, 2>{3, 4}, std::array<int, 2>{3, 4});
	passfail << "single-cell rectangle query equals that one source value: " << (single == (double)src(3, 4) ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(SummedAreaTable, OneDimensional) {
	std::stringstream passfail;
	std::cout << std::endl << "SUMMED-AREA TABLE (1D)" << std::endl;

	std::vector<int> data = { 1, 2, 3, 4, 5 };
	Image<int, 1> src(data.data(), { 5 });
	std::vector<long> tableData(5);
	Image<long, 1> table(tableData.data(), { 5 });
	summed_area_table(src, table);

	long expected[5] = { 1, 3, 6, 10, 15 };
	bool tableMatches = true;
	for (int i = 0; i < 5; i++) if (table(i) != expected[i]) tableMatches = false;
	passfail << "1D running-sum table matches the hand-computed prefix sums: " << (tableMatches ? "Pass" : "Fail") << std::endl;

	long r = rectangle_sum(table, std::array<int, 1>{1}, std::array<int, 1>{3}); // 2+3+4
	passfail << "1D rectangle_sum() over [1,3] equals 2+3+4=9: " << (r == 9 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(SummedAreaTable, BoxBlur) {
	std::stringstream passfail;
	std::cout << std::endl << "SUMMED-AREA TABLE BOX BLUR" << std::endl;

	// A constant image should stay exactly that constant everywhere,
	// regardless of border mode -- every window, however it's resolved at
	// the edges, only ever samples the same one value.
	OwnedImage<uint8_t, 2> constSrc({ 10, 10 });
	constSrc = uint8_t(42);
	OwnedImage<uint8_t, 2> constDst({ 10, 10 });
	bool constantMatches = true;
	for (auto border : { BorderMode::Clamp, BorderMode::Wrap, BorderMode::Reflect })
	{
		box_blur(constSrc, constDst, 3, border);
		for (const auto& c : constDst.coordinates())
			if (constDst.at(c) != 42) constantMatches = false;
	}
	passfail << "box_blur() of a constant image stays constant under every BorderMode: " << (constantMatches ? "Pass" : "Fail") << std::endl;

	// Interior positions (whose window never reaches outside the source)
	// should match a direct brute-force average exactly, independent of
	// which BorderMode was requested -- border handling only ever affects
	// positions whose window actually needs it.
	std::vector<double> rampData(20);
	{ int i = 0; for (auto& v : rampData) v = (double)(i++); }
	Image<double, 1> ramp(rampData.data(), { 20 });
	std::vector<double> rampBlurredData(20);
	Image<double, 1> rampBlurred(rampBlurredData.data(), { 20 });
	box_blur(ramp, rampBlurred, 2, BorderMode::Clamp);
	bool interiorMatches = true;
	for (int x = 2; x < 18; x++)
	{
		double expected = 0;
		for (int k = -2; k <= 2; k++) expected += ramp(x + k);
		expected /= 5.0;
		if (std::abs(rampBlurred(x) - expected) > 1e-9) interiorMatches = false;
	}
	passfail << "box_blur() interior positions match a direct brute-force average: " << (interiorMatches ? "Pass" : "Fail") << std::endl;

	// BorderMode::Clamp at the very first position: the two out-of-bounds
	// taps (-2,-1) both clamp to index 0, so index 0's own value is
	// effectively triple-counted.
	std::vector<double> rampClampData(20);
	Image<double, 1> rampClamp(rampClampData.data(), { 20 });
	box_blur(ramp, rampClamp, 2, BorderMode::Clamp);
	double expectedClampLeft = (ramp(0) * 3 + ramp(1) + ramp(2)) / 5.0;
	passfail << "box_blur() BorderMode::Clamp at the left edge matches hand-computed replication: " << (std::abs(rampClamp(0) - expectedClampLeft) < 1e-9 ? "Pass" : "Fail") << std::endl;

	// BorderMode::Wrap at the very first position: the two out-of-bounds
	// taps wrap around to the far end of the row instead.
	std::vector<double> rampWrapData(20);
	Image<double, 1> rampWrap(rampWrapData.data(), { 20 });
	box_blur(ramp, rampWrap, 2, BorderMode::Wrap);
	double expectedWrapLeft = (ramp(18) + ramp(19) + ramp(0) + ramp(1) + ramp(2)) / 5.0;
	passfail << "box_blur() BorderMode::Wrap at the left edge matches hand-computed wraparound: " << (std::abs(rampWrap(0) - expectedWrapLeft) < 1e-9 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}
