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

TEST(PackedBitImage, PackedBitImage) {
	std::stringstream passfail;

	std::cout << std::endl << "PACKED_BIT_IMAGE" << std::endl;

	PackedBitImage<2> bits({ 5, 5 });
	passfail << "PackedBitImage starts all-zero: " << (bits.count() == 0 && !bits.any() ? "Pass" : "Fail") << std::endl;

	bits(2, 2) = true;
	bits.at({ 0, 0 }) = true;
	passfail << "operator()/at() write-then-read round-trip: " << (bits(2, 2) == true && bits.at({ 0, 0 }) == true && bits(1, 1) == false ? "Pass" : "Fail") << std::endl;
	passfail << "count() tracks the number of set bits: " << (bits.count() == 2 ? "Pass" : "Fail") << std::endl;

	// A size that spans more than one 64-bit storage word, to catch any
	// off-by-word-boundary bug in flatIndex()/word-vs-bit-offset math.
	PackedBitImage<1> big({ 200 });
	for (int i = 0; i < 200; i++) big(i) = (i % 7 == 0);
	bool wordBoundaryOk = true;
	std::size_t expectedCount = 0;
	for (int i = 0; i < 200; i++)
	{
		bool expected = (i % 7 == 0);
		if (bool(big(i)) != expected) wordBoundaryOk = false;
		if (expected) expectedCount++;
	}
	passfail << "PackedBitImage is correct across 64-bit word boundaries (200 bits): " << (wordBoundaryOk && big.count() == expectedCount ? "Pass" : "Fail") << std::endl;

	// Plain value semantics -- unlike OwnedImage, PackedBitImage doesn't
	// inherit from anything, so it has no pointer-into-self to invalidate,
	// and copy/move are both the ordinary compiler-generated defaults.
	passfail << "PackedBitImage is copy-constructible: " << (std::is_copy_constructible_v<PackedBitImage<2>> ? "Pass" : "Fail") << std::endl;
	passfail << "PackedBitImage is copy-assignable: " << (std::is_copy_assignable_v<PackedBitImage<2>> ? "Pass" : "Fail") << std::endl;
	PackedBitImage<2> copy = bits;
	copy(1, 1) = true;
	passfail << "copying a PackedBitImage is a real deep copy, not aliasing: " << (bits(1, 1) == false && copy(1, 1) == true ? "Pass" : "Fail") << std::endl;

	// Cross-check: the same free ndl::erode()/dilate()/median_filter()/
	// threshold() functions Image's own erode()/dilate()/etc. forward to,
	// run directly against a PackedBitImage, should give bit-for-bit the
	// same answer as running the identical operation on an equivalent 0/1
	// Image<uint8_t,2> -- same algorithm, two storage backends.
	std::array<int, 2> ext = { 7, 7 };
	PackedBitImage<2> srcBits(ext);
	OwnedImage<uint8_t, 2> srcBytes(ext);
	srcBytes = uint8_t(0);
	for (int y = 2; y <= 4; y++)
		for (int x = 2; x <= 4; x++)
		{
			srcBits(x, y) = true;
			srcBytes(x, y) = 1;
		}

	OwnedImage<double, 2> box3({ 3, 3 });
	make_box_kernel(box3);
	OwnedImage<double, 2> cross3({ 3, 3 });
	make_cross_kernel(cross3);

	PackedBitImage<2> erodedBits(ext);
	OwnedImage<uint8_t, 2> erodedBytes(ext);
	ndl::erode(srcBits, erodedBits, box3, BorderMode::Clamp);
	ndl::erode(srcBytes, erodedBytes, box3, BorderMode::Clamp);
	bool erodeMatches = true;
	for (const auto& coord : erodedBytes.coordinates())
		if (bool(erodedBits.at(coord)) != bool(erodedBytes.at(coord) != 0)) erodeMatches = false;
	passfail << "ndl::erode() on PackedBitImage matches ndl::erode() on an equivalent 0/1 Image<uint8_t,2>: " << (erodeMatches ? "Pass" : "Fail") << std::endl;

	PackedBitImage<2> dilatedBits(ext);
	OwnedImage<uint8_t, 2> dilatedBytes(ext);
	ndl::dilate(srcBits, dilatedBits, cross3, BorderMode::Clamp);
	ndl::dilate(srcBytes, dilatedBytes, cross3, BorderMode::Clamp);
	bool dilateMatches = true;
	for (const auto& coord : dilatedBytes.coordinates())
		if (bool(dilatedBits.at(coord)) != bool(dilatedBytes.at(coord) != 0)) dilateMatches = false;
	passfail << "ndl::dilate() on PackedBitImage matches ndl::dilate() on an equivalent 0/1 Image<uint8_t,2>: " << (dilateMatches ? "Pass" : "Fail") << std::endl;

	// median_filter with a box kernel is exactly a majority vote over the
	// neighborhood for bool data -- a simple deterministic pseudo-random
	// fill exercises that against the same cross-check.
	PackedBitImage<2> noisyBits(ext);
	OwnedImage<uint8_t, 2> noisyBytes(ext);
	unsigned seed = 12345;
	for (const auto& coord : noisyBytes.coordinates())
	{
		seed = seed * 1103515245u + 12345u;
		bool v = (seed >> 16) & 1;
		noisyBits.at(coord) = v;
		noisyBytes.at(coord) = v ? 1 : 0;
	}
	PackedBitImage<2> medianBits(ext);
	OwnedImage<uint8_t, 2> medianBytes(ext);
	ndl::median_filter(noisyBits, medianBits, box3, BorderMode::Clamp);
	ndl::median_filter(noisyBytes, medianBytes, box3, BorderMode::Clamp);
	bool medianMatches = true;
	for (const auto& coord : medianBytes.coordinates())
		if (bool(medianBits.at(coord)) != bool(medianBytes.at(coord) != 0)) medianMatches = false;
	passfail << "ndl::median_filter() (majority vote) on PackedBitImage matches the Image<uint8_t,2> equivalent: " << (medianMatches ? "Pass" : "Fail") << std::endl;

	// threshold() writing straight into a PackedBitImage mask.
	OwnedImage<uint8_t, 2> grey(ext);
	{ int i = 0; for (auto it = grey.begin(); it != grey.end(); ++it) *it = (uint8_t)((i++ * 5) % 256); }
	PackedBitImage<2> thresholdedBits(ext);
	OwnedImage<uint8_t, 2> thresholdedBytes(ext);
	ndl::threshold(grey, thresholdedBits, (uint8_t)128);
	ndl::threshold(grey, thresholdedBytes, (uint8_t)128, (uint8_t)1, (uint8_t)0);
	bool threshMatches = true;
	for (const auto& coord : thresholdedBytes.coordinates())
		if (bool(thresholdedBits.at(coord)) != bool(thresholdedBytes.at(coord) != 0)) threshMatches = false;
	passfail << "ndl::threshold() writing directly into a PackedBitImage matches thresholding into an Image<uint8_t,2>: " << (threshMatches ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

