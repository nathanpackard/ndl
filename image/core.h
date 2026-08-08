#pragma once
#include <cassert>
#include <sstream>
#include <numeric>
#include <vector>
#include <array>
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <cmath>
#include <iterator>
#include <type_traits>
#include "border_mode.h"
#include "detail.h"
#include "algorithms.h"
#include "../mathHelpers.h"

namespace ndl
{
	template <class T, int DIM>
	class Image
	{
	public:
		friend class Image<T, DIM + 1>;
		friend class Image<T, DIM - 1>;

		typedef T value_type;

		// A single iterator template serves both iterator and const_iterator,
		// distinguished by IsConst -- the same pattern std::vector's implementation
		// uses, so begin()/end() can overload on *this's constness the way every
		// standard container does (non-const object -> iterator, const object ->
		// const_iterator, both walking the same n-dimensional path).
		template<bool IsConst>
		class basic_iterator
		{
		public:
			using self_type = basic_iterator;
			using value_type = T;
			using difference_type = std::ptrdiff_t;
			using pointer = std::conditional_t<IsConst, const T*, T*>;
			using reference = std::conditional_t<IsConst, const T&, T&>;
			using iterator_category = std::forward_iterator_tag;

			basic_iterator(const Image& parent, int last) : index{}, _ptr(parent.data_), _image(parent)
			{
				index.back() = last;
			}

			self_type& operator++() {
				if ((index[0] += _image.stride_[0]) != _image.end_[0])
				{
					return *this;
				}
				if (DIM > 1 && ++index[1] < _image.extent_[1])
				{
					index[0] = 0;
					_ptr += _image.stride_[1];
					return *this;
				}
				if (DIM > 2 && ++index[2] < _image.extent_[2])
				{
					index[0] = 0;
					index[1] = 0;
					_ptr += _image.stride_[2] - _image.stride_[1] * (_image.extent_[1] - 1);
					return *this;
				}
				// _ptr always equals sum_{k=1}^{DIM-1} stride_[k]*index_k (dimension 0's
				// contribution lives in index[0] itself, per the comment on operator==
				// below). When dimension p wraps, every inner dimension 1..p-1 has just
				// cycled through its full extent, so _ptr has accumulated their full drift
				// (stride_[k]*(extent_[k]-1) for every k < p) on top of dimension p's own
				// value -- all of that must be undone before adding one clean step of
				// dimension p. The p==1 and p==2 cases above are this same formula with
				// the (empty, and single-term) sum written out by hand for speed; this
				// general loop just doesn't get to skip the arithmetic.
				for (int p = 3; p < DIM; p++)
				{
					if (++index[p] < _image.extent_[p])
					{
						for (int q = 0; q < p; q++) index[q] = 0;
						int delta = _image.stride_[p];
						for (int k = 1; k < p; k++) delta -= _image.stride_[k] * (_image.extent_[k] - 1);
						_ptr += delta;
						return *this;
					}
				}
				return *this;
			}
			self_type operator++(int) {
				self_type old = *this;
				++(*this);
				return old;
			}

			reference operator*() const { return _ptr[index[0]]; }
			pointer operator->() const { return _ptr + index[0]; }
			// raw pointer to the element currently referenced, matching the
			// std smart-pointer convention (unique_ptr::get(), shared_ptr::get())
			pointer get() const { return _ptr + index[0]; }

			//low level index based relative accessor
			reference operator[](difference_type offset) const { return _ptr[index[0] + offset]; }

			//relative reflection accessors (i.e. overruns will reflect back into the image)
			reference reflect(int delta, int dimensionIndex) const
			{
				return _ptr[index[0] + (_reflect(_image.extent_[dimensionIndex], index[dimensionIndex] + delta) - index[dimensionIndex]) * _image.stride_[dimensionIndex]];
			}
			reference reflect(std::array<int, DIM> delta) const
			{
				int i2 = 0;
				for (int i = 0; i < DIM; i++) i2 += _ptr[index[0] + (_reflect(_image.extent_[i], index[i] + delta[i]) - index[i]) * _image.stride_[i]];
				return _ptr[i2];
			}
			template<int D>
			reference reflect(std::array<int, D>& delta, const std::array<int, D>& dimensionIndices) const
			{
				int i2 = 0;
				for (int i = 0; i < D; i++) {
					int ii = dimensionIndices[i];
					i2 += _ptr[index[ii] + (_reflect(_image.extent_[ii], index[ii] + delta[ii]) - index[ii]) * _image.stride_[ii]];
				}
				return _ptr[i2];
			}

			//relative clamping accessors (i.e. overruns will clip to the edge of the image)
			reference clamp(int delta, int dimensionIndex) const
			{
				return _ptr[index[0] + (_clamp(_image.extent_[dimensionIndex], index[dimensionIndex] + delta) - index[dimensionIndex]) * _image.stride_[dimensionIndex]];
			}
			reference clamp(std::array<int, DIM> delta) const
			{
				int i2 = 0;
				for (int i = 0; i < DIM; i++) i2 += _ptr[index[0] + (_clamp(_image.extent_[i], index[i] + delta[i]) - index[i]) * _image.stride_[i]];
				return _ptr[i2];
			}
			template<int D>
			reference clamp(std::array<int, D>& delta, const std::array<int, D>& dimensionIndices) const
			{
				int i2 = 0;
				for (int i = 0; i < D; i++) {
					int ii = dimensionIndices[i];
					i2 += _ptr[index[ii] + (_clamp(_image.extent_[ii], index[ii] + delta[ii]) - index[ii]) * _image.stride_[ii]];
				}
				return _ptr[i2];
			}

			//relative wrapping accessors (i.e. overruns will wrap around the edge of the image to the other edge)
			reference wrap(int delta, int dimensionIndex) const
			{
				return _ptr[index[0] + (_wrap(_image.extent_[dimensionIndex], index[dimensionIndex] + delta) - index[dimensionIndex]) * _image.stride_[dimensionIndex]];
			}
			reference wrap(std::array<int, DIM> delta) const
			{
				int i2 = 0;
				for (int i = 0; i < DIM; i++) i2 += _ptr[index[0] + (_wrap(_image.extent_[i], index[i] + delta[i]) - index[i]) * _image.stride_[i]];
				return _ptr[i2];
			}
			template<int D>
			reference wrap(std::array<int, D>& delta, const std::array<int, D>& dimensionIndices) const
			{
				int i2 = 0;
				for (int i = 0; i < D; i++) {
					int ii = dimensionIndices[i];
					i2 += _ptr[index[ii] + (_wrap(_image.extent_[ii], index[ii] + delta[ii]) - index[ii]) * _image.stride_[ii]];
				}
				return _ptr[i2];
			}

			// Dimension 0's `index[0]` is a raw pointer delta (it walks 0, stride_[0],
			// 2*stride_[0], ... so operator*/operator[] can dereference without a
			// multiply), while every other dimension's `index[dim]` is a plain
			// logical coordinate (0, 1, 2, ..., extent_[dim]-1). Comparing the
			// outermost (.back()) dimension against extent_.back() is only correct
			// when that's a logical coordinate -- i.e. DIM > 1. For a 1D image,
			// dimension 0 *is* the outermost dimension, so the terminal value to
			// compare against is end_[0] (a pointer delta), not extent_[0]: with a
			// decimating stride, index[0] never lands exactly on extent_[0] (e.g.
			// stride 2 walks 0,2,4,... which skips right past an odd extent_[0]),
			// so using extent_[0] here would make end() unreachable -- an infinite
			// loop that walks off the end of the buffer.
			bool operator==(const self_type& rhs) const {
				if constexpr (DIM == 1) return index[0] == _image.end_[0];
				else return index.back() == _image.extent_.back();
			}
			bool operator!=(const self_type& rhs) const { return !(*this == rhs); }

			std::array<int, DIM> index; // n-dimensional coordinate of the current location
		private:
			pointer _ptr;                // pointer to current location
			const Image& _image;         // reference to current image
		};
		using iterator = basic_iterator<false>;
		using const_iterator = basic_iterator<true>;

		// deep copy constructor
		template<class U>
		Image(T* buffer, const Image<U,DIM>& source) : Image(buffer, source.extent())
		{
			auto sourceIt = source.begin();
			for (auto it = begin(); it != end(); ++it, ++sourceIt) *it = *sourceIt;
		}

		// construct from external memory, be sure you have enough space!!
		Image(T* buffer, std::array<int, DIM> extent) :
			extent_(extent),
			stride_{ makeStride(extent)},
			end_{ makeEnd(extent_, stride_) },
			offset_{ },
			root_data_{ buffer },
			data_{ buffer }
		{ }

		static std::size_t size(std::array<int, DIM> extent)
		{
			return std::accumulate(extent.begin(), extent.end(), std::size_t(1), std::multiplies<std::size_t>());
		}

		// An explicit same-type overload is required alongside the templated
		// one below, not just style: extent_/stride_/etc. are const members,
		// so the compiler-generated copy-assignment operator is implicitly
		// deleted -- and for T==U, that non-template deleted operator beats
		// the template in overload resolution (a non-template candidate wins
		// ties against a template), so without this, `Image<T,DIM> a = b;`
		// (same T on both sides) would select the deleted one and fail to
		// compile, even though the template below does exactly what's needed.
		Image& operator=(const Image& rhs) {
			assert(rhs.extent() == extent_);
			auto rhsIt = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsIt) *it = *rhsIt;
			return *this;
		}
		template<class U>
		Image& operator=(const Image<U,DIM> &rhs) {
			assert(rhs.extent() == extent_);
			auto rhsIt = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsIt) *it = *rhsIt;
			return *this;
		}
		// Every "scalar" overload below (of operator=, comparisons, the
		// compound-assignment operators, and add()/subtract()/multiply()/
		// divide()) is constrained to exclude Image-like U -- see
		// detail::is_image_like's comment for why: without it, passing an
		// OwnedImage argument (which IS-A Image, but is also its own
		// concrete type) prefers these "scalar" overloads over the sibling
		// Image-taking ones, which is never what's meant.
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>>
		Image& operator=(U rhs) {
			for (auto it = begin(); it != end(); ++it)
				*it = rhs;
			return *this;
		}
		template <class U> bool operator== (const Image<U, DIM>& rhs) const {
			if (rhs.extent() != extent_) return false;
			auto rhsIt = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsIt) if (*it != *rhsIt) return false;
			return true;
		}
		template <class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> bool operator== (const U& rhs) const {
			for (auto it = begin(); it != end(); ++it) if (*it != rhs) return false;
			return true;
		}
		// Every ordering comparison below (<, <=, >, >=) requires T to support
		// a real total order (<, >) -- e.g. std::complex has neither, so
		// comparing an Image<std::complex<double>,DIM> would otherwise fail
		// deep inside these loops with a cryptic "no match for operator<"
		// several template-instantiation frames down. std::is_arithmetic_v
		// covers exactly bool/integral/floating-point T -- the types C++
		// itself guarantees a total order for -- and excludes std::complex
		// and any other non-ordered element type, giving a single clear
		// error at the actual call site instead.
		template <class U> bool operator<  (const Image<U, DIM>& rhs) const {
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::operator< requires an arithmetic T (needs a total order) -- not valid for e.g. std::complex<T>");
			assert(rhs.extent() == extent_);
			auto rhsIt = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsIt) if (*it >= *rhsIt) return false;
			return true;
		}
		template <class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> bool operator<  (const U& rhs) const {
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::operator< requires an arithmetic T (needs a total order) -- not valid for e.g. std::complex<T>");
			for (auto it = begin(); it != end(); ++it) if (*it >= rhs) return false;
			return true;
		}
		void negate() { mutableUnaryOp<std::negate<T>>(); }
		template<class U> Image& operator+=(const Image<U, DIM>& rhs) { return mutableBinaryImageOp<std::plus<T>>(rhs); }
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> Image& operator+=(const U rhs) { return mutableBinaryScalarOp<std::plus<T>>(rhs); }
		template<class U> Image& operator-=(const Image<U, DIM>& rhs) { return mutableBinaryImageOp<std::minus<T>>(rhs); }
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> Image& operator-=(const U rhs) { return mutableBinaryScalarOp<std::minus<T>>(rhs); }
		template<class U> Image& operator*=(const Image<U, DIM>& rhs) { return mutableBinaryImageOp<std::multiplies<T>>(rhs); }
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> Image& operator*=(const U rhs) { return mutableBinaryScalarOp<std::multiplies<T>>(rhs); }
		template<class U> Image& operator/=(const Image<U, DIM>& rhs) { return mutableBinaryImageOp<std::divides<T>>(rhs); }
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> Image& operator/=(const U rhs) { return mutableBinaryScalarOp<std::divides<T>>(rhs); }
		// %=/&=/|=/^= need an integral T -- modulus and bitwise ops aren't
		// defined for float/double at all (this fails to compile today
		// too, just from deep inside std::modulus<double>::operator() with
		// no indication *why* -- this puts the real reason at the actual
		// call site). is_integral_v<bool> is true, so these still work for
		// a plain bool mask Image (AND/OR/XOR of two masks is meaningful);
		// is_integral_v<float/double> is false.
		template<class U> Image& operator%=(const Image<U, DIM>& rhs) { static_assert(std::is_integral_v<T>, "Image<T,DIM>::operator%= requires an integral T -- modulus isn't defined for floating-point types"); return mutableBinaryImageOp<std::modulus<T>>(rhs); }
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> Image& operator%=(const U rhs) { static_assert(std::is_integral_v<T>, "Image<T,DIM>::operator%= requires an integral T -- modulus isn't defined for floating-point types"); return mutableBinaryScalarOp<std::modulus<T>>(rhs); }
		template<class U> Image& operator|=(const Image<U, DIM>& rhs) { static_assert(std::is_integral_v<T>, "Image<T,DIM>::operator|= requires an integral T -- bitwise or isn't defined for floating-point types"); return mutableBinaryImageOp<std::bit_or<T>>(rhs); }
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> Image& operator|=(const U rhs) { static_assert(std::is_integral_v<T>, "Image<T,DIM>::operator|= requires an integral T -- bitwise or isn't defined for floating-point types"); return mutableBinaryScalarOp<std::bit_or<T>>(rhs); }
		template<class U> Image& operator&=(const Image<U, DIM>& rhs) { static_assert(std::is_integral_v<T>, "Image<T,DIM>::operator&= requires an integral T -- bitwise and isn't defined for floating-point types"); return mutableBinaryImageOp<std::bit_and<T>>(rhs); }
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> Image& operator&=(const U rhs) { static_assert(std::is_integral_v<T>, "Image<T,DIM>::operator&= requires an integral T -- bitwise and isn't defined for floating-point types"); return mutableBinaryScalarOp<std::bit_and<T>>(rhs); }
		template<class U> Image& operator^=(const Image<U, DIM>& rhs) { static_assert(std::is_integral_v<T>, "Image<T,DIM>::operator^= requires an integral T -- bitwise xor isn't defined for floating-point types"); return mutableBinaryImageOp<std::bit_xor<T>>(rhs); }
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> Image& operator^=(const U rhs) { static_assert(std::is_integral_v<T>, "Image<T,DIM>::operator^= requires an integral T -- bitwise xor isn't defined for floating-point types"); return mutableBinaryScalarOp<std::bit_xor<T>>(rhs); }
		// logical_not() needs T contextually convertible to bool -- true for
		// every arithmetic type (including bool itself) but not for
		// std::complex, which has no operator bool().
		void logical_not() { static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::logical_not() requires an arithmetic T -- std::complex has no contextual bool conversion"); mutableUnaryOp<std::logical_not<T>>(); }

		// Non-mutating arithmetic: writes the result into a caller-provided
		// output image instead of allocating one. A true operator+ returning
		// a new Image would need to allocate memory somewhere, which
		// conflicts with this class's whole design (it never owns memory --
		// the caller always provides the buffer at construction); these are
		// the explicit equivalent. output must share *this's extent.
		template<class U> void add(const Image<U, DIM>& rhs, Image<T, DIM>& output) const { binaryImageOp<std::plus<T>>(rhs, output); }
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> void add(const U rhs, Image<T, DIM>& output) const { binaryScalarOp<std::plus<T>>(rhs, output); }
		template<class U> void subtract(const Image<U, DIM>& rhs, Image<T, DIM>& output) const { binaryImageOp<std::minus<T>>(rhs, output); }
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> void subtract(const U rhs, Image<T, DIM>& output) const { binaryScalarOp<std::minus<T>>(rhs, output); }
		template<class U> void multiply(const Image<U, DIM>& rhs, Image<T, DIM>& output) const { binaryImageOp<std::multiplies<T>>(rhs, output); }
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> void multiply(const U rhs, Image<T, DIM>& output) const { binaryScalarOp<std::multiplies<T>>(rhs, output); }
		template<class U> void divide(const Image<U, DIM>& rhs, Image<T, DIM>& output) const { binaryImageOp<std::divides<T>>(rhs, output); }
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> void divide(const U rhs, Image<T, DIM>& output) const { binaryScalarOp<std::divides<T>>(rhs, output); }
		template<class U> bool operator!= (const Image<U, DIM>& rhs) const { return !(*this == rhs); }
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> bool operator!= (const U& rhs) const { return !(*this == rhs); }
		// <= and >= are deliberately NOT "!(the strict opposite)" (e.g. `<=`
		// as `!(rhs < *this)`) the way a scalar total order would let you
		// define them -- for an elementwise/array comparison, that De
		// Morgan trick silently swaps "every element satisfies" for "at
		// least one element satisfies", since negating an ALL-quantified
		// relation doesn't give you the ALL-quantified relation of the
		// opposite strictness (it gives you an EXISTS-quantified one). E.g.
		// a=[1,5], b=[2,3]: a<=b should be false (a[1]=5 > b[1]=3), but
		// !(b<a) evaluates to true, because b<a itself is already false (at
		// index 0) and its negation says nothing about index 1. So each is
		// spelled out directly below, same ALL-quantified shape as operator<
		// above. (operator> above, by contrast, IS safe as `(rhs < *this)`:
		// swapping operands and flipping strictness preserves ALL-
		// quantification, unlike negating it.) The scalar overloads need
		// the same is_image_like guard as operator< for the same reason:
		// without it, an OwnedImage argument would prefer this "scalar"
		// overload's now-direct-comparison body, which doesn't compile for
		// an Image-shaped rhs, over the sibling Image-taking overload that
		// would work fine.
		template<class U> bool operator<= (const Image<U, DIM>& rhs) const {
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::operator<= requires an arithmetic T (needs a total order) -- not valid for e.g. std::complex<T>");
			assert(rhs.extent() == extent_);
			auto rhsIt = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsIt) if (*it > *rhsIt) return false;
			return true;
		}
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> bool operator<= (const U& rhs) const {
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::operator<= requires an arithmetic T (needs a total order) -- not valid for e.g. std::complex<T>");
			for (auto it = begin(); it != end(); ++it) if (*it > rhs) return false;
			return true;
		}
		template<class U> bool operator>  (const Image<U, DIM>& rhs) const {
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::operator> requires an arithmetic T (needs a total order) -- not valid for e.g. std::complex<T>");
			return (rhs < *this);
		}
		template<class U> bool operator>  (const U& rhs) const {
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::operator> requires an arithmetic T (needs a total order) -- not valid for e.g. std::complex<T>");
			return (rhs < *this);
		}
		template<class U> bool operator>= (const Image<U, DIM>& rhs) const {
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::operator>= requires an arithmetic T (needs a total order) -- not valid for e.g. std::complex<T>");
			assert(rhs.extent() == extent_);
			auto rhsIt = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsIt) if (*it < *rhsIt) return false;
			return true;
		}
		template<class U, class = std::enable_if_t<!detail::is_image_like<U>::value>> bool operator>= (const U& rhs) const {
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::operator>= requires an arithmetic T (needs a total order) -- not valid for e.g. std::complex<T>");
			for (auto it = begin(); it != end(); ++it) if (*it < rhs) return false;
			return true;
		}
		std::size_t size() const { return std::accumulate(extent_.begin(), extent_.end(), std::size_t(1), std::multiplies<std::size_t>()); }

		// Whole-image reductions: fold every element down to a single scalar.
		// mean() returns double regardless of T so an integer image doesn't
		// silently truncate; the others keep T since sum/min/max of T values
		// are themselves meaningfully a T (matching the caller's own domain).
		T sum() const {
			T total{};
			for (auto it = begin(); it != end(); ++it) total = static_cast<T>(total + *it);
			return total;
		}
		// min()/max() need ordering; mean() converts through double. sum()
		// (here and per-axis below) needs neither -- only +, which
		// std::complex supports fine -- so it's deliberately left
		// unrestricted, unlike its min/max/mean siblings.
		T min() const {
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::min() requires an arithmetic T (needs a total order) -- not valid for e.g. std::complex<T>");
			assert(size() > 0);
			auto it = begin();
			T result = *it;
			for (++it; it != end(); ++it) if (*it < result) result = *it;
			return result;
		}
		T max() const {
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::max() requires an arithmetic T (needs a total order) -- not valid for e.g. std::complex<T>");
			assert(size() > 0);
			auto it = begin();
			T result = *it;
			for (++it; it != end(); ++it) if (*it > result) result = *it;
			return result;
		}
		double mean() const {
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::mean() requires a T convertible to double -- not valid for e.g. std::complex<T>; per-axis mean(axis,output) has no such restriction, since it stays in T throughout");
			assert(size() > 0);
			double total = 0;
			for (auto it = begin(); it != end(); ++it) total += static_cast<double>(*it);
			return total / static_cast<double>(size());
		}

		// Per-axis reductions (numpy's keepdims=True convention): output must
		// already exist with *this's own extent except extent 1 along `axis`,
		// so the result can be broadcast back against *this without reshaping.
		// Caller owns output's memory, same as every other operation here.
		// min(axis,...)/max(axis,...) need ordering, same reasoning as the
		// whole-image versions above; sum(axis,...)/mean(axis,...) don't
		// (mean(axis,...) divides by a T-converted count and stays in T the
		// whole way, unlike whole-image mean() above -- verified this
		// actually compiles for std::complex before leaving it unrestricted).
		void sum(int axis, Image<T, DIM>& output) const { axisReduceOp(axis, output, std::plus<T>{}); }
		void min(int axis, Image<T, DIM>& output) const {
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::min(axis,output) requires an arithmetic T (needs a total order) -- not valid for e.g. std::complex<T>");
			axisReduceOp(axis, output, [](T a, T b) { return b < a ? b : a; });
		}
		void max(int axis, Image<T, DIM>& output) const {
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::max(axis,output) requires an arithmetic T (needs a total order) -- not valid for e.g. std::complex<T>");
			axisReduceOp(axis, output, [](T a, T b) { return b > a ? b : a; });
		}

			// A third min()/max() overload, alongside the whole-image and per-axis
			// ones above: a *local* min/max, taken over the neighborhood `kernel`
			// marks (same "kernel centered on every output position" walk
			// convolve() uses). This is exactly morphological erosion/dilation,
			// so both just forward to the free ndl::erode()/ndl::dilate()
			// functions below (see their comment) -- "windowed min/max" and
			// "erode/dilate" are both standard vocabulary for the identical
			// operation, and which one reads clearer depends on context.
			template<class K>
			void min(const Image<K, DIM>& kernel, Image<T, DIM>& output, BorderMode border = BorderMode::Clamp) const { ndl::erode(*this, output, kernel, border); }
			template<class K>
			void max(const Image<K, DIM>& kernel, Image<T, DIM>& output, BorderMode border = BorderMode::Clamp) const { ndl::dilate(*this, output, kernel, border); }

		void mean(int axis, Image<T, DIM>& output) const {
			sum(axis, output);
			T count = static_cast<T>(extent_[axis]);
			for (auto it = output.begin(); it != output.end(); ++it) *it = static_cast<T>(*it / count);
		}

		// human readable dump of the view's internal bookkeeping, useful when debugging strides/offsets
		std::string to_string() const
		{
			std::ostringstream sb;
			sb << "          data_ : " << long(data_ - root_data_) << std::endl;
			for (int i = 0; i < DIM; i++) sb << "          offset_" << i << " : " << offset_[i] << std::endl;
			for (int i = 0; i < DIM; i++) sb << "          end_" << i << " : " << end_[i] << std::endl;
			for (int i = 0; i < DIM; i++) sb << "          stride_" << i << " : " << stride_[i] << std::endl;
			for (int i = 0; i < DIM; i++) sb << "          extent_" << i << " : " << extent_[i] << std::endl;
			return sb.str();
		}

		//iterator methods -- overloaded on *this's constness like every standard container
		iterator begin() { return iterator(*this, 0); }
		const_iterator begin() const { return const_iterator(*this, 0); }
		iterator end() { return iterator(*this, extent_.back()); }
		const_iterator end() const { return const_iterator(*this, extent_.back()); }

		//basic accessors
		T& at(const std::array<int, DIM>& index) { return data_[std::inner_product(index.begin(), index.end(), stride_.begin(), 0)]; }
		const T& at(const std::array<int, DIM>& index) const { return data_[std::inner_product(index.begin(), index.end(), stride_.begin(), 0)]; }

		// element access, Eigen/Matrix-style: image(x, y, z). Requires exactly DIM
		// integer indices, so it can never be confused with view() below, whose
		// arguments are always braced lists (a plain int can't implicitly become
		// an initializer_list<int> the way a brace-init like {1,2} can).
		template<class... Args>
		T& operator()(Args... indices) {
			static_assert(sizeof...(Args) == DIM, "Image::operator() needs exactly DIM integer indices for element access");
			return at({ static_cast<int>(indices)... });
		}
		template<class... Args>
		const T& operator()(Args... indices) const {
			static_assert(sizeof...(Args) == DIM, "Image::operator() needs exactly DIM integer indices for element access");
			return at({ static_cast<int>(indices)... });
		}

		// Build a new view of the same memory: a region of interest, a mirror, a
		// decimation, or any combination -- expressed per dimension as
		// {start, end, step}, all three optional (see the overloads below).
		//
		// Per dimension i, given (start, end, step):
		//   - start/end are both INCLUSIVE element indices into *this* image
		//     (not the underlying root buffer), so {2, 4} keeps 3 elements:
		//     indices 2, 3, and 4.
		//   - Either may be negative to count from the last index, Python-slice
		//     style: -1 is the last element, -2 the second-to-last, and so on.
		//     A negative start is resolved the same way, so {-3, -1} means "the
		//     last 3 elements" regardless of the dimension's extent.
		//   - After resolving negative indices, both must land in
		//     [0, extent(i)-1], and end must not fall before start -- there is
		//     no wraparound: a view is always a single contiguous-in-index
		//     (possibly strided) run, never a range that wraps past the last
		//     element back to the first. Values outside that -- e.g. asking for
		//     {5} on a dimension of extent 3, or an end before its start, like
		//     {5},{2} -- throw std::out_of_range rather than silently reading
		//     or writing past the buffer.
		//   - step's magnitude decimates. A positive step walks forward from
		//     start: |step| = 2 keeps every other element starting at start
		//     (start, start+2, start+4, ..., stopping at or before end). A
		//     negative step mirrors by walking backward from END instead:
		//     end, end-2, end-4, ..., stopping at or before start. Either way
		//     the resulting extent is ceil((end - start + 1) / |step|),
		//     generally NOT end - start + 1, and the walk is guaranteed to
		//     stay within [start, end] -- no wraparound is ever needed, since
		//     that's exactly what the ceil'd element count guarantees.
		//   - Because the two directions anchor at opposite ends of the range,
		//     a negative step is NOT simply "the positive-step elements in
		//     reverse order" once |step| > 1: it's a differently-anchored
		//     decimation grid, so it generally selects different physical
		//     elements. E.g. view({1},{8},{2}) (extent 8, magnitude 2) selects
		//     1,3,5,7 (anchored at start=1); view({1},{8},{-2}) over the same
		//     range selects 8,6,4,2 (anchored at end=8) -- a disjoint set, not
		//     a reordering. At step magnitude 1 (no decimation) there's only
		//     one element per position either way, so a negative step is
		//     exactly a plain reverse, as you'd expect.
		//   - step of 0 is invalid and throws.
		//
		// Any dimension omitted from a list (including entire trailing lists,
		// e.g. calling view({0,1}) on a 3D image) keeps its default: full
		// range ({0, extent(i)-1}) at step 1. Combine start/end (ROI) with a
		// negative and/or >1-magnitude step in the same call to get a mirrored
		// and/or decimated ROI in one view, rather than composing two views.
		//
		// Every view() call is O(1) and shares the same underlying memory as
		// *this -- no elements are copied.
		Image<T, DIM> view(const std::initializer_list<int>& start) const
		{
			return view(start, {}, {});
		}
		Image<T, DIM> view(const std::initializer_list<int>& start, const std::initializer_list<int>& end) const
		{
			return view(start, end, {});
		}
		Image<T, DIM> view(
			const std::initializer_list<int>& start,
			const std::initializer_list<int>& end,
			const std::initializer_list<int>& step) const
		{
			std::array<int, DIM> newExtent;
			std::array<int, DIM> newOffset;
			std::array<int, DIM> newStride;
			std::array<bool, DIM> newMirror;

			auto startIt = start.begin();
			auto endIt = end.begin();
			auto stepIt = step.begin();

			for (int i = 0; i < DIM; i++)
			{
				newMirror[i] = false;

				// Apply default values where needed
				int s = (startIt != start.end() ? *startIt++ : 0);
				int e = (endIt != end.end() ? *endIt++ : -1);
				int st = (stepIt != step.end() ? *stepIt++ : 1);

				if (st == 0)
					throw std::out_of_range("Image::view: step must not be 0 (dimension " + std::to_string(i) + ")");

				// Negative start/end count from the last index, symmetrically.
				if (s < 0) s += extent_[i];
				if (e < 0) e += extent_[i];

				if (s < 0 || s >= extent_[i] || e < 0 || e >= extent_[i] || e < s)
					throw std::out_of_range(
						"Image::view: [start=" + std::to_string(s) + ", end=" + std::to_string(e) +
						"] is not a valid range for dimension " + std::to_string(i) +
						" (extent " + std::to_string(extent_[i]) + "); views may not wrap around");

				// A negative step anchors the view at `end` and walks backward instead of
				// picking a forward-stepped subset and reversing how it's presented. The
				// element count (extent, computed below via ceil) is unchanged either way,
				// and end - (count-1)*|step| is provably >= start (that's what ceil
				// guarantees), so this never needs to wrap: e, e-|step|, e-2|step|, ...
				// always stays inside [start, end]. At step magnitude 1 this lands on
				// exactly the same elements as before (a plain reverse); decimated with a
				// magnitude > 1, it anchors the decimation grid at `end` rather than
				// `start`, so a mirrored decimation is generally a different set of
				// elements than the non-mirrored decimation of the same range -- not just
				// the same elements in reverse order.
				newOffset[i] = (st < 0) ? e : s;
				newExtent[i] = 1 + e - s;
				newStride[i] = std::abs(st);

				if (st < 0)
					newMirror[i] = true;
			}

			return Image<T, DIM>(*this, newOffset, newExtent, newStride, newMirror);
		}

		Image<T, DIM - 1> slice(int sliceDimension, int sliceIndex) const
		{
			return Image<T, DIM - 1>(*this, sliceDimension, sliceIndex);
		}
		Image<T, DIM> swap_axes(int dimension1, int dimension2) const
		{
			std::array<int, DIM> newExtent;
			std::array<int, DIM> newOffset;
			std::array<int, DIM> newStride;
			std::array<bool, DIM> newMirror;
			for (int i = 0; i < DIM; i++)
			{
				newMirror[i] = false;
				newOffset[i] = 0;
				newExtent[i] = extent_[i];
				newStride[i] = 1;
			}
			return Image<T, DIM>(*this, newOffset, newExtent, newStride, newMirror, dimension1, dimension2);
		}
		Image<T, DIM> mirror(int dimension) const
		{
			// offset/stride below are relative to *this (0 = current start, 1 = no
			// decimation) -- the same convention swap_axes() above uses. Passing
			// offset_/stride_ (this view's already-absolute bookkeeping) here was a
			// bug: it double-applied the current offset and squared the stride for
			// any dimension where stride_ != 1, corrupting the extent and stride of
			// the mirrored view. The mirrored dimension's offset is its last valid
			// index (extent_[i]-1), matching view()'s "anchor at end" convention --
			// this is a full-range mirror, so start=0, end=extent_[i]-1 always.
			std::array<int, DIM> newExtent;
			std::array<int, DIM> newOffset;
			std::array<int, DIM> newStride;
			std::array<bool, DIM> newMirror;
			for (int i = 0; i < DIM; i++)
			{
				newMirror[i] = dimension == i;
				newOffset[i] = (dimension == i) ? (extent_[i] - 1) : 0;
				newExtent[i] = extent_[i];
				newStride[i] = 1;
			}
			return Image<T, DIM>(*this, newOffset, newExtent, newStride, newMirror);
		}
		// Applies `kernel` to *this via correlation (the kernel is not flipped --
		// the same convention OpenCV's filter2D uses, as opposed to signal
		// processing's flipped-kernel definition), writing a same-size result
		// into `output`, which must already exist with *this's own extent.
		// Kernel indices are centered: extent K along a dimension covers
		// offsets -(K/2) .. K-(K/2)-1 from the output element being computed,
		// so an odd-sized kernel (e.g. 3x3) reaches an equal number of
		// neighbors in each direction. `border` selects how an offset that
		// falls outside *this is handled, via the same _clamp/_wrap/_reflect
		// primitives the iterator's clamp()/wrap()/reflect() accessors use.
		template<class K>
		void convolve(const Image<K, DIM>& kernel, Image<T, DIM>& output, BorderMode border = BorderMode::Clamp) const
		{
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::convolve() requires a T convertible to double -- not valid for e.g. std::complex<T> (the weighted sum is accumulated in double)");
			assert(output.extent() == extent_);
			auto center = kernelCenter(kernel);
			auto taps = kernelIncludedTaps(kernel); // zero-weight taps contribute nothing to the sum, so skipping them is free

			for (const auto& coord : coordinates())
			{
				double total = 0;
				for (const auto& kCoord : taps)
					total += static_cast<double>(at(kernelTapCoord(coord, kCoord, center, border))) * static_cast<double>(kernel.at(kCoord));
				output.at(coord) = static_cast<T>(total);
			}
		}

			// Morphology: the classic "shrink bright regions" (erode) / "grow
			// bright regions" (dilate) operations are just names for the local
			// min()/max() overload above -- both forward to the free
			// ndl::erode()/ndl::dilate() functions (see their comment above,
			// near BorderMode) rather than repeating the walk here, so the
			// exact same code also runs against a PackedBitImage.
			template<class K>
			void erode(const Image<K, DIM>& kernel, Image<T, DIM>& output, BorderMode border = BorderMode::Clamp) const { ndl::erode(*this, output, kernel, border); }
			template<class K>
			void dilate(const Image<K, DIM>& kernel, Image<T, DIM>& output, BorderMode border = BorderMode::Clamp) const { ndl::dilate(*this, output, kernel, border); }

			// percentile_filter() walks the same "kernel centered on every output
			// position" pattern convolve() above does, but instead of combining
			// the values under the kernel, picks ONE of them: the window is
			// partially sorted (std::nth_element, average O(n) rather than a
			// full O(n log n) sort) and the value at nearest-rank
			// round(percentile/100 * (N-1)) of its N included values is kept.
			// percentile 0/50/100 are exactly min/median/max over the window --
			// median_filter() below is just this at 50 -- so this is a strict
			// generalization of erode()/median_filter()/dilate() rather than a
			// separate operation. Anything in between is a genuine "soft" version
			// of erode/dilate: percentile 10 shrinks bright regions like erode
			// but is more resistant to a single dark outlier pixel, since it
			// takes the 10th-ranked value rather than the strict minimum. Same
			// 0/1-mask "nonzero = included" convention as min/max above --
			// make_box_kernel()/make_cross_kernel() (image/kernels.h) work here too.
			// Unlike a mean/gaussian blur, the output is always one of the actual
			// input values (never an average), which is what makes median_filter()
			// specifically good at removing salt-and-pepper noise without
			// smearing edges. Forwards to the free ndl::percentile_filter(),
			// same reasoning as erode()/dilate() above.
			template<class K>
			void percentile_filter(const Image<K, DIM>& kernel, Image<T, DIM>& output, double percentile, BorderMode border = BorderMode::Clamp) const { ndl::percentile_filter(*this, output, kernel, percentile, border); }
			template<class K>
			void median_filter(const Image<K, DIM>& kernel, Image<T, DIM>& output, BorderMode border = BorderMode::Clamp) const { ndl::median_filter(*this, output, kernel, border); }

			// Binarizes *this into `output`: `onValue` where the source value is
			// greater than `thresholdValue`, `offValue` otherwise. Defaults to a
			// generic 0/1 mask -- the same "nonzero = included" convention
			// erode()/dilate()/median_filter()/percentile_filter()/convolve()
			// already use for kernels -- rather than assuming an 8-bit image;
			// pass onValue=T(255) explicitly for a directly-viewable black/white
			// image instead. Strictly greater-than, not >=, to match
			// otsu_threshold()'s own class split below (background is values <=
			// the threshold, foreground is values above it) -- `thresholdValue`
			// is usually its result, but any fixed cutoff works too. Forwards to
			// the free ndl::threshold(), same reasoning as erode()/dilate() above
			// -- e.g. thresholding straight into a PackedBitImage mask.
			void threshold(T thresholdValue, Image<T, DIM>& output, T onValue = T(1), T offValue = T(0)) const { ndl::threshold(*this, output, thresholdValue, onValue, offValue); }

		// Otsu's method: the value that maximizes between-class variance of
		// *this's own histogram -- equivalently, the split that separates
		// the two classes of values (below/above it) as cleanly as possible.
		// The standard automatic threshold for turning a grayscale image
		// into a binary one without picking a cutoff by hand (Otsu, N.,
		// 1979, "A threshold selection method from gray-level histograms").
		// Generic over T: the histogram spans *this's own [min(),max()]
		// range divided into `bins` equal-width buckets (default 256 --
		// enough resolution for classic 8-bit imagery, and a reasonable
		// default for anything else), rather than assuming any fixed range,
		// so this works the same way for uint8_t, int, float, or double
		// data. A noisy image widens both classes' spread and can shift
		// where they overlap, so Otsu still works on it directly, but
		// denoising first (e.g. median_filter()) generally finds a cleaner
		// split -- see demo/morphology.
		T otsu_threshold(int bins = 256) const
		{
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::otsu_threshold() requires an arithmetic T (needs a total order and a double conversion) -- not valid for e.g. std::complex<T>");
			assert(bins > 1);
			T lo = min();
			T hi = max();
			if (lo == hi) return lo; // degenerate: only one value present, nothing to split

			double range = static_cast<double>(hi) - static_cast<double>(lo);
			auto bucketOf = [&](T value) {
				int b = (int)((static_cast<double>(value) - static_cast<double>(lo)) / range * bins);
				return b < 0 ? 0 : (b >= bins ? bins - 1 : b);
			};

			std::vector<std::size_t> histogram(bins, 0);
			for (auto it = begin(); it != end(); ++it) histogram[bucketOf(*it)]++;

			std::size_t total = size();
			double sumAll = 0;
			for (int i = 0; i < bins; i++) sumAll += (double)i * histogram[i];

			std::size_t weightBackground = 0;
			double sumBackground = 0;
			double bestVariance = -1;
			int bestBucket = 0;
			for (int b = 0; b < bins; b++)
			{
				weightBackground += histogram[b];
				if (weightBackground == 0) continue;
				std::size_t weightForeground = total - weightBackground;
				if (weightForeground == 0) break;

				sumBackground += (double)b * histogram[b];
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
			double thresholdValue = static_cast<double>(lo) + range * static_cast<double>(bestBucket + 1) / static_cast<double>(bins);
			return static_cast<T>(thresholdValue);
		}

		// Gaussian blur: builds a normalized (weights sum to 1) Gaussian
		// kernel and hands it to convolve() above. Kernel radius follows the
		// standard 3-sigma rule (_kernelSize in mathHelpers.h), so larger
		// sigma automatically gets a wider kernel; the same sigma and radius
		// apply along every dimension. Blurring a color image channel-by-
		// channel (so colors don't bleed into each other) is a matter of
		// calling this on each channel's 2D slice rather than the 3D whole --
		// no special-casing needed here, since slice() already shares memory
		// with the original and convolve() is dimension-agnostic.
		void gaussian_blur(double sigma, Image<T, DIM>& output, BorderMode border = BorderMode::Clamp) const
		{
			static_assert(std::is_arithmetic_v<T>, "Image<T,DIM>::gaussian_blur() requires a T convertible to double -- not valid for e.g. std::complex<T> (it calls convolve() internally)");
			assert(sigma > 0);
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

			convolve(kernel, output, border);
		}

		std::vector<std::array<int, DIM>> coordinates() const { return detail::coordinatesOf(extent_); }

		const std::array<int, DIM>& extent() const { return extent_; }
		const std::array<int, DIM>& stride() const { return stride_; }

	protected:
		// protected helper constructor. Construct from image, shares same memory
		//
		// data_ is computed by stepping off source's OWN already-resolved data_
		// pointer, using source's OWN (signed) stride_ -- not by accumulating a
		// running offset from root_data_. That distinction matters as soon as an
		// intermediate view in the chain is mirrored (negative stride): "step
		// offset_[i] units in source's local index direction" means something
		// different depending on which way source's stride currently points,
		// and only source.data_/source.stride_ (source's fully-resolved state)
		// know that -- a cumulative offset_ measured against the root has no way
		// to account for a sign flip partway through the chain. (Concretely:
		// mirroring an already-mirrored view must land back on the original
		// data_, and it only does if each step composes off the immediately
		// preceding one rather than off a flattened root-relative offset.)
		Image(const Image<T, DIM>& source,
			  const std::array<int, DIM>& offset,
			  const std::array<int, DIM>& extent,
			  const std::array<int, DIM>& stride,
			  const std::array<bool, DIM>& mirror,
			  int swapDim1 = 0,
			  int swapDim2 = 0) :
			extent_{ makeExtent(extent, stride, swapDim1, swapDim2) },
			stride_{ makeStride(source.stride_, mirror, stride, swapDim1, swapDim2) },
			end_{ makeEnd(extent_, stride_) },
			offset_{ makeOffset(offset, swapDim1, swapDim2) },
			root_data_{ source.root_data_ },
			data_{ computeDataPtr(source.data_, source.stride_ ) }
		{ }

		// protected helper constructor. Construct from image, shares same memory, reduces dimension
		//
		// Slicing introduces no displacement for the dimensions it doesn't touch --
		// their indexing is untouched by fixing a different dimension to sliceIndex --
		// so, per the comment above, this steps off source.data_ by exactly
		// sliceIndex * source.stride_[sliceDimension] and nothing more.
		Image(const Image<T, DIM + 1>& source, int sliceDimension, int sliceIndex) :
			extent_{ makeExtentSlice(source.extent_, sliceDimension) },
			stride_{ makeStrideSlice(source.stride_, sliceDimension) },
			end_{ makeEnd(extent_, stride_) },
			offset_{ },
			root_data_{ source.root_data_ },
			data_{ source.data_ + sliceIndex * source.stride_[sliceDimension] }
		{ }

		// protected helper methods
		T* computeDataPtr(T* sourceData, const std::array<int,DIM>& sourceStride)
		{
			T* value = sourceData;
			for (int i = 0; i < DIM; i++) value += offset_[i] * sourceStride[i];
			return value;
		}
		std::array<int, DIM> makeStride(const std::array<int, DIM>& sourceStride, const std::array<bool, DIM>& mirror, const std::array<int, DIM>& stride, int swapDim1, int swapDim2)
		{
			std::array<int, DIM> result{};
			for (int i = 0; i < DIM; i++) result[i] = sourceStride[i] * (mirror[i] ? stride[i] * -1 : stride[i]);
			std::swap(result[swapDim1], result[swapDim2]);
			return result;
		}
		std::array<int, DIM> makeStrideSlice(const std::array<int, DIM + 1>& sourceStride, int sliceDimension)
		{
			std::array<int, DIM> result{};
			int t = 0;
			for (int i = 0; i < DIM + 1; i++)
			{
				if (i == sliceDimension) continue;
				result[t] = sourceStride[i];
				t++;
			}
			return result;
		}
		std::array<int, DIM> makeStride(const std::array<int, DIM>& extent)
		{
			std::array<int, DIM> result{};
			result[0] = 1;
			for (int i = 1; i < DIM; i++) result[i] = extent[i - 1] * result[i - 1];
			return result;
		}
		std::array<int, DIM> makeExtent(const std::array<int, DIM>& extent, const std::array<int, DIM>& stride, int swapDim1, int swapDim2)
		{
			std::array<int, DIM> result{};
			for (int i = 0; i < DIM; i++)
				result[i] = _ceil((float)extent[i] / _abs(stride[i]));
			std::swap(result[swapDim1], result[swapDim2]);
			return result;
		}
		std::array<int, DIM> makeExtentSlice(const std::array<int, DIM + 1>& sourceExtent, int sliceDimension)
		{
			std::array<int, DIM> result{};
			int t = 0;
			for (int i = 0; i < DIM + 1; i++)
			{
				if (i == sliceDimension) continue;
				result[t] = sourceExtent[i];
				t++;
			}
			return result;
		}
		// offset_ is purely this construction step's own local displacement (see
		// the constructor comment above) -- no accumulation from source needed.
		std::array<int, DIM> makeOffset(const std::array<int, DIM>& offset, int swapDim1, int swapDim2)
		{
			std::array<int, DIM> result = offset;
			std::swap(result[swapDim1], result[swapDim2]);
			return result;
		}
		std::array<int, DIM> makeEnd(const std::array<int, DIM>& newExtent, const std::array<int, DIM>& newStride)
		{
			std::array<int, DIM> result{};
			for (int i = 0; i < DIM; i++)
				result[i] = newExtent[i] * newStride[i];
			return result;
		}

		// Resolves the border-handled source coordinate for kernel tap
		// `kCoord` (centered via `center`) when computing the value at
		// `coord` -- shared by convolve()/erode()/dilate()/median_filter(),
		// all of which walk the same "kernel centered on every output
		// position" pattern and differ only in how they combine the values
		// found under the kernel. Forwards to the free detail::kernelTapCoord()
		// (see its comment, near BorderMode) so convolve() above shares the
		// exact same logic ndl::erode()/ndl::dilate()/etc. use.
		std::array<int, DIM> kernelTapCoord(const std::array<int, DIM>& coord, const std::array<int, DIM>& kCoord, const std::array<int, DIM>& center, BorderMode border) const
		{
			return detail::kernelTapCoord(coord, kCoord, center, extent_, border);
		}

		// Shared setup for every kernel-walking operation (convolve()/min()/
		// max()/percentile_filter()): the kernel's center, and its nonzero
		// ("included") taps, computed once per call rather than once per
		// output pixel -- see detail::kernelCenter()/kernelIncludedTaps()'s
		// own comment for why this matters. These just forward to those free
		// functions so convolve() shares the same logic as the free
		// ndl::erode()/ndl::dilate()/etc.
		template<class K>
		std::array<int, DIM> kernelCenter(const Image<K, DIM>& kernel) const { return detail::kernelCenter(kernel); }
		template<class K>
		std::vector<std::array<int, DIM>> kernelIncludedTaps(const Image<K, DIM>& kernel) const { return detail::kernelIncludedTaps(kernel); }

		template<class Op, class U>
		Image& mutableBinaryImageOp(const Image<U, DIM>& rhs) {
			assert(rhs.extent() == extent_);
			Op o;
			auto rhsIt = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsIt) *it = static_cast<T>(o(*it, *rhsIt));
			return *this;
		}
		template<class Op, class U>
		Image& mutableBinaryScalarOp(const U rhs) {
			Op o;
			for (auto it = begin(); it != end(); ++it) *it = static_cast<T>(o(*it, rhs));
			return *this;
		}
		template<class Op>
		Image& mutableUnaryOp() {
			Op o;
			for (auto it = begin(); it != end(); ++it) *it = static_cast<T>(o(*it));
			return *this;
		}
		template<class Op, class U>
		void binaryImageOp(const Image<U, DIM>& rhs, Image<T, DIM>& output) const {
			assert(rhs.extent() == extent_);
			assert(output.extent() == extent_);
			Op o;
			auto rhsIt = rhs.begin();
			auto outIt = output.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsIt, ++outIt) *outIt = static_cast<T>(o(*it, *rhsIt));
		}
		template<class Op, class U>
		void binaryScalarOp(const U rhs, Image<T, DIM>& output) const {
			assert(output.extent() == extent_);
			Op o;
			auto outIt = output.begin();
			for (auto it = begin(); it != end(); ++it, ++outIt) *outIt = static_cast<T>(o(*it, rhs));
		}

		// Shared by sum(axis,...)/min(axis,...)/max(axis,...): folds *this down
		// to output along `axis` using `op` as the pairwise combiner. Relies on
		// generateCoordinates()'s fixed nested-loop order (dimension DIM-1
		// outermost, dimension 0 innermost): for any fixed combination of the
		// non-`axis` dimensions, coordinates with index 0 along `axis` are
		// always produced before coordinates with a higher index along `axis`
		// (the `axis` loop is strictly outside every loop below it, so its
		// index-0 pass over the inner dimensions completes before index 1
		// starts) -- so a single pass can seed output on the first visit to
		// each output cell and fold every subsequent visit into it, with no
		// separate initialization pass and no dependency on T having an
		// identity element for `op`.
		template<class Op>
		void axisReduceOp(int axis, Image<T, DIM>& output, Op op) const {
			assert(axis >= 0 && axis < DIM);
			assert(output.extent()[axis] == 1);
			for (int i = 0; i < DIM; i++) assert(i == axis || output.extent()[i] == extent_[i]);

			for (const auto& coord : coordinates())
			{
				auto outCoord = coord;
				outCoord[axis] = 0;
				if (coord[axis] == 0) output.at(outCoord) = at(coord);
				else output.at(outCoord) = static_cast<T>(op(output.at(outCoord), at(coord)));
			}
		}

		// protected internal variables
		const std::array<int, DIM> extent_;   // extent of each dimension
		const std::array<int, DIM> stride_;   // stride of each dimension (linear memory skip factor)
		const std::array<int, DIM> end_;      // (one plus the last point for each dimension) * stride
		const std::array<int, DIM> offset_;
		T* root_data_;
		T* data_;
	};
}
