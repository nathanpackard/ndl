#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <algorithm>
#include <set>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/utility.h>

#include "testHelpers.h"

using namespace ndl;

TEST(ImageCore, ImageLibraryDimensions) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(ImageCore, ImageLibraryAccuracy) {
	std::stringstream passfail;

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
	code_timer("roi1 convolution", [&]() -> void
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
	reportPassFail(passfail);
}

TEST(ImageCore, ImageLibrarySpeedTest) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(ImageCore, ImageLibraryBorders) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(ImageCore, ImageConstruction) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(ImageCore, ImageElementAccess) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(ImageCore, ImageIteration) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(ImageCore, ImageIterationHighDimensional) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(ImageCore, ImageIteratorAccessors) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(ImageCore, ImageCoordinates) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(ImageCore, ImageExtentStride) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

