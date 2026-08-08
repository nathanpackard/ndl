#pragma once
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
#include <functional>
#include <iomanip>
#include <numeric>
#include <vector>
#include <array>
#include <algorithm>
#include <type_traits>
#include <cstddef>
#include <stdexcept>
#include <string>
#include "mathHelpers.h"

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
					index[0] = _image.start_[0];
					_ptr += _image.stride_[1];
					return *this;
				}
				if (DIM > 2 && ++index[2] < _image.extent_[2])
				{
					index[0] = _image.start_[0];
					index[1] = _image.start_[1];
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
						for (int q = 0; q < p; q++) index[q] = _image.start_[q];
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
			start_{ },
			root_data_{ buffer },
			data_{ buffer }
		{ }

		static std::size_t size(std::array<int, DIM> extent)
		{
			return std::accumulate(extent.begin(), extent.end(), std::size_t(1), std::multiplies<std::size_t>());
		}

		template<class U>
		Image& operator=(const Image<U,DIM> &rhs) {
			assert(rhs.extent() == extent_);
			auto rhsIt = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsIt) *it = *rhsIt;
			return *this;
		}
		template<class U>
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
		template <class U> bool operator== (const U& rhs) const {
			for (auto it = begin(); it != end(); ++it) if (*it != rhs) return false;
			return true;
		}
		template <class U> bool operator<  (const Image<U, DIM>& rhs) const {
			assert(rhs.extent() == extent_);
			auto rhsIt = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsIt) if (*it >= *rhsIt) return false;
			return true;
		}
		template <class U> bool operator<  (const U& rhs) const {
			for (auto it = begin(); it != end(); ++it) if (*it >= rhs) return false;
			return true;
		}
		void negate() { mutableUnaryOp<std::negate<T>>(); }
		template<class U> Image& operator+=(const Image<U, DIM>& rhs) { return mutableBinaryImageOp<std::plus<T>>(rhs); }
		template<class U> Image& operator+=(const U rhs) { return mutableBinaryScalarOp<std::plus<T>>(rhs); }
		template<class U> Image& operator-=(const Image<U, DIM>& rhs) { return mutableBinaryImageOp<std::minus<T>>(rhs); }
		template<class U> Image& operator-=(const U rhs) { return mutableBinaryScalarOp<std::minus<T>>(rhs); }
		template<class U> Image& operator*=(const Image<U, DIM>& rhs) { return mutableBinaryImageOp<std::multiplies<T>>(rhs); }
		template<class U> Image& operator*=(const U rhs) { return mutableBinaryScalarOp<std::multiplies<T>>(rhs); }
		template<class U> Image& operator/=(const Image<U, DIM>& rhs) { return mutableBinaryImageOp<std::divides<T>>(rhs); }
		template<class U> Image& operator/=(const U rhs) { return mutableBinaryScalarOp<std::divides<T>>(rhs); }
		template<class U> Image& operator%=(const Image<U, DIM>& rhs) { return mutableBinaryImageOp<std::modulus<T>>(rhs); }
		template<class U> Image& operator%=(const U rhs) { return mutableBinaryScalarOp<std::modulus<T>>(rhs); }
		template<class U> Image& operator|=(const Image<U, DIM>& rhs) { return mutableBinaryImageOp<std::bit_or<T>>(rhs); }
		template<class U> Image& operator|=(const U rhs) { return mutableBinaryScalarOp<std::bit_or<T>>(rhs); }
		template<class U> Image& operator&=(const Image<U, DIM>& rhs) { return mutableBinaryImageOp<std::bit_and<T>>(rhs); }
		template<class U> Image& operator&=(const U rhs) { return mutableBinaryScalarOp<std::bit_and<T>>(rhs); }
		template<class U> Image& operator^=(const Image<U, DIM>& rhs) { return mutableBinaryImageOp<std::bit_xor<T>>(rhs); }
		template<class U> Image& operator^=(const U rhs) { return mutableBinaryScalarOp<std::bit_xor<T>>(rhs); }
		void logical_not() { mutableUnaryOp<std::logical_not<T>>(); }
		template<class U> bool operator!= (const Image<U, DIM>& rhs) const { return !(*this == rhs); }
		template<class U> bool operator!= (const U& rhs) const { return !(*this == rhs); }
		template<class U> bool operator<= (const Image<U, DIM>& rhs) const { return !(rhs < *this); }
		template<class U> bool operator<= (const U& rhs) const { return !(rhs < *this); }
		template<class U> bool operator>  (const Image<U, DIM>& rhs) const { return (rhs < *this); }
		template<class U> bool operator>  (const U& rhs) const { return (rhs < *this); }
		template<class U> bool operator>= (const Image<U, DIM>& rhs) const { return !(*this < rhs); }
		template<class U> bool operator>= (const U& rhs) const { return !(*this < rhs); }
		std::size_t size() const { return std::accumulate(extent_.begin(), extent_.end(), std::size_t(1), std::multiplies<std::size_t>()); }

		// human readable dump of the view's internal bookkeeping, useful when debugging strides/offsets
		std::string to_string() const
		{
			std::ostringstream sb;
			sb << "          data_ : " << long(data_ - root_data_) << std::endl;
			for (int i = 0; i < DIM; i++) sb << "          start_" << i << " : " << start_[i] << std::endl;
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
		//   - step's magnitude decimates: |step| = 2 keeps every other element
		//     of the [start, end] range (element start, start+2, start+4, ...,
		//     stopping at or before end), so the resulting extent is
		//     ceil((end - start + 1) / |step|), generally NOT end - start + 1.
		//   - step's sign controls presentation order, independent of which
		//     elements decimation picked: step < 0 mirrors the dimension, i.e.
		//     the same elements chosen above are walked from last to first
		//     instead of first to last. It does not change *which* elements
		//     are included, only the order you iterate them in -- so e.g. for
		//     view({1},{8},{-2}) on a 1D image, the elements at indices
		//     1,3,5,7 are selected (start=1, step magnitude 2, up to end=8),
		//     then presented in the order 7,5,3,1.
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

				newOffset[i] = s;
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
			// the mirrored view.
			std::array<int, DIM> newExtent;
			std::array<int, DIM> newOffset;
			std::array<int, DIM> newStride;
			std::array<bool, DIM> newMirror;
			for (int i = 0; i < DIM; i++)
			{
				newMirror[i] = dimension == i;
				newOffset[i] = 0;
				newExtent[i] = extent_[i];
				newStride[i] = 1;
			}
			return Image<T, DIM>(*this, newOffset, newExtent, newStride, newMirror);
		}
		std::vector<std::array<int, DIM>> coordinates() const {
			std::vector<std::array<int, DIM>> allIndices;
			std::array<int, DIM> indices = {};
			generateCoordinates(extent_, indices, allIndices);
			return allIndices;
		}

		const std::array<int, DIM>& extent() const { return extent_; }
		const std::array<int, DIM>& stride() const { return stride_; }

	protected:
		// protected helper constructor. Construct from image, shares same memory
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
			offset_{ makeOffset(source.offset_, offset, swapDim1, swapDim2) },
			start_{ makeStart(stride_, extent_, swapDim1, swapDim2) },
			root_data_{ source.root_data_ },
			data_{ computeDataPtr(source.root_data_, source.stride_ ) }
		{ }

		// protected helper constructor. Construct from image, shares same memory, reduces dimension
		//
		// The sliced-away dimension's offset is folded into root_data_ (rather than into
		// offset_, which has no slot for it once that dimension is gone). This keeps the
		// invariant "data_ is reachable from root_data_ via offset_/start_ alone" intact,
		// which matters because further view operations (view()/mirror()/swap_axes()) rebuild
		// data_ from root_data_ + offset_ and know nothing about slicing. Losing the slice
		// offset there would make every further-derived view of a slice silently alias slice 0.
		Image(const Image<T, DIM + 1>& source, int sliceDimension, int sliceIndex) :
			extent_{ makeExtentSlice(source.extent_, sliceDimension) },
			stride_{ makeStrideSlice(source.stride_, sliceDimension) },
			end_{ makeEnd(extent_, stride_) },
			offset_{ makeOffsetSlice(source.offset_, sliceDimension) },
			start_{ makeStart(stride_, extent_, 0, 0) },
			root_data_{ source.root_data_ + sliceIndex * _abs(source.stride_[sliceDimension]) },
			data_{ computeSliceDataPtr(sliceDimension, source.stride_) }
		{ }

		// protected helper methods
		T* computeDataPtr(T* sourceRootData, const std::array<int,DIM>& sourceStride)
		{
			T* value = sourceRootData;
			for (int i = 0; i < DIM; i++) value += start_[i] * _abs(stride_[i]) + offset_[i] * _abs(sourceStride[i]);
			return value;
		}
		T* computeSliceDataPtr(int sliceDimension, const std::array<int, DIM + 1>& sourceStride)
		{
			T* result = root_data_; // already includes the sliceIndex shift
			int t = 0;
			for (int i = 0; i < DIM + 1; i++)
			{
				if (i != sliceDimension)
				{
					result += start_[t] * _abs(stride_[t]) + offset_[t] * _abs(sourceStride[i]);
					t++;
				}
			}
			return result;
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
		std::array<int, DIM> makeOffset(const std::array<int, DIM>& sourceOffset, const std::array<int, DIM>& offset, int swapDim1, int swapDim2)
		{
			std::array<int, DIM> result{};
			for (int i = 0; i < DIM; i++)
				result[i] = sourceOffset[i] + offset[i];
			std::swap(result[swapDim1], result[swapDim2]);
			return result;
		}
		std::array<int, DIM> makeOffsetSlice(const std::array<int, DIM + 1>& sourceOffset, int sliceDimension)
		{
			std::array<int, DIM> result{};
			int t = 0;
			for (int i = 0; i < DIM + 1; i++)
			{
				if (i == sliceDimension) continue;
				result[t] = sourceOffset[i];
				t++;
			}
			return result;
		}
		std::array<int, DIM> makeStart(const std::array<int, DIM>& newStride, const std::array<int, DIM>& newExtent, int swapDim1, int swapDim2)
		{
			std::array<int, DIM> result{};
			for (int i = 0; i < DIM; i++)
				result[i] = (newStride[i] < 0 ? newExtent[i] - 1 : 0);
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

		// Generates multiple dimensional indices in order
		void generateCoordinates(const std::array<int, DIM>& extents, std::array<int, DIM>& indices, std::vector<std::array<int, DIM>>& allIndices, std::size_t depth = 0) const {
			if (depth == DIM) {  // Reached the deepest level
				allIndices.push_back(indices);
				return;
			}

			for (int i = 0; i < extents[DIM - depth - 1]; ++i) {
				indices[DIM - depth - 1] = i;
				generateCoordinates(extents, indices, allIndices, depth + 1);
			}
		}

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

		// protected internal variables
		const std::array<int, DIM> extent_;   // extent of each dimension
		const std::array<int, DIM> stride_;   // stride of each dimension (linear memory skip factor)
		const std::array<int, DIM> end_;      // (one plus the last point for each dimension) * stride
		const std::array<int, DIM> offset_;
		const std::array<int, DIM> start_;
		T* root_data_;
		T* data_;
	};

	//operator overloads
	//
	// Prints dim0 as rows (comma-separated values) and dim1 as which row within a
	// block, both implicit the way any 2D grid is. That stops being enough at 3+
	// dimensions -- there's nothing to distinguish "a blank line because dim2 just
	// advanced" from "a blank line because dim3 just advanced" other than counting
	// them. So for N >= 3, every 2D (dim1 x dim0) block is preceded by an explicit
	// "[dim2=.., dim3=.., ...]" header naming every higher dimension's current
	// index, in addition to the blank-line spacing (kept for readability).
	template<class T, int N>
	std::ostream& operator<<(std::ostream& sb, const Image<T, N>& r)
	{
		sb << std::fixed << std::setprecision(2);
		std::array<int, N> indices = {0}; // Initialize index array

		// Lambda to handle recursion within the same function
		std::function<void(int)> printImage = [&](int dim)
		{
			if (dim == 0) // Base case: last dimension, print the elements
			{
				for (int i = 0; i < r.extent()[dim]; i++)
				{
					indices[dim] = i;
					sb << static_cast<double>(r.at(indices));
					sb << ", ";
				}
				sb << std::endl;
			}
			else // Recursive case: iterate through the current dimension
			{
				for (int i = 0; i < r.extent()[dim]; i++)
				{
					indices[dim] = i;
					// dim==2 is always exactly one level above the 2D (dim1 x dim0)
					// block that dim==1's loop is about to print, regardless of how
					// large N is -- every dimension above it (indices[3..N-1]) has
					// already been fixed by the enclosing recursive calls by now.
					if (dim == 2)
					{
						sb << "[";
						for (int d = 2; d < N; d++)
						{
							if (d > 2) sb << ", ";
							sb << "dim" << d << "=" << indices[d];
						}
						sb << "]" << std::endl;
					}
					printImage(dim - 1);
				}
				sb << std::endl;
			}
		};

		printImage(N - 1); // Start recursion from the first dimension
		return sb;
	}


	template<class T, class U, int DIM>
	bool operator<(const T& lhs, const Image<U, DIM>& rhs) {
		for (auto rhsIt = rhs.begin(); rhsIt != rhs.end(); ++rhsIt) if (lhs >= *rhsIt) return false;
		return true;
	}
}
