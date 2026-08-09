#pragma once
#include <array>
#include "core.h"

namespace ndl
{
	// Structuring-element / kernel shapes for convolve()/erode()/dilate()/
	// median_filter()/percentile_filter() -- all five read a kernel the same
	// way (nonzero tap = included, and convolve() additionally uses the
	// nonzero value as a weight), so the same kernel object works with any
	// of them. `kernel` must already exist (caller-owned, as always in this
	// library); its own extent picks the radius, e.g. a {5,5} kernel is a
	// radius-2 box or cross.
	//
	// make_box_kernel(): every tap set to 1 -- the full rectangular
	// neighborhood, no shape restriction beyond the kernel's own extent.
	/// Fills `kernel` with a full rectangular (box) structuring element -- every tap set to 1.
	/// @tparam T   Kernel element type.
	/// @tparam DIM Number of dimensions.
	/// @param  kernel Kernel to fill in place; its own extent picks the radius (e.g. a {5,5} kernel is a radius-2 box).
	/// @ingroup morphology_filtering
	template<class T, int DIM>
	void make_box_kernel(Image<T, DIM>& kernel) { kernel = T(1); }

	// make_cross_kernel(): only the center and the taps that vary along
	// EXACTLY ONE axis (every other axis pinned to center) are set to 1,
	// everything else to 0 -- a plus sign in 2D, and its N-dimensional
	// generalization in general: a "jack" with one pair of arms per
	// dimension (6 arms in 3D, along +/-x, +/-y, +/-z). Diagonal-adjacent
	// taps a box of the same radius would include (e.g. the 4 corners of a
	// 3x3 box) are deliberately left out -- this is 4-connectivity (2*DIM in
	// general) rather than a box's 8-connectivity (3^DIM - 1 in general), the
	// same distinction that matters for connected-component labeling or
	// thinning. Also cheaper than a box of the same radius: DIM*(2r+1)-(DIM-1)
	// taps instead of (2r+1)^DIM. Note this is a thin cross, not a filled
	// diamond (L1 ball) -- dilating a single point with it reproduces the
	// cross shape exactly (see demo/morphology Part 2), not a solid region.
	/// Fills `kernel` with a plus-sign (4-connected) structuring element: center and one arm per axis.
	/// @tparam T      Kernel element type.
	/// @tparam DIM    Number of dimensions.
	/// @param  kernel Kernel to fill in place; its own extent picks the arm length per axis.
	/// @ingroup morphology_filtering
	template<class T, int DIM>
	void make_cross_kernel(Image<T, DIM>& kernel)
	{
		kernel = T(0);
		std::array<int, DIM> center;
		for (int i = 0; i < DIM; i++) center[i] = kernel.extent()[i] / 2;
		kernel.at(center) = T(1);
		for (int axis = 0; axis < DIM; axis++)
		{
			std::array<int, DIM> coord = center;
			for (int i = 0; i < kernel.extent()[axis]; i++)
			{
				coord[axis] = i;
				kernel.at(coord) = T(1);
			}
		}
	}

	// Applies `fn` independently to each index along `channelAxis`, via
	// slice()'d views into src/dst -- the shared shape behind every "run
	// this per color channel, so channels don't bleed into each other"
	// helper (blur, convolve, erode, ...): `fn` receives
	// (srcChannel, dstChannel), both Image<T,DIM-1>, and is expected to
	// write dstChannel however it likes (e.g. `s.gaussian_blur(2.0, d,
	// BorderMode::Clamp)`). Since slice() shares memory rather than copying,
	// this costs nothing beyond the operation `fn` itself performs.
	/// Applies `fn(srcChannel, dstChannel)` independently to each index along `channelAxis` (e.g. running a filter per color channel).
	/// @tparam T           Element type.
	/// @tparam DIM         Number of dimensions.
	/// @tparam Fn          Callable as `fn(srcChannel, dstChannel)`, both `Image<T,DIM-1>`; expected to write dstChannel.
	/// @param  src         Source image.
	/// @param  dst         Destination image; must already exist with `src`'s own extent.
	/// @param  channelAxis Dimension to iterate over, calling `fn` once per index with a slice()'d (DIM-1)-dimensional view.
	/// @param  fn          Per-channel operation, e.g. `[](auto s, auto d){ s.gaussian_blur(2.0, d, BorderMode::Clamp); }`.
	/// @ingroup morphology_filtering
	template<class T, int DIM, class Fn>
	void per_channel(const Image<T, DIM>& src, Image<T, DIM>& dst, int channelAxis, Fn&& fn)
	{
		for (int c = 0; c < src.extent()[channelAxis]; c++)
		{
			Image<T, DIM - 1> srcChannel = src.slice(channelAxis, c);
			Image<T, DIM - 1> dstChannel = dst.slice(channelAxis, c);
			fn(srcChannel, dstChannel);
		}
	}
}
