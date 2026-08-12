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
	// threshold() functions (morphology.h) are written against:
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
	/// A compact N-dimensional array of bits, 1 bit of real storage per element
	/// (via std::vector<uint64_t>) instead of a whole byte -- for a genuinely binary
	/// image (e.g. a threshold() result) where memory is the concern. Not derived
	/// from Image; see the full comment below for why, and for how it shares
	/// erode()/dilate()/median_filter()/threshold() with Image via the free
	/// functions in morphology.h.
	///
	/// Not a drop-in replacement for Image<bool,DIM> -- the two are meant for
	/// different situations. Use pack()/unpack() to convert between them: pack()
	/// an Image<bool,DIM> (or any minimal-interface image) down into a compact
	/// PackedBitImage once it's final, unpack() a PackedBitImage back out when
	/// something needs real addressable elements (e.g. to hand to code that's
	/// generic over Image<T,DIM>).
	///
	/// @tparam DIM Number of dimensions (>= 1).
	/// @ingroup core_image
	template<int DIM>
	class PackedBitImage
	{
	public:
		using value_type = bool;

		// Proxy reference to a single bit -- the mutable return type of the
		// non-const at()/operator(), standing in for the "bool&" that can't
		// exist over packed storage. Converts to bool for reads; assignment
		// writes the bit in place via read-modify-write on its word.
		/// Proxy reference to a single bit, standing in for the "bool&" that cannot
		/// exist over packed storage -- the mutable return type of at()/operator().
		/// Converts implicitly to bool for reads; `operator=(bool)` writes the bit
		/// in place via read-modify-write on its 64-bit word.
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

		/// Allocates a fresh, all-zero bit array of the given shape.
		/// @param extent Shape: element count along each dimension.
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

		/// Reads the bit at the given N-dimensional coordinate.
		/// @param coord Coordinate, one component per dimension.
		/// @return The bit's value.
		bool at(const std::array<int, DIM>& coord) const
		{
			std::size_t idx = flatIndex(coord);
			return (words_[idx / 64] >> (idx % 64)) & std::uint64_t(1);
		}
		/// Mutable access to the bit at the given N-dimensional coordinate.
		/// @param coord Coordinate, one component per dimension.
		/// @return A BitRef proxy standing in for a real bool&; assign a bool to it to write the bit.
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

		/// Every N-dimensional coordinate this image covers, in row-major order.
		/// @return One entry per element, in the same order the storage is packed.
		std::vector<std::array<int, DIM>> coordinates() const { return detail::coordinatesOf(extent_); }

		// Count of set bits -- the natural whole-image reduction for a binary
		// image (e.g. counting foreground pixels after a threshold()). Words
		// beyond the last valid bit are always 0 (nothing ever sets them --
		// every write goes through at(), which only ever produces indices <
		// size()), so no masking is needed here.
		/// Number of set (true) bits.
		/// @return Count of true bits, in [0, size()].
		std::size_t count() const
		{
			std::size_t total = 0;
			for (auto w : words_) total += std::bitset<64>(w).count();
			return total;
		}
		/// True if at least one bit is set.
		/// @return `count() > 0`.
		bool any() const { return count() > 0; }
		/// True if every bit is set.
		/// @return `count() == size()`.
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

	// PackedBitImage must satisfy the same minimal structural interface
	// (extent()/at()/coordinates()) Image does -- that's the whole reason
	// ndl::erode()/ndl::dilate()/ndl::median_filter()/ndl::threshold()
	// (morphology.h) run against it unmodified. One representative
	// instantiation is enough: the checked members' shapes don't vary with
	// DIM.
	static_assert(detail::satisfies_minimal_interface_v<PackedBitImage<2>, 2>,
		"PackedBitImage<DIM> no longer satisfies the minimal structural interface (extent()/at()/coordinates()) "
		"morphology.h's free functions require -- check for a renamed or reshaped member.");

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
	/// Converts any minimal-interface image into a freshly-allocated PackedBitImage, nonzero -> true.
	/// @tparam ImageT Any type exposing extent()/at(coord)/coordinates() -- Image<T,DIM>, OwnedImage<T,DIM>, etc.
	/// @param  src    Source image; DIM is read off `src.extent()`'s own array size, never named explicitly.
	/// @return A new PackedBitImage<DIM> with `src`'s extent, one bit per element (true where `src`'s element != 0).
	/// @ingroup core_image
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
	// threshold() (morphology.h) itself defaults to.
	/// Expands a PackedBitImage back into a caller-owned Image<T,DIM>: `onValue`/`offValue` for true/false.
	/// @tparam T        dst's element type.
	/// @tparam DIM      Number of dimensions.
	/// @param  src      Source bits.
	/// @param  dst      Destination; must already exist with `src`'s own extent.
	/// @param  onValue  Value written where the source bit is true. Defaults to T(1).
	/// @param  offValue Value written where the source bit is false. Defaults to T(0).
	/// @ingroup core_image
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
	/// Prints a PackedBitImage as one '0'/'1' character per bit, one line per row (DIM 1/2 only -- higher DIM still works, just without per-row line breaks).
	/// @param sb Stream to write to.
	/// @param r  Image to print.
	/// @return `sb`, for chaining.
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
