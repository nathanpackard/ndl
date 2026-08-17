#pragma once
#include <cassert>
#include <cmath>
#include <vector>
#include <array>
#include <utility>
#include <complex>
#include <type_traits>
#include "../image/border_mode.h"
#include "../image.h"
#include "convolution.h"
#include "summed_area_table.h"

// The optical flow toolkit: lucas_kanade_flow(), as a free function over any
// minimal-interface image type. A sibling of fft.h/matrix.h/convolution.h/
// morphology.h/histogram.h/distance_transform.h/summed_area_table.h, not
// part of image.h's core Image object -- #include this directly if you use
// it.
//
// A displacement field for a DIM-dimensional pair of frames is itself an
// Image<double, DIM+1>: a leading "component" axis of size DIM (dx, dy, ...
// one entry per spatial axis), exactly the {channel, x, y} shape this
// library's own color images already use -- so a single component is just
// flow.slice(0, axis) away, no flow-specific plumbing needed anywhere else
// (per_channel(), heatmap(), saveForInspection(), all just work on it
// unmodified). That's the general, any-DIM representation every function
// here produces. For DIM==2 specifically, to_complex()/from_complex() below
// additionally convert to/from Image<std::complex<double>,2> -- complex
// doesn't generalize past 2D (there's no natural "complex-like" encoding of
// a 3+ component vector), but for 2D it's a genuinely idiomatic choice: this
// library already has full complex support (fft.h), and a 2D displacement's
// magnitude/direction are then just std::abs()/std::arg() away, with
// rotation reducing to a single complex multiply.

namespace ndl
{
	namespace detail
	{
		// Solves the DIM x DIM linear system A x = b via Gaussian elimination
		// with partial pivoting, in place on local copies of A/b. Returns
		// false (leaving x unmodified) if A is singular within a small
		// numerical tolerance -- lucas_kanade_flow()'s own "aperture problem"
		// case, where a window has too little gradient variation in some
		// direction to pin down a flow estimate there (e.g. a flat region, or
		// a single straight edge with nothing perpendicular to it in the
		// window) -- a small, self-contained solver rather than a dependency
		// on matrix.h's own (SVD-based, so comparatively expensive to call
		// once per pixel of a real image) Matrix::inverse().
		template<int DIM>
		bool solveLinearSystem(std::array<std::array<double, DIM>, DIM> A, std::array<double, DIM> b, std::array<double, DIM>& x)
		{
			for (int col = 0; col < DIM; col++)
			{
				int pivotRow = col;
				double pivotVal = std::abs(A[col][col]);
				for (int row = col + 1; row < DIM; row++)
				{
					double v = std::abs(A[row][col]);
					if (v > pivotVal) { pivotVal = v; pivotRow = row; }
				}
				if (pivotVal < 1e-10) return false;
				if (pivotRow != col) { std::swap(A[col], A[pivotRow]); std::swap(b[col], b[pivotRow]); }

				for (int row = col + 1; row < DIM; row++)
				{
					double factor = A[row][col] / A[col][col];
					for (int c = col; c < DIM; c++) A[row][c] -= factor * A[col][c];
					b[row] -= factor * b[col];
				}
			}
			for (int row = DIM - 1; row >= 0; row--)
			{
				double sum = b[row];
				for (int c = row + 1; c < DIM; c++) sum -= A[row][c] * x[c];
				x[row] = sum / A[row][row];
			}
			return true;
		}
	}

	// Classic Lucas-Kanade dense optical flow, generalized to any DIM via the
	// structure-tensor formulation: assuming brightness constancy
	// (frame1(x+v) ~= frame0(x) for small v) and Taylor-expanding gives
	// grad(frame0)(x).v ~= frame0(x)-frame1(x) at every position; pooling
	// that one-equation-per-pixel system over a local window and solving it
	// in the least-squares sense gives, at each pixel, the DIM x DIM normal
	// equations (sum over the window of grad grad^T) v = sum over the window
	// of grad*(frame0-frame1) -- the same "aggregate a per-pixel outer
	// product over a local window" shape as any structure-tensor method
	// (e.g. the Harris corner response), solved here via
	// detail::solveLinearSystem().
	//
	// The windowed sums above are computed via box_blur() (summed_area_table.h)
	// on each of the DIM*(DIM+1)/2 unique grad_i*grad_j product images (the
	// structure tensor is symmetric) and each of the DIM grad_i*temporalDiff
	// product images, rather than an explicit window loop per pixel --
	// box_blur() gives the windowed AVERAGE, not the true sum, but that's
	// fine here: scaling both sides of the normal equations by the same
	// constant (1/window area) doesn't change their solution, so the
	// averages plug in directly.
	/// Dense optical flow between two frames via windowed least-squares (Lucas-Kanade), generalized to any DIM.
	/// @tparam SrcImageT   Any minimal-interface image type; its value_type must be arithmetic.
	/// @tparam DstImageT   Any minimal-interface image type with exactly one more axis than SrcImageT (a leading component axis of size DIM); may differ from SrcImageT otherwise.
	/// @param  frame0      Earlier frame.
	/// @param  frame1      Later frame; same extent as frame0.
	/// @param  flowOut     Destination; must already exist, extent {DIM, ...frame0's own extent}.
	/// @param  windowRadius Local window half-width used to pool the per-pixel equations (window size 2*windowRadius+1 along every axis). Larger windows are more robust to noise but blur out fine detail in the flow field.
	/// @param  border      How an out-of-bounds neighbor is resolved, both for the gradient and the windowed pooling. Defaults to BorderMode::Reflect.
	/// @ingroup optical_flow
	template<class SrcImageT, class DstImageT>
	void lucas_kanade_flow(const SrcImageT& frame0, const SrcImageT& frame1, DstImageT& flowOut, int windowRadius = 5, BorderMode border = BorderMode::Reflect)
	{
		using SrcT = typename SrcImageT::value_type;
		static_assert(std::is_arithmetic_v<SrcT>, "ndl::lucas_kanade_flow() requires a value_type convertible to double -- not valid for e.g. std::complex<T>");
		assert(windowRadius > 0);
		assert(frame1.extent() == frame0.extent());

		auto extent = frame0.extent();
		constexpr int DIM = std::tuple_size<decltype(extent)>::value;
		auto flowExtent = flowOut.extent();
		constexpr int FlowDIM = std::tuple_size<decltype(flowExtent)>::value;
		static_assert(FlowDIM == DIM + 1, "ndl::lucas_kanade_flow() requires flowOut to have exactly one more axis than the frames (a leading component axis of size DIM)");
		assert(flowExtent[0] == DIM);
		for (int d = 0; d < DIM; d++) assert(flowExtent[d + 1] == extent[d]);

		std::array<int, DIM + 1> gradExtent;
		gradExtent[0] = DIM;
		for (int d = 0; d < DIM; d++) gradExtent[d + 1] = extent[d];
		std::vector<double> gradData(Image<double, DIM + 1>::size(gradExtent));
		Image<double, DIM + 1> grad(gradData.data(), gradExtent);
		gradient(frame0, grad, border);

		OwnedImage<double, DIM> temporalDiff(extent);
		{
			auto f0 = frame0.begin();
			auto f1 = frame1.begin();
			for (auto it = temporalDiff.begin(); it != temporalDiff.end(); ++it, ++f0, ++f1)
				*it = static_cast<double>(*f1) - static_cast<double>(*f0);
		}

		std::vector<std::pair<int, int>> pairs;
		for (int i = 0; i < DIM; i++)
			for (int j = i; j < DIM; j++)
				pairs.push_back({ i, j });

		std::vector<OwnedImage<double, DIM>> AijAvg;
		AijAvg.reserve(pairs.size());
		for (const auto& p : pairs)
		{
			auto gi = grad.slice(0, p.first);
			auto gj = grad.slice(0, p.second);
			OwnedImage<double, DIM> prod(extent);
			{
				auto giIt = gi.begin();
				auto gjIt = gj.begin();
				for (auto it = prod.begin(); it != prod.end(); ++it, ++giIt, ++gjIt) *it = (*giIt) * (*gjIt);
			}
			OwnedImage<double, DIM> avg(extent);
			box_blur(prod, avg, windowRadius, border);
			AijAvg.push_back(std::move(avg));
		}

		std::vector<OwnedImage<double, DIM>> bAvg;
		bAvg.reserve(DIM);
		for (int i = 0; i < DIM; i++)
		{
			auto gi = grad.slice(0, i);
			OwnedImage<double, DIM> prod(extent);
			{
				auto giIt = gi.begin();
				auto tdIt = temporalDiff.begin();
				for (auto it = prod.begin(); it != prod.end(); ++it, ++giIt, ++tdIt) *it = -(*giIt) * (*tdIt);
			}
			OwnedImage<double, DIM> avg(extent);
			box_blur(prod, avg, windowRadius, border);
			bAvg.push_back(std::move(avg));
		}

		for (const auto& coord : frame0.coordinates())
		{
			std::array<std::array<double, DIM>, DIM> Amat{};
			for (std::size_t k = 0; k < pairs.size(); k++)
			{
				int i = pairs[k].first, j = pairs[k].second;
				double v = AijAvg[k].at(coord);
				Amat[i][j] = v;
				Amat[j][i] = v;
			}
			std::array<double, DIM> bvec;
			for (int i = 0; i < DIM; i++) bvec[i] = bAvg[i].at(coord);

			std::array<double, DIM> v{};
			bool ok = detail::solveLinearSystem<DIM>(Amat, bvec, v);
			for (int d = 0; d < DIM; d++)
				flowOut.slice(0, d).at(coord) = ok ? v[d] : 0.0;
		}
	}

	// Converts the general {2, W, H} flow representation to a complex image:
	// dx (component 0) becomes the real part, dy (component 1) the
	// imaginary part -- the natural encoding once DIM is fixed at 2. Only
	// meaningful for a 2-component flow field; asserts flow's leading axis
	// is exactly 2.
	/// Converts a 2D flow field's {2,W,H} representation into an Image<std::complex<double>,2>: dx -> real part, dy -> imaginary part.
	/// @tparam SrcImageT Any minimal-interface image type with a leading component axis of size 2.
	/// @tparam DstImageT Any minimal-interface 2D image type whose value_type is std::complex<double> (or constructible the same way).
	/// @param  flow       Source flow field, extent {2, W, H}.
	/// @param  complexOut Destination; must already exist, extent {W, H}.
	/// @ingroup optical_flow
	template<class SrcImageT, class DstImageT>
	void to_complex(const SrcImageT& flow, DstImageT& complexOut)
	{
		using DstT = typename DstImageT::value_type;
		auto flowExtent = flow.extent();
		assert(flowExtent[0] == 2);
		auto outExtent = complexOut.extent();
		assert(outExtent[0] == flowExtent[1] && outExtent[1] == flowExtent[2]);

		auto dx = flow.slice(0, 0);
		auto dy = flow.slice(0, 1);
		for (const auto& coord : dx.coordinates())
			complexOut.at(coord) = DstT(dx.at(coord), dy.at(coord));
	}

	// The inverse of to_complex(): real part -> dx (component 0), imaginary
	// part -> dy (component 1).
	/// Converts an Image<std::complex<double>,2> back into the general {2,W,H} flow representation: real part -> dx, imaginary part -> dy.
	/// @tparam SrcImageT Any minimal-interface 2D image type whose value_type is std::complex<double> (or exposes .real()/.imag()).
	/// @tparam DstImageT Any minimal-interface image type with a leading component axis of size 2.
	/// @param  complexFlow Source, extent {W, H}.
	/// @param  flowOut     Destination; must already exist, extent {2, W, H}.
	/// @ingroup optical_flow
	template<class SrcImageT, class DstImageT>
	void from_complex(const SrcImageT& complexFlow, DstImageT& flowOut)
	{
		auto ce = complexFlow.extent();
		auto flowExtent = flowOut.extent();
		assert(flowExtent[0] == 2 && flowExtent[1] == ce[0] && flowExtent[2] == ce[1]);

		auto dx = flowOut.slice(0, 0);
		auto dy = flowOut.slice(0, 1);
		for (const auto& coord : complexFlow.coordinates())
		{
			auto c = complexFlow.at(coord);
			dx.at(coord) = c.real();
			dy.at(coord) = c.imag();
		}
	}
}
