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

TEST(ImageView, ViewSemantics) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(ImageView, ImageSlice) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(ImageView, ImageSwapAxes) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(ImageView, ImageMirror) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

