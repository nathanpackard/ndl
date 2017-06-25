#pragma once
// Conversion to a Template based method by Nathan Packard, 2009
// Copyright (C) 2009   Nathan Packard   <nathanpackard@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as 
// published by the Free Software Foundation; either version 3 of the 
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public 
// License along with this program; if not, see 
// <http://www.gnu.org/licenses/>.

/* Copyright (c) 2009 the authors listed at the following URL, and/or
the authors of referenced articles or incorporated external code:
http://en.literateprograms.org/Cooley-Tukey_FFT_algorithm_(C)?action=history&offset=20081117110818

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Retrieved from: http://en.literateprograms.org/Cooley-Tukey_FFT_algorithm_(C)?oldid=15458
*/

#include <math.h>
#include <cstdlib>
#include <complex>

namespace ndl
{
	namespace fft
	{
		template <typename T>
		T PI()
		{
			constexpr T pi = T(3.14159265358979323846264338327950288419716939937510);
			return pi;
		}

		template<class Real, int N, int SKIP>
		struct FFT_calculate {
			static void evaluate(Real* input, Real* output, Real* D, Real* twiddles) {
				/* for now we can use output as a scratch buffer */
				FFT_calculate<Real, N / 2, SKIP * 2>::evaluate(input, D + N, output, twiddles);
				FFT_calculate<Real, N / 2, SKIP * 2>::evaluate(input + SKIP * 2, D, output, twiddles);

				int skip2 = SKIP * 2;
				int kskip = 0;
				for (int k = 0; k < N; k += 2) {
					int k1 = k + 1;
					int kskip1 = kskip + 1;
					Real Rre = D[k];
					D[k] = twiddles[kskip] * D[k] - twiddles[kskip1] * D[k1];
					D[k1] = twiddles[kskip] * D[k1] + twiddles[kskip1] * Rre;
					kskip += skip2;
				}

				for (int k = 0; k < N; k += 2) {
					int k1 = k + 1;
					output[k] = D[k + N] + D[k];
					output[k1] = D[k1 + N] + D[k1];
					output[k + N] = D[k + N] - D[k];
					output[k1 + N] = D[k1 + N] - D[k1];
				}
			}
		};

		template<class Real, int SKIP>
		struct FFT_calculate<Real, 4, SKIP> {
			static void evaluate(Real* input, Real* output, Real* D, Real* twiddles) {

				FFT_calculate<Real, 2, SKIP * 2>::evaluate(input, D + 4, output, twiddles);
				FFT_calculate<Real, 2, SKIP * 2>::evaluate(input + SKIP * 2, D, output, twiddles);

				Real Rre = D[0];
				D[0] = twiddles[0] * D[0] - twiddles[1] * D[1];
				D[1] = twiddles[0] * D[1] + twiddles[1] * Rre;
				Real Rre2 = D[2];
				D[2] = twiddles[2 * SKIP] * D[2] - twiddles[2 * SKIP + 1] * D[3];
				D[3] = twiddles[2 * SKIP] * D[3] + twiddles[2 * SKIP + 1] * Rre2;
				output[0] = D[4] + D[0];
				output[1] = D[5] + D[1];
				output[4] = D[4] - D[0];
				output[5] = D[5] - D[1];
				output[2] = D[6] + D[2];
				output[3] = D[7] + D[3];
				output[6] = D[6] - D[2];
				output[7] = D[7] - D[3];
			}
		};

		template<class Real, int SKIP>
		struct FFT_calculate<Real, 2, SKIP> {
			static void evaluate(Real* input, Real* output, Real* D, Real* twiddles) {
				D[0] = input[SKIP * 2];
				D[1] = input[SKIP * 2 + 1];
				D[2] = input[0];
				D[3] = input[1];
				D[0] = twiddles[0] * input[SKIP * 2] - twiddles[1] * input[SKIP * 2 + 1];
				D[1] = twiddles[0] * input[SKIP * 2 + 1] + twiddles[1] * input[SKIP * 2];
				output[0] = D[2] + D[0];
				output[1] = D[3] + D[1];
				output[2] = D[2] - D[0];
				output[3] = D[3] - D[1];
			}
		};

		template<class Real, int SKIP>
		struct FFT_calculate<Real, 1, SKIP> {
			static void evaluate(Real* input, Real* output, Real* D, Real* twiddles) {
				output[0] = input[0];
				output[1] = input[1];
			}
		};

		//main class
		template<class Real>
		class FFT {
		public:
			//maxsize_pow_of_2 is the largest sample size,
			FFT(int maxsize_pow_of_2, Real* ScratchBufferOfSizeNTimesFour) {
				fft_twiddles = ScratchBufferOfSizeNTimesFour;
				fft_twiddles2 = ScratchBufferOfSizeNTimesFour + maxsize_pow_of_2;
				scratch = ScratchBufferOfSizeNTimesFour + maxsize_pow_of_2 * 2;
				N = 0;
			}
			void fft(int n, std::complex<Real>* input, std::complex<Real>* output) {
				Real* fft_input = reinterpret_cast<Real*>(input);
				Real* fft_output = reinterpret_cast<Real*>(output);
				if (N != n) update_complex_twiddles(n);
				_fft(N, fft_input, fft_output, fft_twiddles);
			}
			void ifft(int n, std::complex<Real>* input, std::complex<Real>* output) {
				Real* fft_input = reinterpret_cast<Real*>(input);
				Real* fft_output = reinterpret_cast<Real*>(output);
				if (N != n) update_complex_twiddles(n);
				_fft(N, fft_input, fft_output, fft_twiddles2);
				int n2 = n * 2;
				Real inv = 1.0 / n;
				for (int k = 0; k < n2; ++k) fft_output[k] *= inv;
			}
		protected:
			Real* fft_twiddles;
			Real* fft_twiddles2;
			Real* scratch;
			int N;
			void update_complex_twiddles(int num) {
				N = num;
				double delta = -2.0*PI<Real>() / N;
				int nover2 = N / 2;
				double temp = 0;
				for (int k = 0, k2 = 0; k != nover2; ++k, k2 += 2) {
					fft_twiddles[k2] = cos(temp);       //for forward transform
					fft_twiddles[k2 + 1] = sin(temp);   //for forward transform
					fft_twiddles2[k2] = fft_twiddles[k2];           //for reverse transform
					fft_twiddles2[k2 + 1] = -fft_twiddles[k2 + 1];  //for reverse transform
					temp += delta;
				}
			}
			void _fft(int n, Real* input, Real* output, Real* twiddles) {
				switch (n) {
				case 1: {
					FFT_calculate<Real, 1, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 2: {
					FFT_calculate<Real, 2, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 4: {
					FFT_calculate<Real, 4, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 8: {
					FFT_calculate<Real, 8, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 16: {
					FFT_calculate<Real, 16, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 32: {
					FFT_calculate<Real, 32, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 64: {
					FFT_calculate<Real, 64, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 128: {
					FFT_calculate<Real, 128, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 256: {
					FFT_calculate<Real, 256, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 512: {
					FFT_calculate<Real, 512, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 1024: {
					FFT_calculate<Real, 1024, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 2048: {
					FFT_calculate<Real, 2048, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 4096: {
					FFT_calculate<Real, 4096, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 8192: {
					FFT_calculate<Real, 8192, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 16384: {
					FFT_calculate<Real, 16384, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 32768: {
					FFT_calculate<Real, 32768, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 65536: {
					FFT_calculate<Real, 65536, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 131072: {
					FFT_calculate<Real, 131072, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 262144: {
					FFT_calculate<Real, 262144, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 524288: {
					FFT_calculate<Real, 524288, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 1048576: {
					FFT_calculate<Real, 1048576, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 2097152: {
					FFT_calculate<Real, 2097152, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 4194304: {
					FFT_calculate<Real, 4194304, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 8388608: {
					FFT_calculate<Real, 8388608, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 16777216: {
					FFT_calculate<Real, 16777216, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 33554432: {
					FFT_calculate<Real, 33554432, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 67108864: {
					FFT_calculate<Real, 67108864, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				case 134217728: {
					FFT_calculate<Real, 134217728, 1>::evaluate(input, output, scratch, twiddles);
					break;
				}
				}
			}
		};

		template<class Real>
		class FFTReal : public FFT<Real> {
		public:
			//maxsize_pow_of_2 is the largest sample size,
			FFTReal(int maxsize_pow_of_2, Real* ScratchBufferOfSizeNTimesFive) : 
				FFT<Real>(maxsize_pow_of_2, ScratchBufferOfSizeNTimesFive)
			{
				scratch2 = ScratchBufferOfSizeNTimesFive + maxsize_pow_of_2 * 4;
				iRealN = RealN = 0;
			}
			void fft(int n, Real* input, std::complex<Real>* output) {
				Real* fft_input = reinterpret_cast<Real*>(input);
				Real* fft_output = reinterpret_cast<Real*>(output);
				if (n > 1) {
					int nover2 = n / 2;
					if (RealN != nover2) update_real_ftwiddles(nover2);
					_fft(nover2, fft_input, fft_output, fft_twiddles);

					//move to later half
					for (int i = 0; i < n; i++) {
						fft_output[n + i] = fft_output[i];
						fft_output[i] = 0;
					}

					//now postprocess
					fft_output[0] = 0.5*(fft_output[n] + fft_output[n] + (fft_output[n + 1] + fft_output[n + 1]));
					fft_output[1] = 0.5*(fft_output[n + 1] - fft_output[n + 1] - (fft_output[n] - fft_output[n]));
					for (int k = 1; k < nover2; k++) {
						int k2 = k * 2; //0 to n-2
						fft_output[k2] = 0.5*(fft_output[n + k2] + fft_output[n + n - k2] + fft_twiddles2[k2 + 1] * (fft_output[n + k2] - fft_output[n + n - k2]) + fft_twiddles2[k2] * (fft_output[n + k2 + 1] + fft_output[n + 1 + n - k2]));
						fft_output[k2 + 1] = 0.5*(fft_output[n + k2 + 1] - fft_output[n + 1 + n - k2] + fft_twiddles2[k2 + 1] * (fft_output[n + k2 + 1] + fft_output[n + 1 + n - k2]) - fft_twiddles2[k2] * (fft_output[n + k2] - fft_output[n + n - k2]));
					}

					//handle midpoint (n/2)
					fft_output[n] = fft_output[n] - fft_output[n + 1];
					fft_output[n + 1] = 0;

					//make mirror image (n/2 + 1 .. N-1)
					for (int k = nover2 + 1; k < n; k++) {
						int k2 = k * 2;
						fft_output[k2] = fft_output[2 * n - k2];
						fft_output[k2 + 1] = -fft_output[1 + 2 * n - k2];
					}
				}
				else if (n == 1) {
					fft_output[0] = fft_input[0];
					fft_output[1] = 0;
				}
			}
			void ifft(int n, std::complex<Real>* input, Real* output) {
				Real* fft_input = reinterpret_cast<Real*>(input);
				Real* fft_output = reinterpret_cast<Real*>(output);
				if (n > 1) {
					//preprocess
					int nover2 = n / 2;
					if (iRealN != nover2) update_real_itwiddles(nover2);

					for (int k = 0; k < nover2; k++) {
						int k2 = k * 2; //0 to n-2
						float real = 0.5*(fft_input[k2] + fft_input[n - k2]
							- (fft_input[k2 + 1] + fft_input[1 + n - k2])*fft_twiddles2[k2]
							+ (-fft_input[k2] + fft_input[n - k2])*fft_twiddles2[k2 + 1]
							);

						float imag = 0.5*(fft_input[k2 + 1] - fft_input[1 + n - k2]
							+ (fft_input[k2] - fft_input[n - k2])*fft_twiddles2[k2]
							- (fft_input[k2 + 1] + fft_input[1 + n - k2])*fft_twiddles2[k2 + 1]
							);

						scratch2[k2] = real;
						scratch2[k2 + 1] = imag;
					}

					_fft(nover2, scratch2, fft_output, fft_twiddles);

					//post process
					Real inv = 2.0 / n;
					for (int k = 0; k < n; ++k) fft_output[k] *= inv;
				}
				else if (n == 1) 
					fft_output[0] = fft_input[0];
			}
		protected:
			int RealN, iRealN;
			Real* scratch2;
			void update_real_ftwiddles(int num) {
				RealN = num;
				double delta = -2.0*PI<Real>() / RealN;
				int nover2 = RealN / 2;
				double temp = 0;
				for (int k = 0, k2 = 0; k != nover2; ++k, k2 += 2) {
					fft_twiddles[k2] = cos(temp);       //for forward transform
					fft_twiddles[k2 + 1] = sin(temp);   //for forward transform
					temp += delta;
				}

				RealN *= 2;
				delta = -2.0*PI<Real>() / RealN;
				nover2 = RealN / 2;
				temp = 0;
				for (int k = 0, k2 = 0; k != nover2; ++k, k2 += 2) {
					fft_twiddles2[k2] = cos(temp);      //for post processing on forward transform
					fft_twiddles2[k2 + 1] = sin(temp);  //for post processing on forward transform
					temp += delta;
				}
				RealN /= 2;
			}
			void update_real_itwiddles(int num) {
				iRealN = num;
				double delta = 2.0*PI<Real>() / iRealN;
				int nover2 = iRealN / 2;
				double temp = 0;
				for (int k = 0, k2 = 0; k != nover2; ++k, k2 += 2) {
					fft_twiddles[k2] = cos(temp);       //for inverse transform
					fft_twiddles[k2 + 1] = sin(temp);   //for inverse transform
					temp += delta;
				}

				iRealN *= 2;
				delta = 2.0*PI<Real>() / iRealN;
				nover2 = iRealN / 2;
				temp = 0;
				for (int k = 0, k2 = 0; k != nover2; ++k, k2 += 2) {
					fft_twiddles2[k2] = cos(temp);      //for pre processing on inverse transform
					fft_twiddles2[k2 + 1] = sin(temp);  //for pre processing on inverse transform
					temp += delta;
				}
				iRealN /= 2;
			}
		};
	}
}
