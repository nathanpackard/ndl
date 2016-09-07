#pragma once
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
#include <functional>
#include <iomanip>

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

	static constexpr double _pow(double x, int y) { return y == 0 ? 1.0 : x * _pow(x, y - 1); }
	static constexpr int _factorial(int x) { return x == 0 ? 1 : x * _factorial(x - 1); }
	static constexpr double _exp(double x) { return 1.0 + x + _pow(x, 2) / _factorial(2) + _pow(x, 3) / _factorial(3) + _pow(x, 4) / _factorial(4) + _pow(x, 5) / _factorial(5) + _pow(x, 6) / _factorial(6) + _pow(x, 7) / _factorial(7) + _pow(x, 8) / _factorial(8) + _pow(x, 9) / _factorial(9); }
	static constexpr float _gaussian(double sigma, int position) { return (float)_exp(-((float)(position * position) / (2 * sigma * sigma))); }
	static constexpr int _ceil(float num) { return (static_cast<float>(static_cast<int>(num)) == num) ? static_cast<int>(num) : static_cast<int>(num) + ((num > 0) ? 1 : 0); }
	static constexpr int _kernelSize(float sigma) { return (int)_ceil(3.0f * sigma); }
	static constexpr int _kernelRadius(float sigma) { return _kernelSize(sigma) / 2; }
	static constexpr int _swapx(int valuex, int valuey, int valuez, bool swapXY, bool swapYZ, bool swapZX) { return swapXY ? ((swapZX ? (valuez) : (valuey))) : (swapZX ? (valuez) : (valuex)); }
	static constexpr int _swapy(int valuex, int valuey, int valuez, bool swapXY, bool swapYZ, bool swapZX) { return swapXY ? (swapYZ ? (valuez) : (valuex)) : (swapYZ ? (valuez) : (valuey)); }
	static constexpr int _swapz(int valuex, int valuey, int valuez, bool swapXY, bool swapYZ, bool swapZX) { return swapYZ ? (swapZX ? (valuex) : (valuey)) : (swapZX ? (valuex) : (valuez)); }
	static constexpr int _reflect(int M, int x) { return (x < 0) ? (-x - 1) : ((x >= M) ? (2 * M - x - 1) : x); }
	static constexpr int _wrap(int M, int x) { return (x < 0) ? (x + M) : ((x >= M) ? (x - M) : x); }
	static constexpr int _clamp(int M, int x) { return (x < 0) ? 0 : ((x >= M) ? (M - 1) : x); }
	template<typename T> static constexpr auto _abs(const T & value) -> T { return (T{} < value) ? value : -value; }

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
			iterator(Image& parent, int z) : X(0), Y(0), Z(z), _Slice(parent.Data()), _Row(parent.Data()), _myImage(parent) {}
			self_type operator++() {
				self_type i = *this;
				if ((X += _myImage.DX) != _myImage.EndX) {}
				else if (++Y < _myImage.Height)
				{
					X = _myImage.StartX;
					_Row += _myImage.DY;
				}
				else if (++Z < _myImage.Depth)
				{
					X = _myImage.StartX;
					Y = _myImage.StartY;
					_Slice += _myImage.DZ;
					_Row = _Slice;
				}
				return i;
			}
			self_type operator++(int junk) {
				if ((X += _myImage.DX) != _myImage.EndX) {}
				else if (++Y < _myImage.Height)
				{
					X = _myImage.StartX;
					_Row += DY;
				}
				else if (++Z < _myImage.Depth)
				{
					Y = _myImage.StartY;
					X = _myImage.StartX;
					_Slice += DZ;
					_Row = _Slice;
				}
				return *this;
			}
			T& operator*() { return _Row[X]; }
			T* operator->() { return _Row + X; }
			T* Pointer() { return _Row + X; }
			T& operator[](int index) { return _Row[X + index]; }
			const T& operator[](int index) const { return _Row[X + index]; }

			//reflected accessors
			T& operator()(int dx, const _x_reflect_t dummy) { return _Row[_reflect(_myImage.Width, X + dx) * _myImage.DX]; }
			T& operator()(int dy, const _y_reflect_t dummy) { return _Row[X + (_reflect(_myImage.Height, Y + dy) - Y) * _myImage.DY]; }
			T& operator()(int dz, const _z_reflect_t dummy) { return _Row[X + (_reflect(_myImage.Depth, Z + dz) - Z) * _myImage.DZ]; }
			T& operator()(int dx, int dy, const _xy_reflect_t dummy) { return _Row[(_reflect(_myImage.Height, Y + dy) - Y) * _myImage.DY + _reflect(_myImage.Width, X + dx) * _myImage.DX]; }
			T& operator()(int dx, int dz, const _xz_reflect_t dummy) { return _Row[(_reflect(_myImage.Depth, Z + dz) - Z) * _myImage.DZ + _reflect(_myImage.Width, X + dx) * _myImage.DX]; }
			T& operator()(int dy, int dz, const _yz_reflect_t dummy) { return _Row[X + (_reflect(_myImage.Depth, Z + dz) - Z) * _myImage.DZ + (_reflect(_myImage.Height, Y + dy) - Y) * _myImage.DY]; }
			T& operator()(int dx, int dy, int dz, const _xyz_reflect_t dummy) { return _Row[(_reflect(_myImage.Depth, Z + dz) - Z) * _myImage.DZ + (_reflect(_myImage.Height, Y + dy) - Y) * _myImage.DY + _reflect(_myImage.Width, X + dx) * _myImage.DX]; }

			//circular reflected accessors
			T& operator()(int dx, const _x_wrap_t dummy) { return _Row[_wrap(_myImage.Width, X + dx) * _myImage.DX]; }
			T& operator()(int dy, const _y_wrap_t dummy) { return _Row[X + (_wrap(_myImage.Height, Y + dy) - Y) * _myImage.DY]; }
			T& operator()(int dz, const _z_wrap_t dummy) { return _Row[X + (_wrap(_myImage.Depth, Z + dz) - Z) * _myImage.DZ]; }
			T& operator()(int dx, int dy, const _xy_wrap_t dummy) { return _Row[(_wrap(_myImage.Height, Y + dy) - Y) * _myImage.DY + _wrap(_myImage.Width, X + dx) * _myImage.DX]; }
			T& operator()(int dx, int dz, const _xz_wrap_t dummy) { return _Row[(_wrap(_myImage.Depth, Z + dz) - Z) * _myImage.DZ + _wrap(_myImage.Width, X + dx) * _myImage.DX]; }
			T& operator()(int dy, int dz, const _yz_wrap_t dummy) { return _Row[X + (_wrap(_myImage.Depth, Z + dz) - Z) * _myImage.DZ + (_wrap(_myImage.Height, Y + dy) - Y) * _myImage.DY]; }
			T& operator()(int dx, int dy, int dz, const _xyz_wrap_t dummy) { return _Row[(_wrap(_myImage.Depth, Z + dz) - Z) * _myImage.DZ + (_wrap(_myImage.Height, Y + dy) - Y) * _myImage.DY + _wrap(_myImage.Width, X + dx) * _myImage.DX]; }

			//clamped accessors
			T& operator()(int dx, const _x_clamp_t dummy) { return _Row[_clamp(_myImage.Width, X + dx) * _myImage.DX]; }
			T& operator()(int dy, const _y_clamp_t dummy) { return _Row[X + (_clamp(_myImage.Height, Y + dy) - Y) * _myImage.DY]; }
			T& operator()(int dz, const _z_clamp_t dummy) { return _Row[X + (_clamp(_myImage.Depth, Z + dz) - Z) * _myImage.DZ]; }
			T& operator()(int dx, int dy, const _xy_clamp_t dummy) { return _Row[(_clamp(_myImage.Height, Y + dy) - Y) * _myImage.DY + _clamp(_myImage.Width, X + dx) * _myImage.DX]; }
			T& operator()(int dx, int dz, const _xz_clamp_t dummy) { return _Row[(_clamp(_myImage.Depth, Z + dz) - Z) * _myImage.DZ + _clamp(_myImage.Width, X + dx) * _myImage.DX]; }
			T& operator()(int dy, int dz, const _yz_clamp_t dummy) { return _Row[X + (_clamp(_myImage.Depth, Z + dz) - Z) * _myImage.DZ + (_clamp(_myImage.Height, Y + dy) - Y) * _myImage.DY]; }
			T& operator()(int dx, int dy, int dz, const _xyz_clamp_t dummy) { return _Row[(_clamp(_myImage.Depth, Z + dz) - Z) * _myImage.DZ + (_clamp(_myImage.Height, Y + dy) - Y) * _myImage.DY + _clamp(_myImage.Width, X + dx) * _myImage.DX]; }

			bool operator==(const self_type& rhs) { return Z == _myImage.Depth; }
			bool operator!=(const self_type& rhs) { return Z != _myImage.Depth; }
			__readonly int X;
			__readonly int Y;
			__readonly int Z;
		private:
			T* _Row;
			T* _Slice;
			Image& _myImage;
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
			RootDataArray = source.RootDataArray;
			DataArray = RootDataArray + StartX * _abs(DX) + StartY * _abs(DY) + StartZ * _abs(DZ) + OffsetX * _abs(source.DX) + OffsetY * _abs(source.DY) + OffsetZ * _abs(source.DZ);
			referenceCount = source.referenceCount;
			referenceCount[0]++;
		}
		Image(Image& source, int offsetX, int offsetY, int width, int height, int dx, int dy, bool mirrorX = false, bool mirrorY = false, bool swapXY = false) : Image(source, offsetX, offsetY, 0, width, height, 1, dx, dy, 1, mirrorX, mirrorY, false, swapXY) { }
		Image(Image& source, int offsetX, int width, int dx, bool mirrorX = false) : Image(source, offsetX, 0, 0, width, 1, 1, dx, 1, 1, mirrorX) { }
		template<class U> Image(Image<U>& source) : Image(source.Width, source.Height, source.Depth) { 
			assert(source.Width == Width && source.Height == Height && source.Depth == Depth);
			auto sourceit = source.begin();
			for (auto it = begin(); it != end(); ++it, ++sourceit) *it = *sourceit;
		}
		Image(int width, int height, int depth) : 
			DX(1), DY(width), DZ(width * height), 
			Width(width), Height(height), Depth(depth),
			OffsetX(0), OffsetY(0),OffsetZ(0),
			StartX(0), StartY(0), StartZ(0),
			EndX(Width * DX), EndY(Height * DY), EndZ(Depth * DZ)
		{
			if (width < 1 || height < 1 || depth < 1) throw "invalid parameters when allocating image";
			int totalSize = width * height * depth;
			RootDataArray = new T[totalSize * sizeof(T)]();
			for (int i = 0; i < totalSize; i++) RootDataArray[i] = 0;
			DataArray = RootDataArray;
			referenceCount = new int[1];
			referenceCount[0] = 1;
		}
		Image(int width, int height) : Image(width, height, 1) { }
		Image(int width) : Image(width, 1, 1) { }
		Image(void* sourceData, int width, int height, int depth) :
			DX(1), DY(width), DZ(width * height),
			Width(width), Height(height), Depth(depth),
			OffsetX(0), OffsetY(0), OffsetZ(0),
			StartX(0), StartY(0), StartZ(0),
			EndX(Width * DX), EndY(Height * DY), EndZ(Depth * DZ)
		{ 
			if (width < 1 || height < 1 || depth < 1) throw "invalid parameters when allocating image";
			int totalSize = width * height * depth;
			DataArray = (T*)sourceData;
			RootDataArray = DataArray;
			referenceCount = new int[1];
			referenceCount[0] = 2;
		}
		Image(void* sourceData, int width, int height) : Image(sourceData, width, height, 1) { }
		Image(void* sourceData, int width) : Image(sourceData, width, 1, 1) { }

		template<class U> Image& operator=(const Image<U> &rhs) {
			assert(rhs.Width == Width && rhs.Height == Height && rhs.Depth == Depth);
			auto rhsit = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsit) *it = *rhsit;
			return *this;
		}
		template<class U> Image& operator=(const U rhs) {
			for (auto it = begin(); it != end(); ++it) *it = rhs;
			return *this;
		}
		template <class U> bool operator== (const Image<U>& rhs) {
			if (rhs.Width != Width || rhs.Height != Height || rhs.Depth != Depth) return false;
			auto rhsit = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsit) if (*it != *rhsit) return false;
			return true;
		}
		template <class U> bool operator== (const U& rhs) {
			for (auto it = begin(); it != end(); ++it) if (*it != rhs) return false;
			return true;
		}
		template <class U> bool operator<  (const Image<U>& rhs) {
			assert(rhs.Width == Width && rhs.Height == Height && rhs.Depth == Depth);
			auto rhsit = rhs.begin();
			for (auto it = begin(); it != end(); ++it, ++rhsit) if (*it >= *rhsit) return false;
			return true;
		}
		template <class U> bool operator<  (const U& rhs) {
			for (auto it = begin(); it != end(); ++it) if (*it >= rhs) return false;
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
		long size() { return Width * Height * Depth; }
		std::string ToString()
		{
			std::ostringstream sb;
			sb << std::fixed << std::setprecision(2);
			for (int j = 0; j < Height; j++)
			{
				for (int k = 0; k < Depth; k++)
				{
					for (int i = 0; i < Width; i ++)
					{
						if (i != 0) sb << ", ";
						sb << (double)at(i, j, k);
					}
					if (k != Depth - 1) sb << "  |  ";
				}
				sb << std::endl;
			}
			return sb.str();
		}
		std::string CurrentState()
		{
			std::ostringstream sb;
			sb << "DataArray : " << ((long)DataArray - (long)RootDataArray) << std::endl <<
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
		iterator begin() { return iterator(*this, 0); }
		iterator end() { return iterator(*this, Depth); }

		//basic accessors
		T operator()(int x, int y, int z) { return DataArray[z*DZ + y*DY + x*DX]; }
		const T& operator()(int x, int y, int z) const { return DataArray[z*DZ + y*DY + x*DX]; }
		T at(int x, int y, int z) { return DataArray[z*DZ + y*DY + x*DX]; }

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
		template<class Op, class U> Image& MutableBinaryImageOp(const Image<U>& rhs) {
			assert(rhs.Width == Width && rhs.Height == Height && rhs.Depth == Depth);
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
		__readonly T* DataArray;
		__readonly int* referenceCount;
		__readonly T* RootDataArray;
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
		auto rhsit = rhs.begin();
		for (auto rhsit = rhs.begin(); rhsit != rhs.end(); ++rhsit) if (lhs >= *rhsit) return false;
		return true;
	}

	//extension functions
	template<class T> Image<T> Reduce(Image<T>& image, bool w, bool h, bool d) {
		Image<T> s(image, 0, 0, 0, image.Width, image.Height, image.Depth, w ? 2 : 1, h ? 2 : 1, d ? 2 : 1);
		if (s.Width == image.Width) w = false;
		if (s.Height == image.Height) h = false;
		if (s.Depth == image.Depth) d = false;
		if (!w && !h && !d) return Image<T>(image, true); //return a deep copy
		Image<T> r(source.Width, source.Height, source.Depth);
		if (w) {
			if (h) {
				if (d) { for (auto si = s.begin(), ri = result.begin(); si != s.end(); ++si, ++ri) { *ri = (*si + si[s.DX] + si[s.DY] + si[s.DX + s.DY] + si[s.DZ] + si[s.DZ + s.DX] + si[s.DZ + s.DY] + si[s.DZ + s.DX + s.DY]) / 8; } }
				else { for (auto si = s.begin(), ri = result.begin(); si != s.end(); ++si, ++ri) { *ri = (*si + si[s.DX] + si[s.DY] + si[s.DX + s.DY]) / 4; } }
			}
			else {
				if (d) { for (auto si = s.begin(), ri = result.begin(); si != s.end(); ++si, ++ri) { *ri = (*si + si[s.DX] + si[s.DZ] + si[s.DZ + s.DX]) / 4; } }
				else { for (auto si = s.begin(), ri = result.begin(); si != s.end(); ++si, ++ri) { *ri = (*si + si[s.DX]) / 2; } }
			}
		}
		else {
			if (h) {
				if (d) { for (auto si = s.begin(), ri = result.begin(); si != s.end(); ++si, ++ri) { *ri = (*si + si[s.DY] + si[s.DZ] + si[s.DZ + s.DY]) / 4; } }
				else { for (auto si = s.begin(), ri = result.begin(); si != s.end(); ++si, ++ri) { *ri = (*si + si[s.DY]) / 2; } }
			}
			else {
				if (d) { for (auto si = s.begin(), ri = result.begin(); si != s.end(); ++si, ++ri) { *ri = (*si + si[s.DZ]) / 2; } }
				//skip last case because it was handled above
			}
		}
		return r;
	}
	template<class T> Image<T> Gaussian3(Image<T>& image, const double sigma) {
		Image<T> result(image);
		for (auto i = image.begin(), r = result.begin(); i != image.end(); ++i, ++r) {
			*r = *i * _gaussian(sigma, 0) + (i[roi1.DX] + i[-roi1.DX]) * _gaussian(sigma, 1);
		}
	}
	template<class T> Image<T> Gaussian5(Image<T>& image, const double sigma) {
		Image<T> result(image);
		for (auto i = image.begin(), r = result.begin(); i != image.end(); ++i, ++r) {
			*r = *i * _gaussian(sigma, 0) + (i[roi1.DX] + i[-roi1.DX]) * _gaussian(sigma, 1) + (i[roi1.DX * 2] + i[-roi1.DX * 2]) * _gaussian(sigma, 2);
		}
	}
	template<class T, class T2> Image<T> ToIntegralImage(Image<T2>& image)
	{
		Image<T> result = image;
		auto i = image.begin();
		for (auto r = result.begin(); r != result.end(); ++r, ++i) {
			if (r.X > 0 && r.Y > 0 && r.Z > 0) *r += r[-result.DX - result.DY - result.DZ];
			if (r.Z > 0) *r += r[-result.DZ];
			if (r.Y > 0) *r += r[-result.DY];
			if (r.X > 0) *r += r[-result.DX];
			if (r.X > 0 && r.Y > 0) *r -= r[-result.DX - result.DY];
			if (r.Y > 0 && r.Z > 0) *r -= r[-result.DY - result.DZ];
			if (r.X > 0 && r.Z > 0) *r -= r[-result.DX - result.DZ];
		}
		return result;
	}
	template<class T> T SampleIntegralImage(Image<T>& image) {
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