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
		class iterator
		{
		public:
			typedef iterator self_type;
			typedef T value_type;
			iterator(Image& parent, int last) : I{}, _Ptr(parent.DataArray), _myImage(parent) { I.back() = last; }
			self_type operator++() {
				self_type i = *this;
				if ((I[0] += 1) != _myImage.End[0]) return i;
				if (DIM > 1 && ++I[1] < _myImage.Extent[1])
				{
					I[0] = _myImage.Start[0];
					_Ptr += _myImage.Stride[1];
					return i;
				}
				if (DIM > 2 && ++I[2] < _myImage.Extent[2])
				{
					I[0] = _myImage.Start[0];
					I[1] = _myImage.Start[1];
					_Ptr += _myImage.Stride[2] - _myImage.Stride[1] * (_myImage.Extent[1] - 1);
					return i;
				}
				for (int p = 3; p < DIM; p++)
				{
					if (DIM > p && ++I[p] < _myImage.Extent[p])
					{
						for (int q = 0; q < p; q++) I[q] = _myImage.Start[q];
						_Ptr += _myImage.Stride[p];
						for (int q = 1; q < p; q++) _Ptr -= _myImage.Stride[p-q] * (_myImage.Extent[p - q] - 1);
						return i;
					}
				}
				return i;
			}
			self_type operator++(int junk) {
				if ((I[0] += 1) != _myImage.End[0]) return *this;
				if (DIM > 1 && ++I[1] < _myImage.Extent[1])
				{
					I[0] = _myImage.Start[0];
					_Ptr += Stride[1];
					return *this;
				}
				if (DIM > 2 && ++I[2] < _myImage.Extent[2])
				{
					I[0] = _myImage.Start[0];
					I[1] = _myImage.Start[1];
					_Ptr += _myImage.Stride[2] - _myImage.Stride[1] * (_myImage.Extent[1] - 1);
					return *this;
				}
				for (int p = 3; p < DIM; p++)
				{
					if (DIM > p && ++I[p] < _myImage.Extent[p])
					{
						for (int q = 0; q < p; q++) I[q] = _myImage.Start[q];
						_Ptr += _myImage.Stride[p];
						for (int q = 1; q < p; q++) _Ptr -= _myImage.Stride[p-q] * (_myImage.Extent[p-q] - 1);
						return *this;
					}
				}
				return *this;
			}
			T& operator*() { return _Ptr[I[0]]; }
			T* operator->() { return _Ptr + I[0]; }
			T* Pointer() { return _Ptr + I[0]; }

			//low level index based relative accessor
			T& operator[](int index) { return _Ptr[I[0] + index]; }
			const T& operator[](int index) const { return _Ptr[I[0] + index]; }

			//relative reflection accessors
			T& reflect(int delta, const int dimensionIndex)
			{
				return _Ptr[I[0] + (_reflect(_myImage.Extent[dimensionIndex], I[dimensionIndex] + delta) - I[dimensionIndex]) * _myImage.Stride[dimensionIndex]];
			}
			T& reflect(std::array<int, DIM> delta)
			{
				int index = 0;
				for (int i = 0; i < DIM; i++) index += _Ptr[I[0] + (_reflect(_myImage.Extent[i], I[i] + delta[i]) - I[i]) * _myImage.Stride[i]];
				return _Ptr[index];
			}
			template<int D>
			T& reflect(std::array<int, D>& delta, const std::array<int, D>& dimensionIndices)
			{
				int index = 0;
				for (int i = 0; i < D; i++) {
					int ii = dimensionIndices[i];
					index += _Ptr[I[ii] + (_reflect(_myImage.Extent[ii], I[ii] + delta[ii]) - I[ii]) * _myImage.Stride[ii]];
				}
				return _Ptr[index];
			}

			//relative clamping accessors
			T& clamp(int delta, const int dimensionIndex)
			{
				return _Ptr[I[0] + (_clamp(_myImage.Extent[dimensionIndex], I[dimensionIndex] + delta) - I[dimensionIndex]) * _myImage.Stride[dimensionIndex]];
			}
			T& clamp(std::array<int, DIM> delta)
			{
				int index = 0;
				for (int i = 0; i < DIM; i++) index += _Ptr[I[0] + (_clamp(_myImage.Extent[i], I[i] + delta[i]) - I[i]) * _myImage.Stride[i]];
				return _Ptr[index];
			}
			template<int D>
			T& clamp(std::array<int, D>& delta, const std::array<int, D>& dimensionIndices)
			{
				int index = 0;
				for (int i = 0; i < D; i++) {
					int ii = dimensionIndices[i];
					index += _Ptr[I[ii] + (_clamp(_myImage.Extent[ii], I[ii] + delta[ii]) - I[ii]) * _myImage.Stride[ii]];
				}
				return _Ptr[index];
			}

			//relative wrapping accessors
			T& wrap(int delta, const int dimensionIndex)
			{
				return _Ptr[I[0] + (_wrap(_myImage.Extent[dimensionIndex], I[dimensionIndex] + delta) - I[dimensionIndex]) * _myImage.Stride[dimensionIndex]];
			}
			T& wrap(std::array<int, DIM> delta)
			{
				int index = 0;
				for (int i = 0; i < DIM; i++) index += _Ptr[I[0] + (_wrap(_myImage.Extent[i], I[i] + delta[i]) - I[i]) * _myImage.Stride[i]];
				return _Ptr[index];
			}
			template<int D>
			T& wrap(std::array<int, D>& delta, const std::array<int, D>& dimensionIndices)
			{
				int index = 0;
				for (int i = 0; i < D; i++) {
					int ii = dimensionIndices[i];
					index += _Ptr[I[ii] + (_wrap(_myImage.Extent[ii], I[ii] + delta[ii]) - I[ii]) * _myImage.Stride[ii]];
				}
				return _Ptr[index];
			}

			bool operator==(const self_type& rhs) { return I.back() == _myImage.Extent.back(); }
			bool operator!=(const self_type& rhs) { return I.back() != _myImage.Extent.back(); }
			std::array<int, DIM> I;
		private:
			T* _Ptr;
			const Image& _myImage;
		};

		// copy constructor (deep copy)
		template<class U> 
		Image(T* buffer, Image<U,DIM>& source) : Image(buffer, source.Extent)
		{
			auto sourceit = source.begin();
			for (auto it = begin(); it != end(); ++it, ++sourceit) *it = *sourceit;
		}

		// construct from external memory, be sure you have enough space!!
		Image(T* buffer, std::array<int, DIM> extent) :
			Stride{ makeStride(extent)},
			Extent(extent),
			Offset{ },
			Start{ },
			End{ makeEnd(Extent, Stride, 0, 0) },
			RootDataArray{ buffer },
			DataArray{ buffer }
		{ }

		static int size(std::array<int, DIM> extent)
		{
			return std::accumulate(extent.begin(), extent.end(), 1, std::multiplies<int>());
		}

		template<class U> Image& operator=(Image<U,DIM> &rhs) {
			assert(rhs.Extent == Extent);
			auto rhsit = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsit) *it = *rhsit;
			return *this;
		}
		template<class U> Image& operator=(U rhs) {
			for (auto it = begin(); it != end(); ++it)
				*it = rhs;
			return *this;
		}
		template <class U> bool operator== (const Image<U, DIM>& rhs) {
			if (rhs.Extent != Extent) return false;
			auto rhsit = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsit) if (*it != *rhsit) return false;
			return true;
		}
		template <class U> bool operator== (const U& rhs) {
			for (auto it = begin(); it != end(); ++it) if (*it != rhs) return false;
			return true;
		}
		template <class U> bool operator<  (const Image<U, DIM>& rhs) {
			assert(rhs.Extent == Extent);
			auto rhsit = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsit) if (*it >= *rhsit) return false;
			return true;
		}
		template <class U> bool operator<  (const U& rhs) {
			for (auto it = begin(); it != end(); ++it) if (*it >= rhs) return false;
			return true;
		}
		void negate() { MutableUnaryOp<std::negate<T>>(); }
		template<class U> Image& operator+=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::plus<T>>(rhs); }
		template<class U> Image& operator+=(const U rhs) { return MutableBinaryScalarOp<std::plus<T>>(rhs); }
		template<class U> Image& operator-=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::minus<T>>(rhs); }
		template<class U> Image& operator-=(const U rhs) { return MutableBinaryScalarOp<std::minus<T>>(rhs); }
		template<class U> Image& operator*=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::multiplies<T>>(rhs); }
		template<class U> Image& operator*=(const U rhs) { return MutableBinaryScalarOp<std::multiplies<T>>(rhs); }
		template<class U> Image& operator/=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::divides<T>>(rhs); }
		template<class U> Image& operator/=(const U rhs) { return MutableBinaryScalarOp<std::divides<T>>(rhs); }
		template<class U> Image& operator%=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::modulus<T>>(rhs); }
		template<class U> Image& operator%=(const U rhs) { return MutableBinaryScalarOp<std::modulus<T>>(rhs); }
		template<class U> Image& operator|=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::bit_or<T>>(rhs); }
		template<class U> Image& operator|=(const U rhs) { return MutableBinaryScalarOp<std::bit_or<T>>(rhs); }
		template<class U> Image& operator&=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::bit_and<T>>(rhs); }
		template<class U> Image& operator&=(const U rhs) { return MutableBinaryScalarOp<std::bit_and<T>>(rhs); }
		template<class U> Image& operator^=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::bit_xor<T>>(rhs); }
		template<class U> Image& operator^=(const U rhs) { return MutableBinaryScalarOp<std::bit_xor<T>>(rhs); }
		void logical_not() { MutableUnaryOp<std::logical_not<T>>(); }
		template <class U> bool operator!= (const Image<U, DIM>& rhs) { return !(*this == rhs); }
		template <class U> bool operator!= (const U& rhs) { return !(*this == rhs); }
		template <class U> bool operator<= (const Image<U, DIM>& rhs) { return !(rhs < *this); }
		template <class U> bool operator<= (const U& rhs) { return !(rhs < *this); }
		template <class U> bool operator>  (const Image<U, DIM>& rhs) { return (rhs < *this); }
		template <class U> bool operator>  (const U& rhs) { return (rhs < *this); }
		template <class U> bool operator>= (const Image<U, DIM>& rhs) { return !(*this < rhs); }
		template <class U> bool operator>= (const U& rhs) { return !(*this < rhs); }
		long size() const { return std::accumulate(Extent.begin(), Extent.end(), 1, std::multiplies<int>()); }
		std::string state()
		{
			std::ostringstream sb;
			sb << "          DataArray : " << long(DataArray - RootDataArray) << std::endl;
			for (int i = 0; i < DIM; i++) sb << "          Start" << i << " : " << Start[i] << std::endl;
			for (int i = 0; i < DIM; i++) sb << "          Offset" << i << " : " << Offset[i] << std::endl;
			for (int i = 0; i < DIM; i++) sb << "          End" << i << " : " << End[i] << std::endl;
			for (int i = 0; i < DIM; i++) sb << "          Stride" << i << " : " << Stride[i] << std::endl;
			for (int i = 0; i < DIM; i++) sb << "          Extent" << i << " : " << Extent[i] << std::endl;
			return sb.str();
		}

		//iterator methods
		iterator begin() { return iterator(*this, 0); }
		iterator end() { return iterator(*this, Extent.back()); }

		//basic accessors
		T& at(const std::array<int, DIM>& index) { return DataArray[std::inner_product(index.begin(), index.end(), Stride.begin(), 0)]; }
		T& at(const std::array<int, DIM>& index) const { return DataArray[std::inner_product(index.begin(), index.end(), Stride.begin(), 0)]; }
		Image<T, DIM> operator()(const std::initializer_list<indexer>& list)
		{
			std::vector<indexer> index = list;
			int indexSize = index.size();
			std::array<int, DIM> newExtent;
			std::array<int, DIM> newOffset;
			std::array<int, DIM> newStride;
			std::array<bool, DIM> newMirror;
			for (int i = 0; i < DIM; i++)
			{
				int start=-1, end=-1, step=1;
				newMirror[i] = false;
				if (i < indexSize)
				{
					start = index[i].data[0];
					end = index[i].data[1];
					step = index[i].data[2];
				}

				if (start < 0)
					start = 0;
				while (end < start) 
					end += Extent[i];
				newOffset[i] = start;
				newExtent[i] = 1 + end - start;
				newStride[i] = abs(step);
				if (step < 0) newMirror[i] = true;
			}
			return Image<T, DIM>(*this, newOffset, newExtent, newStride, newMirror);
		}
		Image<T, DIM - 1> slice(int sliceDimension, int sliceIndex) const
		{
			return Image<T, DIM - 1>(*this, sliceDimension, Stride[sliceDimension] < 0 ? Extent[sliceDimension] - 1 - sliceIndex : sliceIndex);
		}
		Image<T, DIM> swap(int dimension1, int dimension2) const
		{
			std::array<int, DIM> newExtent;
			std::array<int, DIM> newOffset;
			std::array<int, DIM> newStride;
			std::array<bool, DIM> newMirror;
			for (int i = 0; i < DIM; i++)
			{
				newMirror[i] = false;
				newOffset[i] = 0;
				newExtent[i] = Extent[i];
				newStride[i] = 1;
			}
			return Image<T, DIM>(*this, newOffset, newExtent, newStride, newMirror, dimension1, dimension2);
		}

		// public members
		const std::array<int, DIM> Extent;    // extent of each dimension
		const std::array<int, DIM> Stride;    // stride of each dimension (linear memory skip factor)
		const std::array<int, DIM> End;       // one plus the last point for each dimension

		// protected internal variables (TODO: move to protected later)
		const std::array<int, DIM> Offset;   //
		const std::array<int, DIM> Start;    // the first point for each dimension
		T* RootDataArray;                    // pointer to base memory (safe for all children classes)
		T* DataArray;                        //

	protected:
		// protected helper constructor. Construct from image, shares same memory
		Image(const Image<T, DIM>& source, 
			  const std::array<int, DIM>& offset, 
			  const std::array<int, DIM>& extent, 
			  const std::array<int, DIM>& stride,
			  const std::array<bool, DIM>& mirror,
			  int swapDim1 = 0,
			  int swapDim2 = 0) :
			Stride{ makeStride(source.Stride, mirror, stride, swapDim1, swapDim2) },
			Extent{ makeExtent(extent, stride, swapDim1, swapDim2) },
			Offset{ makeOffset(source.Offset, offset, swapDim1, swapDim2) },
			Start{ makeStart(Stride, Extent, swapDim1, swapDim2) },
			End{ makeEnd(Extent, Stride, swapDim1, swapDim2) },
			RootDataArray{ source.RootDataArray },
			DataArray{ computeDataArrayPtr(source.RootDataArray, source.Stride ) }
		{ }

		// protected helper constructor. Construct from image, shares same memory, reduces dimension
		Image(const Image<T, DIM + 1>& source, int sliceDimension, int sliceIndex) :
			Stride{ makeStrideSlice(source.Stride, sliceDimension) },
			Extent{ makeExtentSlice(source.Extent, sliceDimension) },
			Offset{ makeOffsetSlice(source.Offset, sliceDimension) },
			Start{ makeStart(Stride, Extent, 0, 0) },
			End{ makeEnd(Extent, Stride, 0, 0) },
			RootDataArray { source.RootDataArray },
			DataArray { computeDataArraySlicePtr(sliceDimension, sliceIndex) }
		{ }

		// protected helper methods
		T* computeDataArrayPtr(T* sourceRootArray, const std::array<int,DIM>& sourceStride)
		{
			T* value = sourceRootArray;
			for (int i = 0; i < DIM; i++) value += Start[i] * _abs(Stride[i]) + Offset[i] * _abs(sourceStride[i]);
			return value;
		}
		T* computeDataArraySlicePtr(int sliceDimension, int sliceIndex)
		{
			T* result = RootDataArray;
			int t = 0;
			for (int i = 0; i < DIM + 1; i++)
			{
				if (i == sliceDimension)
				{
					result += sliceIndex * _abs(Stride[i]);
					//result += Start[i] * _abs(Stride[i]) + Offset[i] * _abs(Stride[i]) / Extent[i];
				}
				else
				{
					result += Start[t] * _abs(Stride[t]) + Offset[t] * _abs(Stride[t]) / Extent[t];
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
		std::array<int, DIM> makeEnd(const std::array<int, DIM>& newExtent, const std::array<int, DIM>& newStride, int swapDim1, int swapDim2)
		{
			std::array<int, DIM> result{};
			for (int i = 0; i < DIM; i++)
				result[i] = (newStride[i] < 0 ? -1 : newExtent[i]);
			std::swap(result[swapDim1], result[swapDim2]);
			return result;
		}
		template<class Op, class U> Image& MutableBinaryImageOp(const Image<U, DIM>& rhs) {
			assert(rhs.Extent == Extent);
			Op o;
			auto rhsit = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsit) *it = (T)o(*it, *rhsit);
			return *this;
		}
		template<class Op, class U> Image& MutableBinaryScalarOp(const U rhs) {
			Op o;
			for (auto it = begin(); it != end(); ++it) *it = (T)o(*it, rhs);
			return *this;
		}
		template<class Op> Image& MutableUnaryOp() {
			Op o;
			for (auto it = begin(); it != end(); ++it) *it = (T)o(*it);
			return *this;
		}

	};

	//operator overloads
	template<class T> std::ostream& operator<<(std::ostream& sb, const Image<T, 1>& r)
	{
        std::vector<T> values;
		for (auto it = r.begin(); it != r.end(); ++it)
			values.push_back(*it);
		sb << std::fixed << std::setprecision(2);
		sb << "          ";
        if (r.Stride[0] < 0)
            std::reverse(values.begin(), values.end());
		for (auto it = values.begin(); it != values.end(); ++it)
        {
			if (it != values.begin()) sb << ", ";
            sb << double(*it);
        }
		return sb;
	}
	template<class T, int DIM> std::ostream& operator<<(std::ostream& sb, const Image<T, DIM>& r)
	{
		sb << std::fixed << std::setprecision(2);
        for (int i=0;i<r.Extent[DIM - 1];i++)
        {
            for(int t=0;t<5 - DIM;t++) sb << "  ";
    		sb << " dim=" << DIM << ", slice " << i + 1 << "/" << r.Extent[DIM - 1] << " ";
            for(int t=0;t<5 - DIM;t++) sb << "  ";
            sb << "\n";
            auto current_slice = r.slice(DIM - 1, i);
            sb << current_slice.state();
            sb << current_slice;
            sb << std::endl;
        }
		return sb;
	}
	template <class T, class U, int DIM> bool operator<(const T& lhs, const Image<U, DIM>& rhs) {
		auto rhsit = rhs.begin();
		for (auto rhsit = rhs.begin(); rhsit != rhs.end(); ++rhsit) if (lhs >= *rhsit) return false;
		return true;
	}
}
