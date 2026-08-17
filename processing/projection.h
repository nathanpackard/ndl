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
#include <execution>
#include <mutex>
#include <thread>
#include <numeric>
#include "../image.h"
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
// back_project() is built to be forward_project()'s adjoint by
// CONSTRUCTION, not merely an approximation of one -- a real subtlety
// worth being explicit about. The obvious-looking alternative
// ("voxel-driven": project each voxel's center to its detector coordinate
// per view and gather-interpolate the sinogram there) is what most CT
// libraries actually ship, and is a perfectly reasonable back-projector on
// its own terms, but it is NOT the transpose of ray-marched forward
// projection: forward projection's matrix entry linking a given sinogram
// cell to a given voxel is a SUM over every ray sample point whose
// interpolation support includes that voxel, weighted by that sample's
// own (trapezoid weight * step size); a single voxel-driven gather only
// ever touches one sinogram cell per (voxel, view), with an implicit
// weight of 1 -- a structurally different computation, not just a
// numerically close one. So instead, back_project() re-walks the IDENTICAL
// rays forward_project() would (same geometry, same step size, same
// interpolation kernel -- detail::planRaySamples() is the one function
// both call, so they can't silently drift apart), and at each sample point
// SCATTERS (interpolation.h's scatter_add(), the exact adjoint of
// sample()) the sinogram cell's value back into the volume with the same
// per-sample weight forward_project() used to read it.
//
// Automatic anti-aliasing (autoAA, default on) is a MATCHED pair, not just
// a forward_project()-only feature -- and matched at the level of the
// per-sample READ ITSELF, not as a separate blur stage composed around
// the existing operators. forward_project() doesn't blur the volume and
// then interpolate it normally; at EACH ray sample point it calls
// summed_area_table.h's box_filter_query() DIRECTLY in place of
// interpolation.h's sample(), sized from detail::sampleAAHalfWidth()
// (matrix/projection.h's projectionJacobian(), inverted) evaluated AT
// THAT SAMPLE'S OWN POSITION, not once per view at the volume's center:
// for a true perspective (cone/fan-beam) matrix, a voxel's real footprint
// on the detector depends on its own distance from the source, and
// measured directly on demo/ct_reconstruction_3d's own geometry, that
// footprint varies by a factor of ~1.5x between the near-source and
// near-detector sides of the volume -- a single once-per-view value would
// over-filter (losing real resolution) on one side and under-filter
// (leaving aliasing uncorrected) on the other, in every view. Making this
// genuinely per-sample needs a matrix inversion at every one of tens of
// millions of ray samples, which is why sampleAAHalfWidth() uses
// fast_inverse() (matrix/decomposition.h -- a direct adjugate/cofactor
// inverse, reusing the existing determinant()/cofactor()) instead of the
// library's general SVD-based inverse() -- ~3.6x faster, verified to
// match on well-conditioned input, which is what keeps this practical at
// all. matrix/projection.h's ray_for_pixel() (the ray-resolving step both
// forward_project() and back_project() call at every detector pixel, per
// view) uses the same fast_inverse() for the same reason.
//
// The exact adjoint back_project() needs isn't "scatter normally, then
// blur the result": that composition is NOT the transpose of
// forward_project()'s own per-sample box_filter_query() read, and misses
// the true adjoint badly (off by 100% in places on a direct dot-product
// check) -- it's swapping in box_filter_scatter_add() in place of
// scatter_add() at exactly the same per-sample point, the same way
// box_filter_query() itself replaces sample(), using the SAME per-sample
// sampleAAHalfWidth() evaluation (a pure function of the matrix, the
// camera center, and the sample's own position, so back_project()
// reproduces exactly what forward_project() would have used at that
// point without the two ever exchanging state).
// box_filter_scatter_add() is built to be box_filter_query()'s own exact
// adjoint directly (verified in isolation in
// unitTests/summed_area_table_tests.cpp) -- not by assuming the box blur
// is a symmetric operator (it isn't, right at a volume's boundary, where
// the query window gets clamped asymmetrically) and hoping that's close
// enough, but by literally computing the transpose of what
// box_filter_query() does, via the classic D-dimensional difference-array
// technique (see summed_area_table.h's own comment). That's what makes
// the measured adjointness error stay at machine precision with autoAA on
// -- unitTests/projection_tests.cpp checks this directly, including a
// case where the data touches the volume's own boundary -- rather than
// being only an approximation that happens to be good in the interior.
//
// Both functions parallelize over views via std::execution::par when a
// parallel STL backend is available (this library's own CMakeLists.txt
// links TBB for this if found, the same optional dependency fft.h's
// fftn()/ifftn() already use -- still correct, just single-threaded,
// without it). forward_project() is embarrassingly parallel (each view
// writes only its own disjoint sinogram rows); back_project() has to
// merge every view's own contribution into the SAME shared volume, so it
// chunks views across std::thread::hardware_concurrency() worker chunks,
// each accumulating its own assigned views into a private buffer and
// merging into the shared result once per CHUNK (mutex-protected) rather
// than once per view -- serializing a handful of merges instead of
// hundreds keeps the lock from eating the parallel speedup.

namespace ndl
{
	namespace detail
	{
		// Ray-AABB-clip (the volume's own [0,extent-1] per-axis box, via
		// the standard slab method) plus the resulting evenly-spaced
		// sample-point plan -- shared verbatim by forward_project() and
		// back_project() so they walk IDENTICAL sample points along a
		// given ray, which is what makes them adjoints of each other (see
		// this file's own top comment).
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

		// Per-axis volume-space AA footprint half-width AT A GIVEN POINT --
		// "how far does a unit step in DETECTOR space reach in VOLUME
		// space, right here". The inverse of projectionJacobian() (matrix/
		// projection.h) evaluated at `point` (not fixed at the volume's own
		// center -- for a genuine perspective/cone-beam matrix this
		// footprint really does change with depth along a ray, sometimes
		// substantially: measured directly, on demo/ct_reconstruction_3d's
		// own geometry, the true half-width varies by a factor of ~1.5x
		// between the near-source and near-detector sides of the volume,
		// so a single value evaluated once at the center would be wrong
		// almost everywhere else -- over-filtering (losing real
		// resolution) on one side and under-filtering (leaving aliasing
		// uncorrected) on the other).
		// Recovered the same way ray_for_pixel() recovers a point from a
		// (D-1)-equation system: augment with a probe row to make it
		// square, then invert -- fast_inverse() (matrix/decomposition.h,
		// the same adjugate/cofactor inverse ray_for_pixel() itself uses,
		// for the same reason), not the general SVD-based inverse(), since
		// forward_project()/back_project() below call this at every ray
		// sample -- tens of millions of times over a real reconstruction.
		template<class Real, int D>
		std::array<double, D> sampleAAHalfWidth(const ProjectionMatrix<Real, D>& pm, const ProjectionCenter<Real, D>& center, const Real* point)
		{
			Real J[(D - 1) * D];
			projectionJacobian(pm, point, J);

			Matrix<Real, D> augmented;
			for (int r = 0; r < D - 1; r++)
				for (int c = 0; c < D; c++)
					augmented(r, c) = J[r * D + c];
			for (int c = 0; c < D; c++)
				augmented(D - 1, c) = center.atInfinity ? center.point[c] : (point[c] - center.point[c]);
			Matrix<Real, D> inv = fast_inverse(augmented);

			std::array<double, D> halfWidth{};
			for (int r = 0; r < D; r++)
			{
				double h = 0;
				for (int k = 0; k < D - 1; k++) h += std::abs((double)inv(r, k));
				halfWidth[r] = 0.5 * h;
			}
			return halfWidth;
		}

		template<std::size_t D>
		bool anyAboveHalf(const std::array<double, D>& halfWidth)
		{
			for (double h : halfWidth) if (h > 0.5) return true;
			return false;
		}
	}

	// camera_center(pm) is a pure function of pm alone (matrix/projection.h
	// -- it pads to a (D+1)x(D+1) matrix and runs a full SVD<Real,D+1>, not
	// cheap), so it's invariant across every call in an iterative
	// reconstruction loop that reuses the same `geometry` (Landweber/DART,
	// e.g. demo/ct_reconstruction*/*.cpp's own loops, 25-50+ iterations
	// each calling forward_project() then back_project()). Neither
	// function recomputes it internally more than once per view PER CALL,
	// but with the same geometry every iteration, that's still the same
	// handful of SVDs solved over and over across the whole loop.
	// compute_camera_centers() lets a caller solve them ONCE up front and
	// pass the result to forward_project()/back_project()'s own `centers`
	// overload below instead of their plain `geometry`-only one (which
	// still computes centers internally itself, for source compatibility
	// and one-shot callers where precomputing isn't worth the bother).
	/// Precomputes camera_center() for every view in `geometry` -- pass the result to forward_project()/back_project()'s own `centers` overload to avoid resolving the same camera center (an SVD, not cheap) on every iteration of a reconstruction loop that reuses the same geometry.
	/// @param geometry One ProjectionMatrix<Real,D> per view.
	/// @return One ProjectionCenter<Real,D> per view, same order as `geometry`.
	/// @ingroup projection
	template<class Real, int D>
	std::vector<ProjectionCenter<Real, D>> compute_camera_centers(const std::vector<ProjectionMatrix<Real, D>>& geometry)
	{
		std::vector<ProjectionCenter<Real, D>> centers;
		centers.reserve(geometry.size());
		for (const auto& pm : geometry) centers.push_back(camera_center(pm));
		return centers;
	}

	/// Forward-projects `volume` into `sinogram` (overwritten) by ray-marching through the geometry described by `geometry` (one ProjectionMatrix per view -- matrix/projection.h). Parallelized over views via std::execution::par. See this file's own top comment for the exact algorithm and the anti-aliasing/adjointness details.
	/// @tparam VolumeImageT Any minimal-interface image type; its value_type must be arithmetic.
	/// @tparam SinogramImageT Any minimal-interface image type (D dimensions: 1 view axis + D-1 detector axes); its own extent's leading axis must equal geometry.size().
	/// @tparam Interpolator One of Nearest/Linear/Quadratic/Cubic (interpolation.h). Defaults to Linear.
	/// @param  volume       Source volume, D dimensions.
	/// @param  sinogram     Destination; must already exist with extent {geometry.size(), detector extent...}.
	/// @param  geometry     One ProjectionMatrix<Real,D> per view.
	/// @param  centers      One ProjectionCenter<Real,D> per view, matching `geometry` -- pass compute_camera_centers(geometry)'s own result, precomputed once outside a reconstruction loop that calls this every iteration with the same geometry.
	/// @param  interpolator Tag instance selecting the interpolation kernel; the type parameter is what actually matters.
	/// @param  autoAA       Whether to automatically anti-alias the volume when its resolution would alias against the detector's. Defaults to true.
	/// @param  stepSize     Ray-marching step size, in volume-grid units. Defaults to 1.0 (one step per voxel spacing).
	/// @ingroup projection
	template<class VolumeImageT, class SinogramImageT, class Real, int D, class Interpolator = Linear>
	void forward_project(const VolumeImageT& volume, SinogramImageT& sinogram, const std::vector<ProjectionMatrix<Real, D>>& geometry, const std::vector<ProjectionCenter<Real, D>>& centers, Interpolator interpolator = Interpolator{}, bool autoAA = true, double stepSize = 1.0)
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
		assert(centers.size() == geometry.size());

		std::array<int, D - 1> detExtent;
		for (int i = 0; i < D - 1; i++) detExtent[i] = sinoExtent[i + 1];
		std::size_t numDetPixels = 1;
		for (int i = 0; i < D - 1; i++) numDetPixels *= (std::size_t)detExtent[i];

		for (const auto& c : sinogram.coordinates()) sinogram.at(c) = SinoT(0);

		// The volume's own summed-area table doesn't depend on the view at
		// all (only the per-view query half-width does), so it's built
		// ONCE here -- shared, read-only, safe for every view's parallel
		// task to query concurrently -- rather than once per view, which
		// would be pure duplicated work.
		std::vector<double> tableData;
		std::optional<Image<double, D>> table;
		if (autoAA)
		{
			tableData.resize((std::size_t)Image<double, D>::size(volExtent));
			table.emplace(tableData.data(), volExtent);
			summed_area_table(volume, *table);
		}

		std::vector<int> viewIndices(geometry.size());
		std::iota(viewIndices.begin(), viewIndices.end(), 0);

		// Embarrassingly parallel: each view writes only its own disjoint
		// sinogram rows (view is literally sinogram's own leading axis),
		// so no synchronization is needed between parallel tasks --
		// unlike back_project() below, which has to merge every view's
		// contribution into the same shared volume.
		std::for_each(std::execution::par, viewIndices.begin(), viewIndices.end(), [&](int view)
		{
			const auto& pm = geometry[view];
			const auto& center = centers[view];

			std::array<int, D - 1> detCoord{};
			for (std::size_t idx = 0; idx < numDetPixels; idx++)
			{
				std::array<Real, D - 1> detCoordReal;
				for (int i = 0; i < D - 1; i++) detCoordReal[i] = Real(detCoord[i]);
				auto ray = ray_for_pixel(pm, center, detCoordReal);
				auto plan = detail::planRaySamples<Real, D>(ray, volExtent, stepSize);

				if (plan.valid)
				{
					double rayAccum = 0;
					for (int s = 0; s <= plan.numSteps; s++)
					{
						double t = plan.tMin + s * plan.ds;
						std::array<double, D> pos;
						for (int i = 0; i < D; i++) pos[i] = (double)ray.origin[i] + t * (double)ray.direction[i];

						double val;
						if (autoAA && table)
						{
							Real posReal[D];
							for (int i = 0; i < D; i++) posReal[i] = (Real)pos[i];
							auto halfWidth = detail::sampleAAHalfWidth(pm, center, posReal);
							val = detail::anyAboveHalf(halfWidth) ? box_filter_query(*table, pos, halfWidth) : sample(volume, pos, interpolator, BorderMode::Clamp);
						}
						else val = sample(volume, pos, interpolator, BorderMode::Clamp);

						double w = (s == 0 || s == plan.numSteps) ? 0.5 : 1.0;
						rayAccum += w * val;
					}
					rayAccum *= plan.ds;

					std::array<int, D> fullCoord;
					fullCoord[0] = view;
					for (int i = 0; i < D - 1; i++) fullCoord[i + 1] = detCoord[i];
					sinogram.at(fullCoord) = static_cast<SinoT>(rayAccum);
				}

				for (int d = 0; d < D - 1; d++) { if (++detCoord[d] < detExtent[d]) break; detCoord[d] = 0; }
			}
		});
	}

	/// forward_project(), computing compute_camera_centers(geometry) internally -- see the `centers`-taking overload above for the full contract. Convenient for a one-shot call; precompute centers yourself (once) instead if calling this repeatedly with the same geometry (e.g. an iterative reconstruction loop).
	/// @ingroup projection
	template<class VolumeImageT, class SinogramImageT, class Real, int D, class Interpolator = Linear>
	void forward_project(const VolumeImageT& volume, SinogramImageT& sinogram, const std::vector<ProjectionMatrix<Real, D>>& geometry, Interpolator interpolator = Interpolator{}, bool autoAA = true, double stepSize = 1.0)
	{
		forward_project(volume, sinogram, geometry, compute_camera_centers(geometry), interpolator, autoAA, stepSize);
	}

	/// Back-projects `sinogram` into `volume` (overwritten): forward_project()'s adjoint by construction, including its automatic anti-aliasing (see this file's own top comment for the matched-filter derivation -- verified to machine precision even at the volume's own boundary, not just approximately). Parallelized over view chunks via std::execution::par.
	/// @tparam SinogramImageT Any minimal-interface image type (D dimensions: 1 view axis + D-1 detector axes).
	/// @tparam VolumeImageT Any minimal-interface image type; its value_type must be arithmetic.
	/// @tparam Interpolator One of Nearest/Linear/Quadratic/Cubic (interpolation.h). Defaults to Linear -- MUST match the Interpolator forward_project() was called with for the two to actually be adjoints of each other.
	/// @param  sinogram     Source sinogram.
	/// @param  volume       Destination; must already exist with the target volume extent (D dimensions).
	/// @param  geometry     One ProjectionMatrix<Real,D> per view, matching forward_project()'s own.
	/// @param  centers      One ProjectionCenter<Real,D> per view, matching `geometry` -- see forward_project()'s own `centers` parameter comment.
	/// @param  interpolator Tag instance selecting the interpolation kernel; the type parameter is what actually matters.
	/// @param  autoAA       Whether to apply box_filter_scatter_add() in place of scatter_add() at each ray sample, exactly where forward_project() applied box_filter_query() in place of sample() -- MUST match forward_project()'s own autoAA for the two to be adjoints of each other. Defaults to true.
	/// @param  stepSize     Ray-marching step size, in volume-grid units -- MUST match forward_project()'s own for adjointness. Defaults to 1.0.
	/// @ingroup projection
	template<class SinogramImageT, class VolumeImageT, class Real, int D, class Interpolator = Linear>
	void back_project(const SinogramImageT& sinogram, VolumeImageT& volume, const std::vector<ProjectionMatrix<Real, D>>& geometry, const std::vector<ProjectionCenter<Real, D>>& centers, Interpolator interpolator = Interpolator{}, bool autoAA = true, double stepSize = 1.0)
	{
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
		assert(centers.size() == geometry.size());

		std::array<int, D - 1> detExtent;
		for (int i = 0; i < D - 1; i++) detExtent[i] = sinoExtent[i + 1];
		std::size_t numDetPixels = 1;
		for (int i = 0; i < D - 1; i++) numDetPixels *= (std::size_t)detExtent[i];
		std::size_t numVoxels = (std::size_t)Image<double, D>::size(volExtent);

		// Accumulated in double regardless of VolT (the same reasoning
		// convolve()/box_blur() apply to their own accumulation) -- many
		// overlapping ray contributions land in the same voxel, and a
		// narrow VolT would compound rounding error across them.
		std::vector<double> accumData(numVoxels, 0.0);
		Image<double, D> accum(accumData.data(), volExtent);

		// Views are chunked across roughly one chunk per hardware thread
		// (never more chunks than views) rather than parallelized one
		// task per view directly: every view's own contribution has to be
		// merged into the SAME shared `accum`, and doing that merge once
		// per CHUNK instead of once per view means a handful of
		// mutex-protected merges (~core count) rather than hundreds --
		// the serialized part of the work stays a small fraction of the
		// total, so the lock doesn't eat the parallel speedup the way
		// merging after every single view would.
		int numViews = (int)geometry.size();
		int numChunks = std::max(1, std::min(numViews, (int)std::thread::hardware_concurrency()));
		std::vector<int> chunkIndices(numChunks);
		std::iota(chunkIndices.begin(), chunkIndices.end(), 0);

		std::mutex accumMutex;

		std::for_each(std::execution::par, chunkIndices.begin(), chunkIndices.end(), [&](int chunk)
		{
			int viewsPerChunk = (numViews + numChunks - 1) / numChunks;
			int viewStart = chunk * viewsPerChunk;
			int viewEnd = std::min(numViews, viewStart + viewsPerChunk);
			if (viewStart >= viewEnd) return;

			std::vector<double> chunkAccum(numVoxels, 0.0);

			// Reused across every view this chunk processes (zeroed at the
			// start of each view below) rather than freshly allocated each
			// time -- the same "thread_local scratch, not a per-call
			// allocation" idea fft.h's own std::execution::par usage
			// relies on, though this one is a plain chunk-local (not
			// thread_local) buffer since each chunk already owns its own
			// sequential slice of work. TWO buffers, not one: since
			// filtering is decided per SAMPLE (matching forward_project()'s
			// own per-sample choice -- see this file's top comment for why
			// a per-sample decision is what a true perspective matrix
			// needs), a single view can have some samples going through
			// scatter_add() (writing real values directly) and others
			// through box_filter_scatter_add() (writing into a delta array
			// that only means something AFTER a summed_area_table() pass);
			// mixing both into one buffer would have the materialization
			// pass corrupt the direct contributions by treating them as
			// deltas too.
			std::vector<double> viewDirect(numVoxels);
			std::vector<double> viewDelta(numVoxels);

			for (int view = viewStart; view < viewEnd; view++)
			{
				const auto& pm = geometry[view];
				const auto& center = centers[view];

				std::fill(viewDirect.begin(), viewDirect.end(), 0.0);
				std::fill(viewDelta.begin(), viewDelta.end(), 0.0);
				Image<double, D> viewDirectImg(viewDirect.data(), volExtent);
				Image<double, D> viewDeltaImg(viewDelta.data(), volExtent);
				bool anyFilteringInView = false;

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
								double contribution = w * plan.ds * sinoVal;
								// The exact adjoint of whichever read
								// forward_project() used at THIS SAME sample
								// point (same position, same geometry, same
								// per-sample AA decision -- sampleAAHalfWidth()
								// is a pure function of (pm, center, position),
								// so evaluating it here reproduces exactly what
								// forward_project() would have used there):
								// box_filter_scatter_add() (into the delta
								// array, materialized below) is the adjoint of
								// box_filter_query(), the same way scatter_add()
								// is the adjoint of sample().
								if (autoAA)
								{
									Real posReal[D];
									for (int i = 0; i < D; i++) posReal[i] = (Real)pos[i];
									auto halfWidth = detail::sampleAAHalfWidth(pm, center, posReal);
									if (detail::anyAboveHalf(halfWidth)) { box_filter_scatter_add(viewDeltaImg, pos, halfWidth, contribution); anyFilteringInView = true; }
									else scatter_add(viewDirectImg, pos, contribution, interpolator, BorderMode::Clamp);
								}
								else scatter_add(viewDirectImg, pos, contribution, interpolator, BorderMode::Clamp);
							}
						}
					}

					for (int d = 0; d < D - 1; d++) { if (++detCoord[d] < detExtent[d]) break; detCoord[d] = 0; }
				}

				// box_filter_scatter_add() above only ever wrote a DELTA
				// array (see summed_area_table.h's own comment on it) --
				// this single pass materializes it into this view's actual
				// per-voxel contribution, in place. Skipped entirely when
				// no sample in this view went through the filtered path.
				if (anyFilteringInView) summed_area_table(viewDeltaImg, viewDeltaImg);

				for (std::size_t i = 0; i < numVoxels; i++) chunkAccum[i] += viewDirect[i] + viewDelta[i];
			}

			std::lock_guard<std::mutex> lock(accumMutex);
			for (std::size_t i = 0; i < numVoxels; i++) accumData[i] += chunkAccum[i];
		});

		for (const auto& c : volume.coordinates()) volume.at(c) = static_cast<VolT>(accum.at(c));
	}

	/// back_project(), computing compute_camera_centers(geometry) internally -- see the `centers`-taking overload above for the full contract. Convenient for a one-shot call; precompute centers yourself (once) instead if calling this repeatedly with the same geometry (e.g. an iterative reconstruction loop).
	/// @ingroup projection
	template<class SinogramImageT, class VolumeImageT, class Real, int D, class Interpolator = Linear>
	void back_project(const SinogramImageT& sinogram, VolumeImageT& volume, const std::vector<ProjectionMatrix<Real, D>>& geometry, Interpolator interpolator = Interpolator{}, bool autoAA = true, double stepSize = 1.0)
	{
		back_project(sinogram, volume, geometry, compute_camera_centers(geometry), interpolator, autoAA, stepSize);
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
