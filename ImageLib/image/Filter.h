#pragma once
#include "Image.h";
namespace ImageLib
{
	constexpr double _pow(double x, int y) { return y == 0 ? 1.0 : x * _pow(x, y - 1); }
	constexpr int _factorial(int x) { return x == 0 ? 1 : x * _factorial(x - 1); }
	constexpr double _exp(double x) { return 1.0 + x + _pow(x, 2) / _factorial(2) + _pow(x, 3) / _factorial(3) + _pow(x, 4) / _factorial(4) + _pow(x, 5) / _factorial(5) + _pow(x, 6) / _factorial(6) + _pow(x, 7) / _factorial(7) + _pow(x, 8) / _factorial(8) + _pow(x, 9) / _factorial(9); }
	constexpr float _gaussian(double sigma, int position) { return (float)_exp(-((float)(position * position) / (2 * sigma * sigma))); }
	constexpr int _kernelSize(float sigma) { return (int)_ceil(3.0f * sigma); }
	constexpr int _kernelRadius(float sigma) { return _kernelSize(sigma) / 2; }
	constexpr int _ceil(float num) { return (static_cast<float>(static_cast<int>(num)) == num) ? static_cast<int>(num) : static_cast<int>(num) + ((num > 0) ? 1 : 0); }
	template<class T> constexpr auto _abs(const T & value) -> T { return (T{} < value) ? value : -value; }
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


}