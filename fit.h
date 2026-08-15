#pragma once
#include <cassert>
#include <cmath>

// Ordinary least-squares curve fitting over a plain sequence of (x,y)
// sample pairs -- a sibling of histogram.h/morphology.h, not part of
// image.h's core Image object. Iterator-based (like Histogram's own
// general constructor takes any CoordRange) rather than tied to any
// particular container: the samples being fit are just points, not
// image data, so there's no Image/minimal-interface involvement here at
// all.
/// @ingroup fit

namespace ndl
{
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

	// Ordinary least-squares fit of y = slope*x + intercept over the n
	// (x,y) pairs [xBegin,xBegin+n) / [yBegin,yBegin+n). Closed form (the
	// standard two-variable normal-equation solution), not an iterative
	// method -- exact for any n>=2, not an approximation that improves with
	// more steps.
	/// Least-squares fit of `y = slope*x + intercept` over n (x,y) sample pairs.
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
		double sumX = 0, sumX2 = 0, sumY = 0, sumXY = 0;
		{
			XIt x = xBegin; YIt y = yBegin;
			for (int i = 0; i < n; i++, ++x, ++y)
			{
				double xi = static_cast<double>(*x), yi = static_cast<double>(*y);
				sumX += xi; sumX2 += xi * xi; sumY += yi; sumXY += xi * yi;
			}
		}
		double nd = static_cast<double>(n);
		double denom = nd * sumX2 - sumX * sumX;
		LinearFit result;
		result.slope = (nd * sumXY - sumX * sumY) / denom;
		result.intercept = (sumY - result.slope * sumX) / nd;

		double sumSquaredResiduals = 0;
		{
			XIt x = xBegin; YIt y = yBegin;
			for (int i = 0; i < n; i++, ++x, ++y)
			{
				double resid = static_cast<double>(*y) - (result.slope * static_cast<double>(*x) + result.intercept);
				sumSquaredResiduals += resid * resid;
			}
		}
		result.residualStdDev = (n > 2) ? std::sqrt(sumSquaredResiduals / (nd - 2.0)) : 0.0;
		return result;
	}

	// Ordinary least-squares fit of y = a + b*x + c*x^2 -- same closed-form
	// approach as linear_fit(), just with the 3x3 normal-equation system
	// (in the power sums of x, up to x^4) solved directly via Cramer's
	// rule rather than pulling in matrix.h's general linear-solve
	// machinery for what's always exactly a 3x3 system here.
	/// Least-squares fit of `y = a + b*x + c*x^2` over n (x,y) sample pairs.
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
		double Sx0 = static_cast<double>(n), Sx1 = 0, Sx2 = 0, Sx3 = 0, Sx4 = 0;
		double Sy0 = 0, Sy1 = 0, Sy2 = 0;
		{
			XIt x = xBegin; YIt y = yBegin;
			for (int i = 0; i < n; i++, ++x, ++y)
			{
				double xi = static_cast<double>(*x), yi = static_cast<double>(*y);
				double xi2 = xi * xi;
				Sx1 += xi; Sx2 += xi2; Sx3 += xi2 * xi; Sx4 += xi2 * xi2;
				Sy0 += yi; Sy1 += xi * yi; Sy2 += xi2 * yi;
			}
		}

		// Normal equations, in matrix form M*[a,b,c]^T = rhs:
		//   [Sx0 Sx1 Sx2] [a]   [Sy0]
		//   [Sx1 Sx2 Sx3] [b] = [Sy1]
		//   [Sx2 Sx3 Sx4] [c]   [Sy2]
		// Solved via Cramer's rule: each unknown is det(M with that
		// column replaced by rhs) / det(M).
		auto det3 = [](double m00, double m01, double m02,
		                double m10, double m11, double m12,
		                double m20, double m21, double m22)
		{
			return m00 * (m11 * m22 - m12 * m21)
			     - m01 * (m10 * m22 - m12 * m20)
			     + m02 * (m10 * m21 - m11 * m20);
		};

		double detM = det3(Sx0, Sx1, Sx2, Sx1, Sx2, Sx3, Sx2, Sx3, Sx4);
		ParabolicFit result;
		result.a = det3(Sy0, Sx1, Sx2, Sy1, Sx2, Sx3, Sy2, Sx3, Sx4) / detM;
		result.b = det3(Sx0, Sy0, Sx2, Sx1, Sy1, Sx3, Sx2, Sy2, Sx4) / detM;
		result.c = det3(Sx0, Sx1, Sy0, Sx1, Sx2, Sy1, Sx2, Sx3, Sy2) / detM;

		double sumSquaredResiduals = 0;
		{
			XIt x = xBegin; YIt y = yBegin;
			for (int i = 0; i < n; i++, ++x, ++y)
			{
				double xi = static_cast<double>(*x);
				double resid = static_cast<double>(*y) - (result.a + result.b * xi + result.c * xi * xi);
				sumSquaredResiduals += resid * resid;
			}
		}
		result.residualStdDev = (n > 3) ? std::sqrt(sumSquaredResiduals / (static_cast<double>(n) - 3.0)) : 0.0;
		return result;
	}
}
