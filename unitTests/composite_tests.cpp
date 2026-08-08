#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/imageIO.h>

#include "testHelpers.h"

using namespace ndl;

TEST(Composite, ViewOfView) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(Composite, SwapThenView) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(Composite, MirrorThenSlice) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(Composite, WriteThroughView) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

TEST(Composite, ConvolveThenReduce) {
	std::stringstream passfail;

	std::cout << std::endl << "COMPOSITE: CONVOLVE THEN REDUCE" << std::endl;

	std::vector<int> data(20);
	Image<int, 2> img(data.data(), { 5, 4 });
	int i = 0;
	for (auto it = img.begin(); it != img.end(); ++it) *it = ++i;

	std::vector<int> kernelData = { 1,2,1, 2,4,2, 1,2,1 };
	Image<int, 2> kernel(kernelData.data(), { 3, 3 });

	std::vector<int> outData(20);
	Image<int, 2> out(outData.data(), { 5, 4 });
	img.convolve(kernel, out, BorderMode::Wrap);

	// With wrap (circular) border handling, a fixed circular shift is a
	// bijection over a periodic image, so every image pixel contributes to
	// exactly kernel.size() output positions with the same total weight
	// (kernel.sum()) no matter where it sits -- the sum of the whole
	// convolved output is therefore always exactly kernel.sum() * image.sum(),
	// independent of image content. This checks convolve() and sum() against
	// each other rather than against a hand-computed constant.
	int expected = kernel.sum() * img.sum();
	passfail << "convolve() with wrap border + sum() satisfies kernel.sum()*image.sum() identity: "
		<< (out.sum() == expected ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(Composite, MorphologyOpening) {
	std::stringstream passfail;

	std::cout << std::endl << "COMPOSITE: OPENING (ERODE THEN DILATE)" << std::endl;

	std::vector<int> data(100, 0); // 10x10, all black
	Image<int, 2> img(data.data(), { 10, 10 });
	img(5, 5) = 100;                                                    // an isolated single-pixel speck
	for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) img(x, y) = 100; // a solid 4x4 block

	std::vector<double> boxData(9);
	Image<double, 2> box(boxData.data(), { 3, 3 });
	make_box_kernel(box);

	std::vector<int> erodedData(100), openedData(100);
	Image<int, 2> eroded(erodedData.data(), { 10, 10 }), opened(openedData.data(), { 10, 10 });
	img.erode(box, eroded);
	eroded.dilate(box, opened);

	// No isolated pixel's 3x3 neighborhood is ever entirely bright, so erode()
	// wipes it out completely -- dilating an all-zero region afterward can't
	// bring it back. The solid block, on the other hand, has an interior
	// (1,1)-(2,2) whose full neighborhood survives erosion, and dilating that
	// remnant regrows it back to (close to) the original 4x4 footprint --
	// composing two of the new morphology primitives to reproduce the
	// standard "opening" operation, with no new library code.
	passfail << "opening removes the isolated single-pixel speck: " << (opened(5, 5) == 0 ? "Pass" : "Fail") << std::endl;
	passfail << "opening preserves the interior of the larger solid block: " << (opened(1, 1) == 100 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(Composite, PngWithView) {
	std::stringstream passfail;
	std::string outputFolder = NDL_TEST_OUTPUT_DIR;
	std::filesystem::create_directories(outputFolder);

	std::cout << std::endl << "COMPOSITE: PNG ROUND TRIP THROUGH A VIEW" << std::endl;

	const int W = 8, H = 6;
	std::vector<uint8_t> rgba(W * H * 4);
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
			for (int c = 0; c < 4; c++)
				rgba[(y * W + x) * 4 + c] = (uint8_t)((x * 5 + y * 3 + c) % 251);

	std::array<int, 3> extent{ 4, W, H };
	Image<uint8_t, 3> img(rgba.data(), extent);

	// mirror the x axis (dim1 -- dim0 is channel here) before saving, to prove
	// save() correctly flattens a non-contiguous/mirrored view before handing
	// its bytes to the PNG encoder.
	Image<uint8_t, 3> mirrored = img.mirror(1);
	std::string path = outputFolder + "/ndl_png_roundtrip_view_test.png";
	image_io::save(mirrored, path);

	std::array<int, 3> loadedExtent;
	std::vector<uint8_t> loaded = image_io::load(path, loadedExtent);

	bool ok = loadedExtent == extent;
	if (ok)
	{
		Image<uint8_t, 3> loadedImg(loaded.data(), loadedExtent);
		for (int y = 0; y < H && ok; y++)
			for (int x = 0; x < W && ok; x++)
				for (int c = 0; c < 4 && ok; c++)
					if (loadedImg(c, x, y) != img(c, W - 1 - x, y)) ok = false;
	}
	passfail << "png save() of a mirrored view() round trips to the mirrored pixel data: " << (ok ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(Composite, SliceComposition) {
	std::stringstream passfail;

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
	reportPassFail(passfail);
}

