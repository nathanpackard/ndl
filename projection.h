#pragma once
#include <cassert>
#include <vector>
#include <array>
#include <optional>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <type_traits>
#include "image.h"
#include "interpolation.h"
#include "summed_area_table.h"
#include "matrix/core.h"
#include "matrix/decomposition.h"
#include "matrix/projection.h"

// CT-style forward/back projection: forward_project() (volume -> sinogram,
// D dimensions -> D dimensions with a 1-view + (D-1)-detector layout) and
// back_project() (the reverse), driven by a std::vector<ProjectionMatrix<
// Real,D>>, one per view (matrix/projection.h). A sibling of matrix.h/
// interpolation.h/summed_area_table.h, not part of any of them --
// #include this directly if you use it.
//
// A volume is Image<T,D> (D spatial axes); a sinogram is also Image<T,D>,
// but structured as {numViews, detectorAxis0, ..., detectorAxis(D-2)} --
// one leading view axis (matching this library's existing
// {channel,...}/{view,...} leading-axis convention for color images and
// flow fields) plus D-1 detector axes.
//
// forward_project() ray-marches: for each view and each detector pixel,
// ray_for_pixel() (matrix/projection.h) resolves the corresponding ray
// through the volume, which is then sampled (interpolation.h's sample())
// at evenly-spaced points and Riemann/trapezoid-summed into that sinogram
// cell, scaled by the step size -- a direct discretization of the Radon
// transform's line integral.
//
// back_project() is built to be the EXACT adjoint of forward_project(),
// not merely an approximation of one -- a real subtlety worth being
// explicit about. The obvious-looking alternative ("voxel-driven":
// project each voxel's center to its detector coordinate per view and
// gather-interpolate the sinogram there) is what most CT
// libraries actually ship, and is a perfectly reasonable back-projector
// on its own terms, but it is NOT the exact transpose of ray-marched
// forward projection: forward projection's matrix entry linking a given
// sinogram cell to a given voxel is a SUM over every ray sample point
// whose interpolation support includes that voxel, weighted by that
// sample's own (trapezoid weight * step size); a single voxel-driven
// gather only ever touches one sinogram cell per (voxel, view), with an
// implicit weight of 1 -- a structurally different computation, not just
// a numerically close one. So instead, back_project() re-walks the
// IDENTICAL rays forward_project() would (same geometry, same step size,
// same interpolation kernel -- detail::planRaySamples() is the one
// function both call, so they can't silently drift apart), and at each
// sample point SCATTERS (interpolation.h's scatter_add(), the exact
// adjoint of sample() -- verified directly, see unitTests/) the sinogram
// cell's value back into the volume with the same per-sample weight
// forward_project() used to read it. Every term matches by construction,
// which is what makes forward_project()/back_project() pass a
// dot-product adjointness test (<forward_project(x), y> == <x,
// back_project(y)>) to near machine precision -- not approximately.
//
// Automatic anti-aliasing (autoAA, default on) is scoped narrower than
// that adjoint guarantee: forward_project() prefilters the volume (via
// summed_area_table.h's box_filter_query(), sized once per view from the
// view's own Jacobian at the volume's center -- see
// matrix/projection.h's projectionJacobian()) whenever the volume's own
// resolution would alias against the detector's, but back_project()'s
// scatter pass does not apply a matching filter on the way back. Making
// AA-filtered forward projection and its exact adjoint agree would need
// back_project() to apply the SAME box filter as a second pass over its
// own scatter-accumulated result (matched-filter adjoint pairs, a real
// but separable piece of work) -- not done here. In practice this means:
// adjointness is exact when forward_project()'s AA path doesn't actually
// trigger (matched voxel/detector resolution -- the common case, and
// what this library's own adjointness unit test deliberately arranges),
// and only approximate when it does.

namespace ndl
{
	namespace detail
	{
		// Ray-AABB-clip (the volume's own [0,extent-1] per-axis box, via
		// the standard slab method) plus the resulting evenly-spaced
		// sample-point plan -- shared verbatim by forward_project() and
		// back_project() so they walk IDENTICAL sample points along a
		// given ray, which is what makes them exact adjoints of each
		// other (see this file's own top comment).
		struct RaySamplePlan
		{
			bool valid = false;
			double tMin = 0, tMax = 0;
			int numSteps = 0;
			double ds = 0;
		};

		// M (volExtent's own array size) is an independent template
		// parameter from D, reconciled below via a static_assert, rather
		// than reusing D for both -- the same int-vs-size_t deduction
		// pitfall documented throughout matrix/decomposition.h,
		// matrix/transform.h, and matrix/projection.h: D is already
		// unambiguously deducible from `ray` alone (ProjectionRay<Real,D>
		// is int-parameterized), so letting volExtent's std::array also
		// try to deduce it (as std::size_t) would conflict.
		template<class Real, int D, std::size_t M>
		RaySamplePlan planRaySamples(const ProjectionRay<Real, D>& ray, const std::array<int, M>& volExtent, double stepSize)
		{
			static_assert(M == (std::size_t)D, "ndl::detail::planRaySamples() requires volExtent to have exactly D elements");
			double tMin = -1e300, tMax = 1e300;
			for (int i = 0; i < D; i++)
			{
				double o = (double)ray.origin[i], d = (double)ray.direction[i];
				double lo = 0.0, hi = (double)(volExtent[i] - 1);
				if (std::abs(d) < 1e-12)
				{
					if (o < lo || o > hi) return RaySamplePlan{};
				}
				else
				{
					double t0 = (lo - o) / d, t1 = (hi - o) / d;
					if (t0 > t1) std::swap(t0, t1);
					tMin = std::max(tMin, t0);
					tMax = std::min(tMax, t1);
				}
			}
			if (tMin > tMax) return RaySamplePlan{};

			RaySamplePlan plan;
			plan.valid = true;
			plan.tMin = tMin;
			plan.tMax = tMax;
			plan.numSteps = std::max(1, (int)std::ceil((tMax - tMin) / stepSize));
			plan.ds = (tMax - tMin) / plan.numSteps;
			return plan;
		}

		// Per-axis volume-space AA footprint half-width for forward_project(),
		// evaluated once per view at the volume's own center (see this
		// file's top comment on the resulting scope of automatic
		// anti-aliasing). "How far does a unit step in DETECTOR space
		// reach in VOLUME space" -- the inverse of projectionJacobian()
		// (matrix/projection.h), recovered the same way ray_for_pixel()
		// recovers a point from a (D-1)-equation system: augment with the
		// ray direction as one more probe equation to make it square,
		// then invert (reusing the existing inverse(), matrix/
		// decomposition.h).
		template<class Real, int D, std::size_t M>
		std::array<double, D> forwardAAHalfWidth(const ProjectionMatrix<Real, D>& pm, const ProjectionCenter<Real, D>& center, const std::array<int, M>& volExtent)
		{
			static_assert(M == (std::size_t)D, "ndl::detail::forwardAAHalfWidth() requires volExtent to have exactly D elements");
			Real volCenter[D];
			for (int i = 0; i < D; i++) volCenter[i] = Real(volExtent[i] - 1) / Real(2);
			Real J[(D - 1) * D];
			projectionJacobian(pm, volCenter, J);

			Matrix<Real, D> augmented;
			for (int r = 0; r < D - 1; r++)
				for (int c = 0; c < D; c++)
					augmented(r, c) = J[r * D + c];
			for (int c = 0; c < D; c++)
				augmented(D - 1, c) = center.atInfinity ? center.point[c] : (volCenter[c] - center.point[c]);
			Matrix<Real, D> inv = inverse(augmented);

			std::array<double, D> halfWidth{};
			for (int r = 0; r < D; r++)
			{
				double h = 0;
				for (int k = 0; k < D - 1; k++) h += std::abs((double)inv(r, k));
				halfWidth[r] = 0.5 * h;
			}
			return halfWidth;
		}
	}

	/// Forward-projects `volume` into `sinogram` (overwritten) by ray-marching through the geometry described by `geometry` (one ProjectionMatrix per view -- matrix/projection.h). See this file's own top comment for the exact algorithm and the scope of automatic anti-aliasing.
	/// @tparam VolumeImageT Any minimal-interface image type; its value_type must be arithmetic.
	/// @tparam SinogramImageT Any minimal-interface image type (D dimensions: 1 view axis + D-1 detector axes); its own extent's leading axis must equal geometry.size().
	/// @tparam Interpolator One of Nearest/Linear/Quadratic/Cubic (interpolation.h). Defaults to Linear.
	/// @param  volume       Source volume, D dimensions.
	/// @param  sinogram     Destination; must already exist with extent {geometry.size(), detector extent...}.
	/// @param  geometry     One ProjectionMatrix<Real,D> per view.
	/// @param  interpolator Tag instance selecting the interpolation kernel; the type parameter is what actually matters.
	/// @param  autoAA       Whether to automatically anti-alias the volume when its resolution would alias against the detector's. Defaults to true.
	/// @param  stepSize     Ray-marching step size, in volume-grid units. Defaults to 1.0 (one step per voxel spacing).
	/// @ingroup projection
	template<class VolumeImageT, class SinogramImageT, class Real, int D, class Interpolator = Linear>
	void forward_project(const VolumeImageT& volume, SinogramImageT& sinogram, const std::vector<ProjectionMatrix<Real, D>>& geometry, Interpolator interpolator = Interpolator{}, bool autoAA = true, double stepSize = 1.0)
	{
		using VolT = typename VolumeImageT::value_type;
		using SinoT = typename SinogramImageT::value_type;
		static_assert(std::is_arithmetic_v<VolT>, "ndl::forward_project() requires an arithmetic volume value_type");
		static_assert(std::is_arithmetic_v<SinoT>, "ndl::forward_project() requires an arithmetic sinogram value_type");

		auto volExtent = volume.extent();
		auto sinoExtent = sinogram.extent();
		constexpr int DIM = std::tuple_size<decltype(volExtent)>::value;
		static_assert(DIM == D, "ndl::forward_project() requires volume to have exactly D dimensions");
		static_assert(std::tuple_size<decltype(sinoExtent)>::value == D, "ndl::forward_project() requires sinogram to have exactly D dimensions (1 view axis + D-1 detector axes)");
		assert(sinoExtent[0] == (int)geometry.size());

		std::array<int, D - 1> detExtent;
		for (int i = 0; i < D - 1; i++) detExtent[i] = sinoExtent[i + 1];
		std::size_t numDetPixels = 1;
		for (int i = 0; i < D - 1; i++) numDetPixels *= (std::size_t)detExtent[i];

		for (const auto& c : sinogram.coordinates()) sinogram.at(c) = SinoT(0);

		for (int view = 0; view < (int)geometry.size(); view++)
		{
			const auto& pm = geometry[view];
			auto center = camera_center(pm);

			bool anyFiltering = false;
			std::array<double, D> halfWidth{};
			if (autoAA)
			{
				halfWidth = detail::forwardAAHalfWidth(pm, center, volExtent);
				for (int r = 0; r < D; r++) if (halfWidth[r] > 0.5) anyFiltering = true;
			}

			std::vector<double> tableData;
			std::optional<Image<double, D>> table;
			if (anyFiltering)
			{
				tableData.resize((std::size_t)Image<double, D>::size(volExtent));
				table.emplace(tableData.data(), volExtent);
				summed_area_table(volume, *table);
			}

			std::array<int, D - 1> detCoord{};
			for (std::size_t idx = 0; idx < numDetPixels; idx++)
			{
				std::array<Real, D - 1> detCoordReal;
				for (int i = 0; i < D - 1; i++) detCoordReal[i] = Real(detCoord[i]);
				auto ray = ray_for_pixel(pm, center, detCoordReal);
				auto plan = detail::planRaySamples<Real, D>(ray, volExtent, stepSize);

				if (plan.valid)
				{
					double accum = 0;
					for (int s = 0; s <= plan.numSteps; s++)
					{
						double t = plan.tMin + s * plan.ds;
						std::array<double, D> pos;
						for (int i = 0; i < D; i++) pos[i] = (double)ray.origin[i] + t * (double)ray.direction[i];
						double val = table ? box_filter_query(*table, pos, halfWidth) : sample(volume, pos, interpolator, BorderMode::Clamp);
						double w = (s == 0 || s == plan.numSteps) ? 0.5 : 1.0;
						accum += w * val;
					}
					accum *= plan.ds;

					std::array<int, D> fullCoord;
					fullCoord[0] = view;
					for (int i = 0; i < D - 1; i++) fullCoord[i + 1] = detCoord[i];
					sinogram.at(fullCoord) = static_cast<SinoT>(accum);
				}

				for (int d = 0; d < D - 1; d++) { if (++detCoord[d] < detExtent[d]) break; detCoord[d] = 0; }
			}
		}
	}

	/// Back-projects `sinogram` into `volume` (overwritten): the exact adjoint of forward_project() (see this file's own top comment for the ray-driven-scatter construction that guarantees it, and the scope of automatic anti-aliasing).
	/// @tparam SinogramImageT Any minimal-interface image type (D dimensions: 1 view axis + D-1 detector axes).
	/// @tparam VolumeImageT Any minimal-interface image type; its value_type must be arithmetic.
	/// @tparam Interpolator One of Nearest/Linear/Quadratic/Cubic (interpolation.h). Defaults to Linear -- MUST match the Interpolator forward_project() was called with for the two to actually be adjoints of each other.
	/// @param  sinogram     Source sinogram.
	/// @param  volume       Destination; must already exist with the target volume extent (D dimensions).
	/// @param  geometry     One ProjectionMatrix<Real,D> per view, matching forward_project()'s own.
	/// @param  interpolator Tag instance selecting the interpolation kernel; the type parameter is what actually matters.
	/// @param  autoAA       Accepted for signature symmetry with forward_project(), but has no effect here -- see this file's own top comment on why AA-filtered adjointness isn't implemented.
	/// @param  stepSize     Ray-marching step size, in volume-grid units -- MUST match forward_project()'s own for exact adjointness. Defaults to 1.0.
	/// @ingroup projection
	template<class SinogramImageT, class VolumeImageT, class Real, int D, class Interpolator = Linear>
	void back_project(const SinogramImageT& sinogram, VolumeImageT& volume, const std::vector<ProjectionMatrix<Real, D>>& geometry, Interpolator interpolator = Interpolator{}, bool autoAA = true, double stepSize = 1.0)
	{
		(void)autoAA;
		using SinoT = typename SinogramImageT::value_type;
		using VolT = typename VolumeImageT::value_type;
		static_assert(std::is_arithmetic_v<SinoT>, "ndl::back_project() requires an arithmetic sinogram value_type");
		static_assert(std::is_arithmetic_v<VolT>, "ndl::back_project() requires an arithmetic volume value_type");

		auto volExtent = volume.extent();
		auto sinoExtent = sinogram.extent();
		constexpr int DIM = std::tuple_size<decltype(volExtent)>::value;
		static_assert(DIM == D, "ndl::back_project() requires volume to have exactly D dimensions");
		static_assert(std::tuple_size<decltype(sinoExtent)>::value == D, "ndl::back_project() requires sinogram to have exactly D dimensions (1 view axis + D-1 detector axes)");
		assert(sinoExtent[0] == (int)geometry.size());

		std::array<int, D - 1> detExtent;
		for (int i = 0; i < D - 1; i++) detExtent[i] = sinoExtent[i + 1];
		std::size_t numDetPixels = 1;
		for (int i = 0; i < D - 1; i++) numDetPixels *= (std::size_t)detExtent[i];

		// Accumulated in double regardless of VolT (the same reasoning
		// convolve()/box_blur() apply to their own accumulation) -- many
		// overlapping ray contributions land in the same voxel, and a
		// narrow VolT would compound rounding error across them.
		std::vector<double> accumData((std::size_t)Image<double, D>::size(volExtent), 0.0);
		Image<double, D> accum(accumData.data(), volExtent);

		for (int view = 0; view < (int)geometry.size(); view++)
		{
			const auto& pm = geometry[view];
			auto center = camera_center(pm);

			std::array<int, D - 1> detCoord{};
			for (std::size_t idx = 0; idx < numDetPixels; idx++)
			{
				std::array<Real, D - 1> detCoordReal;
				for (int i = 0; i < D - 1; i++) detCoordReal[i] = Real(detCoord[i]);
				auto ray = ray_for_pixel(pm, center, detCoordReal);
				auto plan = detail::planRaySamples<Real, D>(ray, volExtent, stepSize);

				if (plan.valid)
				{
					std::array<int, D> fullCoord;
					fullCoord[0] = view;
					for (int i = 0; i < D - 1; i++) fullCoord[i + 1] = detCoord[i];
					double sinoVal = (double)sinogram.at(fullCoord);

					if (sinoVal != 0.0)
					{
						for (int s = 0; s <= plan.numSteps; s++)
						{
							double t = plan.tMin + s * plan.ds;
							std::array<double, D> pos;
							for (int i = 0; i < D; i++) pos[i] = (double)ray.origin[i] + t * (double)ray.direction[i];
							double w = (s == 0 || s == plan.numSteps) ? 0.5 : 1.0;
							scatter_add(accum, pos, w * plan.ds * sinoVal, interpolator, BorderMode::Clamp);
						}
					}
				}

				for (int d = 0; d < D - 1; d++) { if (++detCoord[d] < detExtent[d]) break; detCoord[d] = 0; }
			}
		}

		for (const auto& c : volume.coordinates()) volume.at(c) = static_cast<VolT>(accum.at(c));
	}

	// Per-voxel upper density bound, derived directly from the measured
	// sinogram via the same non-negativity assumption that justifies
	// clamping a reconstruction's own LOWER bound to 0 (attenuation/
	// density can't be negative): forward_project()'s ray sum is a
	// Riemann sum, ray_sum = sum_i density_i * stepSize, and since every
	// density_i >= 0, no single sample along that ray can exceed
	// ray_sum / stepSize (every OTHER term in the sum is itself >= 0, so
	// it alone can't be responsible for more than the whole). Taking the
	// MINIMUM of this bound over every view whose ray passes through a
	// given voxel (a nearest-detector-pixel lookup, not full
	// interpolation -- exactness isn't needed for a bound, and this
	// avoids the interpolation kernel's fractional weight-splitting
	// complicating what "this ray's own bound" even means for a single
	// voxel) gives the tightest bound this simple per-ray argument can
	// produce.
	//
	// This bound is genuinely tight for background/empty-space voxels --
	// many views' rays through them measure near-zero, correctly forcing
	// the bound there close to zero -- and much looser for interior
	// voxels of a large object (every view's ray through a big dense
	// region also crosses a lot of OTHER dense material, so
	// ray_sum/stepSize stays large there too, nowhere near that single
	// voxel's own true density). That asymmetry is exactly right for
	// suppressing reconstruction overshoot artifacts specifically in the
	// background, without needing a tighter (and much harder to compute)
	// bound everywhere.
	/// Per-voxel upper density bound derived from `sinogram`, via the non-negativity assumption real CT reconstruction relies on -- see this function's own comment for the derivation.
	/// @tparam SinogramImageT Any minimal-interface image type (D dimensions: 1 view axis + D-1 detector axes).
	/// @tparam VolumeImageT Any minimal-interface image type; its value_type must be arithmetic.
	/// @param  sinogram Source sinogram.
	/// @param  bound    Destination; must already exist with the target volume extent (D dimensions). A voxel never covered by any view's detector is left at +infinity (no constraint from this function).
	/// @param  geometry One ProjectionMatrix<Real,D> per view, matching forward_project()'s own.
	/// @param  stepSize Ray-marching step size used to produce `sinogram` -- MUST match forward_project()'s own for the bound to be valid.
	/// @ingroup projection
	template<class SinogramImageT, class VolumeImageT, class Real, int D>
	void max_density_bound(const SinogramImageT& sinogram, VolumeImageT& bound, const std::vector<ProjectionMatrix<Real, D>>& geometry, double stepSize = 1.0)
	{
		using SinoT = typename SinogramImageT::value_type;
		using BoundT = typename VolumeImageT::value_type;
		static_assert(std::is_arithmetic_v<SinoT>, "ndl::max_density_bound() requires an arithmetic sinogram value_type");
		static_assert(std::is_arithmetic_v<BoundT>, "ndl::max_density_bound() requires an arithmetic bound value_type");

		auto volExtent = bound.extent();
		auto sinoExtent = sinogram.extent();
		constexpr int DIM = std::tuple_size<decltype(volExtent)>::value;
		static_assert(DIM == D, "ndl::max_density_bound() requires bound to have exactly D dimensions");
		static_assert(std::tuple_size<decltype(sinoExtent)>::value == D, "ndl::max_density_bound() requires sinogram to have exactly D dimensions (1 view axis + D-1 detector axes)");
		assert(sinoExtent[0] == (int)geometry.size());

		std::array<int, D - 1> detExtent;
		for (int i = 0; i < D - 1; i++) detExtent[i] = sinoExtent[i + 1];

		for (const auto& voxCoord : bound.coordinates())
		{
			double best = std::numeric_limits<double>::infinity();
			Real p[D];
			for (int i = 0; i < D; i++) p[i] = Real(voxCoord[i]);

			for (int view = 0; view < (int)geometry.size(); view++)
			{
				Real u[D - 1];
				project_point(geometry[view], p, u);

				std::array<int, D> fullCoord;
				fullCoord[0] = view;
				bool inBounds = true;
				for (int i = 0; i < D - 1; i++)
				{
					int idx = (int)std::lround((double)u[i]);
					if (idx < 0 || idx >= detExtent[i]) { inBounds = false; break; }
					fullCoord[i + 1] = idx;
				}
				if (!inBounds) continue;

				double sinoVal = std::max(0.0, (double)sinogram.at(fullCoord));
				best = std::min(best, sinoVal / stepSize);
			}

			bound.at(voxCoord) = static_cast<BoundT>(best);
		}
	}
}
