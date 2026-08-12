#include <gtest/gtest.h>
#include <array>
#include <complex>
#include <type_traits>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/convolution.h>
#include <ndl/morphology.h>

#include "testHelpers.h"

using namespace ndl;

// This file exists to systematically put each image type (Image,
// OwnedImage, PackedBitImage) through its paces with each data type the
// library is meant to support (bool, integral, floating-point, and
// std::complex), and to confirm the operations that only work for SOME of
// those types actually reject the rest -- see image/core.h, morphology.h,
// and convolution.h for the static_assert-based restrictions this checks
// against, and unitTests/negative_compile/ for the restrictions that can
// only be verified by actually failing to compile (a static_assert is a
// hard error, not a SFINAE substitution failure, so it can't be probed
// gracefully via is_constructible_v/decltype from inside a normal TEST()
// the way is_image_like's convertibility check can).

// Compile-time enforcement of the exact trait every ordering/bitwise
// static_assert in the library is gated on. If a future change ever made
// one of these fire "wrong" (e.g. std::is_arithmetic_v<std::complex<double>>
// somehow became true), every static_assert built on it would silently
// stop protecting anything -- so pin the whole matrix down here, where a
// regression fails the build loudly rather than passing silently.
static_assert(std::is_arithmetic_v<bool>, "bool must be arithmetic -- erode()/dilate()/comparisons/etc. on a bool Image rely on this");
static_assert(std::is_arithmetic_v<uint8_t>, "uint8_t must be arithmetic");
static_assert(std::is_arithmetic_v<int>, "int must be arithmetic");
static_assert(std::is_arithmetic_v<float>, "float must be arithmetic");
static_assert(std::is_arithmetic_v<double>, "double must be arithmetic");
static_assert(!std::is_arithmetic_v<std::complex<double>>, "std::complex must NOT be arithmetic -- this is exactly what makes min()/max()/otsu_threshold()/threshold()/convolve()/erode()/dilate()/percentile_filter()/median_filter()/logical_not()/mean()/operator<,<=,>,>= correctly refuse it");
static_assert(std::is_integral_v<bool>, "bool must be integral -- operator%=/&=/|=/^= on a bool mask rely on this");
static_assert(std::is_integral_v<uint8_t>, "uint8_t must be integral");
static_assert(std::is_integral_v<int>, "int must be integral");
static_assert(!std::is_integral_v<float>, "float must NOT be integral -- this is what makes operator%=/&=/|=/^= correctly refuse it");
static_assert(!std::is_integral_v<double>, "double must NOT be integral");
static_assert(!std::is_integral_v<std::complex<double>>, "std::complex must NOT be integral");

TEST(TypeCoverage, BoolImageBasics) {
	// Image<bool,DIM> (a non-owning VIEW, not OwnedImage -- see
	// OwnedImageBoolIsRejected below) works fine as long as the backing
	// storage is a real bool array. std::array<bool,N> here, deliberately
	// NOT std::vector<bool>, which is the bit-packed specialization with no
	// .data() -- the exact thing OwnedImage<bool,DIM> can't work around
	// (see unitTests/negative_compile/owned_image_bool.cpp).
	std::array<bool, 9> data{};
	Image<bool, 2> img(data.data(), { 3, 3 });
	img(0, 0) = true;
	img(1, 1) = true;
	img(2, 2) = true;
	EXPECT_TRUE(img(0, 0));
	EXPECT_FALSE(img(0, 1));
	EXPECT_TRUE(img(2, 2));

	// sum() stays in T the whole way (T total{}; total = static_cast<T>
	// (total + *it);), so on a bool image it's NOT a population count --
	// bool+bool promotes to int, and static_cast<bool> back collapses any
	// nonzero total to true. It's really an "OR of every element" -- a
	// genuinely different operation than PackedBitImage::count(), worth
	// pinning down explicitly rather than leaving it to be discovered by
	// surprise.
	EXPECT_EQ(img.sum(), true);
	std::array<bool, 9> allFalseData{};
	Image<bool, 2> allFalse(allFalseData.data(), { 3, 3 });
	EXPECT_EQ(allFalse.sum(), false);

	// min()/max() on bool are well-defined (bool is arithmetic, false<true):
	// min() is AND-of-all, max() is OR-of-all.
	EXPECT_EQ(img.min(), false);   // not every element is true
	EXPECT_EQ(img.max(), true);    // at least one element is true
	std::array<bool, 9> allTrueData; allTrueData.fill(true);
	Image<bool, 2> allTrue(allTrueData.data(), { 3, 3 });
	EXPECT_EQ(allTrue.min(), true);
}

TEST(TypeCoverage, BoolImageMorphology) {
	// erode()/dilate() are allowed for bool (is_arithmetic_v<bool> is
	// true), and should behave identically here -- AND/OR of the
	// neighborhood -- whether run against a plain Image<bool,DIM> (this
	// test) or a PackedBitImage (packed_bit_image_tests.cpp); both are
	// exercising the exact same shared ndl::erode()/ndl::dilate().
	std::array<bool, 49> srcData{};
	Image<bool, 2> src(srcData.data(), { 7, 7 });
	for (int y = 2; y <= 4; y++)
		for (int x = 2; x <= 4; x++)
			src(x, y) = true;

	OwnedImage<double, 2> box3({ 3, 3 });
	make_box_kernel(box3);

	std::array<bool, 49> erodedData{};
	Image<bool, 2> eroded(erodedData.data(), { 7, 7 });
	ndl::erode(src, eroded, box3, BorderMode::Clamp);
	EXPECT_TRUE(eroded(3, 3));    // center of the filled square survives (all 9 neighbors true)
	EXPECT_FALSE(eroded(2, 2));   // corner's neighborhood includes an outside false

	std::array<bool, 49> dilatedData{};
	Image<bool, 2> dilated(dilatedData.data(), { 7, 7 });
	ndl::dilate(src, dilated, box3, BorderMode::Clamp);
	EXPECT_TRUE(dilated(1, 1));   // one step outside the square, now covered by dilation
	EXPECT_FALSE(dilated(0, 0));  // still too far away
}

TEST(TypeCoverage, FloatImageArithmeticAndOrdering) {
	// A representative sweep of the operations that DO work for float --
	// distinct from double, since is_arithmetic_v<float> also has to hold
	// (confirmed above) and float's own precision characteristics are
	// worth exercising directly rather than assuming "float behaves like
	// double". Values are all exactly representable in float (halves and
	// small integers), so comparisons below can be exact, not tolerance-based.
	std::array<float, 4> dataA{ 1.0f, 2.0f, 3.0f, 4.0f };
	std::array<float, 4> dataB{ 0.5f, 0.5f, 0.5f, 0.5f };
	Image<float, 1> a(dataA.data(), { 4 });
	Image<float, 1> b(dataB.data(), { 4 });

	a += b; // 1.5, 2.5, 3.5, 4.5
	EXPECT_FLOAT_EQ(a(0), 1.5f);
	EXPECT_FLOAT_EQ(a(3), 4.5f);

	a *= 2.0f; // 3, 5, 7, 9
	EXPECT_FLOAT_EQ(a(0), 3.0f);
	EXPECT_FLOAT_EQ(a(3), 9.0f);

	EXPECT_TRUE(a > 0.0f);     // every element > 0
	EXPECT_FALSE(a < 5.0f);    // not every element < 5 (a(3) is 9)
	EXPECT_TRUE(a >= 3.0f);    // every element >= 3
	EXPECT_FALSE(a <= 7.0f);   // a(3)=9 breaks it

	EXPECT_FLOAT_EQ(a.min(), 3.0f);
	EXPECT_FLOAT_EQ(a.max(), 9.0f);
	EXPECT_DOUBLE_EQ(a.mean(), 6.0); // (3+5+7+9)/4

	// convolve() (needs is_arithmetic_v<T>, confirmed allowed for float):
	// a = [3,5,7,9]. A 1x3 box-average kernel's output at index 1 (an
	// interior point, so BorderMode doesn't matter here) averages a's own
	// indices 0,1,2 -- (3+5+7)/3.
	OwnedImage<double, 1> avgKernel({ 3 }, { 1.0 / 3, 1.0 / 3, 1.0 / 3 });
	std::array<float, 4> outData{};
	Image<float, 1> out(outData.data(), { 4 });
	ndl::convolve(a, out, avgKernel, BorderMode::Clamp);
	EXPECT_FLOAT_EQ(out(1), (3.0f + 5.0f + 7.0f) / 3.0f);
}

TEST(TypeCoverage, ComplexAllowedOperations) {
	// std::complex supports +, -, *, /, ==, != natively, so add()/subtract()/
	// multiply()/divide(), the compound-assignment arithmetic operators, and
	// equality all work for Image<std::complex<double>,DIM> -- deliberately
	// NOT restricted, unlike their ordering-dependent siblings (min/max/
	// convolve/etc., covered by unitTests/negative_compile/
	// complex_ordering.cpp). fft_tests.cpp already exercises
	// std::complex through fftn()/ifftn(); this checks the plain
	// arithmetic/comparison path directly, with hand-verified values.
	using C = std::complex<double>;
	std::array<C, 2> dataA{ C(1, 2), C(3, 4) };
	std::array<C, 2> dataB{ C(1, 1), C(1, 1) };
	Image<C, 1> a(dataA.data(), { 2 });
	Image<C, 1> b(dataB.data(), { 2 });

	EXPECT_EQ(a.sum(), C(4, 6));   // (1+2i) + (3+4i)
	EXPECT_TRUE(a == a);
	EXPECT_FALSE(a == b);
	EXPECT_TRUE(a != b);

	a += b; // (2+3i), (4+5i)
	EXPECT_EQ(a(0), C(2, 3));
	EXPECT_EQ(a(1), C(4, 5));

	a *= C(0, 1); // multiply by i: (a+bi)*i = -b+ai
	EXPECT_EQ(a(0), C(-3, 2));
	EXPECT_EQ(a(1), C(-5, 4));

	// mean(axis,...) stays in T throughout (unlike whole-image mean(),
	// which needs static_cast<double> and is correctly rejected -- see
	// unitTests/negative_compile/complex_ordering.cpp's sibling case),
	// so it works for complex: mean over a 2x1 image along axis 0 is just
	// the single remaining value per row.
	std::array<C, 2> data2{ C(2, 0), C(6, 0) };
	Image<C, 2> img2(data2.data(), { 2, 1 });
	std::array<C, 1> meanOutData{};
	Image<C, 2> meanOut(meanOutData.data(), { 1, 1 });
	img2.mean(0, meanOut);
	EXPECT_EQ(meanOut(0, 0), C(4, 0)); // (2+6)/2
}

TEST(TypeCoverage, OwnedImageComplex) {
	// OwnedImage's only type restriction is T=bool (see
	// OwnedImageBoolIsRejected's negative-compile companion); complex isn't
	// restricted at all -- it works exactly like any other non-bool T,
	// through the same converting-constructor and operator= paths already
	// covered for other types in owned_image_tests.cpp.
	using C = std::complex<double>;
	OwnedImage<C, 1> owned({ 2 });
	owned(0) = C(1, 1);
	owned(1) = C(2, -1);
	EXPECT_EQ(owned(0), C(1, 1));
	EXPECT_EQ(owned.sum(), C(3, 0));

	std::array<C, 2> srcData{ C(5, 5), C(-1, 2) };
	Image<C, 1> src(srcData.data(), { 2 });
	OwnedImage<C, 1> converted(src); // converting constructor, deep copy
	EXPECT_EQ(converted(0), C(5, 5));
	EXPECT_EQ(converted(1), C(-1, 2));
}
