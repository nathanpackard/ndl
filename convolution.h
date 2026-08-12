#pragma once
#include <cassert>
#include <cmath>
#include <vector>
#include <array>
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
	/// @tparam ImageT  Any minimal-interface image type (Image<T,DIM>, PackedBitImage<DIM>, ...); src and dst must be the same concrete type.
	/// @tparam KernelT Any minimal-interface image type for the kernel; may differ from ImageT.
	/// @param  src     Source image.
	/// @param  dst     Destination; must already exist with `src`'s own extent.
	/// @param  kernel  Weights, nonzero-tap = included; convolve() also uses the value as a weight. Its own extent sets the neighborhood radius per dimension.
	/// @param  border  How an out-of-bounds neighbor is resolved. Defaults to BorderMode::Clamp.
	/// @ingroup convolution
	template<class ImageT, class KernelT>
	void convolve(const ImageT& src, ImageT& dst, const KernelT& kernel, BorderMode border = BorderMode::Clamp)
	{
		using T = typename ImageT::value_type;
		static_assert(std::is_arithmetic_v<T>, "ndl::convolve() requires a value_type convertible to double -- not valid for e.g. std::complex<T> (the weighted sum is accumulated in double)");
		assert(dst.extent() == src.extent());
		auto extent = src.extent();
		auto center = detail::kernelCenter(kernel);
		auto taps = detail::kernelIncludedTaps(kernel); // zero-weight taps contribute nothing to the sum, so skipping them is free

		for (const auto& coord : src.coordinates())
		{
			double total = 0;
			for (const auto& kCoord : taps)
				total += static_cast<double>(src.at(detail::kernelTapCoord(coord, kCoord, center, extent, border))) * static_cast<double>(kernel.at(kCoord));
			dst.at(coord) = static_cast<T>(total);
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
	/// @tparam ImageT Any minimal-interface image type whose own concrete type also has an Image<double,DIM>-compatible construction path (i.e. any Image<T,DIM>).
	/// @param  src    Source image.
	/// @param  dst    Destination; must already exist with `src`'s own extent.
	/// @param  sigma  Standard deviation; the kernel radius follows the standard 3-sigma rule.
	/// @param  border How an out-of-bounds neighbor is resolved. Defaults to BorderMode::Clamp.
	/// @ingroup convolution
	template<class ImageT>
	void gaussian_blur(const ImageT& src, ImageT& dst, double sigma, BorderMode border = BorderMode::Clamp)
	{
		using T = typename ImageT::value_type;
		static_assert(std::is_arithmetic_v<T>, "ndl::gaussian_blur() requires a value_type convertible to double -- not valid for e.g. std::complex<T> (it calls convolve() internally)");
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
}
