#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <sstream>
#include <iostream>

#include <ndl/image.h>

#include "testHelpers.h"

using namespace ndl;

TEST(ImageArithmetic, ImageArithmeticOperators) {
	std::stringstream passfail;

	std::cout << std::endl << "IMAGE ARITHMETIC OPERATORS" << std::endl;

	std::vector<int> dataA(4), dataB(4);
	Image<int, 1> a(dataA.data(), { 4 });
	Image<int, 1> b(dataB.data(), { 4 });
	for (int i = 0; i < 4; i++) { dataA[i] = i + 1; dataB[i] = 10; } // a=1,2,3,4  b=10,10,10,10

	a += b; // 11,12,13,14
	bool ok1 = true; for (int i = 0; i < 4; i++) if (a(i) != i + 11) ok1 = false;
	passfail << "operator+= (image): " << (ok1 ? "Pass" : "Fail") << std::endl;

	a -= 10; // back to 1,2,3,4
	bool ok2 = true; for (int i = 0; i < 4; i++) if (a(i) != i + 1) ok2 = false;
	passfail << "operator-= (scalar): " << (ok2 ? "Pass" : "Fail") << std::endl;

	a *= 2; // 2,4,6,8
	bool ok3 = true; for (int i = 0; i < 4; i++) if (a(i) != (i + 1) * 2) ok3 = false;
	passfail << "operator*= (scalar): " << (ok3 ? "Pass" : "Fail") << std::endl;

	a /= 2; // back to 1,2,3,4
	bool ok4 = true; for (int i = 0; i < 4; i++) if (a(i) != i + 1) ok4 = false;
	passfail << "operator/= (scalar): " << (ok4 ? "Pass" : "Fail") << std::endl;

	std::vector<int> dataF(4), dataG(4);
	Image<int, 1> f(dataF.data(), { 4 }), g(dataG.data(), { 4 });
	for (int i = 0; i < 4; i++) { dataF[i] = i + 1; dataG[i] = 2; }
	f *= g; // 2,4,6,8
	bool ok11 = true; for (int i = 0; i < 4; i++) if (f(i) != (i + 1) * 2) ok11 = false;
	passfail << "operator*= (image): " << (ok11 ? "Pass" : "Fail") << std::endl;

	a %= 3; // 1,2,0,1
	int expectedMod[4] = { 1, 2, 0, 1 };
	bool ok5 = true; for (int i = 0; i < 4; i++) if (a(i) != expectedMod[i]) ok5 = false;
	passfail << "operator%= (scalar): " << (ok5 ? "Pass" : "Fail") << std::endl;

	std::vector<int> dataC(4);
	Image<int, 1> c(dataC.data(), { 4 });
	for (int i = 0; i < 4; i++) dataC[i] = 5;
	c.negate();
	bool ok6 = true; for (int i = 0; i < 4; i++) if (c(i) != -5) ok6 = false;
	passfail << "negate(): " << (ok6 ? "Pass" : "Fail") << std::endl;

	std::vector<int> dataD(4);
	Image<int, 1> d(dataD.data(), { 4 });
	dataD[0] = 0; dataD[1] = 1; dataD[2] = 5; dataD[3] = 0;
	d.logical_not();
	bool ok7 = d(0) == 1 && d(1) == 0 && d(2) == 0 && d(3) == 1;
	passfail << "logical_not(): " << (ok7 ? "Pass" : "Fail") << std::endl;

	std::vector<int> dataE(4);
	Image<int, 1> e(dataE.data(), { 4 });
	for (int i = 0; i < 4; i++) dataE[i] = 0b0110; // 6
	e |= 0b0001; // 7
	bool ok8 = true; for (int i = 0; i < 4; i++) if (e(i) != 7) ok8 = false;
	passfail << "operator|= (scalar): " << (ok8 ? "Pass" : "Fail") << std::endl;
	e &= 0b0011; // 3
	bool ok9 = true; for (int i = 0; i < 4; i++) if (e(i) != 3) ok9 = false;
	passfail << "operator&= (scalar): " << (ok9 ? "Pass" : "Fail") << std::endl;
	e ^= 0b0001; // 2
	bool ok10 = true; for (int i = 0; i < 4; i++) if (e(i) != 2) ok10 = false;
	passfail << "operator^= (scalar): " << (ok10 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(ImageArithmetic, ImageComparisonOperators) {
	std::stringstream passfail;

	std::cout << std::endl << "IMAGE COMPARISON OPERATORS" << std::endl;

	std::vector<int> dataA(4), dataB(4), dataC(4);
	Image<int, 1> a(dataA.data(), { 4 }), b(dataB.data(), { 4 }), c(dataC.data(), { 4 });
	for (int i = 0; i < 4; i++) { dataA[i] = i; dataB[i] = i; dataC[i] = i + 1; }

	passfail << "operator== (equal images): " << (a == b ? "Pass" : "Fail") << std::endl;
	passfail << "operator!= (different images): " << (a != c ? "Pass" : "Fail") << std::endl;
	passfail << "operator< (every element less): " << (a < c ? "Pass" : "Fail") << std::endl;
	passfail << "operator<= : " << (a <= b ? "Pass" : "Fail") << std::endl;
	passfail << "operator> : " << (c > a ? "Pass" : "Fail") << std::endl;
	passfail << "operator>= : " << (b >= a ? "Pass" : "Fail") << std::endl;

	std::vector<int> dataD(4);
	Image<int, 1> d(dataD.data(), { 4 });
	for (int i = 0; i < 4; i++) dataD[i] = 5;
	passfail << "operator== (vs scalar): " << (d == 5 ? "Pass" : "Fail") << std::endl;
	passfail << "operator!= (vs scalar): " << (d != 6 ? "Pass" : "Fail") << std::endl;
	passfail << "operator< (vs scalar): " << (d < 6 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(ImageArithmetic, ImageReductions) {
	std::stringstream passfail;

	std::cout << std::endl << "IMAGE REDUCTIONS" << std::endl;

	std::vector<int> data(12);
	Image<int, 2> img(data.data(), { 4, 3 }); // x=4 (dim0), y=3 (dim1)
	int i = 0;
	for (auto it = img.begin(); it != img.end(); ++it) *it = ++i; // 1..12, x fastest

	passfail << "sum() whole image: " << (img.sum() == 78 ? "Pass" : "Fail") << std::endl;
	passfail << "min() whole image: " << (img.min() == 1 ? "Pass" : "Fail") << std::endl;
	passfail << "max() whole image: " << (img.max() == 12 ? "Pass" : "Fail") << std::endl;
	passfail << "mean() whole image: " << (std::abs(img.mean() - 6.5) < 1e-9 ? "Pass" : "Fail") << std::endl;

	// Per-axis (keepdims) reduction along dim0 (x): output extent {1,3}, one
	// result per row y. y=0: 1+2+3+4=10, y=1: 5+6+7+8=26, y=2: 9+10+11+12=42.
	std::vector<int> sumData(3);
	Image<int, 2> sumOut(sumData.data(), { 1, 3 });
	img.sum(0, sumOut);
	bool sumOk = sumOut(0, 0) == 10 && sumOut(0, 1) == 26 && sumOut(0, 2) == 42;
	passfail << "sum(axis, output) reduces dim0 correctly: " << (sumOk ? "Pass" : "Fail") << std::endl;

	std::vector<int> minData(3), maxData(3);
	Image<int, 2> minOut(minData.data(), { 1, 3 }), maxOut(maxData.data(), { 1, 3 });
	img.min(0, minOut);
	img.max(0, maxOut);
	bool minOk = minOut(0, 0) == 1 && minOut(0, 1) == 5 && minOut(0, 2) == 9;
	bool maxOk = maxOut(0, 0) == 4 && maxOut(0, 1) == 8 && maxOut(0, 2) == 12;
	passfail << "min(axis, output) reduces dim0 correctly: " << (minOk ? "Pass" : "Fail") << std::endl;
	passfail << "max(axis, output) reduces dim0 correctly: " << (maxOk ? "Pass" : "Fail") << std::endl;

	// Reduce along dim1 (y) instead, to prove axis selection isn't hardcoded to
	// dim0. Output extent {4,1}, one result per column x.
	// x=0: 1+5+9=15, x=1: 2+6+10=18, x=2: 3+7+11=21, x=3: 4+8+12=24.
	std::vector<int> sumYData(4);
	Image<int, 2> sumYOut(sumYData.data(), { 4, 1 });
	img.sum(1, sumYOut);
	bool sumYOk = sumYOut(0, 0) == 15 && sumYOut(1, 0) == 18 && sumYOut(2, 0) == 21 && sumYOut(3, 0) == 24;
	passfail << "sum(axis, output) reduces dim1 correctly: " << (sumYOk ? "Pass" : "Fail") << std::endl;

	std::vector<int> meanData(3);
	Image<int, 2> meanOut(meanData.data(), { 1, 3 });
	img.mean(0, meanOut);
	bool meanOk = meanOut(0, 0) == 2 && meanOut(0, 1) == 6 && meanOut(0, 2) == 10; // integer division of 10,26,42 by 4
	passfail << "mean(axis, output) reduces dim0 correctly: " << (meanOk ? "Pass" : "Fail") << std::endl;

	// The free-function ndl::sum(src,axis)/min/max/mean(src,axis) (owned.h)
	// wrap the exact same member calls above, just allocating their own
	// OwnedImage<T,DIM> output (the reduced/keepdims extent computed
	// internally) instead of requiring the caller to declare one -- same
	// results, checked directly against the same known values rather than
	// just assuming the wrapper is equivalent.
	auto sumFree = ndl::sum(img, 0);
	passfail << "free sum(img, axis) matches the member-call result, with no output buffer to declare: " << (sumFree.extent()[0] == 1 && sumFree.extent()[1] == 3 && sumFree(0, 0) == 10 && sumFree(0, 1) == 26 && sumFree(0, 2) == 42 ? "Pass" : "Fail") << std::endl;
	auto minFree = ndl::min(img, 0);
	auto maxFree = ndl::max(img, 0);
	passfail << "free min(img, axis) matches the member-call result: " << (minFree(0, 0) == 1 && minFree(0, 1) == 5 && minFree(0, 2) == 9 ? "Pass" : "Fail") << std::endl;
	passfail << "free max(img, axis) matches the member-call result: " << (maxFree(0, 0) == 4 && maxFree(0, 1) == 8 && maxFree(0, 2) == 12 ? "Pass" : "Fail") << std::endl;
	auto meanFree = ndl::mean(img, 0);
	passfail << "free mean(img, axis) matches the member-call result: " << (meanFree(0, 0) == 2 && meanFree(0, 1) == 6 && meanFree(0, 2) == 10 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(ImageArithmetic, ImageArithmeticNonMutating) {
	std::stringstream passfail;

	std::cout << std::endl << "IMAGE NON-MUTATING ARITHMETIC" << std::endl;

	std::vector<int> dataA(4), dataB(4), dataOut(4);
	Image<int, 1> a(dataA.data(), { 4 }), b(dataB.data(), { 4 }), out(dataOut.data(), { 4 });
	for (int i = 0; i < 4; i++) { dataA[i] = i + 1; dataB[i] = 10; } // a=1,2,3,4  b=10,10,10,10

	a.add(b, out); // 11,12,13,14
	bool addOk = true; for (int i = 0; i < 4; i++) if (out(i) != i + 11) addOk = false;
	passfail << "add(image, output) computes correct sum: " << (addOk ? "Pass" : "Fail") << std::endl;

	a.subtract(2, out); // -1,0,1,2
	bool subOk = true; for (int i = 0; i < 4; i++) if (out(i) != i - 1) subOk = false;
	passfail << "subtract(scalar, output) computes correct difference: " << (subOk ? "Pass" : "Fail") << std::endl;

	a.multiply(b, out); // 10,20,30,40
	bool mulOk = true; for (int i = 0; i < 4; i++) if (out(i) != (i + 1) * 10) mulOk = false;
	passfail << "multiply(image, output) computes correct product: " << (mulOk ? "Pass" : "Fail") << std::endl;

	a.divide(1, out); // 1,2,3,4 unchanged
	bool divOk = true; for (int i = 0; i < 4; i++) if (out(i) != i + 1) divOk = false;
	passfail << "divide(scalar, output) computes correct quotient: " << (divOk ? "Pass" : "Fail") << std::endl;

	bool aUnchanged = true; for (int i = 0; i < 4; i++) if (a(i) != i + 1) aUnchanged = false;
	passfail << "non-mutating arithmetic never wrote back into *this across all calls: " << (aUnchanged ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

