#pragma once
#include <cassert>
#include <array>
#include <cstddef>
#include <vector>
#include <type_traits>
#include "image.h"

// The summed-area-table toolkit: summed_area_table()/rectangle_sum(), as
// free functions over any minimal-interface image type. A sibling of
// fft.h/matrix.h/convolution.h/morphology.h/histogram.h/
// distance_transform.h, not part of image.h's core Image object --
// #include this directly if you use it.

namespace ndl
{
	namespace detail
	{
		// Enumerate one coordinate per 1D fiber along `axis` (axis' own
		// index fixed at 0, every other axis walked once) -- the same
		// "every fiber along an axis" idea fft.h's fiberOrigins() and
		// distance_transform.h's distanceTransformFiberOrigins() each
		// provide, reimplemented locally (under yet another distinct name)
		// so this file stays independent of both.
		template<int DIM>
		std::vector<std::array<int, DIM>> summedAreaTableFiberOrigins(const std::array<int, DIM>& extent, int axis)
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

	// Builds a summed-area table (a.k.a. integral image): dst(coord) = the
	// sum of every src value in the hyper-rectangle from the origin to
	// coord, inclusive on both ends. Once built, rectangle_sum() below
	// answers "what's the sum over this arbitrary rectangle" in O(1) --
	// 2^DIM lookups into dst, regardless of the rectangle's size -- instead
	// of the O(rectangle size) a direct sum would cost every time, the same
	// "pay a one-time O(image size) setup cost, then answer queries cheaply"
	// trade fftn() makes for repeated convolutions at different kernels.
	//
	// Computed via one in-place prefix-sum pass per axis: seed dst with
	// src's own values (widened to DstT), then for each axis in turn, walk
	// every 1D fiber along it accumulating a running sum -- the same "one
	// 1D pass per axis" structure fftn() (fft.h) and
	// distance_transform_squared() (distance_transform.h) both use, just
	// with a running sum as the 1D primitive instead of a transform or a
	// lower envelope. src and dst are independently typed on purpose (not
	// required to be the same concrete type, unlike convolve()/erode()/
	// etc.): the whole point of a summed-area table is usually to widen the
	// accumulator (e.g. src is Image<uint8_t,DIM>, dst is
	// OwnedImage<double,DIM> or OwnedImage<std::int64_t,DIM>) so summing a
	// large region doesn't overflow src's own element type.
	/// Builds a summed-area table: dst(coord) = sum of every src value from the origin to coord, inclusive.
	/// @tparam SrcImageT Any minimal-interface image type; its value_type must be arithmetic.
	/// @tparam DstImageT Any minimal-interface image type (same DIM as SrcImageT); its value_type must be arithmetic and wide enough to hold the total sum without overflow.
	/// @param  src       Source image.
	/// @param  dst       Destination; must already exist with `src`'s own extent.
	/// @ingroup summed_area_table
	template<class SrcImageT, class DstImageT>
	void summed_area_table(const SrcImageT& src, DstImageT& dst)
	{
		using SrcT = typename SrcImageT::value_type;
		using DstT = typename DstImageT::value_type;
		static_assert(std::is_arithmetic_v<SrcT>, "ndl::summed_area_table() requires an arithmetic source value_type -- not valid for e.g. std::complex<T>");
		static_assert(std::is_arithmetic_v<DstT>, "ndl::summed_area_table() requires an arithmetic destination value_type -- not valid for e.g. std::complex<T>");
		assert(dst.extent() == src.extent());

		auto extent = src.extent();
		constexpr int DIM = std::tuple_size<decltype(extent)>::value;

		for (const auto& coord : src.coordinates())
			dst.at(coord) = static_cast<DstT>(src.at(coord));

		for (int axis = 0; axis < DIM; axis++)
		{
			int n = extent[axis];
			for (const auto& origin : detail::summedAreaTableFiberOrigins<DIM>(extent, axis))
			{
				std::array<int, DIM> coord = origin;
				DstT running = DstT(0);
				for (int i = 0; i < n; i++)
				{
					coord[axis] = i;
					running = static_cast<DstT>(running + dst.at(coord));
					dst.at(coord) = running;
				}
			}
		}
	}

	// Sum of every original src value in the hyper-rectangle [corner0,
	// corner1], inclusive on both ends (same start/end-inclusive convention
	// Image::view() uses) -- O(1) regardless of the rectangle's size, via
	// inclusion-exclusion over the table's own 2^DIM corners, generalizing
	// the classic 2D formula sum(x0..x1,y0..y1) = T(x1,y1) - T(x0-1,y1) -
	// T(x1,y0-1) + T(x0-1,y0-1) (with T(-1,*) = T(*,-1) = 0 by convention)
	// to any DIM: every one of the 2^DIM corners independently picks
	// either corner1[i] or corner0[i]-1 per axis, with a sign of -1 raised
	// to the number of "low" (corner0[i]-1) picks, and any corner with a
	// negative coordinate on some axis contributes 0 (standing in for the
	// zero-valued border the 2D formula's T(-1,*) convention represents).
	/// Sum of every original value in [corner0,corner1], inclusive on both ends. O(1) regardless of rectangle size, given a table already built by summed_area_table().
	/// @tparam TableImageT Any minimal-interface image type; its value_type must be signed (needs subtraction) -- floating-point or a signed integer accumulator.
	/// @tparam DIM         Deduced from corner0/corner1 (std::array<int,DIM>'s own size type, std::size_t -- not int, since that's what std::array<T,N>'s N actually is); table.at() itself enforces that this actually matches TableImageT's own dimensionality.
	/// @param  table       A summed-area table, as built by summed_area_table() above.
	/// @param  corner0     One corner of the query rectangle, inclusive.
	/// @param  corner1     The opposite corner, inclusive.
	/// @ingroup summed_area_table
	template<class TableImageT, std::size_t DIM>
	typename TableImageT::value_type rectangle_sum(const TableImageT& table, std::array<int, DIM> corner0, std::array<int, DIM> corner1)
	{
		using T = typename TableImageT::value_type;
		static_assert(std::is_signed_v<T>, "ndl::rectangle_sum() requires a signed value_type (needs subtraction) -- floating-point or a signed integer accumulator");

		T total = T(0);
		int numCorners = 1 << DIM;
		for (int mask = 0; mask < numCorners; mask++)
		{
			std::array<int, DIM> coord;
			int sign = 1;
			bool outOfBounds = false;
			for (std::size_t i = 0; i < DIM; i++)
			{
				bool pickLow = ((mask >> i) & 1) != 0;
				coord[i] = pickLow ? (corner0[i] - 1) : corner1[i];
				if (pickLow) sign = -sign;
				if (coord[i] < 0) { outOfBounds = true; break; }
			}
			if (outOfBounds) continue; // stands in for the table's implicit zero-valued border at index -1
			total = static_cast<T>(total + sign * table.at(coord));
		}
		return total;
	}

	// Averages every position over a (2*radius+1)^DIM window in O(1) per
	// pixel via rectangle_sum() -- unlike convolve()'s O(radius^DIM), the
	// per-pixel cost here doesn't grow with radius at all. The window a
	// border pixel needs reaches outside src, exactly the situation
	// convolve()/erode()/etc. resolve via detail::kernelTapCoord() and a
	// BorderMode; reused here the same way, but once per PADDED PIXEL rather
	// than once per (output pixel, kernel tap) pair like those do, since a
	// summed-area table can only answer queries against positions that
	// actually exist in it. Building one border-extended copy of src (sized
	// extent+2*radius per axis, each padding position resolved via the same
	// border-handling convolve()/erode() share) up front means every
	// downstream window query is then a ordinary, always-in-bounds
	// rectangle_sum() -- Clamp/Wrap/Reflect all cost exactly the same to
	// support this way, since they only ever affect how the (one-time) pad
	// is filled, not the (per-pixel) query itself.
	/// Averages every position over a (2*radius+1)^DIM window via one rectangle_sum() query per pixel -- O(1) per pixel regardless of radius, unlike convolve()'s O(radius^DIM). Requires an arithmetic, non-bool value_type.
	/// @tparam ImageT Any minimal-interface image type (Image<T,DIM>, ...) whose value_type is arithmetic and not bool; src and dst must be the same concrete type.
	/// @param  src    Source image.
	/// @param  dst    Destination; must already exist with `src`'s own extent.
	/// @param  radius Window half-width along every axis (window size 2*radius+1).
	/// @param  border How an out-of-bounds window position is resolved. Defaults to BorderMode::Clamp.
	/// @ingroup summed_area_table
	template<class ImageT>
	void box_blur(const ImageT& src, ImageT& dst, int radius, BorderMode border = BorderMode::Clamp)
	{
		using T = typename ImageT::value_type;
		static_assert(std::is_arithmetic_v<T>, "ndl::box_blur() requires an arithmetic value_type -- not valid for e.g. std::complex<T>");
		static_assert(!std::is_same_v<T, bool>, "ndl::box_blur() doesn't work on bool -- std::vector<bool> is bit-packed and has no .data() for the padded working copy this needs. Blurring a binary image doesn't have an obvious meaning anyway; convert to a wider type first if you need this.");
		assert(radius > 0);
		assert(dst.extent() == src.extent());

		auto extent = src.extent();
		constexpr int DIM = std::tuple_size<decltype(extent)>::value;

		std::array<int, DIM> paddedExtent;
		for (int i = 0; i < DIM; i++) paddedExtent[i] = extent[i] + 2 * radius;

		std::array<int, DIM> center;
		center.fill(radius);
		std::array<int, DIM> origin{};

		std::vector<T> paddedData(Image<T, DIM>::size(paddedExtent));
		Image<T, DIM> padded(paddedData.data(), paddedExtent);
		for (const auto& pCoord : padded.coordinates())
			padded.at(pCoord) = src.at(detail::kernelTapCoord(origin, pCoord, center, extent, border));

		std::vector<double> tableData(Image<double, DIM>::size(paddedExtent));
		Image<double, DIM> table(tableData.data(), paddedExtent);
		summed_area_table(padded, table);

		double area = 1.0;
		for (int i = 0; i < DIM; i++) area *= (2 * radius + 1);

		for (const auto& coord : src.coordinates())
		{
			std::array<int, DIM> hi;
			for (int i = 0; i < DIM; i++) hi[i] = coord[i] + 2 * radius;
			double sum = rectangle_sum(table, coord, hi);
			dst.at(coord) = static_cast<T>(sum / area);
		}
	}
}
