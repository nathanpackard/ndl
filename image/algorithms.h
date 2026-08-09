#pragma once
#include <cassert>
#include <vector>
#include <algorithm>
#include <cmath>
#include <type_traits>
#include "border_mode.h"
#include "detail.h"

namespace ndl
{
	// Generic, minimal-interface image processing: each of these works on
	// ANY type exposing extent() (-> std::array<int,DIM>), at(coord)
	// (const, returning a value convertible to the pixel type; non-const,
	// returning something assignable from it), and coordinates() (-> an
	// iterable range of std::array<int,DIM> visiting every position) --
	// not just Image. Image satisfies this today (and its own erode()/
	// dilate()/median_filter()/percentile_filter()/threshold() members,
	// in image/core.h, just forward to these), and it's the exact contract
	// PackedBitImage (image/packed_bit.h) is built to satisfy too,
	// without literally deriving from Image -- its bit-packed storage and
	// proxy bit-references make a real is-a relationship with Image
	// impossible (see PackedBitImage's own comment). This is what lets
	// the same erode()/dilate()/threshold() call work unmodified whether
	// `src`/`dst` are a normal Image or a bit-packed one.
	//
	// src and dst must be the same concrete type (e.g. both
	// Image<uint8_t,2>, or both PackedBitImage<2>); kernel can be any
	// (possibly different) minimal-interface type -- in practice always a
	// small Image<K,DIM> of weights/tap-mask values, even when src/dst
	// are a PackedBitImage.
	// erode/dilate/percentile_filter (and median_filter, which is just
	// percentile_filter at 50) all need to order ImageT::value_type values
	// against each other -- gated on is_arithmetic_v rather than left to
	// fail deep inside the comparison/std::nth_element, same reasoning as
	// the ordering-dependent Image members in core.h. is_arithmetic_v<bool>
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
	// Image::otsu_threshold()'s own class split: background is values <=
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
}
