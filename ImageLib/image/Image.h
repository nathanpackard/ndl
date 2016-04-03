#pragma once
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
#include <functional>
#include <iomanip>

namespace ImageLib
{
	enum _dx_reflect_t { _dx_reflect } dx_reflect;
	enum _dy_reflect_t { _dy_reflect } dy_reflect;
	enum _dz_reflect_t { _dz_reflect } dz_reflect;
	enum _dxy_reflect_t { _dxy_reflect } dxy_reflect;
	enum _dxz_reflect_t { _dxz_reflect } dxz_reflect;
	enum _dyz_reflect_t { _dyz_reflect } dyz_reflect;
	enum _dxyz_reflect_t { _dxyz_reflect } dxyz_reflect;
	enum _dx_wrap_t { _dx_wrap } dx_wrap;
	enum _dy_wrap_t { _dy_wrap } dy_wrap;
	enum _dz_wrap_t { _dz_wrap } dz_wrap;
	enum _dxy_wrap_t { _dxy_wrap } dxy_wrap;
	enum _dxz_wrap_t { _dxz_wrap } dxz_wrap;
	enum _dyz_wrap_t { _dyz_wrap } dyz_wrap;
	enum _dxyz_wrap_t { _dxyz_wrap } dxyz_wrap;
	enum _dx_clamp_t { _dx_clamp } dx_clamp;
	enum _dy_clamp_t { _dy_clamp } dy_clamp;
	enum _dz_clamp_t { _dz_clamp } dz_clamp;
	enum _dxy_clamp_t { _dxy_clamp } dxy_clamp;
	enum _dxz_clamp_t { _dxz_clamp } dxz_clamp;
	enum _dyz_clamp_t { _dyz_clamp } dyz_clamp;
	enum _dxyz_clamp_t { _dxyz_clamp } dxyz_clamp;
	
	template <class T>
	class Image
	{
	public:
		typedef T value_type;
		class iterator
		{
		public:
			typedef iterator self_type;
			typedef T value_type;
			typedef T& reference;
			typedef T* pointer;
			typedef std::forward_iterator_tag iterator_category;
			typedef int difference_type;
			iterator(Image& parent, int x, int y, int z, T** slice, T* row) : X(x), Y(y), Z(z), _Slice(slice), Data(row), _myImage(parent) {}
			self_type operator++() {
				self_type i = *this;
				if ((X += _myImage.DX) != _myImage.EndX) {}
				else if (++Y < _myImage.Height)
				{
					X = _myImage.StartX;
					Data = _Slice[Y];
				}
				else if (++Z < _myImage.Depth)
				{
					Y = 0;
					_Slice = _myImage.Data3D[Z];
					X = _myImage.StartX;
					Data = _Slice[0];
				}
				return i;
			}
			self_type operator++(int junk) {
				if ((X += _myImage.DX) != _myImage.EndX) {}
				else if (++Y < _myImage.Height)
				{
					X = _myImage.StartX;
					Data = _Slice[Y];
				}
				else if (++Z < _myImage.Depth)
				{
					Y = 0;
					_Slice = _myImage.Data3D[Z];
					X = _myImage.StartX;
					Data = _Slice[0];
				}
				return *this;
			}
			reference operator*() { return Data[X]; }
			pointer operator->() { return Data + X; }
			T* Pointer() { return Data + X; }
			T& operator[](int index) { return Data[X + index]; }
			const T& operator[](int index) const { return Data[X + index]; }

			//reflected accessors
			T& operator()(int dx, const _dx_reflect_t dummy) { return Data[_reflectD(_myImage.EndX, X + dx * _myImage.DX, _myImage.DX)]; }
			T& operator()(int dy, const _dy_reflect_t dummy) { return _Slice[_reflect(_myImage.Height, Y + dy)][X]; }
			T& operator()(int dz, const _dz_reflect_t dummy) { return _myImage.Data3D[_reflect(_myImage.Depth, Z + dz)][Y][X]; }
			T& operator()(int dx, int dy, const _dxy_reflect_t dummy) { return _Slice[_reflect(_myImage.Height, Y + dy)][_reflectD(_myImage.EndX, X + dx * _myImage.DX, _myImage.DX)]; }
			T& operator()(int dx, int dz, const _dxz_reflect_t dummy) { return _myImage.Data3D[_reflect(_myImage.Depth, Z + dz)][Y][_reflectD(_myImage.EndX, X + dx * _myImage.DX, _myImage.DX)]; }
			T& operator()(int dy, int dz, const _dyz_reflect_t dummy) { return _myImage.Data3D[_reflect(_myImage.Depth, Z + dz)][_reflect(_myImage.Height, Y + dy)][X]; }
			T& operator()(int dx, int dy, int dz, const _dxyz_reflect_t dummy) { return _myImage.Data3D[_reflect(_myImage.Depth, Z + dz)][_reflect(_myImage.Height, Y + dy)][_reflectD(_myImage.EndX, X + dx * _myImage.DX, _myImage.DX)]; }

			//circular reflected accessors
			T& operator()(int dx, const _dx_wrap_t dummy) { return Data[_wrapD(_myImage.EndX, X + dx * _myImage.DX, _myImage.DX)]; }
			T& operator()(int dy, const _dy_wrap_t dummy) { return _Slice[_wrap(_myImage.Height, Y + dy)][X]; }
			T& operator()(int dz, const _dz_wrap_t dummy) { return _myImage.Data3D[_wrap(_myImage.Depth, Z + dz)][Y][X]; }
			T& operator()(int dx, int dy, const _dxy_wrap_t dummy) { return _Slice[_wrap(_myImage.Height, Y + dy)][_wrapD(_myImage.EndX, X + dx * _myImage.DX, _myImage.DX)]; }
			T& operator()(int dx, int dz, const _dxz_wrap_t dummy) { return _myImage.Data3D[_wrap(_myImage.Depth, Z + dz)][Y][_wrapD(_myImage.EndX, X + dx * _myImage.DX, _myImage.DX)]; }
			T& operator()(int dy, int dz, const _dyz_wrap_t dummy) { return _myImage.Data3D[_wrap(_myImage.Depth, Z + dz)][_wrap(_myImage.Height, Y + dy)][X]; }
			T& operator()(int dx, int dy, int dz, const _dxyz_wrap_t dummy) { return _myImage.Data3D[_wrap(_myImage.Depth, Z + dz)][_wrap(_myImage.Height, Y + dy)][_wrapD(_myImage.EndX, X + dx * _myImage.DX, _myImage.DX)]; }

			//clamped accessors
			T& operator()(int dx, const _dx_clamp_t dummy) { return Data[_clampD(_myImage.EndX, X + dx * _myImage.DX, _myImage.DX)]; }
			T& operator()(int dy, const _dy_clamp_t dummy) { return _Slice[_clamp(_myImage.Height, Y + dy)][X]; }
			T& operator()(int dz, const _dz_clamp_t dummy) { return _myImage.Data3D[_clamp(_myImage.Depth, Z + dz)][Y][X]; }
			T& operator()(int dx, int dy, const _dxy_clamp_t dummy) { return _Slice[_clamp(_myImage.Height, Y + dy)][_clampD(_myImage.EndX, X + dx * _myImage.DX, _myImage.DX)]; }
			T& operator()(int dx, int dz, const _dxz_clamp_t dummy) { return _myImage.Data3D[_clamp(_myImage.Depth, Z + dz)][Y][_clampD(_myImage.EndX, X + dx * _myImage.DX, _myImage.DX)]; }
			T& operator()(int dy, int dz, const _dyz_clamp_t dummy) { return _myImage.Data3D[_clamp(_myImage.Depth, Z + dz)][_clamp(_myImage.Height, Y + dy)][X]; }
			T& operator()(int dx, int dy, int dz, const _dxyz_clamp_t dummy) { return _myImage.Data3D[_clamp(_myImage.Depth, Z + dz)][_clamp(_myImage.Height, Y + dy)][_clampD(_myImage.EndX, X + dx * _myImage.DX, _myImage.DX)]; }

			bool operator==(const self_type& rhs) { return Z == _myImage.Depth; }
			bool operator!=(const self_type& rhs) { return Z != _myImage.Depth; }
			__readonly int X;
			__readonly int Y;
			__readonly int Z;
			T* Data;
		private:
			T** _Slice;
			Image& _myImage;
			constexpr int _reflect(int M, int x) { return (x < 0) ? (-x - 1) : ((x >= M) ? (2 * M - x - 1) : x); }
			constexpr int _wrap(int M, int x) { return (x < 0) ? (x + M) : ((x >= M) ? (x - M) : x); }
			constexpr int _clamp(int M, int x) { return (x < 0) ? 0 : ((x >= M) ? (M - 1) : x); }

			//THESE MAY NOT WORK FOR MIRRORED IMAGES!!!
			constexpr int _reflectD(int M, int x, int d) { return (x < 0) ? (-x - d) : ((x >= M) ? (2 * M - x - d) : x); }
			constexpr int _wrapD(int M, int x, int d) { return (x < 0) ? (x + M) : ((x >= M) ? (x - M) : x); }
			constexpr int _clampD(int M, int x, int d) { return (x < 0) ? 0 : ((x >= M) ? (M - 1) : x); }

			template<class T> constexpr auto _abs(const T & value) -> T { return (T{} < value) ? value : -value; }
		};

		//constructors
		Image(Image& source, int offsetX, int offsetY, int offsetZ, int width, int height, int depth, int dx, int dy, int dz, bool mirrorX = false, bool mirrorY = false, bool mirrorZ = false, bool swapXY = false, bool swapYZ = false, bool swapZX = false) :
			DX(_swapx(source.DX * (mirrorX ? dx * -1 : dx), source.DY * (mirrorY ? dy * -1 : dy), source.DZ * (mirrorZ ? dz * -1 : dz), swapXY, swapYZ, swapZX)),
			DY(_swapy(source.DX * (mirrorX ? dx * -1 : dx), source.DY * (mirrorY ? dy * -1 : dy), source.DZ * (mirrorZ ? dz * -1 : dz), swapXY, swapYZ, swapZX)),
			DZ(_swapz(source.DX * (mirrorX ? dx * -1 : dx), source.DY * (mirrorY ? dy * -1 : dy), source.DZ * (mirrorZ ? dz * -1 : dz), swapXY, swapYZ, swapZX)),
			Width(_swapx(_ceil((float)width / _abs(dx)), _ceil((float)height / _abs(dy)), _ceil((float)depth / _abs(dz)), swapXY, swapYZ, swapZX)),
			Height(_swapy(_ceil((float)width / _abs(dx)), _ceil((float)height / _abs(dy)), _ceil((float)depth / _abs(dz)), swapXY, swapYZ, swapZX)),
			Depth(_swapz(_ceil((float)width / _abs(dx)), _ceil((float)height / _abs(dy)), _ceil((float)depth / _abs(dz)), swapXY, swapYZ, swapZX)),
			OffsetX(_swapx(source.OffsetX + offsetX, source.OffsetY + offsetY, source.OffsetZ + offsetZ, swapXY, swapYZ, swapZX)),
			OffsetY(_swapy(source.OffsetX + offsetX, source.OffsetY + offsetY, source.OffsetZ + offsetZ, swapXY, swapYZ, swapZX)),
			OffsetZ(_swapz(source.OffsetX + offsetX, source.OffsetY + offsetY, source.OffsetZ + offsetZ, swapXY, swapYZ, swapZX)),
			StartX(_swapx((DX < 0 ? ((width > 0) ? width : source.Width) - 1 : 0), (DY < 0 ? ((height > 0) ? height : source.Height) - 1 : 0), (DZ < 0 ? ((depth > 0) ? depth : source.Depth) - 1 : 0), swapXY, swapYZ, swapZX)),
			StartY(_swapy((DX < 0 ? ((width > 0) ? width : source.Width) - 1 : 0), (DY < 0 ? ((height > 0) ? height : source.Height) - 1 : 0), (DZ < 0 ? ((depth > 0) ? depth : source.Depth) - 1 : 0), swapXY, swapYZ, swapZX)),
			StartZ(_swapz((DX < 0 ? ((width > 0) ? width : source.Width) - 1 : 0), (DY < 0 ? ((height > 0) ? height : source.Height) - 1 : 0), (DZ < 0 ? ((depth > 0) ? depth : source.Depth) - 1 : 0), swapXY, swapYZ, swapZX)),
			EndX(Width * DX),
			EndY(Height * DY), 
			EndZ(Depth * DZ)
		{
			//allocate 3D array of image
			RootDataArray = source.RootDataArray;
			DataArray = RootDataArray + StartX * _abs(DX) + StartY * _abs(DY) + StartZ * _abs(DZ) + OffsetX * _abs(source.DX) + OffsetY * _abs(source.DY) + OffsetZ * _abs(source.DZ);
			Data3D = new T**[Depth * sizeof(void*)];
			T* t1 = DataArray;
			for (int k = 0; k < Depth; k++)
			{
				T** slice = Data3D[k] = new T*[Height * sizeof(void*)];
				T* t2 = t1;
				for (int j = 0; j < Height; j++)
				{
					slice[j] = t2;
					t2 += DY;
				}
				t1 += DZ;
			}
			referenceCount = source.referenceCount;
			referenceCount[0]++;
		}
		Image(Image& source, int offsetX, int offsetY, int width, int height, int dx, int dy, bool mirrorX = false, bool mirrorY = false, bool swapXY = false) : Image(source, offsetX, offsetY, 0, width, height, 1, dx, dy, 1, mirrorX, mirrorY, false, swapXY) { }
		Image(Image& source, int offsetX, int width, int dx, bool mirrorX = false) : Image(source, offsetX, 0, 0, width, 1, 1, dx, 1, 1, mirrorX) { }
		Image(Image& source) : Image(source, 0, 0, 0, source.Width, source.Height, source.Depth, 1, 1, 1) { }
		Image(Image& source, bool deepCopy) : Image(source.Width, source.Height, source.Depth) { if (deepCopy) InitDeepCopy(source); else throw "error deepCopy must be true"; }
		template<class U> Image(Image<U>& source) : Image(source.Width, source.Height, source.Depth) { InitDeepCopy(source); }
		Image(int width, int height, int depth) : 
			DX(1), 
			DY(width), 
			DZ(width * height), 
			Width(width), 
			Height(height), 
			Depth(depth),
			OffsetX(0),
			OffsetY(0),
			OffsetZ(0),
			StartX(0),
			StartY(0),
			StartZ(0),
			EndX(Width * DX),
			EndY(Height * DY),
			EndZ(Depth * DZ)
		{
			if (width < 1 || height < 1 || depth < 1) throw "invalid parameters when allocating image";
			int totalSize = width * height * depth;
			DataArray = new T[totalSize * sizeof(T)]();
			for (int i = 0; i < totalSize; i++) DataArray[i] = 0;
			RootDataArray = DataArray;

			//allocate 3D array of image
			T* slicePointer = DataArray;
			Data3D = new T**[Depth * sizeof(void*)];
			for (int k = 0; k < Depth; k++)
			{
				T** slice = Data3D[k] = new T*[Height * sizeof(void*)];
				T* rowPointer = slicePointer;
				for (int j = 0; j < Height; j++) { slice[j] = rowPointer; rowPointer += DY; }
				slicePointer += DZ;
			}
			referenceCount = new int[1];
			referenceCount[0] = 1;
		}
		Image(int width, int height) : Image(width, height, 1) { }
		Image(int width) : Image(width, 1, 1) { }
		Image(void* sourceData, int width, int height, int depth) :
			DX(1),
			DY(width),
			DZ(width * height),
			Width(width),
			Height(height),
			Depth(depth),
			OffsetX(0),
			OffsetY(0),
			OffsetZ(0),
			StartX(0),
			StartY(0),
			StartZ(0),
			EndX(Width * DX),
			EndY(Height * DY),
			EndZ(Depth * DZ)
		{ 
			if (width < 1 || height < 1 || depth < 1) throw "invalid parameters when allocating image";
			int totalSize = width * height * depth;
			DataArray = (T*)sourceData;
			RootDataArray = DataArray;

			//allocate 3D array of image
			T* slicePointer = DataArray;
			Data3D = new T**[Depth * sizeof(void*)];
			for (int k = 0; k < Depth; k++)
			{
				T** slice = Data3D[k] = new T*[Height * sizeof(void*)];
				T* rowPointer = slicePointer;
				for (int j = 0; j < Height; j++) { slice[j] = rowPointer; rowPointer += DY; }
				slicePointer += DZ;
			}
			referenceCount = new int[1];
			referenceCount[0] = 2;
		}
		Image(void* sourceData, int width, int height) : Image(sourceData, width, height, 1) { }
		Image(void* sourceData, int width) : Image(sourceData, width, 1, 1) { }

		template<class U> Image& operator=(const Image<U> &rhs) {
			assert(rhs.Width == Width && rhs.Height == Height && rhs.Depth == Depth);
			for (int k = 0; k < rhs.Depth; k++)
			{
				U** rhsSlice = rhs.Data3D[k];
				T** slice = Data3D[k];
				for (int j = 0; j < rhs.Height; j++)
				{
					U* rhsRow = rhsSlice[j];
					T* row = slice[j];
					int t = 0;
					for (int i = 0; i != rhs.EndX; i += rhs.DX)
					{
						row[t] = rhsRow[i];
						t += DX;
					}
				}
			}
			return *this;
		}
		template<class U> Image& operator=(const U rhs) {
			for (int k = 0; k < Depth; k++)
			{
				T** slice = Data3D[k];
				for (int j = 0; j < Height; j++)
				{
					T* row = slice[j];
					int t = 0;
					for (int i = 0; i != EndX; i += DX)
					{
						row[t] = rhs;
						t += DX;
					}
				}
			}
			return *this;
		}
		template <class U> bool operator== (const Image<U>& rhs) {
			if (rhs.Width != Width || rhs.Height != Height || rhs.Depth != Depth) return false;
			for (int k = 0; k < rhs.Depth; k++)
			{
				U** rhsSlice = rhs.Data3D[k];
				T** slice = Data3D[k];
				for (int j = 0; j < rhs.Height; j++)
				{
					U* rhsRow = rhsSlice[j];
					T* row = slice[j];
					int t = 0;
					for (int i = 0; i != rhs.EndX; i += rhs.DX)
					{
						if (row[t] != rhsRow[i]) return false;
						t += DX;
					}
				}
			}
			return true;
		}
		template <class U> bool operator== (const U& rhs) {
			for (int k = 0; k < Depth; k++)
			{
				T** slice = Data3D[k];
				for (int j = 0; j < Height; j++)
				{
					T* row = slice[j];
					for (int i = 0; i != EndX; i += DX)
					{
						if (row[i] != rhs) return false;
					}
				}
			}
			return true;
		}
		template <class U> bool operator<  (const Image<U>& rhs) {
			assert(rhs.Width == Width && rhs.Height == Height && rhs.Depth == Depth);
			for (int k = 0; k < rhs.Depth; k++)
			{
				U** rhsSlice = rhs.Data3D[k];
				T** slice = Data3D[k];
				for (int j = 0; j < rhs.Height; j++)
				{
					U* rhsRow = rhsSlice[j];
					T* row = slice[j];
					int t = 0;
					for (int i = 0; i != rhs.EndX; i += rhs.DX)
					{
						if (row[t] >= rhsRow[i]) return false;
						t += DX;
					}
				}
			}
			return true;
		}
		template <class U> bool operator<  (const U& rhs) {
			for (int k = 0; k < Depth; k++)
			{
				T** slice = Data3D[k];
				for (int j = 0; j < Height; j++)
				{
					T* row = slice[j];
					for (int i = 0; i != EndX; i += DX)
					{
						if (row[i] >= rhs) return false;
					}
				}
			}
			return true;
		}
		Image operator-() { Image result(*this, true); result.MutableUnaryOp<std::negate<T>>(); return result; }
		template<class U> Image& operator+=(const Image<U>& rhs) { return MutableBinaryImageOp<std::plus<T>>(rhs); }
		template<class U> Image& operator+=(const U rhs) { return MutableBinaryScalarOp<std::plus<T>>(rhs); }
		template<class U> Image operator+(const Image<U>& rhs) { Image result(*this, true); result += rhs; return result; }
		template<class U> Image operator+(const U rhs) { Image result(*this, true); result += rhs; return result; }
		template<class U> Image& operator-=(const Image<U>& rhs) { return MutableBinaryImageOp<std::minus<T>>(rhs); }
		template<class U> Image& operator-=(const U rhs) { return MutableBinaryScalarOp<std::minus<T>>(rhs); }
		template<class U> Image operator-(const Image<U>& rhs) { Image result(*this, true); result -= rhs; return result; }
		template<class U> Image operator-(const U rhs) { Image result(*this, true); result -= rhs; return result; }
		template<class U> Image& operator*=(const Image<U>& rhs) { return MutableBinaryImageOp<std::multiplies<T>>(rhs); }
		template<class U> Image& operator*=(const U rhs) { return MutableBinaryScalarOp<std::multiplies<T>>(rhs); }
		template<class U> Image operator*(const Image<U>& rhs) { Image result(*this, true); result *= rhs; return result; }
		template<class U> Image operator*(const U rhs) { Image result(*this, true); result *= rhs; return result; }
		template<class U> Image& operator/=(const Image<U>& rhs) { return MutableBinaryImageOp<std::divides<T>>(rhs); }
		template<class U> Image& operator/=(const U rhs) { return MutableBinaryScalarOp<std::divides<T>>(rhs); }
		template<class U> Image operator/(const Image<U>& rhs) { Image result(*this, true); result /= rhs; return result; }
		template<class U> Image operator/(const U rhs) { Image result(*this, true); result /= rhs; return result; }
		template<class U> Image& operator%=(const Image<U>& rhs) { return MutableBinaryImageOp<std::modulus<T>>(rhs); }
		template<class U> Image& operator%=(const U rhs) { return MutableBinaryScalarOp<std::modulus<T>>(rhs); }
		template<class U> Image operator%(const Image<U>& rhs) { Image result(*this, true); result %= rhs; return result; }
		template<class U> Image operator%(const U rhs) { Image result(*this, true); result %= rhs; return result; }
		template<class U> Image& operator|=(const Image<U>& rhs) { return MutableBinaryImageOp<std::bit_or<T>>(rhs); }
		template<class U> Image& operator|=(const U rhs) { return MutableBinaryScalarOp<std::bit_or<T>>(rhs); }
		template<class U> Image operator|(const Image<U>& rhs) { Image result(*this, true); result |= rhs; return result; }
		template<class U> Image operator|(const U rhs) { Image result(*this, true); result |= rhs; return result; }
		template<class U> Image& operator&=(const Image<U>& rhs) { return MutableBinaryImageOp<std::bit_and<T>>(rhs); }
		template<class U> Image& operator&=(const U rhs) { return MutableBinaryScalarOp<std::bit_and<T>>(rhs); }
		template<class U> Image operator&(const Image<U>& rhs) { Image result(*this, true); result &= rhs; return result; }
		template<class U> Image operator&(const U rhs) { Image result(*this, true); result &= rhs; return result; }
		template<class U> Image& operator^=(const Image<U>& rhs) { return MutableBinaryImageOp<std::bit_xor<T>>(rhs); }
		template<class U> Image& operator^=(const U rhs) { return MutableBinaryScalarOp<std::bit_xor<T>>(rhs); }
		template<class U> Image operator^(const Image<U>& rhs) { Image result(*this, true); result ^= rhs; return result; }
		template<class U> Image operator^(const U rhs) { Image result(*this, true); result ^= rhs; return result; }
		template<class U> Image operator||(const Image<U>& rhs) { Image result(*this, true); result.MutableBinaryImageOp<std::logical_or<T>>(rhs); return result; }
		template<class U> Image operator||(const U rhs) { Image result(*this, true); result.MutableBinaryScalarOp<std::logical_or<T>>(rhs); return result; }
		template<class U> Image operator&&(const Image<U>& rhs) { Image result(*this, true); result.MutableBinaryImageOp<std::logical_and<T>>(rhs); return result; }
		template<class U> Image operator&&(const U rhs) { Image result(*this, true); result.MutableBinaryScalarOp<std::logical_and<T>>(rhs); return result; }
		Image operator!() { Image result(*this, true); result.MutableUnaryOp<std::logical_not<T>>(); return result; }
		template <class U> bool operator!= (const Image<U>& rhs) { return !(*this == rhs); }
		template <class U> bool operator!= (const U& rhs) { return !(*this == rhs); }
		template <class U> bool operator<= (const Image<U>& rhs) { return !(rhs < *this); }
		template <class U> bool operator<= (const U& rhs) { return !(rhs < *this); }
		template <class U> bool operator>  (const Image<U>& rhs) { return (rhs < *this); }
		template <class U> bool operator>  (const U& rhs) { return (rhs < *this); }
		template <class U> bool operator>= (const Image<U>& rhs) { return !(*this < rhs); }
		template <class U> bool operator>= (const U& rhs) { return !(*this < rhs); }

		//misc methods
		Image ToIntegralImage()
		{
			Image& result = *this;
			T** prevSlice = NULL;
			for (int k = 0; k < result.Depth; k++)
			{
				T** resultSlice = result.Data3D[k];
				if (prevSlice == NULL)
				{
					T* prevRow = NULL;
					for (int j = 0; j < result.Height; j++)
					{
						T* resultRow = resultSlice[j];
						double rowSum = 0;
						if (prevRow == NULL) for (int i = 0; i != result.EndX; i += result.DX) resultRow[i] = (T)(rowSum += resultRow[i]);
						else for (int i = 0; i != result.EndX; i += result.DX) resultRow[i] = (T)(prevRow[i] + (rowSum += resultRow[i]));
						prevRow = resultRow;
					}
				}
				else
				{
					T* prevRow = NULL;
					T* prevSlicePrevRow = NULL;
					for (int j = 0; j < result.Height; j++)
					{
						T* prevSliceRow = prevSlice[j];
						T* resultRow = resultSlice[j];
						double rowSum = 0;
						if (prevRow == NULL) for (int i = 0; i != result.EndX; i += result.DX) resultRow[i] = (T)(prevSliceRow[i] + (rowSum += resultRow[i]));
						else for (int i = 0; i != result.EndX; i += result.DX) resultRow[i] = (T)(prevSliceRow[i] - prevSlicePrevRow[i] + prevRow[i] + (rowSum += resultRow[i]));
						prevRow = resultRow;
						prevSlicePrevRow = prevSliceRow;
					}
				}
				prevSlice = resultSlice;
			}
			return result;
		}
		long size() { return Width * Height * Depth; }
		std::string ToString()
		{
			std::ostringstream sb;
			sb << std::fixed << std::setprecision(2);
			for (int j = 0; j < Height; j++)
			{
				for (int k = 0; k < Depth; k++)
				{
					T* row = Data3D[k][j];
					for (int i = 0; i != EndX; i += DX)
					{
						if (i != 0) sb << ", ";
						sb << (double)row[i];
					}
					if (k != Depth - 1) sb << "  |  ";
				}
				sb << std::endl;
			}
			return sb.str();
		}
		std::string CurrentState()
		{
			long start = (long)DataArray;
			long t = start - (long)RootDataArray;

			std::ostringstream sb;
			sb << "FullDataArray : " << t << std::endl <<
				"Data3D : " << ((long)(Data3D[0][0]) - start) << std::endl <<
				"StartX : " << StartX << std::endl <<
				"StartY : " << StartY << std::endl <<
				"StartZ : " << StartZ << std::endl <<
				"OffsetX : " << OffsetX << std::endl <<
				"OffsetY : " << OffsetY << std::endl <<
				"OffsetZ : " << OffsetZ << std::endl <<
				"EndX : " << EndX << std::endl <<
				"EndY : " << EndY << std::endl <<
				"EndZ : " << EndZ << std::endl <<
				"DX : " << DX << std::endl <<
				"DY : " << DY << std::endl <<
				"DZ : " << DZ << std::endl <<
				"Width : " << Width << std::endl <<
				"Height : " << Height << std::endl <<
				"Depth : " << Depth << std::endl;
			return sb.str();
		}

		//iterator methods
		iterator begin() { return iterator(*this, 0, 0, 0, Data3D[0], Data3D[0][0]); }
		iterator end() { return iterator(*this, 0, 0, Depth, NULL, NULL); }

		//Destructors and destruction methods
		~Image()
		{
			for (int k = 0; k < Depth; k++) delete[] Data3D[k];
			delete[] Data3D;
			referenceCount[0]--;
			if (referenceCount[0] == 0) {
				//cout << "delete rootDataArray" << endl;
				delete[] RootDataArray;
				delete[] referenceCount;
			}
		}

	protected:
		//helper methods
		template<class U> void InitDeepCopy(Image<U>& source) {
			assert(source.Width == Width && source.Height == Height && source.Depth == Depth);
			for (int k = 0; k < source.Depth; k++)
			{
				U** sourceSlice = source.Data3D[k];
				T** slice = Data3D[k];
				for (int j = 0; j < source.Height; j++)
				{
					U* sourceRow = sourceSlice[j];
					T* row = slice[j];
					int t = 0;
					for (int i = 0; i != source.EndX; i += source.DX)
					{
						row[t] = (T)sourceRow[i];
						t += DX;
					}
				}
			}
		}
		template<class Op, class U> Image& MutableBinaryImageOp(const Image<U>& rhs) {
			assert(rhs.Width == Width && rhs.Height == Height && rhs.Depth == Depth);
			Op o;
			for (int k = 0; k < rhs.Depth; k++)
			{
				U** rhsSlice = rhs.Data3D[k];
				T** slice = Data3D[k];
				for (int j = 0; j < rhs.Height; j++)
				{
					U* rhsRow = rhsSlice[j];
					T* row = slice[j];
					int t = 0;
					for (int i = 0; i != rhs.EndX; i += rhs.DX)
					{
						row[t] = (T)o(row[t], rhsRow[i]);
						t += DX;
					}
				}
			}
			return *this;
		}
		template<class Op, class U> Image& MutableBinaryScalarOp(const U rhs) {
			Op o;
			for (int k = 0; k < Depth; k++)
			{
				T** slice = Data3D[k];
				for (int j = 0; j < Height; j++)
				{
					T* row = slice[j];
					int t = 0;
					for (int i = 0; i != EndX; i += DX)
					{
						row[t] = (T)o(row[t], (T)rhs);
						t += DX;
					}
				}
			}
			return *this;
		}
		template<class Op> Image& MutableUnaryOp() {
			Op o;
			for (int k = 0; k < Depth; k++)
			{
				T** slice = Data3D[k];
				for (int j = 0; j < Height; j++)
				{
					T* row = slice[j];
					int t = 0;
					for (int i = 0; i != EndX; i += DX)
					{
						row[t] = (T)o(row[t]);
						t += DX;
					}
				}
			}
			return *this;
		}
		constexpr int _ceil(float num) { return (static_cast<float>(static_cast<int>(num)) == num) ? static_cast<int>(num) : static_cast<int>(num) + ((num > 0) ? 1 : 0); }
		template<typename T> constexpr auto _abs(const T & value) -> T { return (T{} < value) ? value : -value; }
		constexpr int _swapx(int valuex, int valuey, int valuez, bool swapXY, bool swapYZ, bool swapZX) { return swapXY ? ((swapZX ? (valuez) : (valuey))) : (swapZX ? (valuez) : (valuex)); }
		constexpr int _swapy(int valuex, int valuey, int valuez, bool swapXY, bool swapYZ, bool swapZX) { return swapXY ? (swapYZ ? (valuez) : (valuex)) : (swapYZ ? (valuez) : (valuey)); }
		constexpr int _swapz(int valuex, int valuey,  int valuez, bool swapXY,  bool swapYZ, bool swapZX) { return swapYZ ? (swapZX ? (valuex) : (valuey)) : (swapZX ? (valuex) : (valuez)); }
	public:
		__readonly T*** Data3D;
		__readonly int Width;
		__readonly int Height;
		__readonly int Depth;
		__readonly int DX;
		__readonly int DY;
		__readonly int DZ;
		__readonly int EndX;
		__readonly int EndY;
		__readonly int EndZ;

	protected:
		//internal data
		__readonly int* referenceCount;
		__readonly T* RootDataArray;
		__readonly T* DataArray;
		__readonly int OffsetX;
		__readonly int OffsetY;
		__readonly int OffsetZ;
		__readonly int StartX; //note: StartX, StartY, and StartZ are listed after DX, DY, and DZ so they will get initialized last in constructor initialization list
		__readonly int StartY;
		__readonly int StartZ;
	};

	//operator overloads
	template<class T> std::ostream& operator<<(std::ostream& out, Image<T>& r) { return out << r.ToString(); }
	template <class T, class U> bool operator<(const T& lhs, const Image<U>& rhs) {
		for (int k = 0; k < rhs.Depth; k++)
		{
			U** slice = rhs.Data3D[k];
			for (int j = 0; j < rhs.Height; j++)
			{
				U* row = slice[j];
				for (int i = 0; i != rhs.EndX; i += rhs.DX)
				{
					if (lhs >= row[i]) return false;
				}
			}
		}
		return true;
	}
}