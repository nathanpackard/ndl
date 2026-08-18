#pragma once
#include <cassert>
#include <cmath>
#include <vector>
#include <array>
#include <limits>
#include <type_traits>
#include <algorithm>
#include <execution>
#include <numeric>
#include "../image/border_mode.h"
#include "../image/detail.h"
#include "../image.h"
#include "../mathHelpers.h"

// The convolution toolkit: convolve()/gaussian_blur(), as free functions
// over any minimal-interface image type. A sibling of fft.h/matrix.h/
// morphology.h, not part of image.h's core Image object -- #include this
// directly if you use these; #include <ndl/image.h> alone does not pull
// it in, the same way it doesn't pull in fft.h. (This file does include
// image.h itself, since gaussian_blur() below needs a real Image<double,DIM>
// to build its own kernel workspace -- but that's convolution.h depending
// on image.h, not the other way around.)

namespace ndl
{
	// Generic, minimal-interface convolution -- same contract as
	// morphology.h's erode()/dilate()/etc. (works on ANY type exposing
	// extent()/at(coord)/coordinates(), not just Image), and built from the
	// exact same detail::kernelCenter()/kernelIncludedTaps()/kernelTapCoord()
	// machinery those use, so a kernel walk means the same thing everywhere
	// in this library. src and dst must be the same concrete type; kernel
	// may be any (possibly different) minimal-interface type.
	//
	// Applies `kernel` to `src` via correlation (the kernel is not flipped --
	// the same convention OpenCV's filter2D uses, as opposed to signal
	// processing's flipped-kernel definition), writing a same-size result
	// into `dst`. Kernel indices are centered: extent K along a dimension
	// covers offsets -(K/2) .. K-(K/2)-1 from the output element being
	// computed, so an odd-sized kernel (e.g. 3x3) reaches an equal number of
	// neighbors in each direction. `border` selects how an offset that falls
	// outside `src` is handled.
	/// Correlates `kernel` against every position of `src`, writing the weighted sum into `dst`. Requires an arithmetic value_type.
	/// @tparam SrcImageT Any minimal-interface image type (Image<T,DIM>, PackedBitImage<DIM>, ...); its value_type must be arithmetic.
	/// @tparam DstImageT Any minimal-interface image type (same DIM as SrcImageT); may differ from SrcImageT (e.g. a different concrete container).
	/// @tparam KernelT Any minimal-interface image type for the kernel; may differ from SrcImageT/DstImageT.
	/// @param  src     Source image.
	/// @param  dst     Destination; must already exist with `src`'s own extent.
	/// @param  kernel  Weights, nonzero-tap = included; convolve() also uses the value as a weight. Its own extent sets the neighborhood radius per dimension.
	/// @param  border  How an out-of-bounds neighbor is resolved. Defaults to BorderMode::Clamp.
	/// @ingroup convolution
	template<class SrcImageT, class DstImageT, class KernelT>
	void convolve(const SrcImageT& src, DstImageT& dst, const KernelT& kernel, BorderMode border = BorderMode::Clamp)
	{
		using SrcT = typename SrcImageT::value_type;
		using DstT = typename DstImageT::value_type;
		static_assert(std::is_arithmetic_v<SrcT>, "ndl::convolve() requires a value_type convertible to double -- not valid for e.g. std::complex<T> (the weighted sum is accumulated in double)");
		static_assert(std::is_arithmetic_v<DstT>, "ndl::convolve() requires a value_type convertible to double -- not valid for e.g. std::complex<T> (the weighted sum is accumulated in double)");
		assert(dst.extent() == src.extent());
		auto extent = src.extent();
		auto center = detail::kernelCenter(kernel);
		auto taps = detail::kernelIncludedTaps(kernel); // zero-weight taps contribute nothing to the sum, so skipping them is free

		for (const auto& coord : src.coordinates())
		{
			double total = 0;
			for (const auto& kCoord : taps)
				total += static_cast<double>(src.at(detail::kernelTapCoord(coord, kCoord, center, extent, border))) * static_cast<double>(kernel.at(kCoord));
			dst.at(coord) = static_cast<DstT>(total);
		}
	}

	// Gaussian blur: a DIM-dimensional Gaussian factors exactly into DIM
	// independent 1D Gaussians (one per axis), so rather than convolving
	// with one full width^DIM kernel, this runs DIM passes of convolve(),
	// each with a kernel shaped {1,...,width,...,1} (width only along the
	// current axis) -- the same per-axis-restricted-kernel trick
	// gradient() below already relies on: convolve()'s own
	// kernelIncludedTaps() skips every zero-weight tap, and an axis stuck
	// at extent 1 only ever has one possible (zero-weight-free) position,
	// so each pass genuinely only visits `width` taps per pixel instead of
	// width^DIM. Mathematically identical result to the single-kernel
	// version (a separable filter's whole point), at O(DIM*width) tap
	// evaluations per pixel instead of O(width^DIM) -- a factor of
	// width^(DIM-1)/DIM cheaper, e.g. ~24x fewer tap evaluations for a
	// DIM=3, width=7 kernel. Kernel radius follows the standard 3-sigma
	// rule (_kernelSize in mathHelpers.h), so larger sigma automatically
	// gets a wider kernel; the same sigma and radius apply along every
	// dimension. Blurring a color image channel-by-channel (so colors
	// don't bleed into each other) is a matter of calling this on each
	// channel's 2D slice rather than the 3D whole -- no special-casing
	// needed here, since slice() already shares memory with the original
	// and convolve() is dimension-agnostic.
	//
	// Unlike convolve() above, this genuinely needs a real Image<double,DIM>
	// to hold the kernel weights it builds (and the double-precision
	// scratch buffers the intermediate passes accumulate into) -- that's
	// why this file includes image.h at all.
	/// Convolves `src` with a normalized Gaussian kernel of the given standard deviation, writing into `dst`. Applied as DIM separable 1D passes, not one full DIM-dimensional kernel -- see this function's own top comment.
	/// @tparam SrcImageT Any minimal-interface image type whose own concrete type also has an Image<double,DIM>-compatible construction path (i.e. any Image<T,DIM>).
	/// @tparam DstImageT Any minimal-interface image type (same DIM as SrcImageT); may differ from SrcImageT (e.g. a different concrete container).
	/// @param  src    Source image.
	/// @param  dst    Destination; must already exist with `src`'s own extent.
	/// @param  sigma  Standard deviation; the kernel radius follows the standard 3-sigma rule.
	/// @param  border How an out-of-bounds neighbor is resolved. Defaults to BorderMode::Clamp.
	/// @ingroup convolution
	template<class SrcImageT, class DstImageT>
	void gaussian_blur(const SrcImageT& src, DstImageT& dst, double sigma, BorderMode border = BorderMode::Clamp)
	{
		using SrcT = typename SrcImageT::value_type;
		static_assert(std::is_arithmetic_v<SrcT>, "ndl::gaussian_blur() requires a value_type convertible to double -- not valid for e.g. std::complex<T> (it calls convolve() internally)");
		assert(sigma > 0);
		auto extent = src.extent();
		constexpr int DIM = std::tuple_size<decltype(extent)>::value;

		int radius = _kernelSize(sigma);
		int width = 2 * radius + 1;
		std::vector<double> weights1D(width);
		double wsum = 0;
		for (int i = 0; i < width; i++)
		{
			double d = i - radius;
			weights1D[i] = std::exp(-(d * d) / (2 * sigma * sigma));
			wsum += weights1D[i];
		}
		for (auto& w : weights1D) w /= wsum;

		// Builds the {1,...,width,...,1} kernel for one axis into
		// caller-owned storage (kept alive by the caller across the
		// convolve() call that immediately follows, since Image doesn't
		// own its backing memory).
		auto buildAxisKernel = [&](int axis, std::vector<double>& kernelData) {
			std::array<int, DIM> kernelExtent;
			for (int i = 0; i < DIM; i++) kernelExtent[i] = (i == axis) ? width : 1;
			Image<double, DIM> kernel(kernelData.data(), kernelExtent);
			std::array<int, DIM> kCoord{};
			for (int i = 0; i < width; i++) { kCoord[axis] = i; kernel.at(kCoord) = weights1D[i]; }
			return kernel;
		};

		if (DIM == 1)
		{
			std::vector<double> kernelData(width);
			convolve(src, dst, buildAxisKernel(0, kernelData), border);
			return;
		}

		// DIM >= 2: axis 0 reads from src into bufA; the last axis writes
		// straight into dst; any axes in between ping-pong through bufA/
		// bufB (only ever two scratch buffers needed, regardless of DIM).
		std::vector<double> bufAData(Image<double, DIM>::size(extent));
		std::vector<double> bufBData(Image<double, DIM>::size(extent));
		Image<double, DIM> bufA(bufAData.data(), extent);
		Image<double, DIM> bufB(bufBData.data(), extent);

		std::vector<double> kernelData0(width);
		convolve(src, bufA, buildAxisKernel(0, kernelData0), border);

		Image<double, DIM>* cur = &bufA;
		Image<double, DIM>* other = &bufB;
		for (int axis = 1; axis < DIM - 1; axis++)
		{
			std::vector<double> kernelDataMid(width);
			convolve(*cur, *other, buildAxisKernel(axis, kernelDataMid), border);
			std::swap(cur, other);
		}

		std::vector<double> kernelDataLast(width);
		convolve(*cur, *other, buildAxisKernel(DIM - 1, kernelDataLast), border);

		// Written through convolve() into a double buffer rather than
		// straight into dst, then rounded (not truncated) on the way into
		// DstT here: convolve()'s own per-pixel cast truncates toward zero
		// (its documented, unchanged behavior -- see its own final line),
		// which is fine after a SINGLE summation but chains two rounds of
		// floating-point error across two separable passes -- occasionally
		// enough to land a value that's mathematically exactly N just
		// under it (e.g. 76.999999999997), truncating to N-1 instead of
		// rounding back to N.
		using DstT = typename DstImageT::value_type;
		for (const auto& coord : other->coordinates())
		{
			double v = other->at(coord);
			dst.at(coord) = static_cast<DstT>(std::is_integral<DstT>::value ? std::round(v) : v);
		}
	}

	// Shrinks src by `factor` along every axis except `channelAxis`, each
	// output pixel the average of the `factor`-sized, NON-OVERLAPPING
	// source block it stands in for -- mip-mapping's own box-per-block
	// prefilter, which is already the textbook-correct antialiasing
	// filter for decimation (not an approximation of a nicer one).
	// Computed by direct summation over each block, not via a summed-area
	// table: an earlier version of this function built one (per channel,
	// via summed_area_table.h's summed_area_table()/box_filter_query())
	// on the theory that its O(1)-per-query cost would pay for itself --
	// measured directly instead of assumed, and it doesn't, for this
	// specific access pattern. A summed-area table earns its O(1)-per-
	// query cost when queries OVERLAP (re-reading the same source pixels
	// from multiple queries, the texture-mapping case it was invented for
	// -- Crow, 1984) or land at arbitrary, non-grid-aligned positions
	// (box_filter_query()'s own real use, projection.h's back_project()).
	// Neither is true here: every output block is a disjoint, exactly-
	// tiled partition of the source, so every source pixel already
	// contributes to exactly ONE query -- direct summation is already
	// optimal (O(source size) total, same asymptotic cost as just
	// building the table would have been), and skips the table's own
	// extra double-precision full-copy pass and 2x-larger buffer entirely.
	// Measured (1280x720x3 color crop): direct summation beat the old
	// summed-area-table version at EVERY factor tested (2 through 32),
	// 3x-13x faster and still single-threaded, before even adding the
	// per-channel parallelism below. It also closes a real cliff the SAT
	// version had: since a table build cost the same regardless of
	// factor, ANY downsampling (even factor=2) paid the SAME large fixed
	// tax an extreme one did -- direct summation's cost instead scales
	// smoothly with the actual amount of source data being averaged.
	// Returns a new OwnedImage rather than writing into a caller-provided
	// dst (unlike every other function in this file): the output extent
	// is itself a function of factor, and there's no simpler way to hand
	// that back than computing and returning the already-sized result
	// directly, the same reasoning OwnedImage::like() and view()
	// themselves already return new objects rather than filling one in
	// place.
	//
	// `border` only ever affects the LAST, possibly-short block along an
	// axis whose extent isn't an exact multiple of `factor` -- Clamp
	// shrinks that block to whatever source pixels actually exist (its
	// average is over fewer than factor^N samples, not padded with
	// repeated edge values); Wrap/Reflect aren't meaningfully different
	// for a same-direction decimation (there's nothing past the edge for
	// this specific block to reach INTO), so only Clamp's behavior is
	// actually reachable -- true of every past caller anyway, since none
	// has ever passed anything but the Clamp default.
	/// Shrinks `src` by `factor` along every axis except `channelAxis`, each output pixel the average of the `factor`-sized, non-overlapping source block it stands in for. Returns a new OwnedImage.
	/// @tparam ImageT Any minimal-interface image type whose own concrete type also has an Image<double,DIM>-compatible construction path (i.e. any Image<T,DIM>).
	/// @param  src         Source image.
	/// @param  factor      Shrink factor (each output pixel averages a `factor`-sized source block along every axis except `channelAxis`).
	/// @param  channelAxis Axis left undecimated (e.g. 0 for a {channel,x,y} color image). Defaults to 0.
	/// @param  border      Unused beyond Clamp (the only behavior a same-direction decimation can actually reach) -- kept for interface stability. Defaults to BorderMode::Clamp.
	/// @ingroup convolution
	template<class ImageT>
	auto downsample(const ImageT& src, int factor, int channelAxis = 0, BorderMode border = BorderMode::Clamp)
	{
		(void)border;
		using T = typename ImageT::value_type;
		auto extent = src.extent();
		constexpr int DIM = std::tuple_size<decltype(extent)>::value;
		static_assert(std::is_arithmetic_v<T>, "ndl::downsample() requires a value_type convertible to double -- not valid for e.g. std::complex<T>");
		assert(factor > 1);

		std::array<int, DIM> outExtent;
		for (int i = 0; i < DIM; i++)
			outExtent[i] = (i == channelAxis) ? extent[i] : (extent[i] - 1) / factor + 1;
		OwnedImage<T, DIM> result(outExtent);

		constexpr int SUBDIM = DIM - 1;

		// One channel per task, run in parallel: each channel only ever
		// reads its own src.slice() and writes its own (disjoint)
		// result.slice() -- a color image's 3 channels share no memory
		// with each other, so this needs no locking, the same "chunks are
		// already independent, no merge step" reasoning fft.h's own
		// std::execution::par usage documents.
		std::vector<int> channelIndices(extent[channelAxis]);
		std::iota(channelIndices.begin(), channelIndices.end(), 0);
		std::for_each(std::execution::par, channelIndices.begin(), channelIndices.end(), [&](int c)
		{
			Image<T, SUBDIM> srcChannel = src.slice(channelAxis, c);
			Image<T, SUBDIM> dstChannel = result.slice(channelAxis, c);
			auto srcExtent = srcChannel.extent();

			for (const auto& outCoord : dstChannel.coordinates())
			{
				std::array<int, SUBDIM> lo, hi, blockExtent;
				int count = 1;
				for (int i = 0; i < SUBDIM; i++)
				{
					lo[i] = outCoord[i] * factor;
					hi[i] = std::min(lo[i] + factor, srcExtent[i]);
					blockExtent[i] = hi[i] - lo[i];
					count *= blockExtent[i];
				}

				// Walks the block's SUBDIM-dimensional coordinates via an
				// inline stack-based odometer (no heap allocation -- the
				// same trick interpolation.h's forEachTap() uses for its
				// own per-call tap walk) rather than materializing a
				// coordinate list per output pixel.
				double sum = 0;
				std::array<int, SUBDIM> srcCoord = lo;
				for (int n = 0; n < count; n++)
				{
					sum += static_cast<double>(srcChannel.at(srcCoord));
					for (int i = 0; i < SUBDIM; i++)
					{
						if (++srcCoord[i] < hi[i]) break;
						srcCoord[i] = lo[i];
					}
				}
				double avg = sum / count;
				dstChannel.at(outCoord) = static_cast<T>(std::is_integral<T>::value ? std::round(avg) : avg);
			}
		});
		return result;
	}

	// Clamps a floating-point (or otherwise wider-range) image into a
	// narrower, directly displayable type's own [0, max] range, optionally
	// re-centering by `bias` first. Needed whenever a computation (a kernel
	// with negative weights, a signed gradient magnitude, ...) can produce
	// values outside the display type's own range: casting a negative or
	// out-of-range floating-point value straight to an unsigned narrow type
	// (e.g. uint8_t) is undefined behavior, not a wraparound, so a value
	// that would land out of range is clamped here instead, on the way
	// through to the narrower type.
	/// Clamps `src` into `dst`'s own [0, numeric_limits<DstT>::max()] range, optionally re-centering by `bias` first.
	/// @tparam SrcImageT Any minimal-interface image type whose value_type converts to double.
	/// @tparam DstImageT Any minimal-interface image type (same DIM as SrcImageT) whose value_type is arithmetic -- the display type being clamped into.
	/// @param  src  Source image.
	/// @param  dst  Destination; must already exist with `src`'s own extent.
	/// @param  bias Added to every value before clamping (e.g. 128 to re-center an asymmetric kernel's "no change" result on mid-grey instead of black). Defaults to 0.
	/// @ingroup convolution
	template<class SrcImageT, class DstImageT>
	void to_displayable(const SrcImageT& src, DstImageT& dst, double bias = 0.0)
	{
		using DstT = typename DstImageT::value_type;
		static_assert(std::is_arithmetic_v<DstT>, "ndl::to_displayable() requires an arithmetic destination value_type -- not valid for e.g. std::complex<T>");
		assert(dst.extent() == src.extent());
		double hi = static_cast<double>(std::numeric_limits<DstT>::max());
		for (const auto& coord : src.coordinates())
		{
			double v = static_cast<double>(src.at(coord)) + bias;
			dst.at(coord) = static_cast<DstT>(v < 0.0 ? 0.0 : (v > hi ? hi : v));
		}
	}

	// The spatial gradient, one central-difference partial derivative per
	// axis, written into a destination with an extra leading "component"
	// axis of size DIM -- the same {component, ...spatial} shape this
	// library's own color images already use ({channel, x, y}), so a single
	// component is just dst.slice(0, axis) away, and the whole thing plugs
	// straight into per_channel()/slice()/etc. without any gradient-specific
	// plumbing elsewhere.
	//
	// Computed via convolve() itself, not a hand-written pixel loop: a
	// central difference along one axis is exactly what a kernel shaped
	// {..., 1 (other axes), 3 (this axis), 1 (other axes), ...} with taps
	// {-0.5, 0, +0.5} along that one axis already computes, one axis-sized
	// convolve() call at a time -- so this reuses convolve()'s own
	// BorderMode handling for free instead of re-deriving how each border
	// mode remaps an out-of-bounds neighbor.
	/// The spatial gradient of `src`: dst.slice(0,axis) is the central-difference partial derivative along `axis`, for every axis.
	/// @tparam SrcImageT Any minimal-interface image type whose value_type converts to double.
	/// @tparam DstImageT Any minimal-interface image type with exactly one more axis than SrcImageT (a leading component axis of size DIM); may differ from SrcImageT otherwise.
	/// @param  src    Source image.
	/// @param  dst    Destination; must already exist, extent {DIM, ...src's own extent}.
	/// @param  border How an out-of-bounds neighbor is resolved. Defaults to BorderMode::Reflect (the natural choice for a derivative -- Clamp would flatten it to 0 right at the border).
	/// @ingroup convolution
	template<class SrcImageT, class DstImageT>
	void gradient(const SrcImageT& src, DstImageT& dst, BorderMode border = BorderMode::Reflect)
	{
		using SrcT = typename SrcImageT::value_type;
		static_assert(std::is_arithmetic_v<SrcT>, "ndl::gradient() requires a value_type convertible to double -- not valid for e.g. std::complex<T> (it calls convolve() internally)");

		auto extent = src.extent();
		constexpr int DIM = std::tuple_size<decltype(extent)>::value;
		auto dstExtent = dst.extent();
		constexpr int DstDIM = std::tuple_size<decltype(dstExtent)>::value;
		static_assert(DstDIM == DIM + 1, "ndl::gradient() requires a destination with exactly one more axis than the source (a leading component axis of size DIM)");
		assert(dstExtent[0] == DIM);
		for (int d = 0; d < DIM; d++) assert(dstExtent[d + 1] == extent[d]);

		for (int axis = 0; axis < DIM; axis++)
		{
			std::array<int, DIM> kernelExtent;
			for (int d = 0; d < DIM; d++) kernelExtent[d] = (d == axis) ? 3 : 1;
			std::vector<double> kernelData(Image<double, DIM>::size(kernelExtent), 0.0);
			Image<double, DIM> kernel(kernelData.data(), kernelExtent);
			std::array<int, DIM> negCoord{}, posCoord{};
			negCoord[axis] = 0;
			posCoord[axis] = 2;
			kernel.at(negCoord) = -0.5;
			kernel.at(posCoord) = 0.5;

			auto dstChannel = dst.slice(0, axis);
			convolve(src, dstChannel, kernel, border);
		}
	}
}
