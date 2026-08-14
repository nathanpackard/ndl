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
#include <vector>
#include <array>
#include <algorithm>
#include <execution>
#include <cassert>
#include <stdexcept>
#include <string>
#include "mathHelpers.h"
#include "image.h"

namespace ndl
{
	namespace fft
	{
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
		};

		//compute fft for size 1
		template<class Real, int SKIP>
		struct FFT_calculate<Real, 1, SKIP> {
			static void evaluate(Real* input, Real* output, Real* D, Real* twiddles) {
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
		};

		//base case for computing fft for any power of 2
		template <class Real>
		struct FFTPowerOfTwo<Real, 0>
		{
			static void compute(int n, Real* input, Real* output, Real* scratch, Real* twiddles) { }
		};

		// Complex-to-complex FFT/IFFT for any power-of-two size up to MaxPo2Size
		// (checked at runtime via the `n` argument to fft()/ifft() -- MaxPo2Size
		// is just the upper bound the caller-provided scratch buffer was sized
		// for, not a fixed transform size). Caller owns all memory, per this
		// library's usual convention: ScratchBufferOfSizeNTimesFour must have
		// room for MaxPo2Size*4 Reals and stays owned by the FFT instance for
		// its lifetime, reused across calls (recomputing the twiddle factors is
		// skipped whenever consecutive calls use the same n).
		/// Complex-to-complex power-of-two-size FFT/IFFT (Cooley-Tukey). See fftn()/ifftn() for the N-dimensional Image-based entry point most callers want.
		///
		/// @tparam Real       Floating-point element type (float or double).
		/// @tparam MaxPo2Size The largest power-of-two transform size this instance's scratch buffer supports; fft()/ifft() may be called with any power-of-two `n <= MaxPo2Size`, changing `n` between calls at will.
		/// @ingroup fft
		template<class Real, int MaxPo2Size>
		class FFT {
		public:
			/// @param ScratchBufferOfSizeNTimesFour Caller-owned buffer of `MaxPo2Size * 4` Reals, kept alive for this FFT instance's whole lifetime and reused across calls.
			FFT(Real* ScratchBufferOfSizeNTimesFour) {
				fft_twiddles = ScratchBufferOfSizeNTimesFour;
				fft_twiddles2 = ScratchBufferOfSizeNTimesFour + MaxPo2Size;
				scratch = ScratchBufferOfSizeNTimesFour + MaxPo2Size * 2;
				N = 0;
			}
			/// Forward transform.
			/// @param n      Sample count; must be a power of two, `<= MaxPo2Size`.
			/// @param input  `n` complex samples.
			/// @param output `n` complex spectrum values; may alias `input`.
			void fft(int n, std::complex<Real>* input, std::complex<Real>* output) {
				Real* fft_input = reinterpret_cast<Real*>(input);
				Real* fft_output = reinterpret_cast<Real*>(output);
				if (N != n) update_complex_twiddles(n);
				FFTPowerOfTwo<Real, MaxPo2Size>::compute(N, fft_input, fft_output, scratch, fft_twiddles);
			}
			/// Inverse transform (includes the 1/n normalization).
			/// @param n      Sample count; must be a power of two, `<= MaxPo2Size`.
			/// @param input  `n` complex spectrum values.
			/// @param output `n` reconstructed complex samples; may alias `input`.
			void ifft(int n, std::complex<Real>* input, std::complex<Real>* output) {
				Real* fft_input = reinterpret_cast<Real*>(input);
				Real* fft_output = reinterpret_cast<Real*>(output);
				if (N != n) update_complex_twiddles(n);
				FFTPowerOfTwo<Real, MaxPo2Size>::compute(N, fft_input, fft_output, scratch, fft_twiddles2);
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

		// Real-to-complex FFT/IFFT: packs N real samples into an N/2-point
		// complex FFT (the standard trick, roughly half the work of promoting
		// the input to complex with a zero imaginary part and running the
		// general FFT) with a pre/post-processing pass to unpack the true
		// N-point real spectrum. Same memory convention as FFT above, just a
		// bigger scratch requirement (room for MaxPo2Size*5 Reals) for the
		// extra unpacking buffer.
		/// Real-to-complex power-of-two-size FFT/IFFT, packing N real samples into an N/2-point complex transform (roughly half the work of FFT above with a zero imaginary part).
		///
		/// @tparam Real       Floating-point element type (float or double).
		/// @tparam MaxPo2Size The largest power-of-two transform size this instance's scratch buffer supports.
		/// @ingroup fft
		template<class Real, int MaxPo2Size>
		class FFTReal {
		public:
			/// @param ScratchBufferOfSizeNTimesFive Caller-owned buffer of `MaxPo2Size * 5` Reals, kept alive for this FFTReal instance's whole lifetime and reused across calls.
			FFTReal(Real* ScratchBufferOfSizeNTimesFive)
			{
				fft_twiddles = ScratchBufferOfSizeNTimesFive;
				fft_twiddles2 = ScratchBufferOfSizeNTimesFive + MaxPo2Size;
				scratch = ScratchBufferOfSizeNTimesFive + MaxPo2Size * 2;
				scratch2 = ScratchBufferOfSizeNTimesFive + MaxPo2Size * 4;
				iRealN = RealN = 0;
			}
			/// Forward transform: n real samples -> n/2+1 unique complex spectrum values (full n-length Hermitian-symmetric output).
			/// @param n      Sample count; must be a power of two, `<= MaxPo2Size`.
			/// @param input  `n` real samples.
			/// @param output `n` complex spectrum values.
			void fft(int n, Real* input, std::complex<Real>* output) {
				Real* fft_input = reinterpret_cast<Real*>(input);
				Real* fft_output = reinterpret_cast<Real*>(output);
				if (n > 1) {
					int nover2 = n / 2;
					if (RealN != nover2) update_real_ftwiddles(nover2);
					FFTPowerOfTwo<Real, MaxPo2Size>::compute(nover2, fft_input, fft_output, scratch, fft_twiddles);

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
			/// Inverse transform (includes normalization).
			/// @param n      Sample count; must be a power of two, `<= MaxPo2Size`.
			/// @param input  `n` complex spectrum values.
			/// @param output `n` reconstructed real samples.
			void ifft(int n, std::complex<Real>* input, Real* output) {
				Real* fft_input = reinterpret_cast<Real*>(input);
				Real* fft_output = reinterpret_cast<Real*>(output);
				if (n > 1) {
					//preprocess
					int nover2 = n / 2;
					if (iRealN != nover2) update_real_itwiddles(nover2);

					for (int k = 0; k < nover2; k++) {
						int k2 = k * 2; //0 to n-2
						Real real = 0.5*(fft_input[k2] + fft_input[n - k2]
							- (fft_input[k2 + 1] + fft_input[1 + n - k2])*fft_twiddles2[k2]
							+ (-fft_input[k2] + fft_input[n - k2])*fft_twiddles2[k2 + 1]
							);

						Real imag = 0.5*(fft_input[k2 + 1] - fft_input[1 + n - k2]
							+ (fft_input[k2] - fft_input[n - k2])*fft_twiddles2[k2]
							- (fft_input[k2 + 1] + fft_input[1 + n - k2])*fft_twiddles2[k2 + 1]
							);

						scratch2[k2] = real;
						scratch2[k2 + 1] = imag;
					}
					FFTPowerOfTwo<Real, MaxPo2Size>::compute(nover2, scratch2, fft_output, scratch, fft_twiddles);

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

		namespace detail
		{
			inline bool isPowerOfTwo(int n) { return n > 0 && (n & (n - 1)) == 0; }
			inline int nextPowerOfTwo(int n) { int p = 1; while (p < n) p <<= 1; return p; }
		}

		// Bluestein's algorithm (the chirp-z transform): computes a complex
		// DFT of ANY size N -- not just a power of two -- by rewriting the
		// DFT sum via the identity 2kn = k^2 + n^2 - (k-n)^2:
		//
		//   e^{-i2*pi*kn/N} = w[k] * w[n] * conj(w[k-n]),  w[m] = e^{-i*pi*m^2/N}
		//
		// which turns X[k] = sum_n x[n]*e^{-i2*pi*kn/N} into
		// X[k] = w[k] * sum_n (x[n]*w[n]) * conj(w[k-n]) -- a single LINEAR
		// convolution of an N-point sequence against a symmetric 2N-1-point
		// chirp kernel, evaluated at k=0..N-1. A linear convolution is
		// exactly what a zero-padded CIRCULAR convolution computes (as long
		// as the padded length M >= 2N-1, so the wraparound never aliases
		// into the range being read), and a circular convolution is just
		// IFFT(FFT(a) .* FFT(b)) -- so this reduces an arbitrary-size DFT to
		// one padded power-of-two-size DFT, reusing FFT<Real,MaxPo2Size>
		// above rather than needing a second transform implementation. M
		// still has to be a power of two <= MaxPo2Size, so an N whose
		// padded size 2N-1 rounds up past MaxPo2Size needs a larger
		// MaxPo2Size passed to fftn()/ifftn(), same as an oversized
		// power-of-two N already would.
		//
		// The inverse transform is derived from the forward one via the
		// standard conjugate identity (ifft_N(x)[k] = conj(fft_N(conj(x)))[k]
		// / N) rather than a second, separately-derived chirp construction
		// -- one code path to get right and cross-check against a
		// brute-force reference DFT, not two.
		//
		// Unlike FFT<Real,MaxPo2Size> above (which takes a caller-owned
		// scratch buffer, this library's usual convention), this class owns
		// its own scratch via plain std::vectors, resized only when N
		// changes: Bluestein's several differently-sized intermediate
		// buffers (the padded FFT engine's own scratch, the N-point chirp
		// table, the M-point padded kernel's own transform) make
		// hand-computed raw-pointer offsets significantly more error-prone
		// than the simpler classes above, and fftn() below already manages
		// its own thread_local buffers rather than exposing every byte to
		// its caller -- so this doesn't introduce a new convention at that
		// level, just at this one, more complex, class.
		/// Complex DFT of ANY size N (not just a power of two), via Bluestein's algorithm (the chirp-z transform) -- see fftn()/ifftn() for the N-dimensional entry point that uses this automatically for non-power-of-two dimensions.
		///
		/// @tparam Real       Floating-point element type (float or double).
		/// @tparam MaxPo2Size The largest power-of-two *padded* transform size this instance can use internally -- an N whose `nextPowerOfTwo(2*N-1)` exceeds this throws std::invalid_argument, not the raw N itself.
		/// @note   Unlike FFT/FFTReal above, this class owns its own scratch (plain std::vectors, resized only when N changes) rather than taking a caller-owned buffer -- see the comment above the class definition for why.
		/// @ingroup fft
		template<class Real, int MaxPo2Size>
		class FFTBluestein
		{
		public:
			FFTBluestein() : engineScratch(MaxPo2Size * 4), engine(engineScratch.data()) { }

			/// Forward transform.
			/// @param n      Sample count; any positive size, not just a power of two.
			/// @param input  `n` complex samples.
			/// @param output `n` complex spectrum values.
			/// @throws std::invalid_argument if `n`'s padded transform size exceeds MaxPo2Size.
			void fft(int n, std::complex<Real>* input, std::complex<Real>* output)
			{
				ensure(n);
				for (int i = 0; i < n; i++) a[i] = input[i] * w[i];
				for (int i = n; i < M; i++) a[i] = std::complex<Real>(0, 0);
				engine.fft(M, a.data(), A.data());
				for (int i = 0; i < M; i++) A[i] *= Bfreq[i];
				engine.ifft(M, A.data(), c.data());
				for (int k = 0; k < n; k++) output[k] = w[k] * c[k];
			}
			/// Inverse transform, via the conjugate identity (see the comment above the class definition).
			/// @param n      Sample count; any positive size, not just a power of two.
			/// @param input  `n` complex spectrum values.
			/// @param output `n` reconstructed complex samples.
			/// @throws std::invalid_argument if `n`'s padded transform size exceeds MaxPo2Size.
			void ifft(int n, std::complex<Real>* input, std::complex<Real>* output)
			{
				conjBuf.resize(n);
				for (int i = 0; i < n; i++) conjBuf[i] = std::conj(input[i]);
				fft(n, conjBuf.data(), output);
				Real invN = Real(1) / n;
				for (int k = 0; k < n; k++) output[k] = std::conj(output[k]) * invN;
			}

		private:
			// Recomputes the chirp table w[], the padded convolution kernel
			// bpad, and its transform Bfreq -- everything that depends on N
			// but not on the actual sample values -- only when N changes,
			// the same "skip recompute on repeat calls" pattern
			// FFT::update_complex_twiddles() above uses for its own
			// twiddle factors.
			void ensure(int n)
			{
				if (n == N) return;
				N = n;
				M = detail::nextPowerOfTwo(2 * n - 1);
				if (M > MaxPo2Size)
					throw std::invalid_argument("FFTBluestein: size " + std::to_string(n) + " needs a padded transform of " + std::to_string(M) + " points, which exceeds MaxPo2Size=" + std::to_string(MaxPo2Size) + " -- pass a larger MaxPo2Size to fftn()/ifftn()");

				w.resize(n);
				for (int k = 0; k < n; k++)
				{
					double angle = -M_PI * double(k) * double(k) / double(n);
					w[k] = std::complex<Real>((Real)cos(angle), (Real)sin(angle));
				}

				std::vector<std::complex<Real>> bpad(M, std::complex<Real>(0, 0));
				bpad[0] = std::complex<Real>(1, 0);
				for (int k = 1; k < n; k++)
				{
					double angle = M_PI * double(k) * double(k) / double(n);
					std::complex<Real> bv((Real)cos(angle), (Real)sin(angle));
					bpad[k] = bv;
					bpad[M - k] = bv;
				}
				Bfreq.resize(M);
				engine.fft(M, bpad.data(), Bfreq.data());

				a.resize(M);
				A.resize(M);
				c.resize(M);
			}

			std::vector<Real> engineScratch;
			FFT<Real, MaxPo2Size> engine;
			int N = 0, M = 0;
			std::vector<std::complex<Real>> w, Bfreq, a, A, c, conjBuf;
		};

		// Separable N-dimensional FFT/IFFT on an Image: transforms along every
		// axis in turn, one full 1D FFT per fiber of that axis (the standard
		// row/column/etc. decomposition of an ND DFT into independent 1D
		// DFTs) -- output must already exist with input's own extent. Any
		// dimension's extent works (via FFTBluestein above for non-power-of-
		// two sizes), though a power of two is faster; a size whose padded
		// Bluestein transform would exceed MaxPo2Size throws
		// std::invalid_argument (see FFTBluestein::ensure()) naming the
		// offending size, rather than silently producing wrong output.
		//
		// The 1D transform itself (FFT<Real,MaxPo2Size>::fft/ifft) is
		// deliberately NOT parallelized internally -- it's the *fibers* that
		// are independent of each other, so it's the loop over fibers that
		// runs concurrently, via std::execution::par, one axis-pass at a
		// time. Passes across different axes stay sequential: axis 1's
		// fibers read values axis 0's pass wrote, so the whole image has to
		// finish axis 0 before axis 1 can start -- that barrier is why this
		// is a loop of parallel passes rather than one single parallel loop
		// over every fiber of every axis at once.
		//
		// Each parallel task gets its own FFT engine and scratch buffer via
		// thread_local, allocated once per worker thread on first use and
		// reused for every fiber that thread goes on to handle, rather than
		// once per fiber.
		/// Separable N-dimensional complex FFT/IFFT: transforms every axis in turn. Any extent works (power of two is fastest; see FFTBluestein for the rest).
		/// @tparam Real       Floating-point element type (float or double).
		/// @tparam DIM        Number of dimensions.
		/// @tparam MaxPo2Size Largest power-of-two *padded* transform size any single non-power-of-two axis may need internally (see FFTBluestein); defaults to 8192. Raise this if `input` has a large non-power-of-two dimension.
		/// @param  input      Source image.
		/// @param  output     Destination; must already exist with `input`'s own extent. May alias `input`.
		/// @param  inverse    If true, computes the inverse transform (with 1/n normalization per axis) instead of the forward one.
		/// @throws std::invalid_argument if some axis's extent is non-power-of-two and its Bluestein-padded size exceeds MaxPo2Size.
		/// @ingroup fft
		template<class Real, int DIM, int MaxPo2Size = 8192>
		void fftn(const Image<std::complex<Real>, DIM>& input, Image<std::complex<Real>, DIM>& output, bool inverse = false)
		{
			assert(output.extent() == input.extent());
			output = input; // deep, elementwise copy-through -- output is the working buffer for every axis pass

			for (int axis = 0; axis < DIM; axis++)
			{
				int n = output.extent()[axis];
				// An exact power-of-two extent takes the direct FFTPowerOfTwo<Real,MaxPo2Size>
				// path; any other extent goes through FFTBluestein above instead -- still
				// correct, just several times more work (one padded FFT/IFFT pair plus the
				// O(n) chirp setup, versus one direct FFT/IFFT), so the fast path stays the
				// one used for the common case.
				bool po2 = detail::isPowerOfTwo(n);
				// Checked here, sequentially, rather than left to
				// FFTBluestein::ensure()'s own throw when the first fiber
				// below hits it: an exception thrown from inside a
				// std::execution::par callable doesn't propagate to the
				// caller at all -- the standard mandates std::terminate()
				// instead, so an uncaught throw from inside the parallel
				// for_each below would abort the whole process rather than
				// being catchable. FFTBluestein keeps its own check too, for
				// when it's used directly outside a parallel context.
				if (!po2 && detail::nextPowerOfTwo(2 * n - 1) > MaxPo2Size)
				{
					int neededM = detail::nextPowerOfTwo(2 * n - 1);
					throw std::invalid_argument("fftn: dimension " + std::to_string(axis) + " has extent " + std::to_string(n) + ", whose Bluestein-padded transform needs " + std::to_string(neededM) + " points, which exceeds MaxPo2Size=" + std::to_string(MaxPo2Size) + " -- pass a larger MaxPo2Size to fftn()/ifftn()");
				}

				auto origins = ndl::detail::fiberOrigins<DIM>(output.extent(), axis);
				std::for_each(std::execution::par, origins.begin(), origins.end(), [&](const std::array<int, DIM>& origin)
				{
					thread_local std::vector<Real> engineScratch(MaxPo2Size * 4);
					thread_local FFT<Real, MaxPo2Size> engine(engineScratch.data());
					thread_local FFTBluestein<Real, MaxPo2Size> bluesteinEngine;
					// Same "thread_local scratch, not a per-call allocation"
					// reasoning as the engines just above -- resized (not
					// reallocated) only when this thread's previous fiber was a
					// different length, so a thread handling thousands of
					// same-length fibers (the common case: every fiber along a
					// given axis has the same length n) pays for the allocation
					// at most once.
					thread_local std::vector<std::complex<Real>> fiber, transformed;
					if (fiber.size() != (std::size_t)n) { fiber.resize(n); transformed.resize(n); }

					std::array<int, DIM> coord = origin;
					for (int i = 0; i < n; i++) { coord[axis] = i; fiber[i] = output.at(coord); }

					if (po2)
					{
						if (inverse) engine.ifft(n, fiber.data(), transformed.data());
						else engine.fft(n, fiber.data(), transformed.data());
					}
					else
					{
						if (inverse) bluesteinEngine.ifft(n, fiber.data(), transformed.data());
						else bluesteinEngine.fft(n, fiber.data(), transformed.data());
					}

					for (int i = 0; i < n; i++) { coord[axis] = i; output.at(coord) = transformed[i]; }
				});
			}
		}

		/// Inverse of fftn(): reconstructs the spatial-domain signal from its complex spectrum.
		/// @tparam Real       Floating-point element type (float or double).
		/// @tparam DIM        Number of dimensions.
		/// @tparam MaxPo2Size Largest power-of-two padded transform size any single non-power-of-two axis may need; see fftn().
		/// @param  input      Complex spectrum.
		/// @param  output     Destination; must already exist with `input`'s own extent. May alias `input`.
		/// @throws std::invalid_argument under the same conditions as fftn().
		/// @ingroup fft
		template<class Real, int DIM, int MaxPo2Size = 8192>
		void ifftn(const Image<std::complex<Real>, DIM>& input, Image<std::complex<Real>, DIM>& output)
		{
			fftn<Real, DIM, MaxPo2Size>(input, output, true);
		}

		// Convenience for a real-valued input image: promotes to complex
		// (imaginary part zero) and runs the same separable ND transform --
		// simpler and more robust than replicating FFTReal's real-input
		// packing trick in a strided, parallel, N-dimensional setting, at the
		// cost of the arithmetic FFTReal saves by working on half-length
		// complex data. FFTReal itself is still available directly above for
		// the 1D real<->complex case.
		/// Convenience overload for a real-valued input: promotes to complex (zero imaginary part), then runs the same transform.
		/// @tparam Real       Floating-point element type (float or double).
		/// @tparam DIM        Number of dimensions.
		/// @tparam MaxPo2Size Largest power-of-two padded transform size any single non-power-of-two axis may need; see fftn().
		/// @param  input      Real-valued source image.
		/// @param  output     Destination for the complex spectrum; must already exist with `input`'s own extent.
		/// @throws std::invalid_argument under the same conditions as fftn().
		/// @ingroup fft
		template<class Real, int DIM, int MaxPo2Size = 8192>
		void fftn(const Image<Real, DIM>& input, Image<std::complex<Real>, DIM>& output)
		{
			assert(output.extent() == input.extent());
			auto sourceIt = input.begin();
			for (auto it = output.begin(); it != output.end(); ++it, ++sourceIt) *it = std::complex<Real>(*sourceIt, Real(0));
			fftn<Real, DIM, MaxPo2Size>(output, output, false);
		}

		// Convenience for reconstructing a real-valued image from a complex
		// spectrum: inverse-transforms and keeps just the real part (the
		// imaginary part is expected to be ~0 already, up to floating-point
		// error, whenever the spectrum came from a real-valued image in the
		// first place).
		/// Convenience overload reconstructing a real-valued image from a complex spectrum (keeps just the real part).
		/// @tparam Real       Floating-point element type (float or double).
		/// @tparam DIM        Number of dimensions.
		/// @tparam MaxPo2Size Largest power-of-two padded transform size any single non-power-of-two axis may need; see fftn().
		/// @param  input      Complex spectrum.
		/// @param  output     Destination for the reconstructed real-valued image; must already exist with `input`'s own extent.
		/// @throws std::invalid_argument under the same conditions as fftn().
		/// @ingroup fft
		template<class Real, int DIM, int MaxPo2Size = 8192>
		void ifftn(const Image<std::complex<Real>, DIM>& input, Image<Real, DIM>& output)
		{
			std::vector<std::complex<Real>> complexData(output.size());
			Image<std::complex<Real>, DIM> complexOut(complexData.data(), output.extent());
			fftn<Real, DIM, MaxPo2Size>(input, complexOut, true);
			auto cIt = complexOut.begin();
			for (auto it = output.begin(); it != output.end(); ++it, ++cIt) *it = cIt->real();
		}

		// Moves the zero-frequency (DC) component from index 0 of each axis
		// to that axis' own middle (extent/2), wrapping the rest around it --
		// the standard rearrangement (numpy calls it fftshift) for
		// *displaying* a spectrum, since fftn()'s raw output has DC in the
		// corner with frequency increasing outward in a way that wraps at
		// the edge, reading as meaningless noise until it's centered. Not
		// restricted to a complex spectrum itself -- works on any
		// elementwise-copyable value_type, e.g. a real-valued log-magnitude
		// image already derived from one (the common case: you shift the
		// display-ready magnitude, not the raw complex values).
		/// Rearranges `src` so the zero-frequency (DC) component moves from index 0 of each axis to that axis' own middle -- the standard "center the spectrum" display convention.
		/// @tparam SrcImageT Any minimal-interface image type.
		/// @tparam DstImageT Any minimal-interface image type (same DIM as SrcImageT); may differ from SrcImageT (e.g. a different concrete container).
		/// @param  src Source image (typically a spectrum, or a magnitude/log-magnitude image derived from one).
		/// @param  dst Destination; must already exist with `src`'s own extent.
		/// @ingroup fft
		template<class SrcImageT, class DstImageT>
		void fftshift(const SrcImageT& src, DstImageT& dst)
		{
			assert(dst.extent() == src.extent());
			auto extent = src.extent();
			constexpr int DIM = std::tuple_size<decltype(extent)>::value;
			for (const auto& coord : src.coordinates())
			{
				std::array<int, DIM> shifted;
				for (int d = 0; d < DIM; d++) shifted[d] = (coord[d] + extent[d] / 2) % extent[d];
				dst.at(shifted) = src.at(coord);
			}
		}
	}
}
