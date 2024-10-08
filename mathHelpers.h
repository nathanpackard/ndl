#pragma once
#include <array>
#include <limits> // nan
#include <vector>
namespace ndl
{
	template<unsigned M, unsigned N, unsigned B, unsigned A>
	struct SinCosSeries {
		static double value() {
			return 1 - (A*M_PI / B)*(A*M_PI / B) / M / (M + 1)
				*SinCosSeries<M + 2, N, B, A>::value();
		}
	};

	template<unsigned N, unsigned B, unsigned A>
	struct SinCosSeries<N, N, B, A> {
		static double value() { return 1.; }
	};

	template<unsigned B, unsigned A, typename T = double>
	struct Sin;

	template<unsigned B, unsigned A>
	struct Sin<B, A, float> {
		static float value() {
			return (A*M_PI / B)*SinCosSeries<2, 24, B, A>::value();
		}
	};

	template<unsigned B, unsigned A>
	struct Sin<B, A, double> {
		static double value() {
			return (A*M_PI / B)*SinCosSeries<2, 34, B, A>::value();
		}
	};

	template<unsigned B, unsigned A, typename T = double>
	struct Cos;

	template<unsigned B, unsigned A>
	struct Cos<B, A, float> {
		static float value() {
			return SinCosSeries<1, 23, B, A>::value();
		}
	};

	template<unsigned B, unsigned A>
	struct Cos<B, A, double> {
		static double value() {
			return SinCosSeries<1, 33, B, A>::value();
		}
	};

	static constexpr double _pow(double x, int y) { return y == 0 ? 1.0 : x * _pow(x, y - 1); }
	static constexpr int _factorial(int x) { return x == 0 ? 1 : x * _factorial(x - 1); }
	static constexpr double _exp(double x) { return 1.0 + x + _pow(x, 2) / _factorial(2) + _pow(x, 3) / _factorial(3) + _pow(x, 4) / _factorial(4) + _pow(x, 5) / _factorial(5) + _pow(x, 6) / _factorial(6) + _pow(x, 7) / _factorial(7) + _pow(x, 8) / _factorial(8) + _pow(x, 9) / _factorial(9); }
	static constexpr double _gaussian(double sigma, double position) { return (double)_exp(-((position * position) / (2 * sigma * sigma))); }
	static constexpr int _ceil(double num) { return (static_cast<double>(static_cast<int>(num)) == num) ? static_cast<int>(num) : static_cast<int>(num) + ((num > 0) ? 1 : 0); }
	static constexpr int _kernelSize(double sigma) { return (int)_ceil(3.0f * sigma); }
	static constexpr int _kernelRadius(double sigma) { return _kernelSize(sigma) / 2; }
	static constexpr int _swapx(int valuex, int valuey, int valuez, bool swapXY, bool swapYZ, bool swapZX) { return swapXY ? ((swapZX ? (valuez) : (valuey))) : (swapZX ? (valuez) : (valuex)); }
	static constexpr int _swapy(int valuex, int valuey, int valuez, bool swapXY, bool swapYZ, bool swapZX) { return swapXY ? (swapYZ ? (valuez) : (valuex)) : (swapYZ ? (valuez) : (valuey)); }
	static constexpr int _swapz(int valuex, int valuey, int valuez, bool swapXY, bool swapYZ, bool swapZX) { return swapYZ ? (swapZX ? (valuex) : (valuey)) : (swapZX ? (valuex) : (valuez)); }
	static constexpr int _reflect(int M, int x) { return (x < 0) ? (-x - 1) : ((x >= M) ? (2 * M - x - 1) : x); }
	static constexpr int _wrap(int M, int x) { return (x < 0) ? (x + M) : ((x >= M) ? (x - M) : x); }
	static constexpr int _clamp(int M, int x) { return (x < 0) ? 0 : ((x >= M) ? (M - 1) : x); }
	template <class T> static constexpr auto _abs(const T & value) -> T { return (T{} < value) ? value : -value; }
    template <class T> static constexpr int _sgn(T val) { return (T(0) < val) - (val < T(0)); }
}