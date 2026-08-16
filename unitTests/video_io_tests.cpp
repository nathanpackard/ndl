#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/imageIO/video_io.h>

#include "testHelpers.h"

using namespace ndl;

namespace
{
	// save_video()/load_video() both shell out to (or, for save_video(),
	// vendor a codec that stands in for) real external tools -- ffmpeg
	// specifically for load_video(), see video_io.h's own top comment for
	// why. Skipping (not failing) when it's missing matches that same
	// design: a machine without ffmpeg installed can't be expected to pass
	// a test that exercises it, any more than a machine without a display
	// could pass a test requiring one.
	bool ffmpegAvailable()
	{
		return std::system("command -v ffmpeg > /dev/null 2>&1") == 0
			&& std::system("command -v ffprobe > /dev/null 2>&1") == 0;
	}
}

TEST(VideoIO, SaveLoadRoundTrip) {
	if (!ffmpegAvailable()) GTEST_SKIP() << "ffmpeg/ffprobe not found on PATH";

	std::stringstream passfail;
	std::string outputFolder = NDL_TEST_OUTPUT_DIR;
	std::filesystem::create_directories(outputFolder);

	std::cout << std::endl << "VIDEO SAVE/LOAD ROUND TRIP" << std::endl;

	// 32x32 (a multiple of 16, save_video()'s own requirement), 6 frames --
	// a solid-color frame per frame index, cycling through 3 distinct
	// colors, so a round trip that silently scrambled frame order or
	// channel order would show up as a color/frame mismatch, not just a
	// dimension mismatch.
	const int W = 32, H = 32, F = 6;
	OwnedImage<uint8_t, 4> frames({ 3, W, H, F });
	uint8_t palette[3][3] = { { 220, 30, 30 }, { 30, 220, 30 }, { 30, 30, 220 } };
	for (const auto& c : frames.coordinates())
		frames.at(c) = palette[c[3] % 3][c[0]];

	std::string path = outputFolder + "/ndl_video_roundtrip_test.mp4";
	image_io::save_video(frames, path, /*fps*/ 10, /*qp*/ 18); // low qp (high quality) -- solid colors should survive compression cleanly

	std::array<int, 4> loadedExtent;
	std::vector<uint8_t> loaded = image_io::load_video(path, loadedExtent);

	bool extentOk = loadedExtent[0] == 3 && loadedExtent[1] == W && loadedExtent[2] == H && loadedExtent[3] == F;
	passfail << "video save/load round trip preserves extent {3," << W << "," << H << "," << F << "}: " << (extentOk ? "Pass" : "Fail") << std::endl;

	// H.264 is lossy even at a low QP, so check colors are CLOSE, not exact
	// -- center pixel of the middle frame of the clip, one per palette
	// entry, tolerance generous enough to absorb ordinary compression
	// noise but tight enough that a wrong frame/channel would still fail.
	bool colorsOk = true;
	if (extentOk)
	{
		Image<uint8_t, 4> loadedView(loaded.data(), loadedExtent);
		for (int f = 0; f < F; f++)
			for (int c = 0; c < 3; c++)
			{
				int got = loadedView.at({ c, W / 2, H / 2, f });
				int expected = palette[f % 3][c];
				if (std::abs(got - expected) > 25) colorsOk = false;
			}
	}
	passfail << "video save/load round trip preserves per-frame color (within lossy-compression tolerance): " << (colorsOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(VideoIO, LoadVideoPreservesAspectRatioWhenOneTargetDimensionOmitted) {
	if (!ffmpegAvailable()) GTEST_SKIP() << "ffmpeg/ffprobe not found on PATH";

	std::stringstream passfail;
	std::string outputFolder = NDL_TEST_OUTPUT_DIR;
	std::filesystem::create_directories(outputFolder);

	std::cout << std::endl << "VIDEO LOAD -- ASPECT-PRESERVING TARGET SIZE" << std::endl;

	// A deliberately non-square 3:2 source (48x32, both still multiples of
	// 16 for save_video()'s own sake) -- if load_video() ever silently
	// treated a caller-omitted target dimension as "same as the other" or
	// "native" instead of actually computing it from the source's aspect
	// ratio, testing against a square source wouldn't catch that.
	const int W = 48, H = 32, F = 2;
	OwnedImage<uint8_t, 4> frames({ 3, W, H, F });
	for (const auto& c : frames.coordinates()) frames.at(c) = 128;
	std::string path = outputFolder + "/ndl_video_aspect_test.mp4";
	image_io::save_video(frames, path, /*fps*/ 10, /*qp*/ 28);

	// Only targetWidth given (24, half of the source's own 48): the
	// source's own 3:2 aspect ratio means the expected computed height is
	// 24 * 32 / 48 == 16.
	std::array<int, 4> extentFromWidth;
	image_io::load_video(path, extentFromWidth, /*targetWidth*/ 24, /*targetHeight*/ 0);
	bool widthCaseOk = extentFromWidth[1] == 24 && extentFromWidth[2] == 16;
	passfail << "load_video(targetWidth=24, targetHeight=0) computes height=16 from the source's own 3:2 aspect ratio: " << (widthCaseOk ? "Pass" : "Fail") << " (got " << extentFromWidth[1] << "x" << extentFromWidth[2] << ")" << std::endl;

	// Symmetric case: only targetHeight given (16) should likewise compute
	// width=24 from the same aspect ratio.
	std::array<int, 4> extentFromHeight;
	image_io::load_video(path, extentFromHeight, /*targetWidth*/ 0, /*targetHeight*/ 16);
	bool heightCaseOk = extentFromHeight[1] == 24 && extentFromHeight[2] == 16;
	passfail << "load_video(targetWidth=0, targetHeight=16) computes width=24 from the source's own 3:2 aspect ratio: " << (heightCaseOk ? "Pass" : "Fail") << " (got " << extentFromHeight[1] << "x" << extentFromHeight[2] << ")" << std::endl;

	reportPassFail(passfail);
}

TEST(VideoIO, SaveVideoRejectsNonMultipleOf16) {
	std::stringstream passfail;
	std::cout << std::endl << "VIDEO SAVE -- WIDTH/HEIGHT MUST BE A MULTIPLE OF 16" << std::endl;

	// save_video() itself doesn't need ffmpeg (it's fully native, see
	// video_io.h's own top comment) -- this checks its own argument
	// validation, not anything ffmpeg-dependent, so it always runs.
	OwnedImage<uint8_t, 4> frames({ 3, 20, 20, 1 }); // 20 is not a multiple of 16
	bool threw = false;
	try { image_io::save_video(frames, "/dev/null", 10, 28); }
	catch (const std::runtime_error&) { threw = true; }
	passfail << "save_video() throws for width/height that isn't a multiple of 16: " << (threw ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}
