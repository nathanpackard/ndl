#include <gtest/gtest.h>
#include <cmath>
#include <array>
#include <sstream>
#include <iostream>
#include <random>

#include <ndl/image.h>
#include <ndl/interpolation.h>

#include "testHelpers.h"

using namespace ndl;

TEST(Interpolation, ReproducesConstantAndLinearRamp) {
	std::stringstream passfail;
	std::cout << std::endl << "INTERPOLATION -- CONSTANT AND LINEAR-RAMP REPRODUCTION" << std::endl;

	// Every interpolator's weights sum to 1 (partition of unity), so a
	// constant image should come back exactly, everywhere.
	const int W = 20, H = 20;
	std::vector<double> constData(W * H, 7.5);
	Image<double, 2> constImg(constData.data(), { W, H });
	bool constOk = true;
	for (double x : {2.3, 5.0, 10.9, 0.0, 19.99})
	for (double y : {1.1, 8.8, 15.0})
	{
		std::array<double, 2> p{ x, y };
		if (std::abs(sample(constImg, p, Nearest{}) - 7.5) > 1e-9) constOk = false;
		if (std::abs(sample(constImg, p, Linear{}) - 7.5) > 1e-9) constOk = false;
		if (std::abs(sample(constImg, p, Quadratic{}) - 7.5) > 1e-9) constOk = false;
		if (std::abs(sample(constImg, p, Cubic{}) - 7.5) > 1e-9) constOk = false;
	}
	passfail << "constant image reproduced exactly by Nearest/Linear/Quadratic/Cubic: " << (constOk ? "Pass" : "Fail") << std::endl;

	// Linear/Quadratic/Cubic all have unit-sum, zero-first-moment weights
	// (symmetric about the query position), so a genuinely linear function
	// is reproduced EXACTLY -- not just approximately -- away from the
	// border (where taps would otherwise reach out of bounds and get
	// clamped, breaking the "linear everywhere" premise the expected value
	// is computed from, not a bug in sample() itself). Nearest is
	// deliberately excluded -- rounding to the nearest sample is not a
	// linear-reproducing operation.
	std::vector<double> rampData(W * H);
	Image<double, 2> ramp(rampData.data(), { W, H });
	for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) ramp(x, y) = 2.0 * x + 3.0 * y + 1.0;

	bool rampOk = true;
	for (double x : {5.3, 10.0, 14.7})
	for (double y : {6.1, 9.9, 13.4})
	{
		double expected = 2.0 * x + 3.0 * y + 1.0;
		std::array<double, 2> p{ x, y };
		if (std::abs(sample(ramp, p, Linear{}) - expected) > 1e-9) rampOk = false;
		if (std::abs(sample(ramp, p, Quadratic{}) - expected) > 1e-9) rampOk = false;
		if (std::abs(sample(ramp, p, Cubic{}) - expected) > 1e-9) rampOk = false;
	}
	passfail << "linear ramp reproduced exactly by Linear/Quadratic/Cubic: " << (rampOk ? "Pass" : "Fail") << std::endl;

	// Sampling exactly at an integer grid position should return exactly
	// that stored value, for every interpolator.
	bool gridOk = true;
	for (auto c : { std::array<double,2>{5,5}, std::array<double,2>{0,0}, std::array<double,2>{19,19} })
	{
		double expected = ramp.at({ (int)c[0], (int)c[1] });
		if (std::abs(sample(ramp, c, Nearest{}) - expected) > 1e-9) gridOk = false;
		if (std::abs(sample(ramp, c, Linear{}) - expected) > 1e-9) gridOk = false;
	}
	passfail << "exact-grid-position sampling matches the stored value: " << (gridOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Interpolation, SampleAndScatterAddAreExactAdjoints) {
	std::stringstream passfail;
	std::cout << std::endl << "INTERPOLATION -- sample()/scatter_add() ADJOINTNESS" << std::endl;

	// scatter_add() exists specifically to be the exact adjoint of
	// sample() (projection.h's forward_project()/back_project() rely on
	// this) -- verified directly via the dot-product identity
	// <sample(x,p), y> == <x, scatter_add(0,p,y)>, for random x and
	// several random query positions, across every interpolator.
	const int W = 20, H = 20;
	std::mt19937 rng(42);
	std::uniform_real_distribution<double> valDist(-5, 5);
	std::uniform_real_distribution<double> posDist(1.0, 18.0);

	std::vector<double> xData(W * H);
	Image<double, 2> x(xData.data(), { W, H });
	for (auto& v : xData) v = valDist(rng);

	auto checkAdjoint = [&](auto interpolator, const char* name) {
		bool ok = true;
		for (int trial = 0; trial < 20; trial++)
		{
			std::array<double, 2> p{ posDist(rng), posDist(rng) };
			double y = valDist(rng);

			double lhs = y * sample(x, p, interpolator);

			std::vector<double> zData(W * H, 0.0);
			Image<double, 2> z(zData.data(), { W, H });
			scatter_add(z, p, y, interpolator);
			double rhs = 0;
			for (int i = 0; i < W * H; i++) rhs += xData[i] * zData[i];

			if (std::abs(lhs - rhs) > 1e-9 * (1 + std::abs(lhs))) ok = false;
		}
		passfail << name << " sample()/scatter_add() are exact adjoints: " << (ok ? "Pass" : "Fail") << std::endl;
	};
	checkAdjoint(Nearest{}, "Nearest");
	checkAdjoint(Linear{}, "Linear");
	checkAdjoint(Quadratic{}, "Quadratic");
	checkAdjoint(Cubic{}, "Cubic");

	reportPassFail(passfail);
}
