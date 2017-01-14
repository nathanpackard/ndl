#pragma once
#include <array>
namespace ImageLib
{
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
	template <class T> static constexpr auto _abs(const T & value) -> T { return (T{} < value) ? value : -value; }

	template <class T> T operator+(const T& a1, const T& a2) { T a; for (typename T::size_type i = 0; i < a1.size(); i++) a[i] = a1[i] + a2[i]; return a; }
	template <class T> T operator-(const T& a1, const T& a2) { T a; for (typename T::size_type i = 0; i < a1.size(); i++) a[i] = a1[i] - a2[i]; return a; }
	template <class T> T operator*(const T& a1, const T& a2) { T a; for (typename T::size_type i = 0; i < a1.size(); i++) a[i] = a1[i] * a2[i]; return a; }
	template <class T> T operator/(const T& a1, const T& a2) { T a; for (typename T::size_type i = 0; i < a1.size(); i++) a[i] = a1[i] / a2[i]; return a; }
}