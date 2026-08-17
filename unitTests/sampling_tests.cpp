#include <gtest/gtest.h>
#include <cmath>
#include <array>
#include <sstream>
#include <iostream>
#include <algorithm>

#include <ndl/image.h>
#include <ndl/processing/matrix.h>
#include <ndl/processing/sampling.h>

#include "testHelpers.h"

using namespace ndl;

TEST(Sampling, IdentityAndTranslation) {
	std::stringstream passfail;
	std::cout << std::endl << "SAMPLING -- IDENTITY AND TRANSLATION" << std::endl;

	const int W = 40, H = 40;
	std::vector<double> srcData(W * H);
	Image<double, 2> src(srcData.data(), { W, H });
	for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) src(x, y) = 2.0 * x + 3.0 * y + 5.0;

	// Identity transform: dst should equal src exactly.
	{
		std::vector<double> dstData(W * H);
		Image<double, 2> dst(dstData.data(), { W, H });
		Matrix<double, 3> identity;
		sample_image(src, dst, identity);
		double maxDiff = 0;
		for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) maxDiff = std::max(maxDiff, std::abs(dst(x, y) - src(x, y)));
		passfail << "identity transform reproduces src exactly (max diff " << maxDiff << "): " << (maxDiff < 1e-9 ? "Pass" : "Fail") << std::endl;
	}

	// dstToSrc is backward-mapped: dst(x,y) should read src(x+5, y+3).
	{
		std::vector<double> dstData(W * H);
		Image<double, 2> dst(dstData.data(), { W, H });
		Matrix<double, 3> translate;
		std::array<double, 2> t{ 5.0, 3.0 };
		make_translate_matrix(translate, t);
		sample_image(src, dst, translate);
		double maxDiff = 0;
		for (int y = 0; y < H - 3; y++) for (int x = 0; x < W - 5; x++)
			maxDiff = std::max(maxDiff, std::abs(dst(x, y) - src(x + 5, y + 3)));
		passfail << "translation transform is backward-mapped correctly (interior max diff " << maxDiff << "): " << (maxDiff < 1e-9 ? "Pass" : "Fail") << std::endl;
	}

	reportPassFail(passfail);
}

TEST(Sampling, AutomaticAntiAliasingOnDownsample) {
	std::stringstream passfail;
	std::cout << std::endl << "SAMPLING -- AUTOMATIC ANTI-ALIASING" << std::endl;

	const int W = 40, H = 40;
	std::vector<double> srcData(W * H);
	Image<double, 2> src(srcData.data(), { W, H });
	for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) src(x, y) = 2.0 * x + 3.0 * y + 5.0;

	// 2x downsample: auto-AA should kick in, and since averaging a linear
	// function over a box returns its value at the box's own center, the
	// downsampled result should track the ramp's local average closely --
	// a real correctness check, not just "doesn't crash".
	const int W2 = W / 2, H2 = H / 2;
	std::vector<double> dstData(W2 * H2);
	Image<double, 2> dst(dstData.data(), { W2, H2 });
	Matrix<double, 3> scale;
	std::array<double, 2> s{ 2.0, 2.0 };
	make_scale_matrix(scale, s);
	sample_image(src, dst, scale);

	bool tracksLocalAverage = true;
	for (int y = 0; y < H2; y++) for (int x = 0; x < W2; x++)
	{
		double expected = 2.0 * (2 * x + 0.5) + 3.0 * (2 * y + 0.5) + 5.0;
		if (std::abs(dst(x, y) - expected) > 3.0) tracksLocalAverage = false;
	}
	passfail << "2x downsample values track the linear ramp's local average: " << (tracksLocalAverage ? "Pass" : "Fail") << std::endl;

	// Confirm autoAA actually changes the result relative to disabling it
	// (i.e. it's really filtering, not silently falling through).
	std::vector<double> dstNoAAData(W2 * H2);
	Image<double, 2> dstNoAA(dstNoAAData.data(), { W2, H2 });
	sample_image(src, dstNoAA, scale, Linear{}, /*autoAA=*/false);
	double totalDiff = 0;
	for (int y = 0; y < H2; y++) for (int x = 0; x < W2; x++) totalDiff += std::abs(dst(x, y) - dstNoAA(x, y));
	passfail << "AA-enabled and AA-disabled downsampling differ (AA is really filtering): " << (totalDiff > 0 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Sampling, RotationRoundTrip) {
	std::stringstream passfail;
	std::cout << std::endl << "SAMPLING -- ROTATION ROUND TRIP" << std::endl;

	// Rotate a smooth blob about its own center, then rotate it back --
	// should approximately reconstruct the original, aside from
	// interpolation blur from the two resampling passes.
	const int W = 40, H = 40;
	std::vector<double> blobData(W * H);
	Image<double, 2> blob(blobData.data(), { W, H });
	double cx = 20, cy = 20, sigma = 6.0;
	for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
	{
		double dx = x - cx, dy = y - cy;
		blob(x, y) = 200.0 * std::exp(-(dx * dx + dy * dy) / (2 * sigma * sigma));
	}

	Matrix<double, 3> rot, toCenter, fromCenter, combined, combinedInv;
	make_rotate_matrix(rot, 0.6);
	std::array<double, 2> toC{ -cx, -cy }, fromC{ cx, cy };
	make_translate_matrix(toCenter, toC);
	make_translate_matrix(fromCenter, fromC);
	combined = fromCenter * rot * toCenter;
	combinedInv = inverse(combined);

	std::vector<double> rotatedData(W * H);
	Image<double, 2> rotated(rotatedData.data(), { W, H });
	sample_image(blob, rotated, combinedInv);

	std::vector<double> backData(W * H);
	Image<double, 2> back(backData.data(), { W, H });
	sample_image(rotated, back, combined);

	double sumSqDiff = 0, sumSq = 0;
	for (int y = 5; y < H - 5; y++) for (int x = 5; x < W - 5; x++)
	{
		double d = back(x, y) - blob(x, y);
		sumSqDiff += d * d;
		sumSq += blob(x, y) * blob(x, y);
	}
	double relErr = std::sqrt(sumSqDiff / sumSq);
	passfail << "rotate-then-rotate-back relative RMS error (" << relErr << ") is small: " << (relErr < 0.1 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}
