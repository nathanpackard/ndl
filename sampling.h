#pragma once
#include <cassert>
#include <array>
#include <vector>
#include <optional>
#include <cmath>
#include <type_traits>
#include "image.h"
#include "image/border_mode.h"
#include "interpolation.h"
#include "summed_area_table.h"
#include "matrix/core.h"
#include "matrix/transform.h"

// General N-to-N image resampling through an arbitrary Matrix<Real,DIM+1>
// homogeneous transform -- a sibling of interpolation.h/matrix.h, not part
// of either. #include this directly if you use sample_image().
//
// sample_image() is BACKWARD-mapped: the transform you pass maps
// DESTINATION coordinates to SOURCE coordinates, not the other way
// around -- the standard convention for resampling (a forward mapping
// leaves holes wherever no source pixel happened to land exactly on an
// integer destination pixel; backward mapping guarantees every
// destination pixel gets a value). If you have the "natural" forward
// transform (e.g. from make_rotate_matrix()), pass its inverse() instead
// (matrix/decomposition.h).
//
// Anti-aliasing is automatic and spatially uniform: the transform's own
// Jacobian (exact, via the same closed-form quotient-rule differentiation
// of the perspective divide transform_point() itself performs -- not a
// finite-difference approximation) is evaluated once, at the destination
// image's own center, giving a single box half-width per source axis
// (half the L1 norm of that Jacobian row -- the standard "AABB of the
// transformed unit destination pixel" bound). For an affine transform
// this Jacobian is exactly constant everywhere, so evaluating it once is
// exact, not an approximation; for a genuinely perspective N-to-N
// transform (rare -- the interesting perspective case is the
// dimension-REDUCING one, projection.h's forward_project()/
// back_project(), where the footprint truly does vary per sample) this is
// a representative approximation, not exact. When the resulting footprint
// exceeds about one source pixel (i.e. downsampling), every destination
// pixel is filled via summed_area_table.h's box_filter_query() (built
// once per call) instead of direct interpolation -- an anisotropic box
// filter, sized from basic sampling theory rather than tuned by hand.
// Below that (no aliasing risk -- same resolution or upsampling), it's
// skipped entirely and interpolation.h's sample() is used directly.
//
// The anti-aliased path only clamps at the source image's own border
// (box_filter_query() doesn't support Wrap/Reflect -- a per-query variable
// box can't be bounded in advance to build a padded copy against, see
// that function's own comment); `border` only governs the non-filtered
// (interpolation-only) path.

namespace ndl
{
	namespace detail
	{
		// Exact Jacobian (d source-coord / d dest-coord) of the perspective-
		// divide transform transform_point() (matrix/transform.h) itself
		// performs, at a given destination point p -- derived via the
		// quotient rule rather than finite-differenced, since the closed
		// form is just as cheap and exact rather than approximate.
		// jacobianOut is DIM*DIM, row-major, DIM = N-1.
		template<class Real, int N>
		void perspectiveJacobian(const Matrix<Real, N>& m, const Real* p, Real* jacobianOut)
		{
			constexpr int DIM = N - 1;
			Real w = m(DIM, DIM);
			for (int c = 0; c < DIM; c++) w += m(DIM, c) * p[c];

			Real out[DIM];
			for (int r = 0; r < DIM; r++)
			{
				Real t = m(r, DIM);
				for (int c = 0; c < DIM; c++) t += m(r, c) * p[c];
				out[r] = t / w;
			}
			for (int r = 0; r < DIM; r++)
				for (int j = 0; j < DIM; j++)
					jacobianOut[r * DIM + j] = (m(r, j) - out[r] * m(DIM, j)) / w;
		}
	}

	/// Resamples `src` into `dst` (which may differ in extent) through the homogeneous transform `dstToSrc`, which maps DESTINATION coordinates to SOURCE coordinates (backward mapping -- pass an inverse() if you have the forward transform instead). Automatically anti-aliases when downsampling (see this file's own top comment for exactly how).
	/// @tparam SrcImageT Any minimal-interface image type; its value_type must be arithmetic.
	/// @tparam DstImageT Any minimal-interface image type (same dimension as SrcImageT, but may have a different extent and a different concrete container).
	/// @tparam Interpolator One of Nearest/Linear/Quadratic/Cubic (interpolation.h). Defaults to Linear.
	/// @param  src         Source image.
	/// @param  dst         Destination; must already exist with its own (possibly different) extent.
	/// @param  dstToSrc    Homogeneous (DIM+1)x(DIM+1) matrix mapping destination coordinates to source coordinates.
	/// @param  interpolator Tag instance selecting the interpolation kernel; the type parameter is what actually matters.
	/// @param  autoAA      Whether to automatically anti-alias when downsampling. Defaults to true.
	/// @param  border      How an out-of-bounds source position is resolved on the non-anti-aliased path. Defaults to BorderMode::Clamp.
	/// @ingroup sampling
	template<class SrcImageT, class DstImageT, class Real, int N, class Interpolator = Linear>
	void sample_image(const SrcImageT& src, DstImageT& dst, const Matrix<Real, N>& dstToSrc, Interpolator interpolator = Interpolator{}, bool autoAA = true, BorderMode border = BorderMode::Clamp)
	{
		(void)interpolator;
		using SrcT = typename SrcImageT::value_type;
		using DstT = typename DstImageT::value_type;
		static_assert(std::is_arithmetic_v<SrcT>, "ndl::sample_image() requires an arithmetic source value_type -- not valid for e.g. std::complex<T>");
		static_assert(std::is_arithmetic_v<DstT>, "ndl::sample_image() requires an arithmetic destination value_type -- not valid for e.g. std::complex<T>");

		auto srcExtent = src.extent();
		auto dstExtent = dst.extent();
		constexpr int DIM = std::tuple_size<decltype(srcExtent)>::value;
		static_assert(std::tuple_size<decltype(dstExtent)>::value == DIM, "ndl::sample_image() requires src and dst to have the same dimensionality");
		static_assert(N == DIM + 1, "ndl::sample_image() requires a (DIM+1)x(DIM+1) homogeneous transform matching src/dst's own dimension");

		std::array<double, DIM> halfWidth{};
		bool anyFiltering = false;
		if (autoAA)
		{
			Real center[DIM];
			for (int i = 0; i < DIM; i++) center[i] = Real(dstExtent[i]) / Real(2);
			Real J[DIM * DIM];
			detail::perspectiveJacobian(dstToSrc, center, J);
			for (int r = 0; r < DIM; r++)
			{
				double h = 0;
				for (int j = 0; j < DIM; j++) h += std::abs((double)J[r * DIM + j]);
				h *= 0.5;
				halfWidth[r] = h;
				if (h > 0.5) anyFiltering = true;
			}
		}

		std::vector<double> tableData;
		std::optional<Image<double, DIM>> table;
		if (anyFiltering)
		{
			tableData.resize((std::size_t)Image<double, DIM>::size(srcExtent));
			table.emplace(tableData.data(), srcExtent);
			summed_area_table(src, *table);
		}

		for (const auto& dstCoord : dst.coordinates())
		{
			Real p[N];
			for (int i = 0; i < DIM; i++) p[i] = Real(dstCoord[i]);
			p[DIM] = Real(1);
			transform_point(dstToSrc, p);

			double val;
			if (table)
			{
				std::array<double, DIM> center;
				for (int i = 0; i < DIM; i++) center[i] = (double)p[i];
				val = box_filter_query(*table, center, halfWidth);
			}
			else
			{
				std::array<double, DIM> pos;
				for (int i = 0; i < DIM; i++) pos[i] = (double)p[i];
				val = sample(src, pos, interpolator, border);
			}
			dst.at(dstCoord) = static_cast<DstT>(val);
		}
	}
}
