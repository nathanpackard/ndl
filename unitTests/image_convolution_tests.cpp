#include <gtest/gtest.h>
#include <vector>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/convolution.h>

#include "testHelpers.h"

using namespace ndl;

TEST(ImageConvolution, ImageConvolution) {
	std::stringstream passfail;

	std::cout << std::endl << "IMAGE CONVOLUTION" << std::endl;

	// 4x4 image, values 1..16 (x fastest)
	std::vector<int> data(16);
	Image<int, 2> img(data.data(), { 4, 4 });
	int i = 0;
	for (auto it = img.begin(); it != img.end(); ++it) *it = ++i;

	// Identity kernel (1 at center, 0 elsewhere) leaves the image unchanged.
	std::vector<int> identityKernelData = { 0,0,0, 0,1,0, 0,0,0 };
	Image<int, 2> identityKernel(identityKernelData.data(), { 3, 3 });
	std::vector<int> outData(16);
	Image<int, 2> out(outData.data(), { 4, 4 });
	ndl::convolve(img, out, identityKernel);
	passfail << "convolve() with identity kernel leaves image unchanged: " << (out == img ? "Pass" : "Fail") << std::endl;

	// All-ones 3x3 kernel: at an interior point every tap is in-bounds, so the
	// result is just the sum of the 3x3 neighborhood.
	std::vector<int> sumKernelData(9, 1);
	Image<int, 2> sumKernel(sumKernelData.data(), { 3, 3 });
	ndl::convolve(img, out, sumKernel, BorderMode::Clamp);
	int expectedInterior = 1 + 2 + 3 + 5 + 6 + 7 + 9 + 10 + 11; // 3x3 block around (1,1)
	passfail << "convolve() sum kernel matches expected interior value: " << (out(1, 1) == expectedInterior ? "Pass" : "Fail") << std::endl;

	// At corner (0,0), clamping the out-of-range taps (-1 -> 0) collapses the
	// 9 taps onto the 2x2 block (0,0),(1,0),(0,1),(1,1), with (0,0) counted 4x
	// and the other two edge cells counted 2x each -- derive that generically
	// from img's own values rather than hardcoding a number.
	ndl::convolve(img, out, sumKernel, BorderMode::Clamp);
	int clampCorner = out(0, 0);
	int expectedClampCorner = 4 * img(0, 0) + 2 * img(1, 0) + 2 * img(0, 1) + img(1, 1);
	passfail << "convolve() clamp border matches hand-derived value: " << (clampCorner == expectedClampCorner ? "Pass" : "Fail") << std::endl;

	// Wrap (circular) border handling picks entirely different source pixels
	// at the corner (it wraps to the opposite edge instead of repeating the
	// edge pixel), so it must disagree with the clamp result computed above.
	ndl::convolve(img, out, sumKernel, BorderMode::Wrap);
	int wrapCorner = out(0, 0);
	passfail << "convolve() wrap border differs from clamp border at the corner: " << (wrapCorner != clampCorner ? "Pass" : "Fail") << std::endl;

	// A radius-2 (5x5) kernel distinguishes reflect from clamp too: clamp maps
	// offsets -2,-1 both to 0, while reflect maps -2 to 1 and -1 to 0 -- a
	// different multiset of source pixels, so the corner values must differ.
	std::vector<int> sumKernel5Data(25, 1);
	Image<int, 2> sumKernel5(sumKernel5Data.data(), { 5, 5 });
	ndl::convolve(img, out, sumKernel5, BorderMode::Clamp);
	int clampCorner5 = out(0, 0);
	ndl::convolve(img, out, sumKernel5, BorderMode::Reflect);
	int reflectCorner5 = out(0, 0);
	passfail << "convolve() reflect border differs from clamp border with a wider kernel: " << (reflectCorner5 != clampCorner5 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(ImageConvolution, Downsample) {
	std::stringstream passfail;

	std::cout << std::endl << "DOWNSAMPLE" << std::endl;

	// A constant color image should downsample to the same constant
	// everywhere -- blurring a flat field changes nothing, and decimation
	// just thins out identical samples.
	OwnedImage<uint8_t, 3> constColor({ 3, 40, 30 });
	constColor = uint8_t(77);
	auto constDown = downsample(constColor, 2);
	passfail << "downsample() extent: channel axis untouched, spatial axes halved: "
		<< (constDown.extent() == std::array<int, 3>{3, 20, 15} ? "Pass" : "Fail") << std::endl;
	bool constMatches = true;
	for (const auto& c : constDown.coordinates()) if (constDown.at(c) != 77) constMatches = false;
	passfail << "downsample() of a constant image stays constant: " << (constMatches ? "Pass" : "Fail") << std::endl;

	// Same result whether channelAxis is explicitly 0 (the default) or
	// passed explicitly -- confirms the default argument actually wires
	// through to the same behavior.
	auto explicitAxis = downsample(constColor, 2, 0, BorderMode::Clamp);
	passfail << "downsample() default channelAxis matches explicit channelAxis=0: " << (explicitAxis == constDown ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(ImageConvolution, ToDisplayable) {
	std::stringstream passfail;

	std::cout << std::endl << "TO_DISPLAYABLE" << std::endl;

	std::vector<double> data = { -50.0, 0.0, 128.0, 300.0, 255.0 };
	Image<double, 1> src(data.data(), { 5 });
	OwnedImage<uint8_t, 1> dst({ 5 });
	to_displayable(src, dst);
	bool clampsCorrectly = dst(0) == 0 && dst(1) == 0 && dst(2) == 128 && dst(3) == 255 && dst(4) == 255;
	passfail << "to_displayable() clamps below 0 and above 255 without a bias: " << (clampsCorrectly ? "Pass" : "Fail") << std::endl;

	std::vector<double> biasedData = { -128.0, 0.0, 127.0 };
	Image<double, 1> biasedSrc(biasedData.data(), { 3 });
	OwnedImage<uint8_t, 1> biasedDst({ 3 });
	to_displayable(biasedSrc, biasedDst, 128.0);
	bool biasCorrect = biasedDst(0) == 0 && biasedDst(1) == 128 && biasedDst(2) == 255;
	passfail << "to_displayable() applies bias before clamping: " << (biasCorrect ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

