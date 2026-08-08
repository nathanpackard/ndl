#include <vector>
#include <algorithm>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdint>
#include <complex>
#include <type_traits>
#include <initializer_list>
#include <cmath>
#include <set>
#include <functional>

#include <ndl/image.h>
#include <ndl/imageIO.h>
#include <ndl/utility.h>
#include <ndl/mathHelpers.h>
#include <ndl/fft.h>
#include <ndl/matrix.h>


using namespace ndl;
using namespace ndl::fft;

std::vector<int> genLinVec(int size) {
    std::vector<int> v(size);
    std::iota(v.begin(), v.end(), int(1));
    return v;
}

std::vector<int> generateFlattenedArray(const std::initializer_list<int>& sizes) {
    // Convert initializer_list to vector
    std::vector<int> sizesVec(sizes);

    // Calculate total number of elements
    int totalSize = 1;
    for (auto size : sizesVec) {
        totalSize *= std::abs(size);
    }

    // Initialize the result vector
    std::vector<int> result(totalSize);

    // Fill the result vector with correct values
    for (int i = 0; i < totalSize; ++i) {
        int index = i;
        int flatIndex = 0;
        int stride = 1;

        for (auto it = sizesVec.begin(); it != sizesVec.end(); ++it) {
            int size = *it;
            int absSize = std::abs(size);
            int coord = index % absSize;

            // Adjust the coordinate if size is negative (reverse the order)
            if (size < 0) {
                coord = absSize - 1 - coord;
            }

            flatIndex += coord * stride;

            // // Debug output for each step
            // std::cout << "i = " << i << ", size = " << size << ", coord = " << coord
            //           << ", flatIndex = " << flatIndex << ", stride = " << stride << std::endl;

            stride *= absSize;
            index /= absSize;
        }
        result[flatIndex] = i + 1;
    }

    return result;
}

template<typename T, int DIM>
void passFailCheck(std::stringstream& passfail, const Image<T, DIM>& image, const std::vector<int>& refVec, const std::string testName) {
    bool testPassed = true;
    int total = 0;
    for (const auto &index : image.coordinates())
	{
        if (image.at(index) != refVec[total])
		{
            testPassed = false;
            break;
        }
        total++;
    }
    passfail << testName << ": " << (testPassed ? "Pass" : "Fail") << std::endl;
	if (!testPassed)
	{
		std::cout << image.to_string() << std::endl;
		total = 0;
		for (const auto &index : image.coordinates())
		{
			passfail << "    img:ref (" << image.at(index) << ":" << refVec[total] << ")" << std::endl;
			total++;
		}
	}
}

void testImageLibraryDimensions(std::stringstream& passfail)
{
	//setup variables
	int size = 4;
	int i;

	//create a 1D image with increasing values
	std::cout << std::endl << "1D Image" << std::endl;
	std::vector<unsigned short> image1Ddata(size);
	Image<unsigned short, 1> image1D(image1Ddata.data(), { size });
	i = 0;
	for (auto it = image1D.begin(); it != image1D.end(); ++it)
		*it = ++i;
	std::cout << image1D;
    passFailCheck(passfail, image1D, generateFlattenedArray({size}), "1D Image Test");


	//create a 2D image with increasing values
	std::cout << std::endl << "2D Image" << std::endl;
	std::vector<unsigned short> image2Ddata(size*size);
	Image<unsigned short, 2> image2D(image2Ddata.data(), { size, size });
	i = 0;
	for (auto it = image2D.begin(); it != image2D.end(); ++it)
		*it = ++i;
	std::cout << image2D;
    passFailCheck(passfail, image2D, generateFlattenedArray({size, size}), "2D Image Test");

	//create a 3D image with increasing values
	std::cout << std::endl << "3D Image" << std::endl;
	std::vector<unsigned short> image3Ddata( size*size*size );
	Image<unsigned short, 3> image3D(image3Ddata.data(), { size, size, size });
	i = 0;
	for (auto it = image3D.begin(); it != image3D.end(); ++it)
		*it = ++i;
	std::cout << image3D;
    passFailCheck(passfail, image3D, generateFlattenedArray({size, size, size}), "3D Image Test");

	//from the 3D image extract an ROI
	std::cout << std::endl;
	std::cout << "ExtractRoi" << std::endl;
	Image<unsigned short, 3> image3DextractRoi = image3D.view({1,1,1}, {-2,-2,-2});

	std::cout << image3DextractRoi;

	//mirror the 3D image along X direction
	std::cout << std::endl << "MirrorX" << std::endl;
	Image<unsigned short, 3> image3DmirrorX = image3D.view({}, {}, {-1, 1, 1});
	std::cout << image3DmirrorX;
    passFailCheck(passfail, image3DmirrorX, generateFlattenedArray({-size, size, size}), "MirrorX 3D Image Test 1");

	//mirror the 3D image along X direction again
	std::cout << std::endl;
	std::cout << "MirrorX" << std::endl;
	Image<unsigned short, 3> image3DmirrorX2 = image3DmirrorX.view({}, {}, {-1, 1, 1});
	std::cout << image3DmirrorX2;
    passFailCheck(passfail, image3DmirrorX2, generateFlattenedArray({size, size, size}), "MirrorX 3D Image Test 2");

	//mirror the 3D image along X direction within an ROI
	std::cout << std::endl;
	std::cout << "MirrorXRoi" << std::endl;
	Image<unsigned short, 3> image3DmirrorX3 = image3DmirrorX2.view({ 1, 1, 1 },{ -2,-2,-2 },{ -1,1,1 });
	std::cout << image3DmirrorX3;

	//mirror the 3D image along Y direction
	std::cout << std::endl;
	std::cout << "MirrorY" << std::endl;
	Image<unsigned short, 3> image3DmirrorY = image3D.view({}, {}, {1, -1, 1});
	std::cout << image3DmirrorY;
    passFailCheck(passfail, image3DmirrorY, generateFlattenedArray({size, -size, size}), "MirrorY 3D Image Test 1");

	//mirror the 3D image along Y direction again
	std::cout << std::endl;
	std::cout << "MirrorY" << std::endl;
	Image<unsigned short, 3> image3DmirrorY2 = image3DmirrorY.view({}, {}, {1, -1, 1});
	std::cout << image3DmirrorY2;
    passFailCheck(passfail, image3DmirrorY2, generateFlattenedArray({size, size, size}), "MirrorY 3D Image Test 2");

	//mirror the 3D image along Y direction within an ROI
	std::cout << std::endl;
	std::cout << "MirrorYRoi" << std::endl;
	Image<unsigned short, 3> image3DmirrorY3 = image3DmirrorY2.view({ 1, 1, 1 },{ -2,-2,-2 },{ 1,-1,1 });
	std::cout << image3DmirrorY3;

	//mirror the 3D image along Z direction
	std::cout << std::endl;
	std::cout << "MirrorZ" << std::endl;
	Image<unsigned short, 3> image3DmirrorZ = image3D.view({}, {}, {1, 1, -1});
	std::cout << image3DmirrorZ;
    passFailCheck(passfail, image3DmirrorZ, generateFlattenedArray({size, size, -size}), "MirrorZ 3D Image Test 1");

	//mirror the 3D image along Z direction again
	std::cout << std::endl;
	std::cout << "MirrorZ" << std::endl;
	Image<unsigned short, 3> image3DmirrorZ2 = image3DmirrorZ.view({}, {}, {1, 1, -1});
	std::cout << image3DmirrorZ2;
    passFailCheck(passfail, image3DmirrorZ2, generateFlattenedArray({size, size, size}), "MirrorZ 3D Image Test 2");

	//mirror the 3D image along Z direction within an ROI
	std::cout << std::endl;
	std::cout << "MirrorZRoi" << std::endl;
	Image<unsigned short, 3> image3DmirrorZ3 = image3DmirrorZ2.view({ 1, 1, 1 },{ -2, -2, -2 },{ 1, 1, -1 });
	std::cout << image3DmirrorZ3;

	//mirror the 3D image along X,Y, and Z directions
	std::cout << std::endl;
	std::cout << "MirrorXYZ" << std::endl;
	Image<unsigned short, 3> image3DmirrorXYZ = image3D.view({}, {}, {-1, -1, -1});
	std::cout << image3DmirrorXYZ;
    passFailCheck(passfail, image3DmirrorXYZ, generateFlattenedArray({-size, -size, -size}), "MirrorXYZ 3D Image");

	//swap X and Y dimensions
	std::cout << std::endl;
	std::cout << "SwapXY" << std::endl;
	Image<unsigned short, 3> image3DswapXY = image3D.swap_axes(0,1);
	std::cout << image3DswapXY;

	//swap X and Y dimensions again
	std::cout << std::endl;
	std::cout << "SwapXY" << std::endl;
	Image<unsigned short, 3> image3DswapXY2 = image3DswapXY.swap_axes(0, 1);
	std::cout << image3DswapXY2;

	//swap Y and Z dimensions
	std::cout << std::endl;
	std::cout << "SwapYZ" << std::endl;
	Image<unsigned short, 3> image3DswapYZ = image3D.swap_axes(1, 2);
	std::cout << image3DswapYZ;

	//swap Y and Z dimensions again
	std::cout << std::endl;
	std::cout << "SwapYZ" << std::endl;
	Image<unsigned short, 3> image3DswapYZ2 = image3DswapYZ.swap_axes(1, 2);
	std::cout << image3DswapYZ2;

	//swap Z and X dimensions
	std::cout << std::endl;
	std::cout << "SwapZX" << std::endl;
	Image<unsigned short, 3> image3DswapZX = image3D.swap_axes(0, 2);
	std::cout << image3DswapZX;

	//swap Z and X dimensions again
	std::cout << std::endl;
	std::cout << "SwapZX" << std::endl;
	Image<unsigned short, 3> image3DswapZX2 = image3DswapZX.swap_axes(0, 2);
	std::cout << image3DswapZX2;

	//get a sub-image while mirroring along y and skipping
	std::cout << std::endl;
	std::cout << "SubImage1" << std::endl;
	Image<unsigned short, 3> subImage1 = image3D.view({},{ -1, 3, -1 }, { 1, -2, 1 });
	std::cout << subImage1;

	//get a sub-image
	std::cout << std::endl;
	std::cout << "SubImage2" << std::endl;
	Image<unsigned short, 3> subImage2 = image3D.view({0,2,0}, {-1,3,-1}, {});
	std::cout << subImage2;

	//combination test
	std::vector<unsigned short> imagedata(8*8*1);
	Image<unsigned short, 3> image(imagedata.data(), { 8, 8, 1 });
	std::cout << std::endl;
	std::cout << "roi1" << std::endl;
	Image<unsigned short, 3> roi1 = image.view({2,2,0}, {-4,-4,0}, {2,2,1});
	for (auto it = roi1.begin(); it != roi1.end(); ++it)
		*it = (unsigned short)(10 * it.index[0] / 2 + 1 + (it.index[1] + 1));
	std::cout << roi1 << std::endl;
	std::cout << "image" << std::endl;
	std::cout << image;
}
void testImageLibraryAccuracy(std::stringstream& passfail)
{
	std::cout << std::endl;
	std::cout << "image" << std::endl;
	std::vector<double> imagedata(12*12*2);
	Image<double, 3> image(imagedata.data(), { 12, 12, 2 });
	std::cout << image;

	std::cout << std::endl;
	std::cout << "roi1" << std::endl;
	Image<double, 3> roi1 = image.view({ 2,2,0 },{ -4,-4,0 },{ 2,2,1 });
	for (auto it = roi1.begin(); it != roi1.end(); ++it)
		*it = it.index[0] / 2 + 1 + (it.index[1] + 1) / 10.0;
	std::cout << roi1;

	std::cout << std::endl;
	std::cout << "roi2" << std::endl;
	Image<double, 3> roi2 = image.view({ 3,1,0 },{ -4,-2,0 },{ 2,1,1 });
	for (auto it = roi2.begin(); it != roi2.end(); ++it)
		*it = 1;
	std::cout << roi2;

	std::cout << std::endl;
	std::cout << "image after roi updates" << std::endl;
	std::cout << image;

	std::cout << std::endl;
	Image<double, 3> result(roi1);
	double basetime = code_timer("roi1 convolution", [&]() -> void
	{
		auto resultit = result.begin();
		for (auto it = roi1.begin(); it != roi1.end(); ++it)
		{
			*resultit = (it[roi1.stride()[0]] + it[-roi1.stride()[0]] + it[roi1.stride()[1]] + it[-roi1.stride()[1]]) * 0.25;
			++resultit;
		}
	});
	std::cout << result;

	std::cout << std::endl;
	std::cout << "ROI2" << std::endl;
	std::vector<double> result2Data(std::accumulate(roi2.extent().begin(), roi2.extent().end(), 1, std::multiplies<int>()));
	Image<double, 3> result2(result2Data.data(), roi2);

	auto result2it = result2.begin();
	for (auto roi2it = roi2.begin(); roi2it != roi2.end(); ++roi2it)
	{
		*result2it = (roi2it[roi2.stride()[0]] + roi2it[-roi2.stride()[0]] + roi2it[roi2.stride()[1]] + roi2it[-roi2.stride()[1]]) / 4;
		++result2it;
	}
	std::cout << result2;

	std::cout << std::endl;
	int zeros = 0;
	for (auto value : roi1)  if (value == 0) zeros++;
	std::cout << "roi1 has " << zeros << " zeros" << std::endl;
	zeros = 0;
	for (auto value : image) if (value == 0) zeros++;
	std::cout << "image has " << zeros << " zeros" << std::endl;
	std::cout << std::endl;
}
void ImageLibrarySpeedTest(std::stringstream& passfail)
{
	int iterations = 100;
	std::vector<double> imagedata(1000 * 1000 * 2);
	Image<double, 3> image(imagedata.data(), { 1000, 1000, 2 });

	Image<double, 3> roi1 = image.view({ 2,2,1 },{ -4,-4,1 },{ 2,2,1 });
	for (auto it = roi1.begin(); it != roi1.end(); ++it)
		*it = it.index[0] / 2 + 1 + (it.index[1] + 1) / 10.0;
	Image<double, 3> roi2 = image.view({ 3,1,1 },{ -4,-2,1 },{ 2,1,1 });
	for (auto it = roi2.begin(); it != roi2.end(); ++it)
		*it = 1;

	double basetime;
	std::vector<double> roi1Data(std::accumulate(roi1.extent().begin(), roi1.extent().end(), 1, std::multiplies<int>()));
	{
		std::fill(roi1Data.begin(), roi1Data.end(), 0);
		Image<double, 3> result(roi1Data.data(), roi1);
		basetime = code_timer("roi1 convolution index", [&]() -> void
		{
			for (auto it = roi1.begin(), resultit = result.begin(); it != roi1.end(); ++it, ++resultit)
				*resultit = (it[roi1.stride()[0]] + it[-roi1.stride()[0]] + it[roi1.stride()[1]] + it[-roi1.stride()[1]]) * 0.25;
		}, iterations);
		std::cout << std::endl;
		std::cout << result.view({}, { 3,3,0 }, {});
	}

	{
		std::fill(roi1Data.begin(), roi1Data.end(), 0);
		Image<double, 3> result(roi1Data.data(), roi1);
		double noAnd = code_timer("roi1 convolution clamp", [&]() -> void
		{
			for (auto it = roi1.begin(), resultit = result.begin(); it != roi1.end(); ++it, ++resultit)
				*resultit = (it.clamp(1, 0) + it.clamp(-1, 0) + it.clamp(1, 1) + it.clamp(-1, 1)) * 0.25;
		}, iterations);
		std::cout << "noAnd is " << (100 * ((basetime / noAnd) - 1)) << "% faster" << std::endl << std::endl;
		std::cout << result.view({},{ 3,3,0 },{});
	}

	{
		std::fill(roi1Data.begin(), roi1Data.end(), 0);
		Image<double, 3> result(roi1Data.data(), roi1);
		double noAnd = code_timer("roi1 convolution wrap", [&]() -> void
		{
			for (auto it = roi1.begin(), resultit = result.begin(); it != roi1.end(); ++it, ++resultit)
				*resultit = (it.wrap(1, 0) + it.wrap(-1, 0) + it.wrap(1, 1) + it.wrap(-1, 1)) * 0.25;
		}, iterations);
		std::cout << "noAnd is " << (100 * ((basetime / noAnd) - 1)) << "% faster" << std::endl << std::endl;
		std::cout << result.view({},{ 3,3,0 },{});
	}
}
void displayBorderTests(Image<unsigned short, 3>& image3D) {
	std::cout << "value: " << std::endl;
	std::cout << "-2 x clamp: " << image3D.begin().clamp(-2, 0);
	std::cout << "-2 x wrap: " << image3D.begin().wrap(-2, 0);
	std::cout << "-2 x reflect: " << image3D.begin().reflect(-2, 0);
	std::cout << "-2 y clamp: " << image3D.begin().clamp(-2, 1);
	std::cout << "-2 y wrap: " << image3D.begin().wrap(-2, 1);
	std::cout << "-2 y reflect: " << image3D.begin().reflect(-2, 1);
	std::cout << "-2 z clamp: " << image3D.begin().clamp(-2, 2);
	std::cout << "-2 z wrap: " << image3D.begin().wrap(-2, 2);
	std::cout << "-2 z reflect: " << image3D.begin().reflect(-2, 2);
}
void testImageLibraryBorders(std::stringstream& passfail) {
	int size = 4;
	int i;

	std::cout << std::endl << "3D Image" << std::endl;
	std::vector<unsigned short> image3Ddata(size*size*size);
	Image<unsigned short, 3> image3D(image3Ddata.data(), { size, size, size });
	i = 0;
	for (auto it = image3D.begin(); it != image3D.end(); ++it)
		*it = ++i;
	std::cout << image3D;
	displayBorderTests(image3D);

	std::cout << std::endl << "MirrorX" << std::endl;
	Image<unsigned short, 3> MirrorX = image3D.view({},{ -1,-1,-1 },{ -1,1,1 });
	std::cout << MirrorX;
	displayBorderTests(MirrorX);

	std::cout << std::endl << "MirrorY" << std::endl;
	Image<unsigned short, 3> MirrorY = image3D.view({},{ -1,-1,-1 },{ 1,-1,1 });
	std::cout << MirrorY;
	displayBorderTests(MirrorY);

	std::cout << std::endl << "MirrorZ" << std::endl;
	Image<unsigned short, 3> MirrorZ = image3D.view({},{ -1,-1,-1 },{ 1,1,-1 });
	std::cout << MirrorZ;
	displayBorderTests(MirrorZ);
}
// Regression coverage for Image::view()'s documented contract (see the
// comment above view() in image.h). Two real bugs were found and fixed here:
//   1. A 1D image's iterator never terminated when decimated (step
//      magnitude > 1): the end-of-range check compared a raw pointer delta
//      against a logical extent that the stride-scaled index would never
//      exactly land on, so `it != end()` stayed true forever and walked off
//      the end of the buffer. (2D+ images were unaffected -- only a 1D
//      image's single dimension is both the "fast" pointer-delta dimension
//      and the ".back()" dimension the end sentinel checks.)
//   2. Out-of-range or end-before-start arguments (e.g. view({5},{2}) on a
//      10-element dimension) silently computed an extent that read past the
//      buffer instead of failing -- there is no wraparound support, so this
//      must be a hard error, not a heap overrun.
void testViewSemantics(std::stringstream& passfail) {
	std::cout << std::endl << "VIEW SEMANTICS" << std::endl;

	std::vector<int> data(10);
	for (int i = 0; i < 10; i++) data[i] = i;
	Image<int, 1> img(data.data(), { 10 });

	auto toVector = [](const Image<int, 1>& v) {
		std::vector<int> result;
		for (auto it = v.begin(); it != v.end(); ++it) result.push_back(*it);
		return result;
	};

	passfail << "view decimate step=2 terminates correctly: "
		<< (toVector(img.view({ 3 }, { 7 }, { 2 })) == std::vector<int>{3, 5, 7} ? "Pass" : "Fail") << std::endl;
	passfail << "view mirror step=-1: "
		<< (toVector(img.view({ 3 }, { 7 }, { -1 })) == std::vector<int>{7, 6, 5, 4, 3} ? "Pass" : "Fail") << std::endl;
	passfail << "view decimate-then-mirror step=-2 anchors at end: "
		<< (toVector(img.view({ 1 }, { 8 }, { -2 })) == std::vector<int>{8, 6, 4, 2} ? "Pass" : "Fail") << std::endl;
	passfail << "mirroring an already-mirrored view returns to the original: "
		<< (toVector(img.view({}, {}, { -1 }).view({}, {}, { -1 })) == std::vector<int>{0,1,2,3,4,5,6,7,8,9} ? "Pass" : "Fail") << std::endl;
	passfail << "view negative start counts from end: "
		<< (toVector(img.view({ -3 }, { -1 })) == std::vector<int>{7, 8, 9} ? "Pass" : "Fail") << std::endl;
	passfail << "view full reverse: "
		<< (toVector(img.view({ 0 }, { -1 }, { -1 })) == std::vector<int>{9, 8, 7, 6, 5, 4, 3, 2, 1, 0} ? "Pass" : "Fail") << std::endl;

	auto throwsOutOfRange = [](std::function<void()> f) {
		try { f(); return false; }
		catch (const std::out_of_range&) { return true; }
	};
	passfail << "view end-before-start throws: "
		<< (throwsOutOfRange([&]{ img.view({5}, {2}); }) ? "Pass" : "Fail") << std::endl;
	passfail << "view out-of-range start throws: "
		<< (throwsOutOfRange([&]{ img.view({15}, {2}); }) ? "Pass" : "Fail") << std::endl;
	passfail << "view step=0 throws: "
		<< (throwsOutOfRange([&]{ img.view({0}, {5}, {0}); }) ? "Pass" : "Fail") << std::endl;
}

void testImageConstruction(std::stringstream& passfail) {
	std::cout << std::endl << "IMAGE CONSTRUCTION" << std::endl;

	std::vector<int> data(6);
	Image<int, 2> img(data.data(), { 3, 2 });
	passfail << "construct from buffer sets extent: "
		<< (img.extent()[0] == 3 && img.extent()[1] == 2 ? "Pass" : "Fail") << std::endl;
	passfail << "static size(extent) computes element count: "
		<< (Image<int, 2>::size({ 3, 2 }) == 6 ? "Pass" : "Fail") << std::endl;

	int i = 0;
	for (auto it = img.begin(); it != img.end(); ++it) *it = ++i;

	std::vector<int> copyData(6);
	Image<int, 2> copy(copyData.data(), img); // deep copy constructor
	bool matches = true;
	for (int idx = 0; idx < 6; idx++) if (copyData[idx] != data[idx]) matches = false;
	passfail << "deep copy constructor copies values: " << (matches ? "Pass" : "Fail") << std::endl;

	data[0] = 999; // mutate original; copy must be untouched (independent memory)
	passfail << "deep copy constructor owns independent memory: "
		<< (copyData[0] != 999 ? "Pass" : "Fail") << std::endl;
}

void testImageElementAccess(std::stringstream& passfail) {
	std::cout << std::endl << "IMAGE ELEMENT ACCESS" << std::endl;

	std::vector<int> data(12);
	Image<int, 2> img(data.data(), { 4, 3 });
	int i = 0;
	for (auto it = img.begin(); it != img.end(); ++it) *it = ++i;

	passfail << "at() reads by coordinate: " << (img.at({ 2, 1 }) == 7 ? "Pass" : "Fail") << std::endl;
	passfail << "operator()(x,y) reads by coordinate: " << (img(2, 1) == 7 ? "Pass" : "Fail") << std::endl;

	img(0, 0) = 42;
	passfail << "operator()(x,y) writes through to backing memory: " << (data[0] == 42 ? "Pass" : "Fail") << std::endl;

	const Image<int, 2>& cimg = img;
	passfail << "const operator()(x,y) reads: " << (cimg(2, 1) == 7 ? "Pass" : "Fail") << std::endl;
	passfail << "const at() reads: " << (cimg.at({ 2, 1 }) == 7 ? "Pass" : "Fail") << std::endl;
}

void testImageIteration(std::stringstream& passfail) {
	std::cout << std::endl << "IMAGE ITERATION" << std::endl;

	std::vector<int> data(5);
	Image<int, 1> img(data.data(), { 5 });
	int i = 0;
	for (auto it = img.begin(); it != img.end(); ++it) *it = ++i; // 1,2,3,4,5

	auto it1 = img.begin();
	auto it2 = it1++;
	passfail << "postfix operator++ returns the pre-increment position: "
		<< (*it2 == 1 && *it1 == 2 ? "Pass" : "Fail") << std::endl;

	auto it3 = img.begin();
	auto& it4 = ++it3;
	passfail << "prefix operator++ returns a reference to itself: " << (&it4 == &it3 ? "Pass" : "Fail") << std::endl;

	const Image<int, 1>& cimg = img;
	int sum = 0;
	for (auto it = cimg.begin(); it != cimg.end(); ++it) sum += *it;
	passfail << "const_iterator visits every element: " << (sum == 15 ? "Pass" : "Fail") << std::endl;

	int sum2 = 0;
	for (auto v : img) sum2 += v;
	passfail << "range-for visits every element: " << (sum2 == 15 ? "Pass" : "Fail") << std::endl;

	passfail << "iterator get() points at the current element: " << (*img.begin().get() == 1 ? "Pass" : "Fail") << std::endl;
}

// Regression test for a bug that only exists at DIM >= 4: when an outer dimension's
// index wraps, the iterator must undo the pointer drift accumulated by *every* inner
// dimension since the outer dimension last advanced (each inner dimension cycles
// through its full extent in between). The fix in operator++ sums stride*(extent-1)
// over all inner dimensions; the previous code only subtracted the immediately-inner
// dimension's term, which is indistinguishable from correct at DIM <= 3 (there's only
// one inner dimension to get wrong) but reads out of bounds at DIM 4+. This is why
// asking for a working 4D demo surfaced it immediately.
void testImageIterationHighDimensional(std::stringstream& passfail) {
	std::cout << std::endl << "IMAGE ITERATION (4D/5D)" << std::endl;

	{
		std::vector<int> data(4 * 3 * 2 * 2);
		Image<int, 4> img(data.data(), { 4, 3, 2, 2 });
		bool ok = true;
		long count = 0;
		for (auto it = img.begin(); it != img.end(); ++it) {
			long x = it.index[0], y = it.index[1], c = it.index[2], f = it.index[3];
			long expected = x + y * 4 + c * 12 + f * 24;
			if (it.get() - data.data() != expected) ok = false;
			count++;
		}
		passfail << "4D iterator visits every element at the correct address: "
			<< (ok && count == (long)data.size() ? "Pass" : "Fail") << std::endl;
	}
	{
		std::vector<int> data(2 * 2 * 2 * 2 * 3);
		Image<int, 5> img(data.data(), { 2, 2, 2, 2, 3 });
		bool ok = true;
		long count = 0;
		for (auto it = img.begin(); it != img.end(); ++it) {
			long a = it.index[0], b = it.index[1], c = it.index[2], d = it.index[3], e = it.index[4];
			long expected = a + b * 2 + c * 4 + d * 8 + e * 16;
			if (it.get() - data.data() != expected) ok = false;
			count++;
		}
		passfail << "5D iterator visits every element at the correct address: "
			<< (ok && count == (long)data.size() ? "Pass" : "Fail") << std::endl;
	}
}

void testImageIteratorAccessors(std::stringstream& passfail) {
	std::cout << std::endl << "IMAGE ITERATOR CLAMP/WRAP/REFLECT" << std::endl;

	std::vector<int> data(5);
	Image<int, 1> img(data.data(), { 5 });
	int i = 0;
	for (auto it = img.begin(); it != img.end(); ++it) *it = ++i; // 1,2,3,4,5 at indices 0..4

	auto it = img.begin(); // sitting at index 0, value 1
	passfail << "clamp() clips an out-of-range offset to the edge: " << (it.clamp(-2, 0) == 1 ? "Pass" : "Fail") << std::endl;
	passfail << "wrap() wraps an out-of-range offset to the far edge: " << (it.wrap(-2, 0) == 4 ? "Pass" : "Fail") << std::endl;
	passfail << "reflect() reflects an out-of-range offset back in: " << (it.reflect(-2, 0) == 2 ? "Pass" : "Fail") << std::endl;
	passfail << "operator[] is a relative offset from the current position: " << (it[2] == 3 ? "Pass" : "Fail") << std::endl;
}

void testImageSlice(std::stringstream& passfail) {
	std::cout << std::endl << "IMAGE SLICE" << std::endl;

	// 3D image, extent {channels=3, w=2, h=2}, channel fastest-varying (interleaved),
	// matching how a loaded color image (e.g. bitmap.h) is laid out.
	std::vector<int> data(12);
	Image<int, 3> img(data.data(), { 3, 2, 2 });
	for (int y = 0; y < 2; y++)
		for (int x = 0; x < 2; x++)
			for (int c = 0; c < 3; c++)
				data[(y * 2 + x) * 3 + c] = c * 100 + y * 10 + x;

	Image<int, 2> ch0 = img.slice(0, 0);
	Image<int, 2> ch1 = img.slice(0, 1);

	passfail << "slice() drops one dimension from extent: "
		<< (ch0.extent()[0] == 2 && ch0.extent()[1] == 2 ? "Pass" : "Fail") << std::endl;

	bool ok = true;
	for (int y = 0; y < 2; y++)
		for (int x = 0; x < 2; x++) {
			if (ch0(x, y) != y * 10 + x) ok = false;
			if (ch1(x, y) != 100 + y * 10 + x) ok = false;
		}
	passfail << "slice() reads the correct channel's values: " << (ok ? "Pass" : "Fail") << std::endl;
}

void testImageSwapAxes(std::stringstream& passfail) {
	std::cout << std::endl << "IMAGE SWAP AXES" << std::endl;

	std::vector<int> data(6);
	Image<int, 2> img(data.data(), { 3, 2 }); // 3 wide, 2 tall
	int i = 0;
	for (auto it = img.begin(); it != img.end(); ++it) *it = ++i;

	Image<int, 2> swapped = img.swap_axes(0, 1);
	passfail << "swap_axes() swaps the extents: "
		<< (swapped.extent()[0] == 2 && swapped.extent()[1] == 3 ? "Pass" : "Fail") << std::endl;

	bool ok = true;
	for (int j = 0; j < 3; j++)
		for (int i2 = 0; i2 < 2; i2++)
			if (swapped(i2, j) != img(j, i2)) ok = false;
	passfail << "swap_axes() transposes values correctly: " << (ok ? "Pass" : "Fail") << std::endl;
}

void testImageMirror(std::stringstream& passfail) {
	std::cout << std::endl << "IMAGE MIRROR" << std::endl;

	std::vector<int> data(12);
	Image<int, 2> img(data.data(), { 4, 3 });
	int i = 0;
	for (auto it = img.begin(); it != img.end(); ++it) *it = ++i;

	Image<int, 2> mirroredX = img.mirror(0);
	bool ok = true;
	for (int y = 0; y < 3; y++)
		for (int x = 0; x < 4; x++)
			if (mirroredX(x, y) != img(3 - x, y)) ok = false;
	passfail << "mirror(dim) flips the requested dimension: " << (ok ? "Pass" : "Fail") << std::endl;

	// mirror(dim) should be equivalent to view() with a negative step across the full range
	Image<int, 2> viewMirroredX = img.view({}, {}, { -1, 1 });
	bool equivalent = true;
	for (int y = 0; y < 3; y++)
		for (int x = 0; x < 4; x++)
			if (mirroredX(x, y) != viewMirroredX(x, y)) equivalent = false;
	passfail << "mirror(dim) is equivalent to view() with step -1: " << (equivalent ? "Pass" : "Fail") << std::endl;
}

void testImageCoordinates(std::stringstream& passfail) {
	std::cout << std::endl << "IMAGE COORDINATES" << std::endl;

	std::vector<int> data(6);
	Image<int, 2> img(data.data(), { 3, 2 });
	auto coords = img.coordinates();

	passfail << "coordinates() returns extent-product count: " << (coords.size() == 6 ? "Pass" : "Fail") << std::endl;

	std::set<std::array<int, 2>> seen;
	bool allInRange = true, allUnique = true;
	for (auto& c : coords) {
		if (c[0] < 0 || c[0] >= 3 || c[1] < 0 || c[1] >= 2) allInRange = false;
		if (!seen.insert(c).second) allUnique = false;
	}
	passfail << "coordinates() are all in range: " << (allInRange ? "Pass" : "Fail") << std::endl;
	passfail << "coordinates() are all unique: " << (allUnique ? "Pass" : "Fail") << std::endl;
}

void testImageExtentStride(std::stringstream& passfail) {
	std::cout << std::endl << "IMAGE EXTENT/STRIDE ACCESSORS" << std::endl;

	std::vector<int> data(24);
	Image<int, 3> img(data.data(), { 4, 3, 2 });
	passfail << "extent() matches construction: "
		<< (img.extent()[0] == 4 && img.extent()[1] == 3 && img.extent()[2] == 2 ? "Pass" : "Fail") << std::endl;
	passfail << "stride() matches row-major layout: "
		<< (img.stride()[0] == 1 && img.stride()[1] == 4 && img.stride()[2] == 12 ? "Pass" : "Fail") << std::endl;

	Image<int, 3> roi = img.view({ 1 }, { 2 });
	passfail << "view() updates extent() for a constrained dimension: " << (roi.extent()[0] == 2 ? "Pass" : "Fail") << std::endl;
	passfail << "view() preserves stride() magnitude when step=1: " << (roi.stride()[0] == 1 ? "Pass" : "Fail") << std::endl;
}

void testImageArithmeticOperators(std::stringstream& passfail) {
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
}

void testImageComparisonOperators(std::stringstream& passfail) {
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
}

// --- Composite Image operations: chaining two or more view-producing operations ---

void testCompositeViewOfView(std::stringstream& passfail) {
	std::cout << std::endl << "COMPOSITE: VIEW OF A VIEW" << std::endl;

	std::vector<int> data(20);
	Image<int, 1> img(data.data(), { 20 });
	int i = 0;
	for (auto it = img.begin(); it != img.end(); ++it) *it = ++i; // 1..20

	Image<int, 1> roi = img.view({ 5 }, { 14 });     // original indices 5..14 -> values 6..15
	Image<int, 1> roiOfRoi = roi.view({ 2 }, { 4 }); // roi-local indices 2..4 -> original indices 7..9 -> values 8,9,10

	std::vector<int> expected = { 8, 9, 10 };
	std::vector<int> actual;
	for (auto it = roiOfRoi.begin(); it != roiOfRoi.end(); ++it) actual.push_back(*it);
	passfail << "view() of a view() composes correctly: " << (actual == expected ? "Pass" : "Fail") << std::endl;
}

void testCompositeSwapThenView(std::stringstream& passfail) {
	std::cout << std::endl << "COMPOSITE: SWAP AXES THEN VIEW" << std::endl;

	std::vector<int> data(12);
	Image<int, 2> img(data.data(), { 4, 3 });
	int i = 0;
	for (auto it = img.begin(); it != img.end(); ++it) *it = ++i;

	Image<int, 2> swapped = img.swap_axes(0, 1); // extent becomes {3,4}
	Image<int, 2> roi = swapped.view({ 1 }, { 2 }); // constrains swapped's dim0, which was img's dim1 (y)

	bool ok = true;
	for (int y = 0; y < 2; y++)
		for (int x = 0; x < 4; x++)
			if (roi(y, x) != img(x, y + 1)) ok = false;
	passfail << "view() after swap_axes() reads the correct memory: " << (ok ? "Pass" : "Fail") << std::endl;
}

void testCompositeMirrorThenSlice(std::stringstream& passfail) {
	std::cout << std::endl << "COMPOSITE: MIRROR THEN SLICE" << std::endl;

	std::vector<int> data(24); // 3 channels x 4 wide x 2 tall, channel-interleaved
	Image<int, 3> img(data.data(), { 3, 4, 2 });
	for (int y = 0; y < 2; y++)
		for (int x = 0; x < 4; x++)
			for (int c = 0; c < 3; c++)
				data[(y * 4 + x) * 3 + c] = c * 100 + y * 10 + x;

	Image<int, 3> mirroredX = img.mirror(1); // mirror the width dimension (dim 1)
	Image<int, 2> ch1 = mirroredX.slice(0, 1); // channel 1, width mirrored

	bool ok = true;
	for (int y = 0; y < 2; y++)
		for (int x = 0; x < 4; x++)
			if (ch1(x, y) != img(1, 3 - x, y)) ok = false;
	passfail << "slice() after mirror() reads the correct (flipped) memory: " << (ok ? "Pass" : "Fail") << std::endl;
}

void testCompositeWriteThroughView(std::stringstream& passfail) {
	std::cout << std::endl << "COMPOSITE: WRITE THROUGH A CHAIN OF VIEWS" << std::endl;

	std::vector<int> data(16);
	Image<int, 2> img(data.data(), { 4, 4 });
	for (auto it = img.begin(); it != img.end(); ++it) *it = 0;

	Image<int, 2> roi = img.view({ 1 }, { 2 });          // columns 1-2, all rows
	Image<int, 2> mirrored = roi.view({}, {}, { -1, 1 }); // mirror that ROI's columns

	mirrored(0, 0) = 77; // mirrored col 0 = roi's last col = roi col 1 = img col 2, row 0
	passfail << "write through a composed view lands at the correct original element: "
		<< (img(2, 0) == 77 ? "Pass" : "Fail") << std::endl;

	int nonZeroCount = 0;
	for (auto v : img) if (v != 0) nonZeroCount++;
	passfail << "write through a composed view only changes that one element: "
		<< (nonZeroCount == 1 ? "Pass" : "Fail") << std::endl;
}

void TestImages(std::stringstream& passfail, std::string inputFolder, std::string outputFolder)
{
	std::array<int, 3> imageExtent;
	std::vector<uint8_t> imageData = image_io::load(inputFolder + "/ng_bwgirl_crop.jpg", imageExtent);
	Image<uint8_t, 3> image(imageData.data(), imageExtent);
	Image<uint8_t, 2> red = image.slice(0, 0);
	Image<uint8_t, 2> green = image.slice(0, 1);
	Image<uint8_t, 2> blue = image.slice(0, 2);
	image_io::save_raw(red, outputFolder + std::string("/red_") + std::to_string(red.extent()[0]) + "x" + std::to_string(red.extent()[1]) + ".raw");
	image_io::save_raw(green, outputFolder + std::string("/green_") + std::to_string(green.extent()[0]) + "x" + std::to_string(green.extent()[1]) + ".raw");
	image_io::save_raw(blue, outputFolder + std::string("/blue_") + std::to_string(blue.extent()[0]) + "x" + std::to_string(blue.extent()[1]) + ".raw");
	image_io::save(image, outputFolder + "/ng_bwgirl_crop.nrrd");
	image_io::save(image, outputFolder + "/ng_bwgirl_crop.bmp");
	image_io::save(red, outputFolder + "/ng_bwgirl_crop_red.nrrd");

	std::array<int, 3> extentNrrd;
	std::vector<uint8_t> reloadVector = image_io::load_nrrd<uint8_t, 3>(extentNrrd, outputFolder + "/ng_bwgirl_crop.nrrd");
	Image<uint8_t, 3> reload(reloadVector.data(), extentNrrd);
	image_io::save(reload, outputFolder + "/ng_bwgirl_crop_RESAVE.bmp");

	std::vector<float> fimagedata(image.size());
	Image<float, 3> fimage(fimagedata.data(), image);
	image_io::save_raw(fimage, outputFolder + std::string("/float_") + std::to_string(fimage.extent()[0]) + "x" + std::to_string(fimage.extent()[1]) + "x" + std::to_string(fimage.extent()[2]) + ".raw");

	std::array<int, 3> bmpImageExtent;
	std::vector<uint8_t> bmpImageData = image_io::load(inputFolder + "/marbles.bmp", bmpImageExtent);
	Image<uint8_t, 3> bmpImage(bmpImageData.data(), bmpImageExtent);
	image_io::save(bmpImage, outputFolder + "/marbles_output.bmp");

	Image<uint8_t, 2> red2 = bmpImage.slice(0, 0);
	Image<uint8_t, 2> green2 = bmpImage.slice(0, 1);
	Image<uint8_t, 2> blue2 = bmpImage.slice(0, 2);

	image_io::save(red2, outputFolder + "/marbles_red2_output.bmp");
	image_io::save(green2, outputFolder + "/marbles_green2_output.bmp");
	image_io::save(blue2, outputFolder + "/marbles_blue2_output.bmp");

	image_io::save_raw(red2, outputFolder + std::string("/red2_") + std::to_string(red2.extent()[0]) + "x" + std::to_string(red2.extent()[1]) + ".raw");
	image_io::save_raw(green2, outputFolder + std::string("/green2_") + std::to_string(green2.extent()[0]) + "x" + std::to_string(green2.extent()[1]) + ".raw");
	image_io::save_raw(blue2, outputFolder + std::string("/blue2_") + std::to_string(blue2.extent()[0]) + "x" + std::to_string(blue2.extent()[1]) + ".raw");
	std::array<int, 3> dcmImage2Extent;
	std::vector<int16_t> dcmImage2data = image_io::load_dicom<int16_t>(inputFolder + "/foot.dcm", dcmImage2Extent);
	Image<int16_t, 3> dcmImage2(dcmImage2data.data(), dcmImage2Extent);
	image_io::save_raw(dcmImage2, outputFolder + std::string("/CT_") + std::to_string(dcmImage2Extent[0]) + "x" + std::to_string(dcmImage2Extent[1]) + ".raw");
}
void testcomplex(std::stringstream& passfail) {
	const int length = 1024;
	const int maxprint = 16;
	//setup a complex input vector
	std::cout << "\nTESTCOMPLEX";
	std::vector<std::complex<double>> time(length);
	for (int i = 0; i<length; i++)
		time[i] = std::min(i + 1, 10);
	std::cout << "\n========================\nORIGINAL\n========================\n";
	for (int i = 0; i<maxprint; i++)
		std::cout << time[i] << "\n";

	//setup an FFT object
	std::vector<std::complex<double>> freq(length);
	std::vector<double> scratch(length * 4);
	FFT<double, length> fft(scratch.data());

	//run the fft a bunch of times
	clock_t start = clock();
	for (int i = 0; i<128; i++) {
		fft.fft(length, time.data(), freq.data());
	}
	double ellapsed = double(clock() - start) / double(CLOCKS_PER_SEC);
	std::cout << "==========================\n Ellapsed time: " << ellapsed << " sec\n";

	//display frequency data
	std::cout << "\n========================\nFREQ\n========================\n";
	for (int i = 0; i<maxprint; i++)
		std::cout << freq[i] << "\n";

	//display the time data again
	fft.ifft(length, freq.data(), time.data());
	std::cout << "\n========================\nAND BACK\n========================\n";
	for (int i = 0; i<maxprint; i++)
		std::cout << time[i] << "\n";
	std::cout << "==========================\n Ellapsed time: " << std::scientific << ellapsed << " sec\n";
}
void testreal(std::stringstream& passfail) {
	const int length = 1024;
	const int maxprint = 16;
	std::cout << "\nTESTREAL";
	std::vector<double> input(length);
	std::vector<std::complex<double>> output(length);
	std::vector<double> scratch(length * 5);
	FFTReal<double, length> fft(scratch.data());
	for (int i = 0; i < length; i++)
		input[i] = std::min(i + 1, 10);

	std::cout << "\n========================\nORIGINAL\n========================\n";
	for (int i = 0; i<maxprint; i++) std::cout << input[i] << "\n";

	std::cout << "\n========================\nFREQ\n========================\n";
	clock_t start = clock();
	for (int i = 0; i<768; i++)
		fft.fft(length, input.data(), output.data());
	double ellapsed = double(clock() - start) / double(CLOCKS_PER_SEC);
	for (int i = 0; i<maxprint; i++)
		std::cout << output[i] << "\n";

	std::cout << "\n========================\nAND BACK\n========================\n";
	fft.ifft(length, output.data(), input.data());
	for (int i = 0; i<maxprint; i++)
		std::cout << input[i] << "\n";
	std::cout << "==========================\n Ellapsed time: " << std::scientific  << ellapsed << " sec\n";
}


void testMatrixConstruction(std::stringstream& passfail) {
	std::cout << std::endl << "MATRIX CONSTRUCTION" << std::endl;

	Matrix<double, 3> m; // default constructor -> identity
	bool isIdentity = true;
	for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) if (m(i, j) != (i == j ? 1.0 : 0.0)) isIdentity = false;
	passfail << "default constructor produces the identity: " << (isIdentity ? "Pass" : "Fail") << std::endl;

	double raw[9] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	Matrix<double, 3> m2(raw);
	bool matches = true;
	for (int i = 0; i < 9; i++) if (m2.data()[i] != raw[i]) matches = false;
	passfail << "explicit constructor copies from a raw array: " << (matches ? "Pass" : "Fail") << std::endl;
}

void testMatrixElementAccess(std::stringstream& passfail) {
	std::cout << std::endl << "MATRIX ELEMENT ACCESS" << std::endl;

	Matrix<double, 3> m;
	m(0, 1) = 5;
	passfail << "operator()(i,j) writes and reads: " << (m(0, 1) == 5 ? "Pass" : "Fail") << std::endl;
	passfail << "operator[] bracket-chain access matches operator(): " << (m[0][1] == 5 ? "Pass" : "Fail") << std::endl;

	const Matrix<double, 3>& cm = m;
	passfail << "const operator()(i,j): " << (cm(0, 1) == 5 ? "Pass" : "Fail") << std::endl;
	passfail << "const operator[]: " << (cm[0][1] == 5 ? "Pass" : "Fail") << std::endl;
}

void testMatrixDeterminant(std::stringstream& passfail) {
	std::cout << std::endl << "MATRIX DETERMINANT" << std::endl;

	Matrix<double, 2> m2;
	m2(0, 0) = 4; m2(0, 1) = 3; m2(1, 0) = 6; m2(1, 1) = 3;
	passfail << "2x2 determinant: " << (std::abs(m2.determinant() - (-6.0)) < 1e-9 ? "Pass" : "Fail") << std::endl;

	// classic worked example (det = -306)
	Matrix<double, 3> m3;
	double vals3[9] = { 6, 1, 1, 4, -2, 5, 2, 8, 7 };
	int k = 0; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m3(i, j) = vals3[k++];
	passfail << "3x3 determinant (non-diagonal): " << (std::abs(m3.determinant() - (-306.0)) < 1e-6 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 4> m4;
	for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) m4(i, j) = (i == j) ? (i + 1) : 0;
	passfail << "4x4 determinant (diagonal): " << (std::abs(m4.determinant() - 24.0) < 1e-9 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 5> m5;
	for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++) m5(i, j) = (i == j) ? (i + 1) : 0;
	passfail << "5x5 determinant (Laplace expansion): " << (std::abs(m5.determinant() - 120.0) < 1e-9 ? "Pass" : "Fail") << std::endl;
}

void testMatrixCofactor(std::stringstream& passfail) {
	std::cout << std::endl << "MATRIX COFACTOR" << std::endl;

	Matrix<double, 3> m;
	double vals[9] = { 1, 2, 3, 0, 4, 5, 1, 0, 6 };
	int k = 0; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m(i, j) = vals[k++];

	// cofactor(0,0) = +det[[4,5],[0,6]] = 24
	passfail << "cofactor(0,0): " << (std::abs(m.cofactor(0, 0) - 24.0) < 1e-9 ? "Pass" : "Fail") << std::endl;
	// cofactor(0,1) = -det[[0,5],[1,6]] = -(0*6-5*1) = 5 -- also checks the sign alternation
	passfail << "cofactor(0,1) sign alternates correctly: " << (std::abs(m.cofactor(0, 1) - 5.0) < 1e-9 ? "Pass" : "Fail") << std::endl;
}

void testMatrixArithmeticOperators(std::stringstream& passfail) {
	std::cout << std::endl << "MATRIX ARITHMETIC OPERATORS" << std::endl;

	Matrix<double, 2> a, b;
	a(0, 0) = 1; a(0, 1) = 2; a(1, 0) = 3; a(1, 1) = 4;
	b(0, 0) = 5; b(0, 1) = 6; b(1, 0) = 7; b(1, 1) = 8;

	Matrix<double, 2> sum = a; sum += b;
	passfail << "operator+= (matrix): " << (sum(0, 0) == 6 && sum(0, 1) == 8 && sum(1, 0) == 10 && sum(1, 1) == 12 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> diff = b; diff -= a;
	passfail << "operator-= (matrix): " << (diff(0, 0) == 4 && diff(0, 1) == 4 && diff(1, 0) == 4 && diff(1, 1) == 4 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> scaled = a; scaled *= 2;
	passfail << "operator*= (scalar): " << (scaled(0, 0) == 2 && scaled(1, 1) == 8 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> divided = scaled; divided /= 2;
	passfail << "operator/= (scalar): " << (divided == a ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> product = a * b; // [1,2;3,4] * [5,6;7,8] = [19,22;43,50]
	passfail << "operator* (matrix*matrix): "
		<< (product(0, 0) == 19 && product(0, 1) == 22 && product(1, 0) == 43 && product(1, 1) == 50 ? "Pass" : "Fail") << std::endl;

	std::array<double, 2> v = { 1, 1 };
	auto r = a * v; // [1*1+2*1, 3*1+4*1] = [3,7]
	passfail << "operator* (matrix*array): " << (std::abs(r[0] - 3.0) < 1e-9 && std::abs(r[1] - 7.0) < 1e-9 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> neg = -a;
	passfail << "unary operator-: " << (neg(0, 0) == -1 && neg(1, 1) == -4 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> plusScalar = a + 10;
	passfail << "operator+ (scalar): " << (plusScalar(0, 0) == 11 && plusScalar(1, 1) == 14 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> minusScalar = a - 1;
	passfail << "operator- (scalar): " << (minusScalar(0, 0) == 0 && minusScalar(1, 1) == 3 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> mulScalar = a * 3;
	passfail << "operator* (scalar): " << (mulScalar(0, 0) == 3 && mulScalar(1, 1) == 12 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> divScalar = a / 2;
	passfail << "operator/ (scalar): " << (std::abs(divScalar(0, 0) - 0.5) < 1e-9 && divScalar(1, 1) == 2 ? "Pass" : "Fail") << std::endl;

	passfail << "operator==: " << (a == a ? "Pass" : "Fail") << std::endl;
	passfail << "operator!=: " << (a != b ? "Pass" : "Fail") << std::endl;
}

void testMatrixSetters(std::stringstream& passfail) {
	std::cout << std::endl << "MATRIX SETTERS" << std::endl;

	Matrix<double, 3> m;
	m(0, 0) = 9;
	m.set_zero();
	bool ok1 = true; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) if (m(i, j) != 0) ok1 = false;
	passfail << "set_zero(): " << (ok1 ? "Pass" : "Fail") << std::endl;

	m.set_identity();
	bool ok2 = true; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) if (m(i, j) != (i == j ? 1.0 : 0.0)) ok2 = false;
	passfail << "set_identity(): " << (ok2 ? "Pass" : "Fail") << std::endl;

	double col[3] = { 1, 2, 3 };
	m.set_column(1, col);
	passfail << "set_column(): " << (m(0, 1) == 1 && m(1, 1) == 2 && m(2, 1) == 3 ? "Pass" : "Fail") << std::endl;

	double row[3] = { 7, 8, 9 };
	m.set_row(0, row);
	passfail << "set_row(): " << (m(0, 0) == 7 && m(0, 1) == 8 && m(0, 2) == 9 ? "Pass" : "Fail") << std::endl;

	double diag[3] = { 4, 5, 6 };
	m.set_diagonal(diag);
	passfail << "set_diagonal(): " << (m(0, 0) == 4 && m(1, 1) == 5 && m(2, 2) == 6 ? "Pass" : "Fail") << std::endl;
}

void testMatrixTranspose(std::stringstream& passfail) {
	std::cout << std::endl << "MATRIX TRANSPOSE" << std::endl;

	Matrix<double, 2> m;
	m(0, 0) = 1; m(0, 1) = 2; m(1, 0) = 3; m(1, 1) = 4;

	Matrix<double, 2> t = m.transpose();
	passfail << "transpose() returns the transposed matrix: " << (t(0, 0) == 1 && t(0, 1) == 3 && t(1, 0) == 2 && t(1, 1) == 4 ? "Pass" : "Fail") << std::endl;
	passfail << "transpose() does not mutate the original: " << (m(0, 1) == 2 ? "Pass" : "Fail") << std::endl;

	m.transpose_in_place();
	passfail << "transpose_in_place() mutates the original: " << (m(0, 0) == 1 && m(0, 1) == 3 && m(1, 0) == 2 && m(1, 1) == 4 ? "Pass" : "Fail") << std::endl;
}

void testMatrixInverse(std::stringstream& passfail) {
	std::cout << std::endl << "MATRIX INVERSE" << std::endl;

	// general (non-rotation) 2x2: det=10, inverse = 1/10 * [6,-7;-2,4]
	Matrix<double, 2> m;
	m(0, 0) = 4; m(0, 1) = 7; m(1, 0) = 2; m(1, 1) = 6;
	Matrix<double, 2> inv = m.inverse();
	bool ok1 = std::abs(inv(0, 0) - 0.6) < 1e-9 && std::abs(inv(0, 1) - (-0.7)) < 1e-9
		&& std::abs(inv(1, 0) - (-0.2)) < 1e-9 && std::abs(inv(1, 1) - 0.4) < 1e-9;
	passfail << "inverse() of a general 2x2 matrix: " << (ok1 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> identity2 = m * inv;
	bool ok2 = std::abs(identity2(0, 0) - 1) < 1e-9 && std::abs(identity2(0, 1)) < 1e-9
		&& std::abs(identity2(1, 0)) < 1e-9 && std::abs(identity2(1, 1) - 1) < 1e-9;
	passfail << "m * m.inverse() == identity: " << (ok2 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> m2 = m;
	m2.invert();
	bool ok3 = true;
	for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) if (std::abs(m2(i, j) - inv(i, j)) > 1e-9) ok3 = false;
	passfail << "invert() matches inverse(): " << (ok3 ? "Pass" : "Fail") << std::endl;

	// a rotation matrix is orthogonal: its inverse equals its transpose
	Matrix<double, 3> rot;
	rot.set_rotate(M_PI / 4, 0, 1);
	Matrix<double, 3> rotInv = rot.inverse();
	Matrix<double, 3> rotTranspose = rot.transpose();
	bool ok4 = true;
	for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) if (std::abs(rotInv(i, j) - rotTranspose(i, j)) > 1e-9) ok4 = false;
	passfail << "rotation matrix inverse() == transpose(): " << (ok4 ? "Pass" : "Fail") << std::endl;
}

void testMatrixTransforms(std::stringstream& passfail) {
	std::cout << std::endl << "MATRIX TRANSFORMS" << std::endl;

	Matrix<double, 3> scaleNonHomog;
	std::array<double, 3> s3 = { 2, 3, 4 };
	scaleNonHomog.set_scale(s3);
	passfail << "set_scale() (non-homogeneous, N components): "
		<< (scaleNonHomog(0, 0) == 2 && scaleNonHomog(1, 1) == 3 && scaleNonHomog(2, 2) == 4 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 4> scaleHomog;
	std::array<double, 3> s2 = { 2, 3, 4 };
	scaleHomog.set_scale(s2);
	passfail << "set_scale() (homogeneous, N-1 components, last stays 1): "
		<< (scaleHomog(0, 0) == 2 && scaleHomog(1, 1) == 3 && scaleHomog(2, 2) == 4 && scaleHomog(3, 3) == 1 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 4> translate;
	std::array<double, 3> t = { 5, 6, 7 };
	translate.set_translate(t);
	double p[4] = { 0, 0, 0, 1 };
	translate.transform_point(p);
	passfail << "set_translate() + transform_point(): "
		<< (std::abs(p[0] - 5) < 1e-9 && std::abs(p[1] - 6) < 1e-9 && std::abs(p[2] - 7) < 1e-9 ? "Pass" : "Fail") << std::endl;

	// set_rotate() on a plain (non-homogeneous) matrix is a direct linear transform,
	// applied via matrix*vector -- transform_point is for homogeneous coordinates only.
	Matrix<double, 2> rot;
	rot.set_rotate(M_PI / 2, 0, 1);
	std::array<double, 2> v = { 1, 0 };
	auto rotated = rot * v;
	passfail << "set_rotate() rotates (1,0) by 90 degrees to (0,1): "
		<< (std::abs(rotated[0]) < 1e-9 && std::abs(rotated[1] - 1) < 1e-9 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> shear;
	shear.set_shear(2, 0, 1);
	passfail << "set_shear(): " << (shear(0, 1) == 2 && shear(0, 0) == 1 && shear(1, 1) == 1 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 3> proj;
	proj.set_projection(5.0, 0);
	passfail << "set_projection(): "
		<< (proj(0, 0) == 5 && proj(1, 1) == 5 && proj(2, 0) == 1 && proj(2, 1) == 0 && proj(2, 2) == 0 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 3> orthoProj;
	orthoProj.set_ortho_projection(5.0, 0);
	passfail << "set_ortho_projection(): "
		<< (orthoProj(0, 0) == 0 && orthoProj(0, 2) == 5 && orthoProj(1, 1) == 1 && orthoProj(2, 2) == 1 ? "Pass" : "Fail") << std::endl;
}

void testMatrixDotOuterProduct(std::stringstream& passfail) {
	std::cout << std::endl << "MATRIX DOT/OUTER PRODUCT" << std::endl;

	Matrix<double, 2> m;
	m(0, 0) = 1; m(0, 1) = 2; m(1, 0) = 3; m(1, 1) = 4;
	std::array<double, 2> v = { 1, 1 };
	std::array<double, 2> result;
	m.dot_product(v, result); // result[i] = sum_j m(i,j)*v[j]
	passfail << "dot_product(): " << (std::abs(result[0] - 3) < 1e-9 && std::abs(result[1] - 7) < 1e-9 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> outer;
	std::array<double, 2> a = { 2, 3 };
	std::array<double, 2> b = { 5, 7 };
	outer.outer_product(a, b); // outer(i,j) = a[i]*b[j]
	passfail << "outer_product(): " << (outer(0, 0) == 10 && outer(0, 1) == 14 && outer(1, 0) == 15 && outer(1, 1) == 21 ? "Pass" : "Fail") << std::endl;
}

void testMatrixEigenDecomposition(std::stringstream& passfail) {
	std::cout << std::endl << "MATRIX EIGEN DECOMPOSITION" << std::endl;

	bool threw = false;
	try {
		Matrix<double, 3> m;
		std::array<double, 3> eigenvalues;
		Matrix<double, 3> eigenvectors;
		m.eigen_decomposition(eigenvalues, eigenvectors);
	} catch (const std::logic_error&) {
		threw = true;
	}
	passfail << "eigen_decomposition() reports unimplemented rather than silently returning zeros: " << (threw ? "Pass" : "Fail") << std::endl;
}

// --- Composite Matrix operations: chaining multiple transforms/operations ---

void testCompositeTransformChain(std::stringstream& passfail) {
	std::cout << std::endl << "COMPOSITE: TRANSFORM CHAIN (scale, then rotate, then translate)" << std::endl;

	Matrix<double, 3> scale;
	std::array<double, 2> s = { 2, 2 };
	scale.set_scale(s);

	Matrix<double, 3> rotate;
	rotate.set_rotate(M_PI / 2, 0, 1);

	Matrix<double, 3> translate;
	std::array<double, 2> t = { 10, 0 };
	translate.set_translate(t);

	// matrix multiplication associates right-to-left when applied to a point:
	// (translate * rotate * scale) * p == translate * (rotate * (scale * p))
	Matrix<double, 3> combined = translate * rotate * scale;

	double p[3] = { 1, 0, 1 }; // homogeneous point (1,0)
	combined.transform_point(p);
	// scale (1,0)->(2,0); rotate 90 deg (2,0)->(0,2); translate +(10,0) -> (10,2)
	passfail << "chained transform applies scale, rotate, translate in the expected order: "
		<< (std::abs(p[0] - 10) < 1e-6 && std::abs(p[1] - 2) < 1e-6 ? "Pass" : "Fail") << std::endl;
}

// Regression test for a bug where composing a further view (roi/mirror/swap via
// view()) on top of a slice()-derived image silently lost the slice's
// channel offset, making every channel of a sliced-then-viewed image alias
// channel 0. See the root_data_ comment on the slice constructor in image.h.
void testSliceComposition(std::stringstream& passfail) {
	std::cout << std::endl << "SLICE COMPOSITION" << std::endl;

	int w = 4, h = 3, channels = 3;
	std::vector<int> data(channels * w * h);
	// Extent = {channels, w, h} means the channel dimension is fastest-varying
	// (stride 1), i.e. per-pixel interleaved storage -- matching how a loaded
	// color image (e.g. from bitmap.h) is laid out.
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			for (int c = 0; c < channels; c++)
				data[(y * w + x) * channels + c] = c * 1000 + y * 10 + x;
	Image<int, 3> img(data.data(), { channels, w, h });

	bool allMatch = true;
	for (int c = 0; c < channels; c++)
	{
		Image<int, 2> channelSlice = img.slice(0, c);
		Image<int, 2> mirroredY = channelSlice.view({}, {}, { 1, -1 });
		for (int y = 0; y < h && allMatch; y++)
			for (int x = 0; x < w && allMatch; x++)
			{
				int expected = c * 1000 + (h - 1 - y) * 10 + x;
				if (mirroredY.at({ x, y }) != expected) allMatch = false;
			}
	}
	passfail << "Slice then mirror reads correct channel: " << (allMatch ? "Pass" : "Fail") << std::endl;
}

#include <filesystem>
namespace fs = std::filesystem;

#ifndef NDL_TEST_DATA_DIR
#define NDL_TEST_DATA_DIR "."
#endif

#ifndef NDL_TEST_OUTPUT_DIR
#define NDL_TEST_OUTPUT_DIR "output"
#endif

int main()
{
    std::stringstream passfail;

    // Both directories are baked in at configure time (see CMakeLists.txt) so this
    // works regardless of where the build directory lives relative to the source tree.
    fs::path dataPath = NDL_TEST_DATA_DIR;
    fs::path tmpPath  = NDL_TEST_OUTPUT_DIR;
    fs::create_directories(tmpPath);

    testImageLibraryDimensions(passfail);
    testImageLibraryAccuracy(passfail);
    ImageLibrarySpeedTest(passfail);
    testImageLibraryBorders(passfail);

    // Image: individual operations
    testImageConstruction(passfail);
    testImageElementAccess(passfail);
    testImageIteration(passfail);
    testImageIterationHighDimensional(passfail);
    testImageIteratorAccessors(passfail);
    testViewSemantics(passfail);
    testImageSlice(passfail);
    testImageSwapAxes(passfail);
    testImageMirror(passfail);
    testImageCoordinates(passfail);
    testImageExtentStride(passfail);
    testImageArithmeticOperators(passfail);
    testImageComparisonOperators(passfail);

    // Image: composite operations
    testCompositeViewOfView(passfail);
    testCompositeSwapThenView(passfail);
    testCompositeMirrorThenSlice(passfail);
    testCompositeWriteThroughView(passfail);
    testSliceComposition(passfail);

    // Matrix: individual operations
    testMatrixConstruction(passfail);
    testMatrixElementAccess(passfail);
    testMatrixDeterminant(passfail);
    testMatrixCofactor(passfail);
    testMatrixArithmeticOperators(passfail);
    testMatrixSetters(passfail);
    testMatrixTranspose(passfail);
    testMatrixInverse(passfail);
    testMatrixTransforms(passfail);
    testMatrixDotOuterProduct(passfail);
    testMatrixEigenDecomposition(passfail);

    // Matrix: composite operations
    testCompositeTransformChain(passfail);

    TestImages(passfail, dataPath.string(), tmpPath.string());
    testreal(passfail);
    testcomplex(passfail);

    std::cout << "\n\nPass/Fail Results:\n" << passfail.str() << "\n";
    return 0;
}
