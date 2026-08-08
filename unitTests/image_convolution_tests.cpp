#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstdint>
#include <type_traits>
#include <initializer_list>
#include <cmath>
#include <set>
#include <functional>
#include <filesystem>

#include <ndl/image.h>
#include <ndl/imageIO.h>
#include <ndl/utility.h>
#include <ndl/mathHelpers.h>
#include <ndl/matrix.h>

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
	img.convolve(identityKernel, out);
	passfail << "convolve() with identity kernel leaves image unchanged: " << (out == img ? "Pass" : "Fail") << std::endl;

	// All-ones 3x3 kernel: at an interior point every tap is in-bounds, so the
	// result is just the sum of the 3x3 neighborhood.
	std::vector<int> sumKernelData(9, 1);
	Image<int, 2> sumKernel(sumKernelData.data(), { 3, 3 });
	img.convolve(sumKernel, out, BorderMode::Clamp);
	int expectedInterior = 1 + 2 + 3 + 5 + 6 + 7 + 9 + 10 + 11; // 3x3 block around (1,1)
	passfail << "convolve() sum kernel matches expected interior value: " << (out(1, 1) == expectedInterior ? "Pass" : "Fail") << std::endl;

	// At corner (0,0), clamping the out-of-range taps (-1 -> 0) collapses the
	// 9 taps onto the 2x2 block (0,0),(1,0),(0,1),(1,1), with (0,0) counted 4x
	// and the other two edge cells counted 2x each -- derive that generically
	// from img's own values rather than hardcoding a number.
	img.convolve(sumKernel, out, BorderMode::Clamp);
	int clampCorner = out(0, 0);
	int expectedClampCorner = 4 * img(0, 0) + 2 * img(1, 0) + 2 * img(0, 1) + img(1, 1);
	passfail << "convolve() clamp border matches hand-derived value: " << (clampCorner == expectedClampCorner ? "Pass" : "Fail") << std::endl;

	// Wrap (circular) border handling picks entirely different source pixels
	// at the corner (it wraps to the opposite edge instead of repeating the
	// edge pixel), so it must disagree with the clamp result computed above.
	img.convolve(sumKernel, out, BorderMode::Wrap);
	int wrapCorner = out(0, 0);
	passfail << "convolve() wrap border differs from clamp border at the corner: " << (wrapCorner != clampCorner ? "Pass" : "Fail") << std::endl;

	// A radius-2 (5x5) kernel distinguishes reflect from clamp too: clamp maps
	// offsets -2,-1 both to 0, while reflect maps -2 to 1 and -1 to 0 -- a
	// different multiset of source pixels, so the corner values must differ.
	std::vector<int> sumKernel5Data(25, 1);
	Image<int, 2> sumKernel5(sumKernel5Data.data(), { 5, 5 });
	img.convolve(sumKernel5, out, BorderMode::Clamp);
	int clampCorner5 = out(0, 0);
	img.convolve(sumKernel5, out, BorderMode::Reflect);
	int reflectCorner5 = out(0, 0);
	passfail << "convolve() reflect border differs from clamp border with a wider kernel: " << (reflectCorner5 != clampCorner5 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

