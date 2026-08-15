#pragma once
#include <cassert>
#include <cmath>
#include <array>

#include "matrix.h"

// Ordinary least-squares curve fitting over a plain sequence of (x,y)
// sample pairs -- a sibling of histogram.h/morphology.h, not part of
// image.h's core Image object. Iterator-based (like Histogram's own
// general constructor takes any CoordRange) rather than tied to any
// particular container: the samples being fit are just points, not
// image data, so there's no Image/minimal-interface involvement here at
// all.
//
// polynomial_fit<Degree>() is the one real primitive: it builds the
// (Degree+1)x(Degree+1) Vandermonde normal-equation system as a
// Matrix<double,Degree+1> (in power-sum form -- sum(x^k) for
// k=0..2*Degree -- never materializing the full n x (Degree+1)
// Vandermonde matrix itself) and solves it via matrix.h's general
// SVD-based inverse(), the same "reuse the matrix toolkit, don't
// hand-roll a solver" approach projection.h's iterative CT
// reconstruction and optical_flow.h's structure-tensor eigensolve
// already take. linear_fit()/parabolic_fit() are kept as the small,
// named-field (slope/intercept, a/b/c) convenience wrappers most
// callers actually want, implemented as one-line calls into
// polynomial_fit<1>()/polynomial_fit<2>() rather than their own
// separate closed-form math -- there is exactly one fitting
// implementation in this header now, not three.
/// @ingroup fit

namespace ndl
{
	/// Result of polynomial_fit<Degree>(): the least-squares coefficients of
	/// `y = coefficients[0] + coefficients[1]*x + ... + coefficients[Degree]*x^Degree`,
	/// plus the residual standard deviation. Callable directly to evaluate the fitted
	/// curve at a given x.
	/// @tparam Degree Polynomial degree (1 = line, 2 = parabola, ...).
	/// @ingroup fit
	template<int Degree>
	struct PolynomialFit
	{
		std::array<double, Degree + 1> coefficients;
		/// Unbiased sample standard deviation of the fit's own residuals (y - fitted value), n-(Degree+1) degrees of freedom. 0 when there's no residual degree of freedom left to estimate it from (n <= Degree+1).
		double residualStdDev;

		/// Evaluates the fitted polynomial at `x`.
		double operator()(double x) const
		{
			double result = 0.0, xPower = 1.0;
			for (int i = 0; i <= Degree; i++)
			{
				result += coefficients[i] * xPower;
				xPower *= x;
			}
			return result;
		}
	};

	// Ordinary least-squares fit of a degree-Degree polynomial over the n
	// (x,y) pairs [xBegin,xBegin+n) / [yBegin,yBegin+n). Closed form (the
	// standard normal-equation solution via the Moore-Penrose-equivalent
	// SVD inverse()), not an iterative method -- exact for any
	// n>=Degree+1, not an approximation that improves with more steps.
	/// Least-squares fit of a degree-`Degree` polynomial over n (x,y) sample pairs.
	/// @tparam Degree Polynomial degree; must be >= 1.
	/// @tparam XIt Forward iterator (or plain pointer) over x values, dereferencing to something convertible to double.
	/// @tparam YIt Forward iterator (or plain pointer) over y values, dereferencing to something convertible to double.
	/// @param  n      Number of samples; must be >= Degree+1.
	/// @param  xBegin First x sample.
	/// @param  yBegin First y sample.
	/// @return The fitted coefficients and residual standard deviation.
	/// @ingroup fit
	template<int Degree, class XIt, class YIt>
	PolynomialFit<Degree> polynomial_fit(int n, XIt xBegin, YIt yBegin)
	{
		static_assert(Degree >= 1, "ndl::polynomial_fit<Degree>() requires Degree >= 1");
		assert(n >= Degree + 1);
		constexpr int P = Degree + 1;

		// Power sums of x (indices 0..2*Degree) and of x^k*y (indices
		// 0..Degree) -- exactly the entries the Vandermonde normal-equation
		// matrix M (M[i][j] = sum(x^(i+j))) and right-hand side
		// (rhs[i] = sum(x^i * y)) are built from.
		double powerSumsX[2 * P - 1] = {};
		double powerSumsXY[P] = {};
		{
			XIt x = xBegin; YIt y = yBegin;
			for (int i = 0; i < n; i++, ++x, ++y)
			{
				double xi = static_cast<double>(*x), yi = static_cast<double>(*y);
				double xPower = 1.0;
				for (int k = 0; k < 2 * P - 1; k++) { powerSumsX[k] += xPower; xPower *= xi; }
				double xPowerY = yi;
				for (int k = 0; k < P; k++) { powerSumsXY[k] += xPowerY; xPowerY *= xi; }
			}
		}

		Matrix<double, P> M;
		for (int i = 0; i < P; i++)
			for (int j = 0; j < P; j++)
				M(i, j) = powerSumsX[i + j];

		std::array<double, P> rhs;
		for (int i = 0; i < P; i++) rhs[i] = powerSumsXY[i];

		PolynomialFit<Degree> result;
		result.coefficients = inverse(M) * rhs;

		double sumSquaredResiduals = 0.0;
		{
			XIt x = xBegin; YIt y = yBegin;
			for (int i = 0; i < n; i++, ++x, ++y)
			{
				double resid = static_cast<double>(*y) - result(static_cast<double>(*x));
				sumSquaredResiduals += resid * resid;
			}
		}
		int degreesOfFreedom = n - P;
		result.residualStdDev = (degreesOfFreedom > 0) ? std::sqrt(sumSquaredResiduals / degreesOfFreedom) : 0.0;
		return result;
	}

	/// Result of linear_fit(): the least-squares line `y = slope*x + intercept`, plus the residual standard deviation.
	struct LinearFit
	{
		double slope;
		double intercept;
		/// Unbiased sample standard deviation of the fit's own residuals (y - fitted value), n-2 degrees of freedom. 0 for n<=2 (no residual degree of freedom left to estimate it from).
		double residualStdDev;
	};

	/// Result of parabolic_fit(): the least-squares parabola `y = a + b*x + c*x^2`, plus the residual standard deviation.
	struct ParabolicFit
	{
		double a, b, c;
		/// Unbiased sample standard deviation of the fit's own residuals, n-3 degrees of freedom (3 fitted parameters). 0 for n<=3.
		double residualStdDev;
	};

	/// Least-squares fit of `y = slope*x + intercept` over n (x,y) sample pairs. A thin, named-field convenience wrapper over polynomial_fit<1>().
	/// @tparam XIt Forward iterator (or plain pointer) over x values, dereferencing to something convertible to double.
	/// @tparam YIt Forward iterator (or plain pointer) over y values, dereferencing to something convertible to double.
	/// @param  n      Number of samples; must be >= 2.
	/// @param  xBegin First x sample.
	/// @param  yBegin First y sample.
	/// @return The fitted slope/intercept and residual standard deviation.
	/// @ingroup fit
	template<class XIt, class YIt>
	LinearFit linear_fit(int n, XIt xBegin, YIt yBegin)
	{
		assert(n >= 2);
		PolynomialFit<1> fit = polynomial_fit<1>(n, xBegin, yBegin);
		return LinearFit{ fit.coefficients[1], fit.coefficients[0], fit.residualStdDev };
	}

	/// Least-squares fit of `y = a + b*x + c*x^2` over n (x,y) sample pairs. A thin, named-field convenience wrapper over polynomial_fit<2>().
	/// @tparam XIt Forward iterator (or plain pointer) over x values, dereferencing to something convertible to double.
	/// @tparam YIt Forward iterator (or plain pointer) over y values, dereferencing to something convertible to double.
	/// @param  n      Number of samples; must be >= 3.
	/// @param  xBegin First x sample.
	/// @param  yBegin First y sample.
	/// @return The fitted a/b/c and residual standard deviation.
	/// @ingroup fit
	template<class XIt, class YIt>
	ParabolicFit parabolic_fit(int n, XIt xBegin, YIt yBegin)
	{
		assert(n >= 3);
		PolynomialFit<2> fit = polynomial_fit<2>(n, xBegin, yBegin);
		return ParabolicFit{ fit.coefficients[0], fit.coefficients[1], fit.coefficients[2], fit.residualStdDev };
	}
}
