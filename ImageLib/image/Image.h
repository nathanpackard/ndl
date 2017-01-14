#pragma once
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
#include <functional>
#include <iomanip>
#include <numeric>
#include "mathHelpers.h"

namespace ImageLib
{
	enum _x_reflect_t { _x_reflect } x_reflect;
	enum _y_reflect_t { _y_reflect } y_reflect;
	enum _z_reflect_t { _z_reflect } z_reflect;
	enum _xy_reflect_t { _xy_reflect } xy_reflect;
	enum _xz_reflect_t { _xz_reflect } xz_reflect;
	enum _yz_reflect_t { _yz_reflect } yz_reflect;
	enum _xyz_reflect_t { _xyz_reflect } xyz_reflect;
	enum _x_wrap_t { _x_wrap } x_wrap;
	enum _y_wrap_t { _y_wrap } y_wrap;
	enum _z_wrap_t { _z_wrap } z_wrap;
	enum _xy_wrap_t { _xy_wrap } xy_wrap;
	enum _xz_wrap_t { _xz_wrap } xz_wrap;
	enum _yz_wrap_t { _yz_wrap } yz_wrap;
	enum _xyz_wrap_t { _xyz_wrap } xyz_wrap;
	enum _x_clamp_t { _x_clamp } x_clamp;
	enum _y_clamp_t { _y_clamp } y_clamp;
	enum _z_clamp_t { _z_clamp } z_clamp;
	enum _xy_clamp_t { _xy_clamp } xy_clamp;
	enum _xz_clamp_t { _xz_clamp } xz_clamp;
	enum _yz_clamp_t { _yz_clamp } yz_clamp;
	enum _xyz_clamp_t { _xyz_clamp } xyz_clamp;

	template <class T, int DIM>
	class Image
	{
	public:
		typedef T value_type;
		class iterator
		{
		public:
			typedef iterator self_type;
			typedef T value_type;
			iterator(Image& parent, int last) : I{}, _Ptr(parent.Data()), _myImage(parent) { I.back() = last; }
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
				if (DIM > 1 ++I[1] < _myImage.Extent[1])
				{
					I[0] = _myImage.Start[0];
					_Ptr += StepSize[1];
					return *this;
				}
				if (DIM > 2 ++I[2] < _myImage.Extent[2])
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
			T& operator[](int index) { return _Ptr[I[0] + index]; }
			const T& operator[](int index) const { return _Ptr[I[0] + index]; }

			//reflected accessors
			T& operator()(int dx, const _x_reflect_t dummy) { return _Ptr[_reflect(_myImage.Extent[0], I[0] + dx) * _myImage.StepSize[0]]; }
			T& operator()(int dy, const _y_reflect_t dummy) { return _Ptr[I[0] + (_reflect(_myImage.Extent[1], I[1] + dy) - I[1]) * _myImage.StepSize[1]]; }
			T& operator()(int dz, const _z_reflect_t dummy) { return _Ptr[I[0] + (_reflect(_myImage.Extent[2], I[2] + dz) - I[2]) * _myImage.StepSize[2]]; }
			T& operator()(int dx, int dy, const _xy_reflect_t dummy) { return _Ptr[(_reflect(_myImage.Extent[1], I[1] + dy) - I[1]) * _myImage.StepSize[1] + _reflect(_myImage.Extent[0], I[0] + dx) * _myImage.StepSize[0]]; }
			T& operator()(int dx, int dz, const _xz_reflect_t dummy) { return _Ptr[(_reflect(_myImage.Extent[2], I[2] + dz) - I[2]) * _myImage.StepSize[2] + _reflect(_myImage.Extent[0], I[0] + dx) * _myImage.StepSize[0]]; }
			T& operator()(int dy, int dz, const _yz_reflect_t dummy) { return _Ptr[I[0] + (_reflect(_myImage.Extent[2], I[2] + dz) - I[2]) * _myImage.StepSize[2] + (_reflect(_myImage.Extent[1], I[1] + dy) - I[1]) * _myImage.StepSize[1]]; }
			T& operator()(int dx, int dy, int dz, const _xyz_reflect_t dummy) { return _Ptr[(_reflect(_myImage.Extent[2], I[2] + dz) - I[2]) * _myImage.StepSize[2] + (_reflect(_myImage.Extent[1], I[1] + dy) - I[1]) * _myImage.StepSize[1] + _reflect(_myImage.Extent[0], I[0] + dx) * _myImage.StepSize[0]]; }
			T& operator()(std::array<int, DIM> delta, const _xyz_reflect_t dummy)
			{
				int index = 0;
				for (int i = 0; i < DIM; i++) index += _reflect(_myImage.Extent[i], I[i] + delta[i]) * _myImage.StepSize[i];
				return _Ptr[index];
			}

			//circular reflected accessors
			T& operator()(int dx, const _x_wrap_t dummy) { return _Ptr[_wrap(_myImage.Extent[0], I[0] + dx) * _myImage.StepSize[0]]; }
			T& operator()(int dy, const _y_wrap_t dummy) { return _Ptr[I[0] + (_wrap(_myImage.Extent[1], I[1] + dy) - I[1]) * _myImage.StepSize[1]]; }
			T& operator()(int dz, const _z_wrap_t dummy) { return _Ptr[I[0] + (_wrap(_myImage.Extent[2], I[2] + dz) - I[2]) * _myImage.StepSize[2]]; }
			T& operator()(int dx, int dy, const _xy_wrap_t dummy) { return _Ptr[(_wrap(_myImage.Extent[1], I[1] + dy) - I[1]) * _myImage.StepSize[1] + _wrap(_myImage.Extent[0], I[0] + dx) * _myImage.StepSize[0]]; }
			T& operator()(int dx, int dz, const _xz_wrap_t dummy) { return _Ptr[(_wrap(_myImage.Extent[2], I[2] + dz) - I[2]) * _myImage.StepSize[2] + _wrap(_myImage.Extent[0], I[0] + dx) * _myImage.StepSize[0]]; }
			T& operator()(int dy, int dz, const _yz_wrap_t dummy) { return _Ptr[I[0] + (_wrap(_myImage.Extent[2], I[2] + dz) - I[2]) * _myImage.StepSize[2] + (_wrap(_myImage.Extent[1], I[1] + dy) - I[1]) * _myImage.StepSize[1]]; }
			T& operator()(int dx, int dy, int dz, const _xyz_wrap_t dummy) { return _Ptr[(_wrap(_myImage.Extent[2], I[2] + dz) - I[2]) * _myImage.StepSize[2] + (_wrap(_myImage.Extent[1], I[1] + dy) - I[1]) * _myImage.StepSize[1] + _wrap(_myImage.Extent[0], I[0] + dx) * _myImage.StepSize[0]]; }
			T& operator()(std::array<int, DIM> delta, const _xyz_wrap_t dummy)
			{
				int index = 0;
				for (int i = 0; i < DIM; i++) index += _wrap(_myImage.Extent[i], I[i] + delta[i]) * _myImage.StepSize[i];
				return _Ptr[index];
			}

			//clamped accessors
			T& operator()(int dx, const _x_clamp_t dummy) { return _Ptr[_clamp(_myImage.Extent[0], I[0] + dx) * _myImage.StepSize[0]]; }
			T& operator()(int dy, const _y_clamp_t dummy) { return _Ptr[I[0] + (_clamp(_myImage.Extent[1], I[1] + dy) - I[1]) * _myImage.StepSize[1]]; }
			T& operator()(int dz, const _z_clamp_t dummy) { return _Ptr[I[0] + (_clamp(_myImage.Extent[2], I[2] + dz) - I[2]) * _myImage.StepSize[2]]; }
			T& operator()(int dx, int dy, const _xy_clamp_t dummy) { return _Ptr[(_clamp(_myImage.Extent[1], I[1] + dy) - I[1]) * _myImage.StepSize[1] + _clamp(_myImage.Extent[0], I[0] + dx) * _myImage.StepSize[0]]; }
			T& operator()(int dx, int dz, const _xz_clamp_t dummy) { return _Ptr[(_clamp(_myImage.Extent[2], I[2] + dz) - I[2]) * _myImage.StepSize[2] + _clamp(_myImage.Extent[0], I[0] + dx) * _myImage.StepSize[0]]; }
			T& operator()(int dy, int dz, const _yz_clamp_t dummy) { return _Ptr[I[0] + (_clamp(_myImage.Extent[2], I[2] + dz) - I[2]) * _myImage.StepSize[2] + (_clamp(_myImage.Extent[1], I[1] + dy) - I[1]) * _myImage.StepSize[1]]; }
			T& operator()(int dx, int dy, int dz, const _xyz_clamp_t dummy) { return _Ptr[(_clamp(_myImage.Extent[2], I[2] + dz) - I[2]) * _myImage.StepSize[2] + (_clamp(_myImage.Extent[1], I[1] + dy) - I[1]) * _myImage.StepSize[1] + _clamp(_myImage.Extent[0], I[0] + dx) * _myImage.StepSize[0]]; }
			T& operator()(std::array<int, DIM> delta, const _xyz_clamp_t dummy)
			{
				int index = 0;
				for (int i = 0; i < DIM; i++) index += _clamp(_myImage.Extent[i], I[i] + delta[i]) * _myImage.StepSize[i];
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
		template<class U> Image(Image<U,DIM>& source) : Image(source.Extent) 
		{
			auto sourceit = source.begin();
			for (auto it = begin(); it != end(); ++it, ++sourceit) *it = *sourceit;
		}

		// construct an image of the specified size
		Image(std::array<int, DIM> extent) :
			StepSize{ makeStepSize(extent)},
			Extent(extent),
			Offset{ },
			Start{ },
			End{ makeEnd(Extent, StepSize) }
		{
			int totalSize = 1;
			for(int i=0;i<DIM;i++) totalSize *= extent[i];
			RootDataArray = new T[totalSize * sizeof(T)]();
			DataArray = RootDataArray;
			referenceCount = new int[1];
			referenceCount[0] = 1;
		}

		// construct from external data
		Image(void* sourceData, std::array<int, DIM> extent) :
			StepSize{ makeStepSize(extent)},
			Extent(extent),
			Offset{ },
			Start{ },
			End{ makeEnd(Extent, StepSize) }
		{
			DataArray = (T*)sourceData;
			RootDataArray = DataArray;
			referenceCount = new int[1];
			referenceCount[0] = 2;
		}

		// construct from image, shares same memory
		Image(Image<T, DIM>& source, std::array<int, DIM> offset, std::array<int, DIM> extent, std::array<int, DIM> stepSize, std::array<bool, DIM> mirror, int swapDim1 = 0, int swapDim2 = 0) :
			StepSize{ makeStepSize(source.StepSize, mirror, stepSize, swapDim1, swapDim2) },
			Extent{ makeExtent(extent, stepSize, swapDim1, swapDim2) },
			Offset{ makeOffset(source.Offset, offset, swapDim1, swapDim2) },
			Start{ makeStart(StepSize, extent, source.Extent, swapDim1, swapDim2) },
			End{ makeEnd(Extent, StepSize) }
		{
			RootDataArray = source.RootDataArray;
			DataArray = RootDataArray;
			for (int i = 0; i < DIM; i++) DataArray += Start[i] * _abs(StepSize[i]) + Offset[i] * _abs(source.StepSize[i]);
			referenceCount = source.referenceCount;
			referenceCount[0]++;
		}

		//TODO: make ROI, swap, mirror, and skip constructors

		// construct from image, reducing dimension
		Image(Image<T, DIM + 1>& source, int sliceDimension, int sliceIndex) : Image<T, DIM>(source, makeSliceOffset(sliceDimension, sliceIndex), makeSliceExtent(source, sliceDimension), std::array<int, DIM>{1}, std::array<bool, DIM>{}) { }

		template<class U> Image& operator=(const Image<U,DIM> &rhs) {
			assert(rhs.Extent == Extent);
			auto rhsit = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsit) *it = *rhsit;
			return *this;
		}
		template<class U> Image& operator=(const U rhs) {
			for (auto it = begin(); it != end(); ++it) *it = rhs;
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
		Image operator-() { Image result(*this, true); result.MutableUnaryOp<std::negate<T>>(); return result; }
		template<class U> Image& operator+=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::plus<T>>(rhs); }
		template<class U> Image& operator+=(const U rhs) { return MutableBinaryScalarOp<std::plus<T>>(rhs); }
		template<class U> Image operator+(const Image<U, DIM>& rhs) { Image result(*this, true); result += rhs; return result; }
		template<class U> Image operator+(const U rhs) { Image result(*this, true); result += rhs; return result; }
		template<class U> Image& operator-=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::minus<T>>(rhs); }
		template<class U> Image& operator-=(const U rhs) { return MutableBinaryScalarOp<std::minus<T>>(rhs); }
		template<class U> Image operator-(const Image<U, DIM>& rhs) { Image result(*this, true); result -= rhs; return result; }
		template<class U> Image operator-(const U rhs) { Image result(*this, true); result -= rhs; return result; }
		template<class U> Image& operator*=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::multiplies<T>>(rhs); }
		template<class U> Image& operator*=(const U rhs) { return MutableBinaryScalarOp<std::multiplies<T>>(rhs); }
		template<class U> Image operator*(const Image<U, DIM>& rhs) { Image result(*this, true); result *= rhs; return result; }
		template<class U> Image operator*(const U rhs) { Image result(*this, true); result *= rhs; return result; }
		template<class U> Image& operator/=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::divides<T>>(rhs); }
		template<class U> Image& operator/=(const U rhs) { return MutableBinaryScalarOp<std::divides<T>>(rhs); }
		template<class U> Image operator/(const Image<U, DIM>& rhs) { Image result(*this, true); result /= rhs; return result; }
		template<class U> Image operator/(const U rhs) { Image result(*this, true); result /= rhs; return result; }
		template<class U> Image& operator%=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::modulus<T>>(rhs); }
		template<class U> Image& operator%=(const U rhs) { return MutableBinaryScalarOp<std::modulus<T>>(rhs); }
		template<class U> Image operator%(const Image<U, DIM>& rhs) { Image result(*this, true); result %= rhs; return result; }
		template<class U> Image operator%(const U rhs) { Image result(*this, true); result %= rhs; return result; }
		template<class U> Image& operator|=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::bit_or<T>>(rhs); }
		template<class U> Image& operator|=(const U rhs) { return MutableBinaryScalarOp<std::bit_or<T>>(rhs); }
		template<class U> Image operator|(const Image<U, DIM>& rhs) { Image result(*this, true); result |= rhs; return result; }
		template<class U> Image operator|(const U rhs) { Image result(*this, true); result |= rhs; return result; }
		template<class U> Image& operator&=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::bit_and<T>>(rhs); }
		template<class U> Image& operator&=(const U rhs) { return MutableBinaryScalarOp<std::bit_and<T>>(rhs); }
		template<class U> Image operator&(const Image<U, DIM>& rhs) { Image result(*this, true); result &= rhs; return result; }
		template<class U> Image operator&(const U rhs) { Image result(*this, true); result &= rhs; return result; }
		template<class U> Image& operator^=(const Image<U, DIM>& rhs) { return MutableBinaryImageOp<std::bit_xor<T>>(rhs); }
		template<class U> Image& operator^=(const U rhs) { return MutableBinaryScalarOp<std::bit_xor<T>>(rhs); }
		template<class U> Image operator^(const Image<U, DIM>& rhs) { Image result(*this, true); result ^= rhs; return result; }
		template<class U> Image operator^(const U rhs) { Image result(*this, true); result ^= rhs; return result; }
		template<class U> Image operator||(const Image<U, DIM>& rhs) { Image result(*this, true); result.MutableBinaryImageOp<std::logical_or<T>>(rhs); return result; }
		template<class U> Image operator||(const U rhs) { Image result(*this, true); result.MutableBinaryScalarOp<std::logical_or<T>>(rhs); return result; }
		template<class U> Image operator&&(const Image<U, DIM>& rhs) { Image result(*this, true); result.MutableBinaryImageOp<std::logical_and<T>>(rhs); return result; }
		template<class U> Image operator&&(const U rhs) { Image result(*this, true); result.MutableBinaryScalarOp<std::logical_and<T>>(rhs); return result; }
		Image operator!() { Image result(*this, true); result.MutableUnaryOp<std::logical_not<T>>(); return result; }
		template <class U> bool operator!= (const Image<U, DIM>& rhs) { return !(*this == rhs); }
		template <class U> bool operator!= (const U& rhs) { return !(*this == rhs); }
		template <class U> bool operator<= (const Image<U, DIM>& rhs) { return !(rhs < *this); }
		template <class U> bool operator<= (const U& rhs) { return !(rhs < *this); }
		template <class U> bool operator>  (const Image<U, DIM>& rhs) { return (rhs < *this); }
		template <class U> bool operator>  (const U& rhs) { return (rhs < *this); }
		template <class U> bool operator>= (const Image<U, DIM>& rhs) { return !(*this < rhs); }
		template <class U> bool operator>= (const U& rhs) { return !(*this < rhs); }
		long size() { return std::accumulate(Extent.begin(), Extent.end(), 1, std::multiplies<int>()); }
		std::string CurrentState()
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
		T operator()(std::array<int, DIM> index){ return DataArray[std::inner_product(index.begin(), index.end(), StepSize.begin(), 0)]; }
		const T& operator()(std::array<int, DIM> index) const {	return DataArray[std::inner_product(index.begin(), index.end(), StepSize.begin(), 0)]; }
		T at(std::array<int, DIM> index) { return DataArray[std::inner_product(index.begin(), index.end(), StepSize.begin(), 0)]; }
		T operator()(int index) { return DataArray[index]; }
		const T& operator()(int index) const { return DataArray[index]; }
		T at(int index) { return DataArray[index]; }

		//Destructors and destruction methods
		~Image()
		{
			referenceCount[0]--;
			if (referenceCount[0] == 0) {
				delete[] RootDataArray;
				delete[] referenceCount;
			}
		}

		T* Data() { return DataArray; }

	protected:
		//helper methods
		std::array<int, DIM> makeStepSize(const std::array<int, DIM>& sourceStepSize, const std::array<bool, DIM>& mirror, const std::array<int, DIM>& stepSize, int swapDim1, int swapDim2)
		{
			std::array<int, DIM> result{};
			for (int i = 0; i < DIM; i++) result[i] = sourceStepSize[i] * (mirror[i] ? stepSize[i] * -1 : stepSize[i]);
			std::swap(result[swapDim1], result[swapDim2]);
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
		std::array<int, DIM> makeOffset(const std::array<int, DIM>& sourceOffset, const std::array<int, DIM>& offset, int swapDim1, int swapDim2)
		{
			std::array<int, DIM> result{};
			for (int i = 0; i < DIM; i++)
				result[i] = sourceOffset[i] + offset[i];
			std::swap(result[swapDim1], result[swapDim2]);
			return result;
		}
		std::array<int, DIM> makeStart(const std::array<int, DIM>& newStepSize, const std::array<int, DIM>& extent, const std::array<int, DIM>& sourceExtent, int swapDim1, int swapDim2)
		{
			std::array<int, DIM> result{};
			for (int i = 0; i < DIM; i++)
				result[i] = (newStepSize[i] < 0 ? ((extent[i] > 0) ? extent[i] : sourceExtent[i]) - 1 : 0);
			std::swap(result[swapDim1], result[swapDim2]);
			return result;
		}
		std::array<int, DIM> makeEnd(const std::array<int, DIM>& newExtent, const std::array<int, DIM>& newStepSize)
		{
			std::array<int, DIM> result{};
			for (int i = 0; i < DIM; i++)
				result[i] = newExtent[i] * newStepSize[i];
			return result;
		}
		std::array<int, DIM> makeSliceOffset(int sliceDimension, int sliceIndex)
		{
			std::array<int, DIM> result{};
			result[sliceDimension] = sliceIndex;
			return result;
		}
		std::array<int, DIM> makeSliceExtent(Image<T, DIM + 1>& source, int sliceDimension)
		{
			std::array<int, DIM> result{};
			int t = 0;
			for (int i = 0; i < DIM + 1; i++)
				if (i != sliceDimension) result[t++] = source.Extent[i];
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

	public:
		const std::array<int, DIM> Extent;
		const std::array<int, DIM> StepSize;
		const std::array<int, DIM> End;

	protected:
		int* referenceCount;
		T* DataArray;
		T* RootDataArray;
		const std::array<int, DIM> Offset;
		const std::array<int, DIM> Start;
	};

	//operator overloads
	template<class T> std::ostream& operator<<(std::ostream& sb, Image<T, 1>& r)
	{
		sb << std::fixed << std::setprecision(2);
		for (int i = 0; i < r.Extent[0]; i++)
		{
			if (i != 0) sb << ", ";
			sb << (double)r.at(std::array<int, 1>{i});
		}
		return sb;
	}
	template<class T> std::ostream& operator<<(std::ostream& sb, Image<T, 2>& r)
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
	template<class T> std::ostream& operator<<(std::ostream& sb, Image<T, 3>& r)
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

	//extension functions
	template<class T, int DIM> Image<T, DIM> Gaussian3(Image<T, DIM>& image, const double sigma) {
		Image<T, DIM> result(image);
		for (auto i = image.begin(), r = result.begin(); i != image.end(); ++i, ++r) {
			*r = *i * _gaussian(sigma, 0) + (i[roi1.StepSize[0]] + i[-roi1.StepSize[0]]) * _gaussian(sigma, 1);
		}
	}
	template<class T, int DIM> Image<T, DIM> Gaussian5(Image<T, DIM>& image, const double sigma) {
		Image<T, DIM> result(image);
		for (auto i = image.begin(), r = result.begin(); i != image.end(); ++i, ++r) {
			*r = *i * _gaussian(sigma, 0) + (i[roi1.StepSize[0]] + i[-roi1.StepSize[0]]) * _gaussian(sigma, 1) + (i[roi1.StepSize[0] * 2] + i[-roi1.StepSize[0] * 2]) * _gaussian(sigma, 2);
		}
	}
	template<class T, class T2> Image<T,3> ToIntegralImage(Image<T2,3>& image)
	{
		Image<T,3> result = image;
		auto i = image.begin();
		for (auto r = result.begin(); r != result.end(); ++r, ++i) {
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
	template<class T> T SampleIntegralImage(Image<T,3>& image) {
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