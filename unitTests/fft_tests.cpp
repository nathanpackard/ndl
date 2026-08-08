#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <complex>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/fft.h>

#include "testHelpers.h"

using namespace ndl;
using namespace ndl::fft;

std::vector<std::complex<double>> bruteForceDFT(const std::vector<std::complex<double>>& x, bool inverse) {
	int n = (int)x.size();
	std::vector<std::complex<double>> X(n);
	double sign = inverse ? 1.0 : -1.0;
	for (int k = 0; k < n; k++) {
		std::complex<double> sum(0, 0);
		for (int m = 0; m < n; m++) {
			double angle = sign * 2.0 * M_PI * k * m / n;
			sum += x[m] * std::complex<double>(cos(angle), sin(angle));
		}
		X[k] = inverse ? sum / (double)n : sum;
	}
	return X;
}
double maxAbsDiff(const std::vector<std::complex<double>>& a, const std::vector<std::complex<double>>& b) {
	double worst = 0;
	for (size_t i = 0; i < a.size(); i++) worst = std::max(worst, std::abs(a[i] - b[i]));
	return worst;
}

TEST(FFT, FFT1DComplex) {
	std::stringstream passfail;

	std::cout << std::endl << "FFT 1D COMPLEX" << std::endl;

	const int length = 1024;
	std::vector<std::complex<double>> time(length);
	for (int i = 0; i < length; i++) time[i] = std::complex<double>(std::min(i + 1, 10), std::sin(i * 0.3));

	std::vector<double> scratch(length * 4);
	FFT<double, length> fft(scratch.data());

	clock_t start = clock();
	std::vector<std::complex<double>> freq(length);
	for (int i = 0; i < 128; i++) fft.fft(length, time.data(), freq.data());
	double elapsed = double(clock() - start) / double(CLOCKS_PER_SEC);
	std::cout << "128 forward FFTs of length " << length << " took " << elapsed << " sec\n";

	auto expected = bruteForceDFT(time, false);
	double err = maxAbsDiff(freq, expected);
	passfail << "FFT<double," << length << ">::fft matches a brute-force O(n^2) reference DFT: " << (err < 1e-8 ? "Pass" : "Fail") << std::endl;

	std::vector<std::complex<double>> roundtrip(length);
	fft.ifft(length, freq.data(), roundtrip.data());
	double rtErr = maxAbsDiff(roundtrip, time);
	passfail << "FFT::fft followed by FFT::ifft round trips to the original signal: " << (rtErr < 1e-8 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(FFT, FFT1DReal) {
	std::stringstream passfail;

	std::cout << std::endl << "FFT 1D REAL" << std::endl;

	const int length = 1024;
	std::vector<double> input(length);
	for (int i = 0; i < length; i++) input[i] = std::min(i + 1, 10);

	std::vector<std::complex<double>> output(length);
	std::vector<double> scratch(length * 5);
	FFTReal<double, length> fft(scratch.data());

	clock_t start = clock();
	for (int i = 0; i < 768; i++) fft.fft(length, input.data(), output.data());
	double elapsed = double(clock() - start) / double(CLOCKS_PER_SEC);
	std::cout << "768 forward real FFTs of length " << length << " took " << elapsed << " sec\n";

	std::vector<std::complex<double>> complexInput(length);
	for (int i = 0; i < length; i++) complexInput[i] = std::complex<double>(input[i], 0);
	auto expected = bruteForceDFT(complexInput, false);
	double err = maxAbsDiff(output, expected);
	passfail << "FFTReal<double," << length << ">::fft matches a brute-force O(n^2) reference DFT: " << (err < 1e-8 ? "Pass" : "Fail") << std::endl;

	std::vector<double> roundtrip(length);
	fft.ifft(length, output.data(), roundtrip.data());
	double rtErr = 0;
	for (int i = 0; i < length; i++) rtErr = std::max(rtErr, std::abs(roundtrip[i] - input[i]));
	passfail << "FFTReal::fft followed by FFTReal::ifft round trips to the original signal: " << (rtErr < 1e-6 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(FFT, FFTImageRoundTrip) {
	std::stringstream passfail;

	std::cout << std::endl << "FFT IMAGE ROUND TRIP" << std::endl;

	// 1D, via the Image-based fftn/ifftn wrappers rather than FFT directly
	{
		const int N = 16;
		std::vector<std::complex<double>> inData(N);
		for (int i = 0; i < N; i++) inData[i] = std::complex<double>(std::sin(i * 0.7) + 2.0, std::cos(i * 0.3));
		Image<std::complex<double>, 1> in(inData.data(), { N });

		std::vector<std::complex<double>> freqData(N), backData(N);
		Image<std::complex<double>, 1> freq(freqData.data(), { N }), back(backData.data(), { N });
		fftn<double, 1>(in, freq);
		ifftn<double, 1>(freq, back);

		double err = 0;
		for (int i = 0; i < N; i++) err = std::max(err, std::abs(back(i) - in(i)));
		passfail << "fftn/ifftn round trips a 1D complex Image: " << (err < 1e-9 ? "Pass" : "Fail") << std::endl;
	}

	// 2D complex, exercising the fiber-parallel row/column decomposition on both axes
	{
		const int W = 32, H = 16;
		std::vector<std::complex<double>> inData((size_t)W * H);
		Image<std::complex<double>, 2> in(inData.data(), { W, H });
		int i = 0;
		for (auto it = in.begin(); it != in.end(); ++it) { ++i; *it = std::complex<double>(i * 0.37, i * -0.11); }

		std::vector<std::complex<double>> freqData((size_t)W * H), backData((size_t)W * H);
		Image<std::complex<double>, 2> freq(freqData.data(), { W, H }), back(backData.data(), { W, H });
		fftn<double, 2>(in, freq);
		ifftn<double, 2>(freq, back);

		double err = 0;
		for (const auto& coord : in.coordinates()) err = std::max(err, std::abs(back.at(coord) - in.at(coord)));
		passfail << "fftn/ifftn round trips a 2D complex Image: " << (err < 1e-9 ? "Pass" : "Fail") << std::endl;
	}

	// 2D real-image convenience overloads (promote to complex, transform, take the real part back)
	{
		const int W = 16, H = 16;
		std::vector<double> realData((size_t)W * H);
		Image<double, 2> realImg(realData.data(), { W, H });
		int i = 0;
		for (auto it = realImg.begin(); it != realImg.end(); ++it) *it = (double)((++i) % 17);

		std::vector<std::complex<double>> freqData((size_t)W * H);
		Image<std::complex<double>, 2> freq(freqData.data(), { W, H });
		fftn<double, 2>(realImg, freq);

		std::vector<double> backData((size_t)W * H);
		Image<double, 2> back(backData.data(), { W, H });
		ifftn<double, 2>(freq, back);

		double err = 0;
		for (const auto& coord : realImg.coordinates()) err = std::max(err, std::abs(back.at(coord) - realImg.at(coord)));
		passfail << "fftn/ifftn round trips a 2D real-valued Image through its complex spectrum: " << (err < 1e-9 ? "Pass" : "Fail") << std::endl;
	}
	reportPassFail(passfail);
}

TEST(FFT, FFTMatchesSpatialConvolution) {
	std::stringstream passfail;

	std::cout << std::endl << "COMPOSITE: FFT-BASED CORRELATION MATCHES convolve(BorderMode::Wrap)" << std::endl;

	const int W = 16, H = 8;
	std::vector<double> imgData((size_t)W * H);
	Image<double, 2> img(imgData.data(), { W, H });
	int i = 0;
	for (auto it = img.begin(); it != img.end(); ++it) *it = (double)((++i * 37) % 251);

	std::vector<double> kernelData = { 1,2,1, 0,1,0, -1,0,2 };
	Image<double, 2> kernel(kernelData.data(), { 3, 3 });

	// --- spatial domain ---
	std::vector<double> spatialData((size_t)W * H);
	Image<double, 2> spatial(spatialData.data(), { W, H });
	img.convolve(kernel, spatial, BorderMode::Wrap);

	// --- frequency domain ---
	std::vector<std::complex<double>> imgFreqData((size_t)W * H), kernelPaddedData((size_t)W * H),
		kernelFreqData((size_t)W * H), productData((size_t)W * H), backData((size_t)W * H);
	Image<std::complex<double>, 2> imgFreq(imgFreqData.data(), { W, H });
	Image<std::complex<double>, 2> kernelPadded(kernelPaddedData.data(), { W, H });
	Image<std::complex<double>, 2> kernelFreq(kernelFreqData.data(), { W, H });
	Image<std::complex<double>, 2> product(productData.data(), { W, H });
	Image<std::complex<double>, 2> back(backData.data(), { W, H });

	fftn<double, 2>(img, imgFreq);

	kernelPadded = std::complex<double>(0, 0);
	std::array<int, 2> center{ kernel.extent()[0] / 2, kernel.extent()[1] / 2 };
	for (const auto& kCoord : kernel.coordinates())
	{
		std::array<int, 2> dst;
		for (int d = 0; d < 2; d++)
		{
			int delta = kCoord[d] - center[d];
			int m = kernelPadded.extent()[d];
			dst[d] = ((delta % m) + m) % m;
		}
		kernelPadded.at(dst) = std::complex<double>(kernel.at(kCoord), 0);
	}
	fftn<double, 2>(kernelPadded, kernelFreq);

	for (const auto& coord : product.coordinates())
		product.at(coord) = std::conj(kernelFreq.at(coord)) * imgFreq.at(coord);

	ifftn<double, 2>(product, back);

	double maxErr = 0, maxImag = 0;
	for (const auto& coord : spatial.coordinates())
	{
		maxErr = std::max(maxErr, std::abs(back.at(coord).real() - spatial.at(coord)));
		maxImag = std::max(maxImag, std::abs(back.at(coord).imag()));
	}
	passfail << "FFT-based circular cross-correlation has ~0 imaginary part (as expected for real inputs): " << (maxImag < 1e-6 ? "Pass" : "Fail") << std::endl;
	passfail << "FFT-based circular cross-correlation matches image.convolve(kernel, out, BorderMode::Wrap): " << (maxErr < 1e-6 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

