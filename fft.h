#pragma once
// Conversion to a Template based method by Nathan Packard, 2009, 2017
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
#include "mathHelpers.h"

namespace ndl
{
	namespace fft
	{
		//template based loop unroller
		template <class Real, int N, int NOVER2> struct unroll {
			static inline void apply_forward_twiddles(Real* D) {
				constexpr int k = NOVER2 - 1;
				unroll<Real, N, k>::apply_forward_twiddles(D);
				Real Rre = D[k * 2];
				D[k * 2] = ndl::Cos<N, k * 2, Real>::value() * D[k * 2] + ndl::Sin<N, k * 2, Real>::value() * D[k * 2 + 1];
				D[k * 2 + 1] = ndl::Cos<N, k * 2, Real>::value() * D[k * 2 + 1] - ndl::Sin<N, k * 2, Real>::value() * Rre;
			}
			static inline void apply_reverse_twiddles(Real* D) {
				constexpr int k = NOVER2 - 1;
				unroll<Real, N, k>::apply_reverse_twiddles(D);
				Real Rre = D[k * 2];
				D[k * 2] = ndl::Cos<N, k * 2, Real>::value() * D[k * 2] - ndl::Sin<N, k * 2, Real>::value() * D[k * 2 + 1];
				D[k * 2 + 1] = ndl::Cos<N, k * 2, Real>::value() * D[k * 2 + 1] + ndl::Sin<N, k * 2, Real>::value() * Rre;
			}
			static inline void apply_real_forward_twiddles(Real* D) {
				constexpr int k = NOVER2 - 1;
				unroll<Real, N, k>::apply_forward_twiddles(D);
				Real Rre = D[k * 2];
				//TODO: finish this!!
				//D[k * 2] = ndl::Cos<N, k * 2, Real>::value() * D[k * 2] + ndl::Sin<N, k * 2, Real>::value() * D[k * 2 + 1];
				//D[k * 2 + 1] = ndl::Cos<N, k * 2, Real>::value() * D[k * 2 + 1] - ndl::Sin<N, k * 2, Real>::value() * Rre;
			}
			static inline void apply_real_reverse_twiddles(Real* D) {
				constexpr int k = NOVER2 - 1;
				unroll<Real, N, k>::apply_reverse_twiddles(D);
				Real Rre = D[k * 2];
				//TODO: finish this!!
				//D[k * 2] = ndl::Cos<N, k * 2, Real>::value() * D[k * 2] - ndl::Sin<N, k * 2, Real>::value() * D[k * 2 + 1];
				//D[k * 2 + 1] = ndl::Cos<N, k * 2, Real>::value() * D[k * 2 + 1] + ndl::Sin<N, k * 2, Real>::value() * Rre;
			}
			static inline void assign_output(Real* D, Real* output) {
				constexpr int k = NOVER2 - 1;
				unroll<Real, N, k>::assign_output(D, output);
				output[k * 2] = D[k * 2 + N] + D[k * 2];
				output[k * 2 + 1] = D[k * 2 + 1 + N] + D[k * 2 + 1];
				output[k * 2 + N] = D[k * 2 + N] - D[k * 2];
				output[k * 2 + 1 + N] = D[k * 2 + 1 + N] - D[k * 2 + 1];
			}
		};
		template <class Real, int N> struct unroll<Real, N, 0> {
			static inline void apply_forward_twiddles(Real*) {}
			static inline void apply_reverse_twiddles(Real*) {}
			static inline void apply_real_forward_twiddles(Real*) {}
			static inline void apply_real_reverse_twiddles(Real*) {}
			static inline void assign_output(Real*, Real*) {}
		};

		//compute fft for size N (power of 2)
		template<class Real, int N, int SKIP=1>
		struct FFT_calculate {
			static void evaluate(Real* input, Real* output, Real* D, Real* twiddles) {
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
			static inline void evaluate_forward(Real* input, Real* output, Real* D) {
				FFT_calculate<Real, N / 2, SKIP * 2>::evaluate_forward(input, D + N, output);
				FFT_calculate<Real, N / 2, SKIP * 2>::evaluate_forward(input + SKIP * 2, D, output);
				unroll<Real, N, N / 2>::apply_forward_twiddles(D);
				unroll<Real, N, N / 2>::assign_output(D, output);
			}
			static inline void evaluate_reverse(Real* input, Real* output, Real* D) {
				FFT_calculate<Real, N / 2, SKIP * 2>::evaluate_reverse(input, D + N, output);
				FFT_calculate<Real, N / 2, SKIP * 2>::evaluate_reverse(input + SKIP * 2, D, output);
				unroll<Real, N, N / 2>::apply_reverse_twiddles(D);
				unroll<Real, N, N / 2>::assign_output(D, output);
			}
			static inline void evaluate_real_forward(Real* input, Real* output, Real* D) {
				FFT_calculate<Real, N / 2, SKIP * 2>::evaluate_real_forward(input, D + N, output);
				FFT_calculate<Real, N / 2, SKIP * 2>::evaluate_real_forward(input + SKIP * 2, D, output);
				unroll<Real, N, N / 2>::apply_real_forward_twiddles(D);
				unroll<Real, N, N / 2>::assign_output(D, output);
			}
			static inline void evaluate_real_reverse(Real* input, Real* output, Real* D) {
				FFT_calculate<Real, N / 2, SKIP * 2>::evaluate_real_reverse(input, D + N, output);
				FFT_calculate<Real, N / 2, SKIP * 2>::evaluate_real_reverse(input + SKIP * 2, D, output);
				unroll<Real, N, N / 2>::apply_real_reverse_twiddles(D);
				unroll<Real, N, N / 2>::assign_output(D, output);
			}
		};

		//compute fft for size 4
		template<class Real, int SKIP>
		struct FFT_calculate<Real, 4, SKIP> {
			static void evaluate(Real* input, Real* output, Real* D, Real* twiddles) {
				FFT_calculate<Real, 2, SKIP * 2>::evaluate(input, D + 4, output, twiddles);
				FFT_calculate<Real, 2, SKIP * 2>::evaluate(input + SKIP * 2, D, output, twiddles);
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
			static inline void evaluate_forward(Real* input, Real* output, Real* D) {
				FFT_calculate<Real, 2, SKIP * 2>::evaluate_forward(input, D + 4, output);
				FFT_calculate<Real, 2, SKIP * 2>::evaluate_forward(input + SKIP * 2, D, output);
				output[0] = D[4] + D[0];
				output[1] = D[5] + D[1];
				output[2] = D[6] + D[3];
				output[3] = D[7] - D[3];
				output[4] = D[4] - D[0];
				output[5] = D[5] - D[1];
				output[6] = D[6] - D[3];
				output[7] = D[7] + D[3];
			}
			static inline void evaluate_reverse(Real* input, Real* output, Real* D) {
				FFT_calculate<Real, 2, SKIP * 2>::evaluate_reverse(input, D + 4, output);
				FFT_calculate<Real, 2, SKIP * 2>::evaluate_reverse(input + SKIP * 2, D, output);
				output[0] = D[4] + D[0];
				output[1] = D[5] + D[1];
				output[4] = D[4] - D[0];
				output[5] = D[5] - D[1];
				output[2] = D[6] - D[3];
				output[3] = D[7] + D[2];
				output[6] = D[6] + D[3];
				output[7] = D[7] - D[2];
			}
			static inline void evaluate_real_forward(Real* input, Real* output, Real* D) {
				FFT_calculate<Real, 2, SKIP * 2>::evaluate_forward(input, D + 4, output);
				FFT_calculate<Real, 2, SKIP * 2>::evaluate_forward(input + SKIP * 2, D, output);
				Real Rre2 = D[2];
				//not needed?? double check...
				//D[2] = D[3];
				//D[3] = - Rre2;
				output[0] = D[4] + D[0];
				output[1] = D[5] + D[1];
				output[4] = D[4] - D[0];
				output[5] = D[5] - D[1];
				output[2] = D[6] + D[2];
				output[3] = D[7] + D[3];
				output[6] = D[6] - D[2];
				output[7] = D[7] - D[3];
			}
			static inline void evaluate_real_reverse(Real* input, Real* output, Real* D) {
				FFT_calculate<Real, 2, SKIP * 2>::evaluate_reverse(input, D + 4, output);
				FFT_calculate<Real, 2, SKIP * 2>::evaluate_reverse(input + SKIP * 2, D, output);
				Real Rre2 = D[2];
				//not needed?? double check...
				//D[2] = - D[3];
				//D[3] = Rre2;
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

		//compute fft for size 2
		template<class Real, int SKIP>
		struct FFT_calculate<Real, 2, SKIP> {
			static void evaluate(Real* input, Real* output, Real* D, Real* twiddles) {
				output[0] = input[0] + input[SKIP * 2];
				output[1] = input[1] + input[SKIP * 2 + 1];
				output[2] = input[0] - input[SKIP * 2];
				output[3] = input[1] - input[SKIP * 2 + 1];
			}
			static inline void evaluate_forward(Real* input, Real* output, Real* D) {
				output[0] = input[0] + input[SKIP * 2];
				output[1] = input[1] + input[SKIP * 2 + 1];
				output[2] = input[0] - input[SKIP * 2];
				output[3] = input[1] - input[SKIP * 2 + 1];
			}
			static inline void evaluate_reverse(Real* input, Real* output, Real* D) {
				output[0] = input[0] + input[SKIP * 2];
				output[1] = input[1] + input[SKIP * 2 + 1];
				output[2] = input[0] - input[SKIP * 2];
				output[3] = input[1] - input[SKIP * 2 + 1];
			}
			static inline void evaluate_real_forward(Real* input, Real* output, Real* D) {
				output[0] = input[0] + input[SKIP * 2];
				output[1] = input[1] + input[SKIP * 2 + 1];
				output[2] = input[0] - input[SKIP * 2];
				output[3] = input[1] - input[SKIP * 2 + 1];
			}
			static inline void evaluate_real_reverse(Real* input, Real* output, Real* D) {
				output[0] = input[0] + input[SKIP * 2];
				output[1] = input[1] + input[SKIP * 2 + 1];
				output[2] = input[0] - input[SKIP * 2];
				output[3] = input[1] - input[SKIP * 2 + 1];
			}
		};

		//compute fft for size 1
		template<class Real, int SKIP>
		struct FFT_calculate<Real, 1, SKIP> {
			static void evaluate(Real* input, Real* output, Real* D, Real* twiddles) {
				output[0] = input[0];
				output[1] = input[1];
			}
			static inline void evaluate_forward(Real* input, Real* output, Real* D) {
				output[0] = input[0];
				output[1] = input[1];
			}
			static inline void evaluate_reverse(Real* input, Real* output, Real* D) {
				output[0] = input[0];
				output[1] = input[1];
			}
			static inline void evaluate_real_forward(Real* input, Real* output, Real* D) {
				output[0] = input[0];
				output[1] = input[1];
			}
			static inline void evaluate_real_reverse(Real* input, Real* output, Real* D) {
				output[0] = input[0];
				output[1] = input[1];
			}
		};

		//compute fft for any power of 2 (<=MaxPo2Size)
		template <class Real, int MaxPo2Size>
		struct FFTPowerOfTwo
		{
			static void compute(int n, Real* input, Real* output, Real* scratch, Real* twiddles)
			{
				if (n == MaxPo2Size) { return FFT_calculate<Real, MaxPo2Size, 1>::evaluate(input, output, scratch, twiddles); }
				else { return FFTPowerOfTwo<Real, MaxPo2Size / 2>::compute(n, input, output, scratch, twiddles); }
			}
			static void compute_forward(int n, Real* input, Real* output, Real* scratch)
			{
				if (n == MaxPo2Size) { return FFT_calculate<Real, MaxPo2Size, 1>::evaluate_forward(input, output, scratch); }
				else { return FFTPowerOfTwo<Real, MaxPo2Size / 2>::compute_forward(n, input, output, scratch); }
			}
			static void compute_reverse(int n, Real* input, Real* output, Real* scratch)
			{
				if (n == MaxPo2Size) { return FFT_calculate<Real, MaxPo2Size, 1>::evaluate_reverse(input, output, scratch); }
				else { return FFTPowerOfTwo<Real, MaxPo2Size / 2>::compute_reverse(n, input, output, scratch); }
			}
			static void compute_real_forward(int n, Real* input, Real* output, Real* scratch) {
				if (n == MaxPo2Size) { return FFT_calculate<Real, MaxPo2Size, 1>::evaluate_real_forward(input, output, scratch); }
				else { return FFTPowerOfTwo<Real, MaxPo2Size / 2>::compute_real_forward(n, input, output, scratch); }
			}
			static void compute_real_reverse(int n, Real* input, Real* output, Real* scratch) {
				if (n == MaxPo2Size) { return FFT_calculate<Real, MaxPo2Size, 1>::evaluate_real_reverse(input, output, scratch); }
				else { return FFTPowerOfTwo<Real, MaxPo2Size / 2>::compute_real_reverse(n, input, output, scratch); }
			}
		};

		//base case for computing fft for any power of 2
		template <class Real>
		struct FFTPowerOfTwo<Real, 0>
		{
			static void compute(int n, Real* input, Real* output, Real* scratch, Real* twiddles) { }
			static void compute_forward(int n, Real* input, Real* output, Real* scratch) { }
			static void compute_reverse(int n, Real* input, Real* output, Real* scratch) { }
			static void compute_real_forward(int n, Real* input, Real* output, Real* scratch) { }
			static void compute_real_reverse(int n, Real* input, Real* output, Real* scratch) { }
		};

		//main class
		template<class Real, int MaxPo2Size>
		class FFT {
		public:
			//maxsize_pow_of_2 is the largest sample size,
			FFT(Real* ScratchBufferOfSizeNTimesFour) {
				fft_twiddles = ScratchBufferOfSizeNTimesFour;
				fft_twiddles2 = ScratchBufferOfSizeNTimesFour + MaxPo2Size;
				scratch = ScratchBufferOfSizeNTimesFour + MaxPo2Size * 2;
				N = 0;
			}
			void fft(int n, std::complex<Real>* input, std::complex<Real>* output) {
				Real* fft_input = reinterpret_cast<Real*>(input);
				Real* fft_output = reinterpret_cast<Real*>(output);
				if (N != n) update_complex_twiddles(n);
				FFTPowerOfTwo<Real, MaxPo2Size>::compute(N, fft_input, fft_output, scratch, fft_twiddles);
				//FFTPowerOfTwo<Real, MaxPo2Size>::compute_forward(N, fft_input, fft_output, scratch);
			}
			void ifft(int n, std::complex<Real>* input, std::complex<Real>* output) {
				Real* fft_input = reinterpret_cast<Real*>(input);
				Real* fft_output = reinterpret_cast<Real*>(output);
				if (N != n) update_complex_twiddles(n);
				FFTPowerOfTwo<Real, MaxPo2Size>::compute(N, fft_input, fft_output, scratch, fft_twiddles2);
				//FFTPowerOfTwo<Real, MaxPo2Size>::compute_reverse(N, fft_input, fft_output, scratch);
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
				double delta = -2.0*M_PI / N;
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
		};

		template<class Real, int MaxPo2Size>
		class FFTReal {
		public:
			//maxsize_pow_of_2 is the largest sample size,
			FFTReal(Real* ScratchBufferOfSizeNTimesFive)
			{
				fft_twiddles = ScratchBufferOfSizeNTimesFive;
				fft_twiddles2 = ScratchBufferOfSizeNTimesFive + MaxPo2Size;
				scratch = ScratchBufferOfSizeNTimesFive + MaxPo2Size * 2;
				scratch2 = ScratchBufferOfSizeNTimesFive + MaxPo2Size * 4;
				iRealN = RealN = 0;
			}
			void fft(int n, Real* input, std::complex<Real>* output) {
				Real* fft_input = reinterpret_cast<Real*>(input);
				Real* fft_output = reinterpret_cast<Real*>(output);
				if (n > 1) {
					int nover2 = n / 2;
					if (RealN != nover2) update_real_ftwiddles(nover2);
					FFTPowerOfTwo<Real, MaxPo2Size>::compute(nover2, fft_input, fft_output, scratch, fft_twiddles);
					//FFTPowerOfTwo<Real, MaxPo2Size>::compute_real_forward(nover2, fft_input, fft_output, scratch);

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
					FFTPowerOfTwo<Real, MaxPo2Size>::compute(nover2, scratch2, fft_output, scratch, fft_twiddles);
					//FFTPowerOfTwo<Real, MaxPo2Size>::compute_real_reverse(nover2, scratch2, fft_output, scratch);

					//post process
					Real inv = 2.0 / n;
					for (int k = 0; k < n; ++k) fft_output[k] *= inv;
				}
				else if (n == 1) 
					fft_output[0] = fft_input[0];
			}
		protected:
			Real* fft_twiddles;
			Real* fft_twiddles2;
			Real* scratch;
			Real* scratch2;
			int RealN, iRealN;
			void update_real_ftwiddles(int num) {
				RealN = num;
				double delta = -2.0*M_PI / RealN;
				int nover2 = RealN / 2;
				double temp = 0;
				for (int k = 0, k2 = 0; k != nover2; ++k, k2 += 2) {
					fft_twiddles[k2] = cos(temp);       //for forward transform
					fft_twiddles[k2 + 1] = sin(temp);   //for forward transform
					temp += delta;
				}

				RealN *= 2;
				delta = -2.0*M_PI / RealN;
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
				double delta = 2.0*M_PI / iRealN;
				int nover2 = iRealN / 2;
				double temp = 0;
				for (int k = 0, k2 = 0; k != nover2; ++k, k2 += 2) {
					fft_twiddles[k2] = cos(temp);       //for inverse transform
					fft_twiddles[k2 + 1] = sin(temp);   //for inverse transform
					temp += delta;
				}

				iRealN *= 2;
				delta = 2.0*M_PI / iRealN;
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
