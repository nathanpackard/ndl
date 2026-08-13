#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <cmath>
#include <complex>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/optical_flow.h>

#include "testHelpers.h"

using namespace ndl;

TEST(OpticalFlow, LinearSolver) {
	std::stringstream passfail;
	std::cout << std::endl << "OPTICAL FLOW LINEAR SOLVER" << std::endl;

	// 2a+b=5, a+3b=10 -> a=1, b=3 (checked by hand).
	std::array<std::array<double, 2>, 2> A = { { {2, 1}, {1, 3} } };
	std::array<double, 2> b = { 5, 10 };
	std::array<double, 2> x;
	bool ok = detail::solveLinearSystem<2>(A, b, x);
	passfail << "2x2 system solves to the hand-checked answer: " << (ok && std::abs(x[0] - 1) < 1e-9 && std::abs(x[1] - 3) < 1e-9 ? "Pass" : "Fail") << std::endl;

	// [[1,2],[2,4]] is singular (row 2 = 2 * row 1) -- must be rejected, not silently blow up.
	std::array<std::array<double, 2>, 2> singular = { { {1, 2}, {2, 4} } };
	std::array<double, 2> bs = { 1, 2 };
	std::array<double, 2> xs;
	bool singularOk = detail::solveLinearSystem<2>(singular, bs, xs);
	passfail << "singular 2x2 system is correctly rejected: " << (!singularOk ? "Pass" : "Fail") << std::endl;

	std::array<std::array<double, 3>, 3> identity = { { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} } };
	std::array<double, 3> b3 = { 7, 8, 9 };
	std::array<double, 3> x3;
	bool ok3 = detail::solveLinearSystem<3>(identity, b3, x3);
	passfail << "3x3 identity system returns b unchanged: " << (ok3 && x3[0] == 7 && x3[1] == 8 && x3[2] == 9 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(OpticalFlow, LucasKanadeRecoversKnownShift) {
	std::stringstream passfail;
	std::cout << std::endl << "LUCAS-KANADE KNOWN-SHIFT RECOVERY" << std::endl;

	// A smooth, texture-everywhere synthetic pattern (a real photo would work
	// too, but this keeps the test self-contained and avoids depending on
	// unitTests/data): no flat regions anywhere, so every window has real
	// gradient content to estimate flow from.
	const int W = 80, H = 80;
	std::vector<double> data0(W * H);
	Image<double, 2> frame0(data0.data(), { W, H });
	for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
		frame0(x, y) = std::sin(x * 0.3) * std::cos(y * 0.3) * 100.0 + 128.0;

	double shiftX = 1.5, shiftY = -0.8;
	std::vector<double> data1(W * H);
	Image<double, 2> frame1(data1.data(), { W, H });
	for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
		frame1(x, y) = std::sin((x - shiftX) * 0.3) * std::cos((y - shiftY) * 0.3) * 100.0 + 128.0;

	std::vector<double> flowData(2 * W * H);
	Image<double, 3> flow(flowData.data(), { 2, W, H });
	lucas_kanade_flow(frame0, frame1, flow, 7, BorderMode::Reflect);

	Image<double, 2> fx = flow.slice(0, 0);
	Image<double, 2> fy = flow.slice(0, 1);

	double sumX = 0, sumY = 0;
	int count = 0;
	for (int y = 15; y < H - 15; y++)
		for (int x = 15; x < W - 15; x++)
		{
			sumX += fx(x, y);
			sumY += fy(x, y);
			count++;
		}
	double avgX = sumX / count, avgY = sumY / count;
	passfail << "recovered average flow (" << avgX << "," << avgY << ") matches the known shift (" << shiftX << "," << shiftY << ") within 0.3px: "
		<< (std::abs(avgX - shiftX) < 0.3 && std::abs(avgY - shiftY) < 0.3 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(OpticalFlow, ComplexConversionRoundTrip) {
	std::stringstream passfail;
	std::cout << std::endl << "COMPLEX FLOW CONVERSION" << std::endl;

	const int W = 5, H = 4;
	std::vector<double> flowData(2 * W * H);
	Image<double, 3> flow(flowData.data(), { 2, W, H });
	for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { flow(0, x, y) = x + 1; flow(1, x, y) = y * 2; }

	OwnedImage<std::complex<double>, 2> cplx({ W, H });
	to_complex(flow, cplx);
	bool toOk = true;
	for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
		if (cplx(x, y).real() != x + 1 || cplx(x, y).imag() != y * 2) toOk = false;
	passfail << "to_complex() maps dx/dy to real/imaginary parts: " << (toOk ? "Pass" : "Fail") << std::endl;

	OwnedImage<double, 3> back(std::array<int, 3>{2, W, H});
	from_complex(cplx, back);
	Image<double, 2> bx = back.slice(0, 0), by = back.slice(0, 1);
	bool fromOk = true;
	for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
		if (bx(x, y) != x + 1 || by(x, y) != y * 2) fromOk = false;
	passfail << "from_complex() round-trips back to the original flow field exactly: " << (fromOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}
