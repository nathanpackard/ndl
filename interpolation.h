#pragma once
#include <cmath>
#include <array>
#include <vector>
#include <cstddef>
#include <type_traits>
#include "image/border_mode.h"
#include "image/detail.h"
#include "image.h"

// The interpolation toolkit: Nearest/Linear/Quadratic/Cubic interpolator
// tags plus sample() -- a free function over any minimal-interface image
// type, the same relationship convolution.h/morphology.h have to Image. A
// sibling of those, not part of image.h's core object -- #include this
// directly if you use it.
//
// Every interpolator here is SEPARABLE: sampling at a DIM-dimensional
// fractional position is the product of DIM independent 1D weight
// profiles, one per axis, the same separability convolve()'s own kernel
// walk relies on. Widths are fixed per interpolator (1/2/3/4 taps along
// each axis for Nearest/Linear/Quadratic/Cubic respectively), and border
// handling for a tap that lands outside the image reuses
// detail::kernelTapCoord() -- the exact same helper convolve()/box_blur()
// already use -- rather than re-deriving Clamp/Wrap/Reflect a third time.
//
// Quadratic and Cubic are uniform B-SPLINE bases (order 2 and 3), not
// Catmull-Rom/Lagrange: they don't pass exactly through the original
// sample values (a known, deliberate B-spline property -- they trade exact
// pass-through for a smoother, better-behaved frequency response), which
// matters for sampling.h/projection.h's use of these as anti-aliased
// resampling kernels. sample_image()/forward_project()/back_project() all
// default to Linear specifically -- simplest, and the one with a trivial,
// exactly-matched adjoint, which is what makes forward/back projection a
// literal adjoint pair rather than an approximate one. Quadratic/Cubic
// remain available for general resampling where a smoother kernel is
// worth the extra taps and adjointness isn't a concern.

namespace ndl
{
	/// Nearest-neighbor interpolation: a single tap, no blending. @ingroup interpolation
	struct Nearest
	{
		static constexpr int width = 1;
		static void taps(double pos, int& base, double* weights)
		{
			base = (int)std::lround(pos);
			weights[0] = 1.0;
		}
	};

	/// N-linear interpolation (bilinear/trilinear/... depending on DIM): 2 taps per axis. @ingroup interpolation
	struct Linear
	{
		static constexpr int width = 2;
		static void taps(double pos, int& base, double* weights)
		{
			base = (int)std::floor(pos);
			double t = pos - base;
			weights[0] = 1.0 - t;
			weights[1] = t;
		}
	};

	/// Quadratic interpolation (uniform quadratic B-spline, order 2): 3 taps per axis. @ingroup interpolation
	struct Quadratic
	{
		static constexpr int width = 3;
		static void taps(double pos, int& base, double* weights)
		{
			int center = (int)std::lround(pos);
			base = center - 1;
			double t = pos - center; // in [-0.5, 0.5)
			weights[0] = 0.5 * (0.5 - t) * (0.5 - t);
			weights[1] = 0.75 - t * t;
			weights[2] = 0.5 * (0.5 + t) * (0.5 + t);
		}
	};

	/// Cubic interpolation (uniform cubic B-spline, order 3): 4 taps per axis. @ingroup interpolation
	struct Cubic
	{
		static constexpr int width = 4;
		static void taps(double pos, int& base, double* weights)
		{
			int floorPos = (int)std::floor(pos);
			base = floorPos - 1;
			double t = pos - floorPos; // in [0, 1)
			double t2 = t * t, t3 = t2 * t;
			weights[0] = (1.0 - t) * (1.0 - t) * (1.0 - t) / 6.0;
			weights[1] = (3.0 * t3 - 6.0 * t2 + 4.0) / 6.0;
			weights[2] = (-3.0 * t3 + 3.0 * t2 + 3.0 * t + 1.0) / 6.0;
			weights[3] = t3 / 6.0;
		}
	};

	namespace detail
	{
		// Every combination of per-axis tap index in [0, width) -- width^DIM
		// combinations total, walked via the same little-endian odometer
		// increment summed_area_table.h's own fiber-origin helper uses.
		// width is at most 4 and DIM is a handful at most, so materializing
		// the whole list up front is simpler than a lazy generator and not a
		// real cost.
		template<int DIM>
		std::vector<std::array<int, DIM>> interpolationTapCombinations(int width)
		{
			std::size_t count = 1;
			for (int d = 0; d < DIM; d++) count *= (std::size_t)width;

			std::vector<std::array<int, DIM>> combos;
			combos.reserve(count);
			std::array<int, DIM> idx{};
			for (std::size_t i = 0; i < count; i++)
			{
				combos.push_back(idx);
				for (int d = 0; d < DIM; d++)
				{
					if (++idx[d] < width) break;
					idx[d] = 0;
				}
			}
			return combos;
		}
	}

	namespace detail
	{
		// Shared setup for sample()/scatter_add() below: the per-axis tap
		// base index and 1D weights -- factored out so the two stay
		// exactly in sync (scatter_add() is meant to be the exact adjoint
		// of sample(), which only holds if they use literally the same
		// weight computation, not two independently-maintained copies of
		// it).
		template<class Interpolator, int IDIM>
		void interpolationWeights(const double* position, std::array<int, IDIM>& base, std::array<std::array<double, Interpolator::width>, IDIM>& weights1D)
		{
			constexpr int width = Interpolator::width;
			for (int axis = 0; axis < IDIM; axis++)
			{
				double w[width];
				Interpolator::taps(position[axis], base[axis], w);
				for (int k = 0; k < width; k++) weights1D[axis][k] = w[k];
			}
		}
	}

	// DIM is deduced from `position` alone (not also from `image`'s own
	// extent()) for the same reason eigen_decomposition()/
	// make_translate_matrix() (matrix/decomposition.h, matrix/transform.h)
	// take an independent size_t template parameter rather than reusing one
	// symbol for both an int-dimensioned and a size_t-dimensioned argument:
	// deduction requires consistent TYPES, not just consistent values, and
	// Image<T,DIM>'s own DIM is int while std::array's own size is
	// std::size_t. Reconciled below via a static_assert instead.
	/// Interpolated value of `image` at the fractional `position`, via `Interpolator` (defaults to Linear).
	/// @tparam ImageT Any minimal-interface image type (Image<T,DIM>, ...); its value_type must be arithmetic.
	/// @tparam DIM Deduced from `position`.
	/// @tparam Interpolator One of Nearest/Linear/Quadratic/Cubic (or a type with the same static `width`/`taps()` shape). Defaults to Linear.
	/// @param  image       Source image.
	/// @param  position    Fractional position, one coordinate per axis, in the same units as `image`'s own indices.
	/// @param  interpolator Tag instance selecting the interpolation kernel; the type parameter is what actually matters, this is just how it's spelled at the call site.
	/// @param  border      How a tap outside `image` is resolved. Defaults to BorderMode::Clamp.
	/// @return The interpolated value, as a double (accumulated in double regardless of ImageT's own value_type, the same convention convolve() uses).
	/// @ingroup interpolation
	template<class ImageT, std::size_t DIM, class Interpolator = Linear>
	double sample(const ImageT& image, const std::array<double, DIM>& position, Interpolator interpolator = Interpolator{}, BorderMode border = BorderMode::Clamp)
	{
		(void)interpolator;
		using T = typename ImageT::value_type;
		static_assert(std::is_arithmetic_v<T>, "ndl::sample() requires an arithmetic value_type -- not valid for e.g. std::complex<T>");

		auto extent = image.extent();
		constexpr int IDIM = std::tuple_size<decltype(extent)>::value;
		static_assert(IDIM == (int)DIM, "ndl::sample() requires position to have exactly image's own dimension");

		constexpr int width = Interpolator::width;
		std::array<int, IDIM> base;
		std::array<std::array<double, width>, IDIM> weights1D;
		detail::interpolationWeights<Interpolator, IDIM>(position.data(), base, weights1D);

		std::array<int, IDIM> zero{};
		double total = 0;
		for (const auto& tapIdx : detail::interpolationTapCombinations<IDIM>(width))
		{
			double weight = 1.0;
			for (int axis = 0; axis < IDIM; axis++) weight *= weights1D[axis][tapIdx[axis]];
			if (weight == 0.0) continue; // e.g. a Nearest tap or a B-spline tap right at its support boundary
			auto srcCoord = detail::kernelTapCoord(base, tapIdx, zero, extent, border);
			total += weight * static_cast<double>(image.at(srcCoord));
		}
		return total;
	}

	// The exact adjoint of sample(): rather than reading a weighted blend
	// of `image`'s taps, adds `value` times each of the same weights into
	// those same taps. Built on the identical detail::interpolationWeights()
	// call sample() itself uses, specifically so that for any Interpolator/
	// border, <sample(image,p), y> == <image, scatter_add(image',p,y)> --
	// the property projection.h's forward_project()/back_project() rely on
	// to be exact adjoints of each other (see that file's own top comment).
	/// Scatters `value` into `image` at the fractional `position`, weighted by `Interpolator` (defaults to Linear) -- the exact adjoint of sample().
	/// @tparam ImageT Any minimal-interface image type exposing a mutable at(); its value_type must be arithmetic.
	/// @tparam DIM Deduced from `position`.
	/// @tparam Interpolator One of Nearest/Linear/Quadratic/Cubic (or a type with the same static `width`/`taps()` shape). Defaults to Linear.
	/// @param  image       Destination; each affected tap is incremented (+=), not overwritten.
	/// @param  position    Fractional position, one coordinate per axis, in the same units as `image`'s own indices.
	/// @param  value       Value to scatter; distributed across taps by the same weights sample() would use to read them back.
	/// @param  interpolator Tag instance selecting the interpolation kernel; the type parameter is what actually matters.
	/// @param  border      How a tap outside `image` is resolved. Defaults to BorderMode::Clamp.
	/// @ingroup interpolation
	template<class ImageT, std::size_t DIM, class Interpolator = Linear>
	void scatter_add(ImageT& image, const std::array<double, DIM>& position, double value, Interpolator interpolator = Interpolator{}, BorderMode border = BorderMode::Clamp)
	{
		(void)interpolator;
		using T = typename ImageT::value_type;
		static_assert(std::is_arithmetic_v<T>, "ndl::scatter_add() requires an arithmetic value_type -- not valid for e.g. std::complex<T>");

		auto extent = image.extent();
		constexpr int IDIM = std::tuple_size<decltype(extent)>::value;
		static_assert(IDIM == (int)DIM, "ndl::scatter_add() requires position to have exactly image's own dimension");

		constexpr int width = Interpolator::width;
		std::array<int, IDIM> base;
		std::array<std::array<double, width>, IDIM> weights1D;
		detail::interpolationWeights<Interpolator, IDIM>(position.data(), base, weights1D);

		std::array<int, IDIM> zero{};
		for (const auto& tapIdx : detail::interpolationTapCombinations<IDIM>(width))
		{
			double weight = 1.0;
			for (int axis = 0; axis < IDIM; axis++) weight *= weights1D[axis][tapIdx[axis]];
			if (weight == 0.0) continue;
			auto dstCoord = detail::kernelTapCoord(base, tapIdx, zero, extent, border);
			image.at(dstCoord) = static_cast<T>(image.at(dstCoord) + weight * value);
		}
	}
}
