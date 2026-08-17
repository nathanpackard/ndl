#pragma once
#include <array>
#include <vector>
#include <cstddef>
#include <type_traits>
#include <utility>
#include "border_mode.h"
#include "../mathHelpers.h"

namespace ndl
{
	namespace detail
	{
		// Detects "U is Image<X,D>, or publicly derives from it, for some X
		// and D" -- used to keep the "scalar" overloads of
		// operator=/+=/-=/etc. and add()/subtract()/multiply()/divide() from
		// matching an Image (or Image-derived, e.g. OwnedImage) argument
		// better than the sibling Image-taking overload does. Without this,
		// passing an OwnedImage -- which IS-A Image, but is also its own
		// concrete type -- prefers the "scalar" overload (an exact type
		// match beats the derived-to-base conversion the Image overload
		// needs), which then fails to compile for the by-value overloads
		// (OwnedImage isn't copyable, and those take their argument by
		// value) or would silently do the wrong thing for the by-reference
		// ones (broadcasting/comparing against the whole object rather than
		// treating it as another image).
		template<class X, int D> std::true_type is_image_test(const Image<X, D>*);
		std::false_type is_image_test(...);
		template<class U> struct is_image_like : decltype(is_image_test(std::declval<std::remove_reference_t<U>*>())) {};

		// Detects "ImageT exposes the minimal structural interface
		// morphology.h/convolution.h's free functions are templated
		// against, for a DIM-dimensional type": extent() convertible to
		// std::array<int,DIM>, at(coord) callable with a
		// std::array<int,DIM>, and coordinates() callable at all.
		// Deliberately does NOT pin down at()'s or coordinates()'s exact
		// return type -- that's precisely what varies between Image (at()
		// returns T&) and PackedBitImage (at() returns BitRef/bool by
		// value), and constraining it here would reject the very
		// difference this whole minimal-interface design exists to allow.
		// Meant to be used as a static_assert right after each
		// minimal-interface type's own definition (see Image in core.h,
		// PackedBitImage in packed_bit.h), so a signature that drifts out
		// of shape fails to compile at the type's own definition, with a
		// message naming exactly that, instead of later and more
		// confusingly, deep inside whichever free function first tries to
		// instantiate against it.
		template<class ImageT, int DIM, class = void>
		struct satisfies_minimal_interface : std::false_type {};
		template<class ImageT, int DIM>
		struct satisfies_minimal_interface<ImageT, DIM, std::void_t<
			std::enable_if_t<std::is_convertible_v<decltype(std::declval<const ImageT&>().extent()), std::array<int, DIM>>>,
			decltype(std::declval<const ImageT&>().at(std::declval<std::array<int, DIM>>())),
			decltype(std::declval<const ImageT&>().coordinates())
		>> : std::true_type {};
		template<class ImageT, int DIM>
		inline constexpr bool satisfies_minimal_interface_v = satisfies_minimal_interface<ImageT, DIM>::value;

		// Detects "ImageT exposes .stride(), AND at(coord) returns a
		// genuine T& (not a proxy, e.g. PackedBitImage's packed-bit
		// reference)" -- i.e. ImageT's storage is genuinely linear/
		// strided, so &at(origin) + i*stride()[axis] is a CORRECT way to
		// reach the element at origin with axis advanced by i, for every
		// i in range. True for Image<T,DIM> and everything derived from
		// it (OwnedImage, every view()/slice()/swap_axes() result).
		//
		// Used as a static_assert requirement (not a fast/slow dispatch
		// switch -- an earlier version of this file tried that and
		// measured no actual speed difference between the two paths at
		// -O3: Image::at()'s stride dot-product has a compile-time-
		// constant trip count, so the compiler already inlines and
		// strength-reduces it down to the same code a raw pointer walk
		// would produce. What the dot-product form genuinely can't do is
		// be walked with a raw pointer at all when storage ISN'T linear
		// -- PackedBitImage's individual bits aren't addressable, and
		// RingBufferImage deliberately doesn't expose stride() since its
		// own physicalOffset() wraps one axis modulo the ring's capacity,
		// not a constant stride) -- so per-axis algorithms that only ever
		// make sense against a real materialized image anyway
		// (summed_area_table(), fftn(), distance_transform_squared() --
		// none of which have a coherent meaning run directly against
		// packed bits or a live ring buffer without first snapshotting
		// into a real Image) require it outright via this trait, rather
		// than carrying a second, slower, generic implementation for a
		// combination that was never actually going to be used. A
		// PackedBitImage's own role is disk-storage compactness, not
		// in-memory processing -- convert to a bool/int Image first (a
		// deliberate, visible step) if one of these needs to run against
		// bit-packed data.
		template<class ImageT, int DIM, class = void>
		struct has_stride_access : std::false_type {};
		template<class ImageT, int DIM>
		struct has_stride_access<ImageT, DIM, std::void_t<
			std::enable_if_t<std::is_convertible_v<decltype(std::declval<const ImageT&>().stride()), std::array<int, DIM>>>,
			std::enable_if_t<std::is_lvalue_reference_v<decltype(std::declval<ImageT&>().at(std::declval<std::array<int, DIM>>()))>>
		>> : std::true_type {};
		template<class ImageT, int DIM>
		inline constexpr bool has_stride_access_v = has_stride_access<ImageT, DIM>::value;

		// The shared "get a pointer to walk one fiber" primitive every
		// per-axis algorithm in this library (summed_area_table()'s
		// prefix-sum pass, fftn()'s per-axis transform,
		// distance_transform_squared()'s per-axis envelope) needs, pushed
		// up here once instead of each re-deriving "&image.at(origin)" +
		// "image.stride()[axis]" on its own. Requires
		// has_stride_access_v<ImageT, DIM> (a static_assert at the call
		// site, not here, so the failure names the actual algorithm that
		// needed it). The caller walks the fiber via `cursor.ptr[i *
		// cursor.stride]` for i in 0..n-1, or increments `cursor.ptr` by
		// `cursor.stride` directly.
		template<class ImageT, std::size_t DIM>
		struct FiberCursor
		{
			typename ImageT::value_type* ptr;
			std::ptrdiff_t stride;
		};
		template<class ImageT, std::size_t DIM>
		FiberCursor<ImageT, DIM> fiberCursor(ImageT& image, int axis, const std::array<int, DIM>& origin)
		{
			return { &image.at(origin), (std::ptrdiff_t)image.stride()[axis] };
		}

		// Shared coordinate-list generator: every DIM-dimensional "visit
		// every position" walk (Image::coordinates(), PackedBitImage::
		// coordinates(), below) is this same recursion, so it lives once
		// here rather than once per class.
		template<std::size_t DIM>
		void generateCoordinates(const std::array<int, DIM>& extents, std::array<int, DIM>& indices, std::vector<std::array<int, DIM>>& allIndices, std::size_t depth = 0)
		{
			if (depth == DIM) { allIndices.push_back(indices); return; }
			for (int i = 0; i < extents[DIM - depth - 1]; ++i) {
				indices[DIM - depth - 1] = i;
				generateCoordinates(extents, indices, allIndices, depth + 1);
			}
		}
		template<std::size_t DIM>
		std::vector<std::array<int, DIM>> coordinatesOf(const std::array<int, DIM>& extent)
		{
			std::vector<std::array<int, DIM>> allIndices;
			std::array<int, DIM> indices{};
			generateCoordinates(extent, indices, allIndices);
			return allIndices;
		}

		// Shared setup for every kernel-walking operation (erode()/dilate()/
		// median_filter()/percentile_filter()/convolve(), in morphology.h
		// and convolution.h): the kernel's center, and its nonzero
		// ("included") taps, computed once per call rather than once per
		// output pixel. DIM is read off kernel.extent()'s own
		// std::array<int,DIM> return type via std::tuple_size, rather
		// than being named as a separate template parameter -- kernel
		// only needs to expose extent()/at()/coordinates() (the same
		// minimal interface described on the free functions below), not
		// literally be an Image.
		template<class KernelT>
		auto kernelCenter(const KernelT& kernel)
		{
			auto extent = kernel.extent();
			constexpr int DIM = std::tuple_size<decltype(extent)>::value;
			std::array<int, DIM> center;
			for (int i = 0; i < DIM; i++) center[i] = extent[i] / 2;
			return center;
		}
		template<class KernelT>
		auto kernelIncludedTaps(const KernelT& kernel)
		{
			auto extent = kernel.extent();
			constexpr int DIM = std::tuple_size<decltype(extent)>::value;
			using K = typename KernelT::value_type;
			std::vector<std::array<int, DIM>> taps;
			for (const auto& kCoord : kernel.coordinates())
				if (kernel.at(kCoord) != K(0)) taps.push_back(kCoord);
			return taps;
		}
		// Resolves the border-handled source coordinate for kernel tap
		// `kCoord` (centered via `center`) when computing the value at
		// `coord`, against an image of the given `extent`.
		template<std::size_t DIM>
		std::array<int, DIM> kernelTapCoord(const std::array<int, DIM>& coord, const std::array<int, DIM>& kCoord, const std::array<int, DIM>& center, const std::array<int, DIM>& extent, BorderMode border)
		{
			std::array<int, DIM> srcCoord;
			for (std::size_t i = 0; i < DIM; i++)
			{
				int x = coord[i] + kCoord[i] - center[i];
				switch (border)
				{
				case BorderMode::Wrap:    x = _wrap(extent[i], x); break;
				case BorderMode::Reflect: x = _reflect(extent[i], x); break;
				default:                  x = _clamp(extent[i], x); break;
				}
				srcCoord[i] = x;
			}
			return srcCoord;
		}

		// One representative coordinate per "fiber" along `axis`: every
		// other dimension enumerated over its full range, with `axis`
		// itself fixed at 0 (the caller sweeps just that component from
		// 0..extent[axis]-1 to walk the fiber). Shared by every "process an
		// N-dimensional array one axis-aligned 1D fiber at a time"
		// algorithm in this library -- fft.h's fftn() (one axis at a time,
		// via std::execution::par, since two different fibers for the same
		// axis never touch the same element), summed_area_table.h's
		// box_blur()/summed_area_table(), and distance_transform.h's
		// distance_transform() all walk exactly this same shape.
		template<std::size_t DIM>
		std::vector<std::array<int, DIM>> fiberOrigins(const std::array<int, DIM>& extent, int axis)
		{
			std::size_t count = 1;
			for (std::size_t d = 0; d < DIM; d++) if ((int)d != axis) count *= extent[d];

			std::vector<std::array<int, DIM>> origins;
			origins.reserve(count);

			std::array<int, DIM> coord{};
			coord[axis] = 0;
			for (std::size_t i = 0; i < count; i++)
			{
				origins.push_back(coord);
				for (std::size_t d = 0; d < DIM; d++)
				{
					if ((int)d == axis) continue;
					if (++coord[d] < extent[d]) break;
					coord[d] = 0;
				}
			}
			return origins;
		}
	}
}
