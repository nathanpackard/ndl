#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <cmath>
#include <random>
#include <memory>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/processing/morphology.h>
#include <ndl/processing/distance_transform.h>

#include "testHelpers.h"

using namespace ndl;

namespace
{
	// O(pixels^2) reference: for each position, the minimum squared
	// distance to any position where src is false -- the direct
	// definition distance_transform_squared() is meant to compute exactly,
	// just without the separable-per-axis speedup, so an independent check
	// of it (same reasoning as fft_tests.cpp's own brute-force DFT check).
	template<int DIM>
	void bruteForceSquared(const Image<bool, DIM>& src, Image<double, DIM>& dst)
	{
		auto coords = src.coordinates();
		std::vector<std::array<int, DIM>> background;
		for (const auto& c : coords) if (!src.at(c)) background.push_back(c);

		for (const auto& p : coords)
		{
			if (background.empty()) { dst.at(p) = 1e18; continue; }
			double best = -1;
			for (const auto& q : background)
			{
				double d2 = 0;
				for (int i = 0; i < DIM; i++) { double diff = p[i] - q[i]; d2 += diff * diff; }
				if (best < 0 || d2 < best) best = d2;
			}
			dst.at(p) = best;
		}
	}

	template<int DIM>
	double maxAbsDiff(const Image<double, DIM>& a, const Image<double, DIM>& b)
	{
		double m = 0;
		for (const auto& c : a.coordinates()) m = std::max(m, std::abs(a.at(c) - b.at(c)));
		return m;
	}
}

TEST(DistanceTransform, MatchesBruteForce2D) {
	std::stringstream passfail;
	std::cout << std::endl << "DISTANCE TRANSFORM VS BRUTE FORCE (2D)" << std::endl;

	std::mt19937 rng(42);
	const int W = 17, H = 13;
	auto data = std::make_unique<bool[]>(W * H);
	std::bernoulli_distribution coin(0.3);
	for (int i = 0; i < W * H; i++) data[i] = coin(rng);
	Image<bool, 2> src(data.get(), { W, H });

	std::vector<double> dtData(W * H), bfData(W * H);
	Image<double, 2> dt(dtData.data(), { W, H });
	Image<double, 2> bf(bfData.data(), { W, H });

	distance_transform_squared(src, dt);
	bruteForceSquared<2>(src, bf);

	double err = maxAbsDiff(dt, bf);
	passfail << "separable squared-Euclidean distance transform matches brute force exactly (2D, 30% random fill): " << (err < 1e-9 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(DistanceTransform, MatchesBruteForce3D) {
	std::stringstream passfail;
	std::cout << std::endl << "DISTANCE TRANSFORM VS BRUTE FORCE (3D)" << std::endl;

	std::mt19937 rng(7);
	const int W = 6, H = 7, D = 5;
	auto data = std::make_unique<bool[]>(W * H * D);
	std::bernoulli_distribution coin(0.25);
	for (int i = 0; i < W * H * D; i++) data[i] = coin(rng);
	Image<bool, 3> src(data.get(), { W, H, D });

	std::vector<double> dtData(W * H * D), bfData(W * H * D);
	Image<double, 3> dt(dtData.data(), { W, H, D });
	Image<double, 3> bf(bfData.data(), { W, H, D });

	distance_transform_squared(src, dt);
	bruteForceSquared<3>(src, bf);

	double err = maxAbsDiff(dt, bf);
	passfail << "separable squared-Euclidean distance transform generalizes correctly to 3D: " << (err < 1e-9 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(DistanceTransform, HandCheckable1D) {
	std::stringstream passfail;
	std::cout << std::endl << "DISTANCE TRANSFORM (1D, HAND-CHECKABLE)" << std::endl;

	bool data[10] = { false, true, true, true, false, true, true, false, true, true };
	Image<bool, 1> src(data, { 10 });
	double outData[10];
	Image<double, 1> dt(outData, { 10 });
	distance_transform(src, dt); // unsquared

	double expected[10] = { 0, 1, 2, 1, 0, 1, 1, 0, 1, 2 };
	bool allMatch = true;
	for (int i = 0; i < 10; i++) if (std::abs(outData[i] - expected[i]) > 1e-9) allMatch = false;
	passfail << "1D distances to nearest false pixel match by-hand expectation: " << (allMatch ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(DistanceTransform, EdgeCases) {
	std::stringstream passfail;
	std::cout << std::endl << "DISTANCE TRANSFORM EDGE CASES" << std::endl;

	// All-background: every distance is trivially 0.
	{
		bool data[9] = { false, false, false, false, false, false, false, false, false };
		Image<bool, 2> src(data, { 3, 3 });
		double outData[9];
		Image<double, 2> dt(outData, { 3, 3 });
		distance_transform_squared(src, dt);
		bool allZero = true;
		for (int i = 0; i < 9; i++) if (outData[i] != 0.0) allZero = false;
		passfail << "all-background image: every squared distance is exactly 0: " << (allZero ? "Pass" : "Fail") << std::endl;
	}

	// All-foreground: no background pixel exists at all -- must not
	// produce NaN or crash, and should read as "very large" rather than a
	// misleadingly small number.
	{
		bool data[9] = { true, true, true, true, true, true, true, true, true };
		Image<bool, 2> src(data, { 3, 3 });
		double outData[9];
		Image<double, 2> dt(outData, { 3, 3 });
		distance_transform_squared(src, dt);
		bool noNaN = true, allLarge = true;
		for (int i = 0; i < 9; i++)
		{
			if (std::isnan(outData[i])) noNaN = false;
			if (!(outData[i] > 1e15)) allLarge = false;
		}
		passfail << "all-foreground image: no NaN: " << (noNaN ? "Pass" : "Fail") << std::endl;
		passfail << "all-foreground image: every squared distance reads as a large sentinel, not 0 or garbage: " << (allLarge ? "Pass" : "Fail") << std::endl;
	}

	reportPassFail(passfail);
}

TEST(DistanceTransform, InvertGivesDistanceToForeground) {
	std::stringstream passfail;
	std::cout << std::endl << "DISTANCE TRANSFORM + invert() -- DISTANCE TO NEAREST FOREGROUND" << std::endl;

	// distance_transform() itself always measures to the nearest
	// background (false) pixel; inverting the source first (morphology.h)
	// is the documented way to get distance-to-nearest-foreground instead.
	bool data[5] = { false, false, true, false, false };
	Image<bool, 1> src(data, { 5 });
	bool invData[5];
	Image<bool, 1> inv(invData, { 5 });
	ndl::invert(src, inv);

	double outData[5];
	Image<double, 1> dt(outData, { 5 });
	distance_transform(inv, dt);

	double expected[5] = { 2, 1, 0, 1, 2 };
	bool allMatch = true;
	for (int i = 0; i < 5; i++) if (std::abs(outData[i] - expected[i]) > 1e-9) allMatch = false;
	passfail << "distance_transform(invert(src)) gives distance to nearest TRUE pixel in the original: " << (allMatch ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(DistanceTransform, ToForegroundSideMatchesInvertWorkaround) {
	std::stringstream passfail;
	std::cout << std::endl << "DISTANCE TRANSFORM -- DistanceSide::ToForeground MATCHES THE invert() WORKAROUND" << std::endl;

	// Same source/expectation as InvertGivesDistanceToForeground above, but
	// via the new side parameter directly on the ORIGINAL (non-inverted)
	// source -- should give byte-for-byte the same answer as inverting
	// first and running the default ToBackground transform, without the
	// extra image.
	bool data[5] = { false, false, true, false, false };
	Image<bool, 1> src(data, { 5 });

	double outData[5];
	Image<double, 1> dt(outData, { 5 });
	distance_transform(src, dt, DistanceSide::ToForeground);

	double expected[5] = { 2, 1, 0, 1, 2 };
	bool allMatch = true;
	for (int i = 0; i < 5; i++) if (std::abs(outData[i] - expected[i]) > 1e-9) allMatch = false;
	passfail << "distance_transform(src, DistanceSide::ToForeground) gives distance to nearest TRUE pixel directly: " << (allMatch ? "Pass" : "Fail") << std::endl;

	// And ToBackground (explicit or defaulted) must still match the
	// original, un-inverted behavior.
	double bgData[5];
	Image<double, 1> bg(bgData, { 5 });
	distance_transform(src, bg); // defaults to ToBackground
	double expectedBg[5] = { 0, 0, 1, 0, 0 };
	bool bgMatch = true;
	for (int i = 0; i < 5; i++) if (std::abs(bgData[i] - expectedBg[i]) > 1e-9) bgMatch = false;
	passfail << "distance_transform(src) (defaulted ToBackground) still gives distance to nearest FALSE pixel: " << (bgMatch ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(DistanceTransform, SignedCombinesBothSides) {
	std::stringstream passfail;
	std::cout << std::endl << "DISTANCE TRANSFORM -- distance_transform_signed()" << std::endl;

	// A single foreground run in the middle: signed distance should be
	// POSITIVE inside it (matching the plain ToBackground magnitude
	// exactly) and NEGATIVE outside it (matching the ToForeground
	// magnitude, negated), zero nowhere here since no pixel sits exactly
	// on a fractional boundary at integer sample points.
	bool data[7] = { false, false, false, true, true, false, false };
	Image<bool, 1> src(data, { 7 });

	double outData[7];
	Image<double, 1> dt(outData, { 7 });
	distance_transform_signed(src, dt);

	double expected[7] = { -3, -2, -1, 1, 1, -1, -2 };
	bool allMatch = true;
	for (int i = 0; i < 7; i++) if (std::abs(outData[i] - expected[i]) > 1e-9) allMatch = false;
	passfail << "distance_transform_signed(): positive inside foreground, negative outside, matching magnitudes: " << (allMatch ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}
