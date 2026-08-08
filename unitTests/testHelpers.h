#pragma once
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>
#include <numeric>
#include <initializer_list>
#include <cmath>
#include <ndl/image.h>

// Shared helpers used across the split-out test files (originally all
// defined once at the top of unitTests.cpp). Kept byte-identical to their
// original bodies -- only reportPassFail() below is new, added as the
// bridge between the pre-existing "stringstream of "label: Pass/Fail"
// lines" idiom every test function already used and GoogleTest: each
// TEST() still builds its own local passfail stringstream and runs its
// exact original checks unmodified, then hands the accumulated text to
// reportPassFail(), which turns any line ending in "Fail" into a gtest
// failure (via ADD_FAILURE(), so one failing check doesn't abort the
// others in the same TEST) while printing every line either way, matching
// the original tool's visible output.
//
// Every function here is `inline` -- this header is included by several
// independent test executables' translation units, and while each one
// today happens to include it exactly once, `inline` is what actually
// makes that safe (no multiple-definition risk if that ever changes)
// rather than relying on it staying true by convention. Deliberately not
// `using namespace ndl;` at this header's scope, unlike each .cpp file's
// own explicit using-directive -- a shared header pulling a namespace into
// every includer's global scope is a surprise callers didn't ask for, so
// Image is qualified as ndl::Image here instead.
inline std::vector<int> genLinVec(int size) {
    std::vector<int> v(size);
    std::iota(v.begin(), v.end(), int(1));
    return v;
}
inline std::vector<int> generateFlattenedArray(const std::initializer_list<int>& sizes) {
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
void passFailCheck(std::stringstream& passfail, const ndl::Image<T, DIM>& image, const std::vector<int>& refVec, const std::string testName) {
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
inline void displayBorderTests(ndl::Image<unsigned short, 3>& image3D) {
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
inline void reportPassFail(std::stringstream& passfail) {
	std::string line;
	while (std::getline(passfail, line)) {
		std::cout << line << "\n";
		if (line.size() >= 4 && line.compare(line.size() - 4, 4, "Fail") == 0) {
			ADD_FAILURE() << line;
		}
	}
}
