#pragma once
#include <cassert>
#include <cmath>
#include <vector>
#include <array>
#include <cstddef>
#include <type_traits>
#include "image.h"

// The distance transform toolkit: distance_transform_squared()/
// distance_transform(), as free functions over any minimal-interface image
// type. A sibling of fft.h/matrix.h/convolution.h/morphology.h/
// histogram.h, not part of image.h's core Image object -- #include this
// directly if you use it.

namespace ndl
{
	namespace detail
	{
		// A finite stand-in for "infinity" used to seed non-foreground
		// positions before the first axis pass below: comfortably larger
		// (by many orders of magnitude) than the squared distance across
		// any image actually small enough to fit in memory, but a real
		// finite double -- unlike literal
		// std::numeric_limits<double>::infinity(), whose whole problem is
		// that two "infinite" seed positions compared against each other
		// during the envelope construction below would subtract to
		// infinity-minus-infinity, i.e. NaN, silently corrupting every
		// result downstream of it. With a finite sentinel, two such
		// positions instead subtract to a real (large but finite, usually
		// exactly 0) number, which is what makes this algorithm actually
		// work when most of the image is non-foreground -- the common case.
		constexpr double kDistanceTransformInfinity = 1e18;

		// The 1D squared-distance lower envelope: given f[0..n), computes
		// d[x] = min over q of f[q] + (x-q)^2 for every x in [0,n) -- exact,
		// O(n) amortized (each candidate parabola is pushed onto and popped
		// off the envelope at most once, so the inner while loop's total
		// work across the whole outer loop is bounded by n). This is the
		// classic separable building block from Felzenszwalt & Huttenlocher,
		// "Distance Transforms of Sampled Functions" (Theory of Computing,
		// 2012): applying it once along each axis in turn (see
		// distance_transform_squared() below) gives the exact squared
		// Euclidean distance transform of an N-dimensional array -- the same
		// "one 1D pass per axis" structure fftn() (fft.h) uses for its own
		// N-dimensional transform, just with a different 1D primitive.
		inline void lowerEnvelope1DSquared(const std::vector<double>& f, std::vector<double>& d)
		{
			int n = (int)f.size();
			assert((int)d.size() == n);
			if (n == 0) return;

			std::vector<int> v(n);       // v[k]: index of the k-th parabola in the envelope, left to right
			std::vector<double> z(n + 1); // z[k]/z[k+1]: left/right boundary of the region where v[k] wins
			int k = 0;
			v[0] = 0;
			z[0] = -kDistanceTransformInfinity;
			z[1] = kDistanceTransformInfinity;

			for (int q = 1; q < n; q++)
			{
				double s;
				while (true)
				{
					double vk = (double)v[k];
					s = ((f[q] + (double)q * q) - (f[v[k]] + vk * vk)) / (2.0 * q - 2.0 * vk);
					// k>0 guards against reading v[-1]/z[-1]; see this
					// function's own comment on kDistanceTransformInfinity
					// for why this shouldn't actually trigger in practice
					// (s can't get more negative than roughly
					// -kDistanceTransformInfinity/2), kept anyway as a
					// hard backstop rather than relying on that margin.
					if (k > 0 && s <= z[k]) { k--; continue; }
					break;
				}
				k++;
				v[k] = q;
				z[k] = s;
				z[k + 1] = kDistanceTransformInfinity;
			}

			k = 0;
			for (int q = 0; q < n; q++)
			{
				while (z[k + 1] < q) k++;
				double dx = (double)q - (double)v[k];
				d[q] = dx * dx + f[v[k]];
			}
		}

		// Enumerate one coordinate per 1D fiber along `axis` (axis' own
		// index fixed at 0, every other axis walked once) -- the same
		// "every fiber along an axis" idea fft.h's own fiberOrigins()
		// helper provides, reimplemented locally (under a distinct name)
		// rather than shared across headers, so distance_transform.h has
		// no dependency on fft.h and the two can't collide if a
		// translation unit includes both.
		template<int DIM>
		std::vector<std::array<int, DIM>> distanceTransformFiberOrigins(const std::array<int, DIM>& extent, int axis)
		{
			std::size_t count = 1;
			for (int d = 0; d < DIM; d++) if (d != axis) count *= extent[d];

			std::vector<std::array<int, DIM>> origins;
			origins.reserve(count);

			std::array<int, DIM> coord{};
			coord[axis] = 0;
			for (std::size_t i = 0; i < count; i++)
			{
				origins.push_back(coord);
				for (int d = 0; d < DIM; d++)
				{
					if (d == axis) continue;
					if (++coord[d] < extent[d]) break;
					coord[d] = 0;
				}
			}
			return origins;
		}
	}

	// Exact squared Euclidean distance transform: dst(coord) = squared
	// distance from coord to the nearest position where src is
	// "background" (contextually false/zero) -- the same convention
	// OpenCV's own distanceTransform() uses (a foreground pixel gets its
	// distance to the nearest background pixel; a background pixel is
	// trivially 0). Want distance to the nearest FOREGROUND pixel instead?
	// Run this against an ndl::invert(src, ...)'d source (morphology.h) --
	// distance_transform.h doesn't need its own separate "which way" flag,
	// since inverting the source is already a one-line, well-tested
	// operation.
	//
	// Computed via one detail::lowerEnvelope1DSquared() pass per axis:
	// background positions seed f=0, foreground positions seed
	// f=kDistanceTransformInfinity; the first axis pass turns that into the
	// exact squared distance considering only movement along axis 0, and
	// each subsequent axis pass folds in another axis, so after all DIM
	// passes every position holds its true squared distance considering
	// movement along every axis at once. An image with no background pixel
	// at all has no finite answer (there's nothing to measure distance to);
	// such a position's result is exactly kDistanceTransformInfinity,
	// unmodified by any axis pass, rather than a crash or a misleadingly
	// small number.
	/// Exact squared Euclidean distance transform: dst(coord) = squared distance from coord to the nearest position where src is false/zero.
	/// @tparam SrcImageT Any minimal-interface image type whose value_type is contextually convertible to bool (nonzero/true = foreground).
	/// @tparam DstImageT Any minimal-interface image type (same DIM as SrcImageT) whose value_type is floating-point, to hold a squared distance.
	/// @param  src       Source image.
	/// @param  dst       Destination; must already exist with `src`'s own extent.
	/// @ingroup distance_transform
	template<class SrcImageT, class DstImageT>
	void distance_transform_squared(const SrcImageT& src, DstImageT& dst)
	{
		using SrcT = typename SrcImageT::value_type;
		using DstT = typename DstImageT::value_type;
		static_assert(std::is_arithmetic_v<SrcT>, "ndl::distance_transform_squared() requires a source value_type contextually convertible to bool -- not valid for e.g. std::complex<T>");
		static_assert(std::is_floating_point_v<DstT>, "ndl::distance_transform_squared() requires a floating-point destination value_type, to hold a squared distance");
		assert(dst.extent() == src.extent());

		auto extent = src.extent();
		constexpr int DIM = std::tuple_size<decltype(extent)>::value;

		for (const auto& coord : src.coordinates())
			dst.at(coord) = static_cast<DstT>((src.at(coord) != SrcT(0)) ? detail::kDistanceTransformInfinity : 0.0);

		std::vector<double> fiber, transformed;
		for (int axis = 0; axis < DIM; axis++)
		{
			int n = extent[axis];
			fiber.resize(n);
			transformed.resize(n);
			for (const auto& origin : detail::distanceTransformFiberOrigins<DIM>(extent, axis))
			{
				std::array<int, DIM> coord = origin;
				for (int i = 0; i < n; i++) { coord[axis] = i; fiber[i] = static_cast<double>(dst.at(coord)); }
				detail::lowerEnvelope1DSquared(fiber, transformed);
				for (int i = 0; i < n; i++) { coord[axis] = i; dst.at(coord) = static_cast<DstT>(transformed[i]); }
			}
		}
	}

	// True Euclidean distance transform: distance_transform_squared() (see
	// its own comment for the exact algorithm and the background/foreground
	// convention) followed by one elementwise sqrt pass -- computed
	// directly into dst, then square-rooted in place, so no separate
	// scratch buffer is needed for the unsquared result.
	/// Exact Euclidean distance transform: dst(coord) = distance from coord to the nearest position where src is false/zero.
	/// @tparam SrcImageT Any minimal-interface image type whose value_type is contextually convertible to bool (nonzero/true = foreground).
	/// @tparam DstImageT Any minimal-interface image type (same DIM as SrcImageT) whose value_type is floating-point, to hold a distance.
	/// @param  src       Source image.
	/// @param  dst       Destination; must already exist with `src`'s own extent.
	/// @ingroup distance_transform
	template<class SrcImageT, class DstImageT>
	void distance_transform(const SrcImageT& src, DstImageT& dst)
	{
		distance_transform_squared(src, dst);
		for (const auto& coord : dst.coordinates())
			dst.at(coord) = static_cast<typename DstImageT::value_type>(std::sqrt(static_cast<double>(dst.at(coord))));
	}
}
