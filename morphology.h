#pragma once
#include <cassert>
#include <vector>
#include <algorithm>
#include <cmath>
#include <type_traits>
#include "image/border_mode.h"
#include "image/detail.h"
#include "histogram.h"

// The morphology/thresholding toolkit: erode()/dilate()/median_filter()/
// percentile_filter()/threshold()/otsu_threshold()/invert(), as free
// functions over any minimal-interface image type. A sibling of
// fft.h/matrix.h/convolution.h, not part of image.h's core Image object --
// #include this directly if you use these; #include <ndl/image.h> alone
// does not pull it in, the same way it doesn't pull in fft.h. (This file
// does include histogram.h -- and therefore image.h -- itself, since
// otsu_threshold() below builds a Histogram<1>, which needs a real
// OwnedImage<std::size_t,1> for its own bin-count storage; every other
// function in this file has no such dependency.)

namespace ndl
{
	// Generic, minimal-interface image processing: each of these works on
	// ANY type exposing extent() (-> std::array<int,DIM>), at(coord)
	// (const, returning a value convertible to the pixel type; non-const,
	// returning something assignable from it), and coordinates() (-> an
	// iterable range of std::array<int,DIM> visiting every position) --
	// Image<T,DIM> (image/core.h) satisfies this, and so does PackedBitImage
	// (image/packed_bit.h), without PackedBitImage literally deriving from
	// Image -- its bit-packed storage and proxy bit-references make a real
	// is-a relationship with Image impossible (see PackedBitImage's own
	// comment). This is what lets the same erode()/dilate()/threshold()
	// call work unmodified whether `src`/`dst` are a normal Image or a
	// bit-packed one.
	//
	// src and dst must be the same concrete type (e.g. both
	// Image<uint8_t,2>, or both PackedBitImage<2>); kernel can be any
	// (possibly different) minimal-interface type -- in practice always a
	// small Image<K,DIM> of weights/tap-mask values, even when src/dst
	// are a PackedBitImage.
	// erode/dilate/percentile_filter (and median_filter, which is just
	// percentile_filter at 50) all need to order ImageT::value_type values
	// against each other -- gated on is_arithmetic_v rather than left to
	// fail deep inside the comparison/std::nth_element. is_arithmetic_v<bool>
	// is true, so these still work unmodified for a PackedBitImage (whose
	// value_type is bool) -- erosion/dilation reduce to AND/OR there.
	/// Morphological erosion: replaces each element with the minimum of its `kernel`-shaped neighborhood. Works on Image or PackedBitImage.
	/// @tparam ImageT  Any minimal-interface image type (Image<T,DIM>, PackedBitImage<DIM>, ...); src and dst must be the same concrete type.
	/// @tparam KernelT Any minimal-interface image type for the structuring element; may differ from ImageT.
	/// @param  src     Source image.
	/// @param  dst     Destination; must already exist with `src`'s own extent.
	/// @param  kernel  Structuring element (nonzero tap = included); see make_box_kernel()/make_cross_kernel().
	/// @param  border  How an out-of-bounds neighbor is resolved. Defaults to BorderMode::Clamp.
	/// @ingroup morphology_filtering
	template<class ImageT, class KernelT>
	void erode(const ImageT& src, ImageT& dst, const KernelT& kernel, BorderMode border = BorderMode::Clamp)
	{
		using T = typename ImageT::value_type;
		static_assert(std::is_arithmetic_v<T>, "ndl::erode() requires an arithmetic value_type (needs a total order) -- not valid for e.g. std::complex<T>");
		assert(dst.extent() == src.extent());
		auto extent = src.extent();
		auto center = detail::kernelCenter(kernel);
		auto taps = detail::kernelIncludedTaps(kernel);

		for (const auto& coord : src.coordinates())
		{
			bool first = true;
			T best{};
			for (const auto& kCoord : taps)
			{
				T v = src.at(detail::kernelTapCoord(coord, kCoord, center, extent, border));
				if (first || v < best) { best = v; first = false; }
			}
			dst.at(coord) = best;
		}
	}
	/// Morphological dilation: replaces each element with the maximum of its `kernel`-shaped neighborhood. Works on Image or PackedBitImage.
	/// @tparam ImageT  Any minimal-interface image type (Image<T,DIM>, PackedBitImage<DIM>, ...); src and dst must be the same concrete type.
	/// @tparam KernelT Any minimal-interface image type for the structuring element; may differ from ImageT.
	/// @param  src     Source image.
	/// @param  dst     Destination; must already exist with `src`'s own extent.
	/// @param  kernel  Structuring element (nonzero tap = included); see make_box_kernel()/make_cross_kernel().
	/// @param  border  How an out-of-bounds neighbor is resolved. Defaults to BorderMode::Clamp.
	/// @ingroup morphology_filtering
	template<class ImageT, class KernelT>
	void dilate(const ImageT& src, ImageT& dst, const KernelT& kernel, BorderMode border = BorderMode::Clamp)
	{
		using T = typename ImageT::value_type;
		static_assert(std::is_arithmetic_v<T>, "ndl::dilate() requires an arithmetic value_type (needs a total order) -- not valid for e.g. std::complex<T>");
		assert(dst.extent() == src.extent());
		auto extent = src.extent();
		auto center = detail::kernelCenter(kernel);
		auto taps = detail::kernelIncludedTaps(kernel);

		for (const auto& coord : src.coordinates())
		{
			bool first = true;
			T best{};
			for (const auto& kCoord : taps)
			{
				T v = src.at(detail::kernelTapCoord(coord, kCoord, center, extent, border));
				if (first || v > best) { best = v; first = false; }
			}
			dst.at(coord) = best;
		}
	}
	/// Replaces each element with the given percentile (0=min, 50=median, 100=max) of its `kernel`-shaped neighborhood.
	/// @tparam ImageT    Any minimal-interface image type (Image<T,DIM>, PackedBitImage<DIM>, ...); src and dst must be the same concrete type.
	/// @tparam KernelT   Any minimal-interface image type for the structuring element; may differ from ImageT.
	/// @param  src       Source image.
	/// @param  dst       Destination; must already exist with `src`'s own extent.
	/// @param  kernel    Structuring element (nonzero tap = included).
	/// @param  percentile 0-100.
	/// @param  border    How an out-of-bounds neighbor is resolved. Defaults to BorderMode::Clamp.
	/// @ingroup morphology_filtering
	template<class ImageT, class KernelT>
	void percentile_filter(const ImageT& src, ImageT& dst, const KernelT& kernel, double percentile, BorderMode border = BorderMode::Clamp)
	{
		using T = typename ImageT::value_type;
		static_assert(std::is_arithmetic_v<T>, "ndl::percentile_filter() requires an arithmetic value_type (needs a total order for std::nth_element) -- not valid for e.g. std::complex<T>");
		assert(dst.extent() == src.extent());
		assert(percentile >= 0.0 && percentile <= 100.0);
		auto extent = src.extent();
		auto center = detail::kernelCenter(kernel);
		auto taps = detail::kernelIncludedTaps(kernel);

		std::vector<T> window;
		for (const auto& coord : src.coordinates())
		{
			window.clear();
			for (const auto& kCoord : taps)
				window.push_back(src.at(detail::kernelTapCoord(coord, kCoord, center, extent, border)));
			std::size_t rank = (std::size_t)std::llround((percentile / 100.0) * (window.size() - 1));
			auto mid = window.begin() + rank;
			std::nth_element(window.begin(), mid, window.end());
			dst.at(coord) = *mid;
		}
	}
	/// Replaces each element with the median of its `kernel`-shaped neighborhood -- percentile_filter() at 50.
	/// @tparam ImageT  Any minimal-interface image type (Image<T,DIM>, PackedBitImage<DIM>, ...); src and dst must be the same concrete type.
	/// @tparam KernelT Any minimal-interface image type for the structuring element; may differ from ImageT.
	/// @param  src     Source image.
	/// @param  dst     Destination; must already exist with `src`'s own extent.
	/// @param  kernel  Structuring element (nonzero tap = included).
	/// @param  border  How an out-of-bounds neighbor is resolved. Defaults to BorderMode::Clamp.
	/// @ingroup morphology_filtering
	template<class ImageT, class KernelT>
	void median_filter(const ImageT& src, ImageT& dst, const KernelT& kernel, BorderMode border = BorderMode::Clamp) {
		static_assert(std::is_arithmetic_v<typename ImageT::value_type>, "ndl::median_filter() requires an arithmetic value_type (needs a total order) -- not valid for e.g. std::complex<T>");
		percentile_filter(src, dst, kernel, 50.0, border);
	}

	// Binarizes src into dst: onValue where src's value is greater than
	// thresholdValue, offValue otherwise (strictly greater-than, matching
	// otsu_threshold()'s own class split, below: background is values <=
	// the threshold, foreground is values above it). src and dst need
	// not be the same concrete type -- e.g. thresholding an
	// Image<uint8_t,DIM> greyscale source directly into a compact
	// PackedBitImage<DIM> mask is exactly the intended use.
	// Only SrcImageT::value_type needs ordering (the > comparison below);
	// DstImageT::value_type just needs to be assignable from onValue/
	// offValue, which works for any type -- e.g. thresholding a
	// std::complex source wouldn't make sense (no ordering) and is
	// rejected below, but the *destination* type is never restricted.
	/// Binarizes src into dst: `onValue` where greater than `thresholdValue`, `offValue` otherwise. src and dst may be different image types (e.g. thresholding an Image into a PackedBitImage).
	/// @tparam SrcImageT   Source's minimal-interface image type; its value_type must be arithmetic (needs ordering).
	/// @tparam DstImageT   Destination's minimal-interface image type; may differ from SrcImageT, and its value_type is unrestricted.
	/// @param  src         Source image.
	/// @param  dst         Destination; must already exist with `src`'s own extent.
	/// @param  thresholdValue Cutoff; strictly-greater-than, matching otsu_threshold()'s own class split.
	/// @param  onValue     Value written where the source is greater than `thresholdValue`.
	/// @param  offValue    Value written elsewhere.
	/// @ingroup morphology_filtering
	template<class SrcImageT, class DstImageT>
	void threshold(const SrcImageT& src, DstImageT& dst, typename SrcImageT::value_type thresholdValue, typename DstImageT::value_type onValue, typename DstImageT::value_type offValue)
	{
		static_assert(std::is_arithmetic_v<typename SrcImageT::value_type>, "ndl::threshold() requires an arithmetic source value_type (needs a total order) -- not valid for e.g. std::complex<T>");
		assert(dst.extent() == src.extent());
		for (const auto& coord : src.coordinates())
			dst.at(coord) = (src.at(coord) > thresholdValue) ? onValue : offValue;
	}
	// Defaults onValue/offValue to DstImageT's own T(1)/T(0) -- 1/0 for an
	// integral mask image, true/false for a PackedBitImage.
	/// Binarizes src into dst using dst's own T(1)/T(0) as onValue/offValue -- see the 5-argument overload for the full contract.
	/// @tparam SrcImageT      Source's minimal-interface image type; its value_type must be arithmetic (needs ordering).
	/// @tparam DstImageT      Destination's minimal-interface image type; may differ from SrcImageT.
	/// @param  src            Source image.
	/// @param  dst            Destination; must already exist with `src`'s own extent.
	/// @param  thresholdValue Cutoff; strictly-greater-than.
	/// @ingroup morphology_filtering
	template<class SrcImageT, class DstImageT>
	void threshold(const SrcImageT& src, DstImageT& dst, typename SrcImageT::value_type thresholdValue)
	{
		using DstT = typename DstImageT::value_type;
		threshold(src, dst, thresholdValue, DstT(1), DstT(0));
	}

	// Binary/logical NOT: dst = !src, elementwise -- the natural complement
	// to threshold() above (which is exactly what produces this kind of
	// image). Written once against the minimal interface, so the same code
	// runs unmodified whether src/dst are an Image<bool,DIM> or a
	// PackedBitImage<DIM>: `src.at(coord)` on the (const) source side reads
	// a plain bool either way (Image<bool,DIM>::at() const returns bool,
	// PackedBitImage::at() const returns bool too, not BitRef -- BitRef is
	// only ever the *mutable* accessor), so `!src.at(coord)` is always a
	// plain bool negation with no operator! needed on BitRef itself; then
	// `dst.at(coord) = ...` writes back through Image<bool,DIM>'s real
	// bool& or PackedBitImage's BitRef::operator=(bool), whichever dst is.
	// Not restricted to bool: any value_type contextually convertible to
	// bool works (same restriction Image::logical_not() already has), so
	// inverting an Image<uint8_t,DIM> mask of 0/1 (or 0/255) values works
	// too -- though the result is always exactly 0 or 1, not the source's
	// own on/off convention; threshold()'s own onValue/offValue is the
	// tool for that if you need something other than 0/1 out.
	/// Binary/logical NOT: dst(coord) = !src(coord), elementwise. Works on Image or PackedBitImage.
	/// @tparam ImageT Any minimal-interface image type (Image<T,DIM>, PackedBitImage<DIM>, ...); src and dst must be the same concrete type.
	/// @param  src    Source image.
	/// @param  dst    Destination; must already exist with `src`'s own extent.
	/// @ingroup morphology_filtering
	template<class ImageT>
	void invert(const ImageT& src, ImageT& dst)
	{
		static_assert(std::is_arithmetic_v<typename ImageT::value_type>, "ndl::invert() requires a value_type contextually convertible to bool -- not valid for e.g. std::complex<T>");
		assert(dst.extent() == src.extent());
		for (const auto& coord : src.coordinates())
			dst.at(coord) = !src.at(coord);
	}

	// Otsu's method: the value that maximizes between-class variance of
	// src's own histogram -- equivalently, the split that separates the two
	// classes of values (below/above it) as cleanly as possible. The
	// standard automatic threshold for turning a grayscale image into a
	// binary one without picking a cutoff by hand (Otsu, N., 1979, "A
	// threshold selection method from gray-level histograms"). Generic over
	// ImageT::value_type: the histogram (built via Histogram<1> above) spans
	// src's own [min,max] range, auto-detected the same way for any
	// minimal-interface image type, divided into `bins` equal-width buckets
	// (default 256 -- enough resolution for classic 8-bit imagery, and a
	// reasonable default for anything else). A noisy image widens both
	// classes' spread and can shift where they overlap, so Otsu still works
	// on it directly, but denoising first (e.g. median_filter()) generally
	// finds a cleaner split -- see demo/morphology.
	/// Otsu's method: the threshold value that maximizes between-class variance of src's own histogram.
	/// @tparam ImageT Any minimal-interface image type (Image<T,DIM>, PackedBitImage<DIM>, ...); its value_type must be arithmetic (needs ordering and a double conversion).
	/// @param  src    Source image.
	/// @param  bins   Histogram resolution; the value range is src's own [min,max], divided into this many equal-width buckets.
	/// @return The threshold value, in ImageT::value_type's own range -- pass directly to threshold().
	/// @ingroup morphology_filtering
	template<class ImageT>
	typename ImageT::value_type otsu_threshold(const ImageT& src, int bins = 256)
	{
		using T = typename ImageT::value_type;
		static_assert(std::is_arithmetic_v<T>, "ndl::otsu_threshold() requires an arithmetic value_type (needs a total order and a double conversion) -- not valid for e.g. std::complex<T>");
		assert(bins > 1);

		Histogram<1> hist(src, bins);
		double lo = hist.lo()[0];
		double range = hist.hi()[0] - lo;
		// No special-casing needed here for a degenerate (lo==hi) source:
		// Histogram<1> puts every sample in bucket 0 for a zero-range axis,
		// so weightForeground is 0 at b==0 and the loop below stops
		// immediately, leaving bestBucket at its initial 0 -- and since
		// range is 0 too, the final formula's `range * (bestBucket+1)/bins`
		// term vanishes, returning exactly `lo`, same as the single value
		// actually present.

		std::size_t total = hist.total();
		double sumAll = 0;
		for (int i = 0; i < bins; i++) sumAll += (double)i * hist.count(i);

		std::size_t weightBackground = 0;
		double sumBackground = 0;
		double bestVariance = -1;
		int bestBucket = 0;
		for (int b = 0; b < bins; b++)
		{
			weightBackground += hist.count(b);
			if (weightBackground == 0) continue;
			std::size_t weightForeground = total - weightBackground;
			if (weightForeground == 0) break;

			sumBackground += (double)b * hist.count(b);
			double meanBackground = sumBackground / weightBackground;
			double meanForeground = (sumAll - sumBackground) / weightForeground;

			double diff = meanBackground - meanForeground;
			double variance = (double)weightBackground * (double)weightForeground * diff * diff;
			if (variance > bestVariance)
			{
				bestVariance = variance;
				bestBucket = b;
			}
		}

		// Map the winning bucket back to a value in T's own range -- its
		// upper edge, so "value <= threshold" / "value > threshold"
		// reproduce the same background/foreground split the bucket
		// boundary represented.
		double thresholdValue = lo + range * static_cast<double>(bestBucket + 1) / static_cast<double>(bins);
		return static_cast<T>(thresholdValue);
	}
}
