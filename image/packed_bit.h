#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <bitset>
#include <numeric>
#include <functional>
#include <ostream>
#include <cassert>
#include "detail.h"
#include "core.h"

namespace ndl
{
	// A compact, mutable N-dimensional array of bits -- 1 bit of real
	// storage per element instead of a whole byte, for the very common case
	// of a genuinely binary image (e.g. a threshold() result) where memory,
	// not per-pixel generality, is the concern.
	//
	// Deliberately NOT derived from Image<bool,DIM>, and doesn't try to be
	// one: Image's whole contract is that at()/operator() return a real
	// T& you can take the address of, bind a reference to, or hand to any
	// T&-expecting generic code -- there is no such thing as a "bool&" into
	// a packed bit, the same reason std::vector<bool> famously isn't a real
	// Container either. Rather than lie about that (or inherit from Image
	// and silently break the reference contract every other Image consumer
	// relies on), PackedBitImage is its own class with its own proxy
	// reference type (BitRef, below) for mutation -- exactly how
	// std::vector<bool>/std::bitset already do this successfully; it's only
	// a footgun when a type does this while *pretending* to satisfy
	// generic T&-expecting code, which PackedBitImage never claims to.
	//
	// What it DOES share with Image, deliberately, is the same minimal
	// structural interface the free ndl::erode()/dilate()/median_filter()/
	// threshold() functions (image/algorithms.h) are written against:
	// extent(), at(coord) (const returns bool by value; non-const returns a
	// BitRef, assignable from bool), and coordinates(). That's the whole
	// contract those functions need, so they run against a PackedBitImage
	// completely unmodified -- e.g. ndl::erode(bits, dst, kernel) works
	// today, with erosion/dilation reducing to AND/OR of the neighborhood,
	// the standard definition of binary morphology.
	//
	// Unlike OwnedImage, PackedBitImage doesn't need any base-class
	// construction-ordering trick: it doesn't inherit from anything, so
	// there's no raw pointer captured by a base constructor to keep valid.
	// It's just an ordinary value type wrapping a std::vector<uint64_t> --
	// copy/move construction/assignment are all the compiler-generated
	// defaults, and all just work.
	template<int DIM>
	class PackedBitImage
	{
	public:
		using value_type = bool;

		// Proxy reference to a single bit -- the mutable return type of the
		// non-const at()/operator(), standing in for the "bool&" that can't
		// exist over packed storage. Converts to bool for reads; assignment
		// writes the bit in place via read-modify-write on its word.
		class BitRef
		{
		public:
			BitRef(std::uint64_t* word, int bit) : word_(word), bit_(bit) { }
			operator bool() const { return ((*word_) >> bit_) & std::uint64_t(1); }
			BitRef& operator=(bool value)
			{
				if (value) *word_ |= (std::uint64_t(1) << bit_);
				else       *word_ &= ~(std::uint64_t(1) << bit_);
				return *this;
			}
			BitRef& operator=(const BitRef& rhs) { return *this = bool(rhs); }
		private:
			std::uint64_t* word_;
			int bit_;
		};

		explicit PackedBitImage(std::array<int, DIM> extent) :
			extent_(extent),
			stride_(makeStride(extent)),
			words_((size(extent) + 63) / 64, std::uint64_t(0))
		{ }

		static std::size_t size(const std::array<int, DIM>& extent)
		{
			return std::accumulate(extent.begin(), extent.end(), std::size_t(1), std::multiplies<std::size_t>());
		}
		std::size_t size() const { return size(extent_); }

		const std::array<int, DIM>& extent() const { return extent_; }

		bool at(const std::array<int, DIM>& coord) const
		{
			std::size_t idx = flatIndex(coord);
			return (words_[idx / 64] >> (idx % 64)) & std::uint64_t(1);
		}
		BitRef at(const std::array<int, DIM>& coord)
		{
			std::size_t idx = flatIndex(coord);
			return BitRef(&words_[idx / 64], (int)(idx % 64));
		}

		// element access, Eigen/Matrix-style, matching Image::operator()
		template<class... Args>
		bool operator()(Args... indices) const {
			static_assert(sizeof...(Args) == DIM, "PackedBitImage::operator() needs exactly DIM integer indices");
			return at({ static_cast<int>(indices)... });
		}
		template<class... Args>
		BitRef operator()(Args... indices) {
			static_assert(sizeof...(Args) == DIM, "PackedBitImage::operator() needs exactly DIM integer indices");
			return at({ static_cast<int>(indices)... });
		}

		std::vector<std::array<int, DIM>> coordinates() const { return detail::coordinatesOf(extent_); }

		// Count of set bits -- the natural whole-image reduction for a binary
		// image (e.g. counting foreground pixels after a threshold()). Words
		// beyond the last valid bit are always 0 (nothing ever sets them --
		// every write goes through at(), which only ever produces indices <
		// size()), so no masking is needed here.
		std::size_t count() const
		{
			std::size_t total = 0;
			for (auto w : words_) total += std::bitset<64>(w).count();
			return total;
		}
		bool any() const { return count() > 0; }
		bool all() const { return count() == size(); }

	private:
		std::size_t flatIndex(const std::array<int, DIM>& coord) const
		{
			return std::inner_product(coord.begin(), coord.end(), stride_.begin(), std::size_t(0));
		}
		static std::array<std::size_t, DIM> makeStride(const std::array<int, DIM>& extent)
		{
			std::array<std::size_t, DIM> result{};
			result[0] = 1;
			for (int i = 1; i < DIM; i++) result[i] = (std::size_t)extent[i - 1] * result[i - 1];
			return result;
		}

		std::array<int, DIM> extent_;
		std::array<std::size_t, DIM> stride_;
		std::vector<std::uint64_t> words_;
	};

	// Converts any minimal-interface image (Image<bool,DIM>, Image<uint8_t,DIM>,
	// an already-thresholded mask, ...) into a freshly-allocated PackedBitImage,
	// nonzero -> true -- the "I have an ordinary image I want to shrink down"
	// direction. DIM is read off src.extent()'s own std::array<int,DIM> return
	// type, same trick detail::kernelCenter() uses, so it's never named
	// explicitly at the call site. Not restricted to arithmetic src types the
	// way the ordering-dependent operations are (image/core.h) -- != 0 alone
	// is all this needs, and std::complex supports that natively -- but
	// packing a complex image this way only tests "is it exactly zero", which
	// is rarely the intent; more likely you want to threshold a derived
	// magnitude/real-part image first and pack *that*.
	template<class ImageT>
	auto pack(const ImageT& src)
	{
		using T = typename ImageT::value_type;
		auto extent = src.extent();
		constexpr int DIM = std::tuple_size<decltype(extent)>::value;
		PackedBitImage<DIM> dst(extent);
		for (const auto& coord : src.coordinates())
			dst.at(coord) = (src.at(coord) != T(0));
		return dst;
	}

	// The other direction: expands a PackedBitImage back out into a real,
	// caller-owned Image<T,DIM> (or OwnedImage<T,DIM>) -- onValue/offValue
	// default to T(1)/T(0), the same "generic 0/1 mask" convention
	// Image::threshold() itself defaults to.
	template<class T, int DIM>
	void unpack(const PackedBitImage<DIM>& src, Image<T, DIM>& dst, T onValue = T(1), T offValue = T(0))
	{
		assert(dst.extent() == src.extent());
		for (const auto& coord : src.coordinates())
			dst.at(coord) = src.at(coord) ? onValue : offValue;
	}

	// Prints a PackedBitImage as 0/1 per cell -- deliberately a separate,
	// concrete overload rather than trying to generalize Image's own
	// operator<< to any minimal-interface type: that would need its own
	// SFINAE guard to avoid becoming an accidental catch-all for every type
	// in any translation unit that includes this header, which isn't worth
	// it for a debug-print convenience. Only handles DIM 1/2 (2D grids,
	// same as the common case for Image's own printer); for higher DIM, the
	// coordinates loop below still works, just without per-row breaks.
	template<int DIM>
	std::ostream& operator<<(std::ostream& sb, const PackedBitImage<DIM>& r)
	{
		std::array<int, DIM> indices{};
		std::function<void(int)> printImage = [&](int dim)
		{
			if (dim == 0)
			{
				for (int i = 0; i < r.extent()[dim]; i++)
				{
					indices[dim] = i;
					sb << (r.at(indices) ? '1' : '0');
				}
				sb << std::endl;
			}
			else
			{
				for (int i = 0; i < r.extent()[dim]; i++)
				{
					indices[dim] = i;
					printImage(dim - 1);
				}
			}
		};
		printImage(DIM - 1);
		return sb;
	}
}
