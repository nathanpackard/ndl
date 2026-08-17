#pragma once
#include <cassert>
#include <cmath>
#include <vector>
#include <array>
#include <cstddef>
#include <type_traits>
#include "../image.h"

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
	}

	// Which side of the foreground/background boundary a distance transform
	// measures FROM -- ToBackground (the default, and the only option this
	// file offered before) gives every foreground pixel its distance to the
	// nearest background pixel, background pixels trivially 0 (OpenCV's own
	// distanceTransform() convention); ToForeground is the mirror image,
	// every background pixel's distance to the nearest foreground pixel,
	// foreground trivially 0. ToForeground used to require a separate
	// ndl::invert(src, ...)'d copy (morphology.h) passed back into the
	// ToBackground-only transform -- one line, but a whole extra image
	// allocated and swept just to flip a comparison. Instead this flips the
	// seed predicate directly (see distance_transform_squared() below),
	// which costs nothing extra and is also what distance_transform_signed()
	// (further below) needs internally anyway, computing both sides at once.
	/// @ingroup distance_transform
	enum class DistanceSide { ToBackground, ToForeground };

	// Exact squared Euclidean distance transform: dst(coord) = squared
	// distance from coord to the nearest position on the OTHER side of the
	// foreground/background boundary from coord itself -- see DistanceSide's
	// own comment for the exact convention each side means, and note the
	// asymmetry: a coordinate on side X always reads its distance to the
	// nearest position on the OPPOSITE side, is trivially 0 if coord is
	// already NOT on side... concretely: side==ToBackground means "distance
	// to background", so foreground coords get a real distance and
	// background coords read 0 (they already sit on the target side);
	// side==ToForeground is the mirror image.
	//
	// Computed via one detail::lowerEnvelope1DSquared() pass per axis:
	// positions already on the target side seed f=0, the rest seed
	// f=kDistanceTransformInfinity; the first axis pass turns that into the
	// exact squared distance considering only movement along axis 0, and
	// each subsequent axis pass folds in another axis, so after all DIM
	// passes every position holds its true squared distance considering
	// movement along every axis at once. An image with no position at all
	// on the target side has no finite answer (there's nothing to measure
	// distance to); such a position's result is exactly
	// kDistanceTransformInfinity, unmodified by any axis pass, rather than
	// a crash or a misleadingly small number.
	/// Exact squared Euclidean distance transform: dst(coord) = squared distance from coord to the nearest position on the opposite side of the foreground/background boundary (see DistanceSide).
	/// @tparam SrcImageT Any minimal-interface image type whose value_type is contextually convertible to bool (nonzero/true = foreground).
	/// @tparam DstImageT Any minimal-interface image type (same DIM as SrcImageT) whose value_type is floating-point, to hold a squared distance.
	/// @param  src       Source image.
	/// @param  dst       Destination; must already exist with `src`'s own extent.
	/// @param  side      Which side to measure distance TO -- ToBackground (default) or ToForeground; see DistanceSide's own comment.
	/// @ingroup distance_transform
	template<class SrcImageT, class DstImageT>
	void distance_transform_squared(const SrcImageT& src, DstImageT& dst, DistanceSide side = DistanceSide::ToBackground)
	{
		using SrcT = typename SrcImageT::value_type;
		using DstT = typename DstImageT::value_type;
		static_assert(std::is_arithmetic_v<SrcT>, "ndl::distance_transform_squared() requires a source value_type contextually convertible to bool -- not valid for e.g. std::complex<T>");
		static_assert(std::is_floating_point_v<DstT>, "ndl::distance_transform_squared() requires a floating-point destination value_type, to hold a squared distance");
		assert(dst.extent() == src.extent());

		auto extent = src.extent();
		constexpr int DIM = std::tuple_size<decltype(extent)>::value;

		bool seedIsForeground = (side == DistanceSide::ToForeground);
		for (const auto& coord : src.coordinates())
		{
			bool isForeground = (src.at(coord) != SrcT(0));
			dst.at(coord) = static_cast<DstT>((isForeground == seedIsForeground) ? 0.0 : detail::kDistanceTransformInfinity);
		}

		std::vector<double> fiber, transformed;
		for (int axis = 0; axis < DIM; axis++)
		{
			int n = extent[axis];
			fiber.resize(n);
			transformed.resize(n);
			for (const auto& origin : detail::fiberOrigins<DIM>(extent, axis))
			{
				std::array<int, DIM> coord = origin;
				for (int i = 0; i < n; i++) { coord[axis] = i; fiber[i] = static_cast<double>(dst.at(coord)); }
				detail::lowerEnvelope1DSquared(fiber, transformed);
				for (int i = 0; i < n; i++) { coord[axis] = i; dst.at(coord) = static_cast<DstT>(transformed[i]); }
			}
		}
	}

	// True Euclidean distance transform: distance_transform_squared() (see
	// its own comment for the exact algorithm and DistanceSide for the
	// direction convention) followed by one elementwise sqrt pass --
	// computed directly into dst, then square-rooted in place, so no
	// separate scratch buffer is needed for the unsquared result.
	/// Exact Euclidean distance transform: dst(coord) = distance from coord to the nearest position on the opposite side of the foreground/background boundary (see DistanceSide).
	/// @tparam SrcImageT Any minimal-interface image type whose value_type is contextually convertible to bool (nonzero/true = foreground).
	/// @tparam DstImageT Any minimal-interface image type (same DIM as SrcImageT) whose value_type is floating-point, to hold a distance.
	/// @param  src       Source image.
	/// @param  dst       Destination; must already exist with `src`'s own extent.
	/// @param  side      Which side to measure distance TO -- ToBackground (default) or ToForeground; see DistanceSide's own comment.
	/// @ingroup distance_transform
	template<class SrcImageT, class DstImageT>
	void distance_transform(const SrcImageT& src, DstImageT& dst, DistanceSide side = DistanceSide::ToBackground)
	{
		distance_transform_squared(src, dst, side);
		for (const auto& coord : dst.coordinates())
			dst.at(coord) = static_cast<typename DstImageT::value_type>(std::sqrt(static_cast<double>(dst.at(coord))));
	}

	// Signed ("bidirectional") distance transform: positive inside
	// foreground (its own distance to the nearest background pixel, same
	// magnitude distance_transform()'s default ToBackground mode already
	// gives), negative inside background (the negated distance to the
	// nearest foreground pixel) -- the classic signed-distance-field
	// convention (zero exactly at the boundary, magnitude growing either
	// direction away from it), useful wherever a single field needs to
	// describe "how far, and which side" at once, e.g. isosurface
	// extraction or a smooth interior/exterior falloff. Genuinely needs
	// BOTH directions computed (unlike ToForeground alone, this can't be
	// had by flipping one predicate), so this runs
	// distance_transform_squared() twice -- once per side, ToForeground
	// into a scratch buffer -- and combines with a sign based on which side
	// `src` itself is actually on at each coordinate.
	/// Signed Euclidean distance transform: positive inside foreground (distance to nearest background), negative inside background (distance to nearest foreground), zero at the boundary.
	/// @tparam SrcImageT Any minimal-interface image type whose value_type is contextually convertible to bool (nonzero/true = foreground).
	/// @tparam DstImageT Any minimal-interface image type (same DIM as SrcImageT) whose value_type is floating-point, to hold a signed distance.
	/// @param  src       Source image.
	/// @param  dst       Destination; must already exist with `src`'s own extent.
	/// @ingroup distance_transform
	template<class SrcImageT, class DstImageT>
	void distance_transform_signed(const SrcImageT& src, DstImageT& dst)
	{
		using SrcT = typename SrcImageT::value_type;
		using DstT = typename DstImageT::value_type;
		static_assert(std::is_floating_point_v<DstT>, "ndl::distance_transform_signed() requires a floating-point destination value_type, to hold a signed distance");
		assert(dst.extent() == src.extent());

		distance_transform_squared(src, dst, DistanceSide::ToBackground);

		auto extent = src.extent();
		constexpr int DIM = std::tuple_size<decltype(extent)>::value;
		OwnedImage<DstT, DIM> toForeground(extent);
		distance_transform_squared(src, toForeground, DistanceSide::ToForeground);

		for (const auto& coord : src.coordinates())
		{
			bool isForeground = (src.at(coord) != SrcT(0));
			double sq = isForeground ? static_cast<double>(dst.at(coord)) : static_cast<double>(toForeground.at(coord));
			double signedDist = std::sqrt(sq) * (isForeground ? 1.0 : -1.0);
			dst.at(coord) = static_cast<DstT>(signedDist);
		}
	}
}
