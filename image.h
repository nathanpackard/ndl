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
				if ((I[0] += _myImage.StepSize[0]) != _myImage.End[0]) return i;
				if (DIM > 1 && ++I[1] < _myImage.Extent[1])
				{
					I[0] = _myImage.Start[0];
					_Ptr += _myImage.StepSize[1];
					return i;
				}
				if (DIM > 2 && ++I[2] < _myImage.Extent[2])
				{
					I[0] = _myImage.Start[0];
					I[1] = _myImage.Start[1];
					_Ptr += _myImage.StepSize[2] - _myImage.StepSize[1] * (_myImage.Extent[1] - 1);
					return i;
				}
				for (int p = 3; p < DIM; p++)
				{
					if (++I[p] < _myImage.Extent[p])
					{
						for (int q = 0; q < p; q++) I[q] = _myImage.Start[q];
						_Ptr += _myImage.StepSize[p] - _myImage.StepSize[p-1] * (_myImage.Extent[p - 1] - 1);
						return i;
					}
				}
				return i;
			}
			self_type operator++(int junk) {
				if ((I[0] += _myImage.StepSize[0]) != _myImage.End[0]) return *this;
				if (DIM > 1 && ++I[1] < _myImage.Extent[1])
				{
					I[0] = _myImage.Start[0];
					_Ptr += StepSize[1];
					return *this;
				}
				if (DIM > 2 && ++I[2] < _myImage.Extent[2])
				{
					I[0] = _myImage.Start[0];
					I[1] = _myImage.Start[1];
					_Ptr += _myImage.StepSize[2] - _myImage.StepSize[1] * (_myImage.Extent[1] - 1);
					return *this;
				}
				for (int p = 3; p < DIM; p++)
				{
					if (++I[p] < _myImage.Extent[p])
					{
						for (int q = 0; q < p; q++) I[q] = _myImage.Start[q];
						_Ptr += _myImage.StepSize[p] - _myImage.StepSize[p - 1] * (_myImage.Extent[p - 1] - 1);
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
				return _Ptr[I[0] + (_reflect(_myImage.Extent[dimensionIndex], I[dimensionIndex] + delta) - I[dimensionIndex]) * _myImage.StepSize[dimensionIndex]];
			}
			T& reflect(std::array<int, DIM> delta)
			{
				int index = 0;
				for (int i = 0; i < DIM; i++) index += _Ptr[I[0] + (_reflect(_myImage.Extent[i], I[i] + delta[i]) - I[i]) * _myImage.StepSize[i]];
				return _Ptr[index];
			}
			template<int D>
			T& reflect(std::array<int, D>& delta, const std::array<int, D>& dimensionIndices)
			{
				int index = 0;
				for (int i = 0; i < D; i++) {
					int ii = dimensionIndices[i];
					index += _Ptr[I[ii] + (_reflect(_myImage.Extent[ii], I[ii] + delta[ii]) - I[ii]) * _myImage.StepSize[ii]];
				}
				return _Ptr[index];
			}

			//relative clamping accessors
			T& clamp(int delta, const int dimensionIndex)
			{
				return _Ptr[I[0] + (_clamp(_myImage.Extent[dimensionIndex], I[dimensionIndex] + delta) - I[dimensionIndex]) * _myImage.StepSize[dimensionIndex]];
			}
			T& clamp(std::array<int, DIM> delta)
			{
				int index = 0;
				for (int i = 0; i < DIM; i++) index += _Ptr[I[0] + (_clamp(_myImage.Extent[i], I[i] + delta[i]) - I[i]) * _myImage.StepSize[i]];
				return _Ptr[index];
			}
			template<int D>
			T& clamp(std::array<int, D>& delta, const std::array<int, D>& dimensionIndices)
			{
				int index = 0;
				for (int i = 0; i < D; i++) {
					int ii = dimensionIndices[i];
					index += _Ptr[I[ii] + (_clamp(_myImage.Extent[ii], I[ii] + delta[ii]) - I[ii]) * _myImage.StepSize[ii]];
				}
				return _Ptr[index];
			}

			//relative wrapping accessors
			T& wrap(int delta, const int dimensionIndex)
			{
				return _Ptr[I[0] + (_wrap(_myImage.Extent[dimensionIndex], I[dimensionIndex] + delta) - I[dimensionIndex]) * _myImage.StepSize[dimensionIndex]];
			}
			T& wrap(std::array<int, DIM> delta)
			{
				int index = 0;
				for (int i = 0; i < DIM; i++) index += _Ptr[I[0] + (_wrap(_myImage.Extent[i], I[i] + delta[i]) - I[i]) * _myImage.StepSize[i]];
				return _Ptr[index];
			}
			template<int D>
			T& wrap(std::array<int, D>& delta, const std::array<int, D>& dimensionIndices)
			{
				int index = 0;
				for (int i = 0; i < D; i++) {
					int ii = dimensionIndices[i];
					index += _Ptr[I[ii] + (_wrap(_myImage.Extent[ii], I[ii] + delta[ii]) - I[ii]) * _myImage.StepSize[ii]];
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
			StepSize{ makeStepSize(extent)},
			Extent(extent),
			Offset{ },
			Start{ },
			End{ makeEnd(Extent, StepSize) },
			RootDataArray{ buffer },
			DataArray{ buffer }
		{ }

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
		long size() { return std::accumulate(Extent.begin(), Extent.end(), 1, std::multiplies<int>()); }
		std::string state()
		{
			std::ostringstream sb;
			sb << "DataArray : " << ((long)DataArray - (long)RootDataArray) << std::endl;
			for (int i = 0; i < DIM; i++) sb << "Start" << i << " : " << Start[i] << std::endl;
			for (int i = 0; i < DIM; i++) sb << "Offset" << i << " : " << Offset[i] << std::endl;
			for (int i = 0; i < DIM; i++) sb << "End" << i << " : " << End[i] << std::endl;
			for (int i = 0; i < DIM; i++) sb << "StepSize" << i << " : " << StepSize[i] << std::endl;
			for (int i = 0; i < DIM; i++) sb << "Extent" << i << " : " << Extent[i] << std::endl;
			return sb.str();
		}

		//iterator methods
		iterator begin() { return iterator(*this, 0); }
		iterator end() { return iterator(*this, Extent.back()); }

		//basic accessors
		T& at(const std::array<int, DIM>& index) { return DataArray[std::inner_product(index.begin(), index.end(), StepSize.begin(), 0)]; }
		T& at(const std::array<int, DIM>& index) const { return DataArray[std::inner_product(index.begin(), index.end(), StepSize.begin(), 0)]; }
		Image<T, DIM> operator()(const std::initializer_list<indexer>& list)
		{
			std::vector<indexer> index = list;
			std::array<int, DIM> newExtent;
			std::array<int, DIM> newOffset;
			std::array<int, DIM> newStepSize;
			std::array<bool, DIM> newMirror;
			for (int i = 0; i < DIM; i++)
			{
				newMirror[i] = false;
				int start = index[i].data[0];
				int end = index[i].data[1];
				int step = index[i].data[2];

				std::assert(start < 0);
				std::assert(end < Extent[i]);
				while (end < start) 
					end += Extent[i];
				newOffset[i] = start;
				newExtent[i] = 1 + end - start;
				newStepSize[i] = abs(step);
				if (step < 0) newMirror[i] = true;
			}
			return Image<T, DIM>(*this, newOffset, newExtent, newStepSize, newMirror);
		}
		Image<T, DIM - 1> slice(int sliceDimension, int sliceIndex) 
		{
			return Image<T, DIM - 1>(*this, sliceDimension, sliceIndex);
		}
		Image<T, DIM> swap(int dimension1, int dimension2)
		{
			std::array<int, DIM> newExtent;
			std::array<int, DIM> newOffset;
			std::array<int, DIM> newStepSize;
			std::array<bool, DIM> newMirror;
			for (int i = 0; i < DIM; i++)
			{
				newMirror[i] = false;
				newOffset[i] = 0;
				newExtent[i] = Extent[i];
				newStepSize[i] = 1;
			}
			return Image<T, DIM>(*this, newOffset, newExtent, newStepSize, newMirror, dimension1, dimension2);
		}

		// public members
		const std::array<int, DIM> Extent;
		const std::array<int, DIM> StepSize;
		const std::array<int, DIM> End;

	protected:
		// protected helper constructor. Construct from image, shares same memory
		Image(const Image<T, DIM>& source, 
			  const std::array<int, DIM>& offset, 
			  const std::array<int, DIM>& extent, 
			  const std::array<int, DIM>& stepSize, 
			  const std::array<bool, DIM>& mirror, 
			  int swapDim1 = 0, 
			  int swapDim2 = 0) :
			StepSize{ makeStepSize(source.StepSize, mirror, stepSize, swapDim1, swapDim2) },
			Extent{ makeExtent(extent, stepSize, swapDim1, swapDim2) },
			Offset{ makeOffset(source.Offset, offset, swapDim1, swapDim2) },
			Start{ makeStart(StepSize, Extent, swapDim1, swapDim2) },
			End{ makeEnd(Extent, StepSize) },
			RootDataArray{ source.RootDataArray },
			DataArray{ computeDataArrayPtr(source.RootDataArray, source.StepSize ) }
		{ }

		// protected helper constructor. Construct from image, shares same memory, reduces dimension
		Image(const Image<T, DIM + 1>& source, int sliceDimension, int sliceIndex) :
			StepSize{ makeStepSizeSlice(source.StepSize, sliceDimension) },
			Extent{ makeExtentSlice(source.Extent, sliceDimension) },
			Offset{ makeOffsetSlice(source.Offset, sliceDimension) },
			Start{ makeStart(StepSize, Extent, 0, 0) },
			End{ makeEnd(Extent, StepSize) },
			RootDataArray { source.RootDataArray },
			DataArray { computeDataArraySlicePtr(sliceDimension, sliceIndex, source.StepSize) }
		{ }

		// protected helper methods
		T* computeDataArrayPtr(T* sourceRootArray, const std::array<int,DIM>& sourceStepSize)
		{
			T* value = sourceRootArray;
			for (int i = 0; i < DIM; i++) value += Start[i] * _abs(StepSize[i]) + Offset[i] * _abs(sourceStepSize[i]);
			return value;
		}
		T* computeDataArraySlicePtr(int sliceDimension, int sliceIndex, const std::array<int, DIM + 1>& sourceStepSize)
		{
			T* result = RootDataArray;
			int t = 0;
			for (int i = 0; i < DIM + 1; i++)
			{
				if (i == sliceDimension)
					result += sliceIndex * _abs(sourceStepSize[i]);
				else
				{
					result += Start[t] * _abs(StepSize[t]) + Offset[t] * _abs(sourceStepSize[i]);
					t++;
				}
			}
			return result;
		}
		std::array<int, DIM> makeStepSize(const std::array<int, DIM>& sourceStepSize, const std::array<bool, DIM>& mirror, const std::array<int, DIM>& stepSize, int swapDim1, int swapDim2)
		{
			std::array<int, DIM> result{};
			for (int i = 0; i < DIM; i++) result[i] = sourceStepSize[i] * (mirror[i] ? stepSize[i] * -1 : stepSize[i]);
			std::swap(result[swapDim1], result[swapDim2]);
			return result;
		}
		std::array<int, DIM> makeStepSizeSlice(const std::array<int, DIM + 1>& sourceStepSize, int sliceDimension)
		{
			std::array<int, DIM> result{};
			int t = 0;
			for (int i = 0; i < DIM + 1; i++)
			{
				if (i == sliceDimension) continue;
				result[t] = sourceStepSize[i];
				t++;
			}
			return result;
		}
		std::array<int, DIM> makeStepSize(const std::array<int, DIM>& extent)
		{
			std::array<int, DIM> result{};
			result[0] = 1;
			for (int i = 1; i < DIM; i++) result[i] = extent[i - 1] * result[i - 1];
			return result;
		}
		std::array<int, DIM> makeExtent(const std::array<int, DIM>& extent, const std::array<int, DIM>& stepSize, int swapDim1, int swapDim2)
		{
			std::array<int, DIM> result{};
			for (int i = 0; i < DIM; i++)
				result[i] = _ceil((float)extent[i] / _abs(stepSize[i]));
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
		std::array<int, DIM> makeStart(const std::array<int, DIM>& newStepSize, const std::array<int, DIM>& newExtent, int swapDim1, int swapDim2)
		{
			std::array<int, DIM> result{};
			for (int i = 0; i < DIM; i++)
				result[i] = (newStepSize[i] < 0 ? newExtent[i] - 1 : 0);
			std::swap(result[swapDim1], result[swapDim2]);
			return result;
		}
		std::array<int, DIM> makeStartSlice(const std::array<int, DIM + 1>& newStepSize, const std::array<int, DIM>& newExtent, int sliceDimension)
		{
			std::array<int, DIM> result{};
			int t = 0;
			for (int i = 0; i < DIM; i++)
			{
				if (i == sliceDimension) continue;
				result[t] = (newStepSize[i] < 0 ? newExtent[i] - 1 : 0);
				t++;
			}
			return result;
		}
		std::array<int, DIM> makeEnd(const std::array<int, DIM>& newExtent, const std::array<int, DIM>& newStepSize)
		{
			std::array<int, DIM> result{};
			for (int i = 0; i < DIM; i++)
				result[i] = newExtent[i] * newStepSize[i];
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

		// protected internal variables
		const std::array<int, DIM> Offset;
		const std::array<int, DIM> Start;
		T* RootDataArray;
		T* DataArray;
	};

	//operator overloads
	template<class T> std::ostream& operator<<(std::ostream& sb, const Image<T, 1>& r)
	{
		sb << std::fixed << std::setprecision(2);
		for (int i = 0; i < r.Extent[0]; i++)
		{
			if (i != 0) sb << ", ";
			sb << (double)r.at(std::array<int, 1>{i});
		}
		return sb;
	}
	template<class T> std::ostream& operator<<(std::ostream& sb, const Image<T, 2>& r)
	{
		sb << std::fixed << std::setprecision(2);
		for (int j = 0; j < r.Extent[1]; j++)
		{
			for (int i = 0; i < r.Extent[0]; i++)
			{
				if (i != 0) sb << ", ";
				sb << (double)r.at(std::array<int, 2>{ i, j });
			}
			sb << std::endl;
		}
		return sb;
	}
	template<class T> std::ostream& operator<<(std::ostream& sb, const Image<T, 3>& r)
	{
		sb << std::fixed << std::setprecision(2);
		for (int j = 0; j < r.Extent[1]; j++)
		{
			for (int k = 0; k < r.Extent[2]; k++)
			{
				for (int i = 0; i < r.Extent[0]; i++)
				{
					if (i != 0) sb << ", ";
					sb << (double)r.at(std::array<int, 3>{ i, j, k });
				}
				if (k != r.Extent[2] - 1) sb << "  |  ";
			}
			sb << std::endl;
		}
		return sb;
	}
	template <class T, class U, int DIM> bool operator<(const T& lhs, const Image<U, DIM>& rhs) {
		auto rhsit = rhs.begin();
		for (auto rhsit = rhs.begin(); rhsit != rhs.end(); ++rhsit) if (lhs >= *rhsit) return false;
		return true;
	}

	template<class T, class T2> Image<T, 3>& ToIntegralImage(Image<T2,3>& image, Image<T, 3>& result)
	{
		result = image;
		for (auto r = result.begin(); r != result.end(); ++r) {
			if (r.I[0] > 0 && r.I[1] > 0 && r.I[2] > 0) *r += r[-result.StepSize[0] - result.StepSize[1] - result.StepSize[2]];
			if (r.I[2] > 0) *r += r[-result.StepSize[2]];
			if (r.I[1] > 0) *r += r[-result.StepSize[1]];
			if (r.I[0] > 0) *r += r[-result.StepSize[0]];
			if (r.I[0] > 0 && r.I[1] > 0) *r -= r[-result.StepSize[0] - result.StepSize[1]];
			if (r.I[1] > 0 && r.I[2] > 0) *r -= r[-result.StepSize[1] - result.StepSize[2]];
			if (r.I[0] > 0 && r.I[2] > 0) *r -= r[-result.StepSize[0] - result.StepSize[2]];
		}
		return result;
	}
	template<class T> T SampleIntegralImage(Image<T,3>& image) 
	{
		////Now for every query(x1, y1, z1) to(x2, y2, z2), 
		//first convert the coordinates so that x1, y1, z1 is the corner of the cuboid closest to origin 
		//and x2, y2, z2 is the corner that is farthest from origin.

		//S((x1, y1, z1) to(x2, y2, z2)) = S0(x2, y2, z2) - S0(x2, y2, z1)
		//- S0(x2, y1, z2) - S0(x1, y2, z2)
		//+ S0(x1, y1, z2) + S0(x1, y2, z1) + S0(x2, y1, z1)
		//- S0(x1, y1, z1)
		return 0;
	}

}