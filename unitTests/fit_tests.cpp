#include <gtest/gtest.h>
#include <sstream>
#include <iostream>
#include <cmath>
#include <vector>

#include <ndl/processing/fit.h>

#include "testHelpers.h"

using namespace ndl;

TEST(Fit, LinearFitRecoversExactLine) {
	std::stringstream passfail;
	// y = 3x + 7, exactly -- residualStdDev should come back ~0.
	std::vector<double> x = { 0, 1, 2, 3, 4, 5 };
	std::vector<double> y;
	for (double xi : x) y.push_back(3.0 * xi + 7.0);

	LinearFit fit = linear_fit(static_cast<int>(x.size()), x.begin(), y.begin());
	bool slopeOk = std::abs(fit.slope - 3.0) < 1e-9;
	bool interceptOk = std::abs(fit.intercept - 7.0) < 1e-9;
	bool residualOk = fit.residualStdDev < 1e-9;
	passfail << "slope recovered exactly (3.0): " << (slopeOk ? "Pass" : "Fail") << std::endl;
	passfail << "intercept recovered exactly (7.0): " << (interceptOk ? "Pass" : "Fail") << std::endl;
	passfail << "residualStdDev ~0 for an exact fit: " << (residualOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Fit, LinearFitSymmetricNoiseAveragesOut) {
	std::stringstream passfail;
	// y = 2x + 1 plus a +1,-1,-1,+1 perturbation -- chosen so that
	// sum((x_i - mean(x)) * noise_i) == 0 (the actual condition for an
	// added perturbation to leave OLS's slope/intercept unchanged, not
	// just "equal counts of + and -": x-mean(x) here is -1.5,-0.5,0.5,1.5,
	// so pairing +1 with the outer (|x-mean(x)|=1.5) points and -1 with
	// the inner (0.5) points makes each product cancel against its
	// mirror). The fitted line should still recover the true
	// slope/intercept exactly, with a nonzero (but bounded)
	// residualStdDev this time, unlike the exact-fit case above.
	std::vector<double> x = { 0, 1, 2, 3 };
	std::vector<double> y = { 1.0 + 1.0, 3.0 - 1.0, 5.0 - 1.0, 7.0 + 1.0 };

	LinearFit fit = linear_fit(static_cast<int>(x.size()), x.begin(), y.begin());
	bool slopeOk = std::abs(fit.slope - 2.0) < 1e-9;
	bool interceptOk = std::abs(fit.intercept - 1.0) < 1e-9;
	bool residualPositive = fit.residualStdDev > 0.0;
	passfail << "slope recovered despite symmetric noise (2.0): " << (slopeOk ? "Pass" : "Fail") << std::endl;
	passfail << "intercept recovered despite symmetric noise (1.0): " << (interceptOk ? "Pass" : "Fail") << std::endl;
	passfail << "residualStdDev is positive (the noise itself is measured, not averaged away to 0): " << (residualPositive ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Fit, ParabolicFitRecoversExactParabola) {
	std::stringstream passfail;
	// y = 2 + 3x - x^2, exactly.
	std::vector<double> x = { -2, -1, 0, 1, 2, 3 };
	std::vector<double> y;
	for (double xi : x) y.push_back(2.0 + 3.0 * xi - xi * xi);

	ParabolicFit fit = parabolic_fit(static_cast<int>(x.size()), x.begin(), y.begin());
	bool aOk = std::abs(fit.a - 2.0) < 1e-6;
	bool bOk = std::abs(fit.b - 3.0) < 1e-6;
	bool cOk = std::abs(fit.c - (-1.0)) < 1e-6;
	bool residualOk = fit.residualStdDev < 1e-6;
	passfail << "a recovered exactly (2.0): " << (aOk ? "Pass" : "Fail") << std::endl;
	passfail << "b recovered exactly (3.0): " << (bOk ? "Pass" : "Fail") << std::endl;
	passfail << "c recovered exactly (-1.0): " << (cOk ? "Pass" : "Fail") << std::endl;
	passfail << "residualStdDev ~0 for an exact fit: " << (residualOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}
