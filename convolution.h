#pragma once
#include <cassert>
#include <cmath>
#include <vector>
#include <array>
#include <limits>
#include <type_traits>
#include "image/border_mode.h"
#include "image/detail.h"
#include "image.h"
#include "mathHelpers.h"

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

	// Gaussian blur: builds a normalized (weights sum to 1) Gaussian kernel
	// and hands it to convolve() above. Kernel radius follows the standard
	// 3-sigma rule (_kernelSize in mathHelpers.h), so larger sigma
	// automatically gets a wider kernel; the same sigma and radius apply
	// along every dimension. Blurring a color image channel-by-channel (so
	// colors don't bleed into each other) is a matter of calling this on
	// each channel's 2D slice rather than the 3D whole -- no special-casing
	// needed here, since slice() already shares memory with the original
	// and convolve() is dimension-agnostic.
	//
	// Unlike convolve() above, this genuinely needs a real Image<double,DIM>
	// to hold the kernel weights it builds -- that's why this file includes
	// image.h at all.
	/// Convolves `src` with a normalized Gaussian kernel of the given standard deviation, writing into `dst`.
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
		auto srcExtent = src.extent();
		constexpr int DIM = std::tuple_size<decltype(srcExtent)>::value;

		int radius = _kernelSize(sigma);
		std::array<int, DIM> kernelExtent;
		for (int i = 0; i < DIM; i++) kernelExtent[i] = 2 * radius + 1;

		std::vector<double> kernelData(Image<double, DIM>::size(kernelExtent));
		Image<double, DIM> kernel(kernelData.data(), kernelExtent);

		double total = 0;
		for (const auto& coord : kernel.coordinates())
		{
			double distSq = 0;
			for (int i = 0; i < DIM; i++)
			{
				double d = coord[i] - radius;
				distSq += d * d;
			}
			double w = std::exp(-distSq / (2 * sigma * sigma));
			kernel.at(coord) = w;
			total += w;
		}
		for (auto it = kernel.begin(); it != kernel.end(); ++it) *it /= total;

		convolve(src, dst, kernel, border);
	}

	// Shrinks src by `factor` along every axis except `channelAxis`: blurs
	// first via gaussian_blur() (per_channel()'d over channelAxis, so colors
	// don't bleed into each other) so the pixels a plain strided view() would
	// otherwise just skip get averaged in instead of aliased into noise, then
	// a step={1,...,factor,...,1} view() (1 at channelAxis, factor everywhere
	// else) does the actual decimation -- the standard "blur, then keep every
	// Nth pixel" resize most image libraries use. Returns a new OwnedImage
	// rather than writing into a caller-provided dst (unlike every other
	// function in this file): the output extent is itself a function of
	// factor, and there's no simpler way to hand that back than computing and
	// returning the already-sized result directly, the same reasoning
	// OwnedImage::like() and view() themselves already return new objects
	// rather than filling one in place.
	/// Shrinks `src` by `factor` along every axis except `channelAxis` (blurring first so the pixels a strided view() would otherwise skip get averaged in, not aliased into noise). Returns a new OwnedImage.
	/// @tparam ImageT Any minimal-interface image type whose own concrete type also has an Image<double,DIM>-compatible construction path (i.e. any Image<T,DIM>).
	/// @param  src         Source image.
	/// @param  factor      Shrink factor (keep every `factor`-th pixel along every axis except `channelAxis`).
	/// @param  channelAxis Axis left undecimated (e.g. 0 for a {channel,x,y} color image). Defaults to 0.
	/// @param  border      How an out-of-bounds neighbor is resolved during the blur pass. Defaults to BorderMode::Clamp.
	/// @ingroup convolution
	template<class ImageT>
	auto downsample(const ImageT& src, int factor, int channelAxis = 0, BorderMode border = BorderMode::Clamp)
	{
		using T = typename ImageT::value_type;
		auto extent = src.extent();
		constexpr int DIM = std::tuple_size<decltype(extent)>::value;
		static_assert(std::is_arithmetic_v<T>, "ndl::downsample() requires a value_type convertible to double -- not valid for e.g. std::complex<T> (it calls gaussian_blur() internally)");
		assert(factor > 1);

		auto blurred = OwnedImage<T, DIM>::like(src);
		per_channel(src, blurred, channelAxis, [&](const auto& s, auto& d) { gaussian_blur(s, d, factor * 0.5, border); });

		// view()'s own step argument only takes a compile-time
		// std::initializer_list, not a runtime-computed std::array (the
		// step needs to vary by axis here, dictated by the runtime
		// `channelAxis` argument) -- so the decimation below is a direct
		// coordinate-mapping copy instead, picking every `factor`-th
		// position along every axis except channelAxis. Same element
		// count view()'s own step semantics would produce for a full-range,
		// stride-`factor`, start-at-0 selection: floor((extent-1)/factor)+1.
		std::array<int, DIM> outExtent;
		for (int i = 0; i < DIM; i++)
			outExtent[i] = (i == channelAxis) ? extent[i] : (extent[i] - 1) / factor + 1;

		OwnedImage<T, DIM> result(outExtent);
		for (const auto& outCoord : result.coordinates())
		{
			std::array<int, DIM> srcCoord;
			for (int i = 0; i < DIM; i++) srcCoord[i] = (i == channelAxis) ? outCoord[i] : outCoord[i] * factor;
			result.at(outCoord) = blurred.at(srcCoord);
		}
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
