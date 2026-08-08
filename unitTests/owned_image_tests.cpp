#include <gtest/gtest.h>
#include <vector>
#include <type_traits>
#include <sstream>
#include <iostream>

#include <ndl/image.h>

#include "testHelpers.h"

using namespace ndl;

TEST(OwnedImage, OwnedImage) {
	std::stringstream passfail;

	std::cout << std::endl << "OWNED IMAGE" << std::endl;

	OwnedImage<int, 2> a({ 4, 3 });
	int i = 0;
	for (auto it = a.begin(); it != a.end(); ++it) *it = ++i; // 1..12
	passfail << "OwnedImage construction from extent has the right shape: " << (a.extent()[0] == 4 && a.extent()[1] == 3 ? "Pass" : "Fail") << std::endl;
	passfail << "OwnedImage supports element access via the inherited operator(): " << (a(2, 1) == 7 ? "Pass" : "Fail") << std::endl;
	passfail << "OwnedImage supports inherited reductions (sum() of 1..12): " << (a.sum() == 78 ? "Pass" : "Fail") << std::endl;

	OwnedImage<double, 2> box({ 3, 3 });
	make_box_kernel(box);
	OwnedImage<int, 2> eroded({ 4, 3 });
	a.erode(box, eroded, BorderMode::Clamp);
	// 3x3 neighborhood around (2,1) is x=1..3,y=0..2 -> {2,3,4,6,7,8,10,11,12}, min=2
	passfail << "OwnedImage works directly as both source and output of an inherited method (erode()): " << (eroded(2, 1) == 2 ? "Pass" : "Fail") << std::endl;

	auto likeA = OwnedImage<int, 2>::like(a);
	passfail << "OwnedImage::like() produces a buffer with the source's extent: " << (likeA.extent() == a.extent() ? "Pass" : "Fail") << std::endl;

	OwnedImage<double, 2> converted(a);
	bool convOk = true;
	for (const auto& coord : a.coordinates()) if (converted.at(coord) != (double)a.at(coord)) convOk = false;
	passfail << "OwnedImage's converting constructor deep-copies and converts element type: " << (convOk ? "Pass" : "Fail") << std::endl;

	int sumBeforeMove = a.sum();
	OwnedImage<int, 2> moved = std::move(a);
	passfail << "OwnedImage move construction preserves data: " << (moved.sum() == sumBeforeMove && moved(2, 1) == 7 ? "Pass" : "Fail") << std::endl;

	passfail << "OwnedImage is not copy-constructible: " << (!std::is_copy_constructible_v<OwnedImage<int, 2>> ? "Pass" : "Fail") << std::endl;
	passfail << "OwnedImage is move-constructible: " << (std::is_move_constructible_v<OwnedImage<int, 2>> ? "Pass" : "Fail") << std::endl;

	OwnedImage<double, 2> sobelX({ 3, 3 }, { -1,0,1, -2,0,2, -1,0,1 });
	bool initListOk = sobelX(0, 0) == -1 && sobelX(1, 0) == 0 && sobelX(2, 0) == 1 &&
		sobelX(0, 1) == -2 && sobelX(1, 1) == 0 && sobelX(2, 1) == 2 &&
		sobelX(0, 2) == -1 && sobelX(1, 2) == 0 && sobelX(2, 2) == 1;
	passfail << "OwnedImage's initializer-list constructor fills values in the expected order: " << (initListOk ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(OwnedImage, PerChannel) {
	std::stringstream passfail;

	std::cout << std::endl << "PER_CHANNEL" << std::endl;

	// 3 channels, 4x3 each, channel c filled with c*100 + 1..12 -- distinct
	// enough per channel that any cross-channel bleeding would show up
	// immediately in the result.
	OwnedImage<int, 3> img({ 3, 4, 3 });
	for (int c = 0; c < 3; c++)
	{
		Image<int, 2> channel = img.slice(0, c);
		int i = 0;
		for (auto it = channel.begin(); it != channel.end(); ++it) *it = c * 100 + (++i);
	}

	std::vector<double> boxData(9, 1.0);
	Image<double, 2> box(boxData.data(), { 3, 3 });
	OwnedImage<int, 3> eroded = OwnedImage<int, 3>::like(img);
	per_channel(img, eroded, 0, [&](const auto& s, auto& d) { s.erode(box, d, BorderMode::Clamp); });

	// Same hand-derived expectation as testOwnedImage's erode() check (min of
	// the 3x3 neighborhood around (2,1) is the block's own +2), just offset
	// by c*100 per channel -- correct only if per_channel() kept each
	// channel's erode() working against its own data alone.
	bool ok = true;
	for (int c = 0; c < 3; c++)
	{
		Image<int, 2> erodedChannel = eroded.slice(0, c);
		if (erodedChannel(2, 1) != c * 100 + 2) ok = false;
	}
	passfail << "per_channel() processes each channel independently without cross-channel bleeding: " << (ok ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

