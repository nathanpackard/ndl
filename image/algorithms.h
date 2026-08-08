#pragma once
#include <cassert>
#include <vector>
#include <algorithm>
#include <cmath>
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
	template<class ImageT, class KernelT>
	void erode(const ImageT& src, ImageT& dst, const KernelT& kernel, BorderMode border = BorderMode::Clamp)
	{
		using T = typename ImageT::value_type;
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
	template<class ImageT, class KernelT>
	void dilate(const ImageT& src, ImageT& dst, const KernelT& kernel, BorderMode border = BorderMode::Clamp)
	{
		using T = typename ImageT::value_type;
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
	template<class ImageT, class KernelT>
	void percentile_filter(const ImageT& src, ImageT& dst, const KernelT& kernel, double percentile, BorderMode border = BorderMode::Clamp)
	{
		using T = typename ImageT::value_type;
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
	template<class ImageT, class KernelT>
	void median_filter(const ImageT& src, ImageT& dst, const KernelT& kernel, BorderMode border = BorderMode::Clamp) { percentile_filter(src, dst, kernel, 50.0, border); }

	// Binarizes src into dst: onValue where src's value is greater than
	// thresholdValue, offValue otherwise (strictly greater-than, matching
	// Image::otsu_threshold()'s own class split: background is values <=
	// the threshold, foreground is values above it). src and dst need
	// not be the same concrete type -- e.g. thresholding an
	// Image<uint8_t,DIM> greyscale source directly into a compact
	// PackedBitImage<DIM> mask is exactly the intended use.
	template<class SrcImageT, class DstImageT>
	void threshold(const SrcImageT& src, DstImageT& dst, typename SrcImageT::value_type thresholdValue, typename DstImageT::value_type onValue, typename DstImageT::value_type offValue)
	{
		assert(dst.extent() == src.extent());
		for (const auto& coord : src.coordinates())
			dst.at(coord) = (src.at(coord) > thresholdValue) ? onValue : offValue;
	}
	// Defaults onValue/offValue to DstImageT's own T(1)/T(0) -- 1/0 for an
	// integral mask image, true/false for a PackedBitImage.
	template<class SrcImageT, class DstImageT>
	void threshold(const SrcImageT& src, DstImageT& dst, typename SrcImageT::value_type thresholdValue)
	{
		using DstT = typename DstImageT::value_type;
		threshold(src, dst, thresholdValue, DstT(1), DstT(0));
	}
}
