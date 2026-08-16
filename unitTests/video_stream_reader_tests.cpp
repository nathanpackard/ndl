#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <algorithm>
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
	// Same skip-if-missing convention video_io_tests.cpp itself uses (see
	// that file's own comment) -- VideoStreamReader shells out to ffmpeg/
	// ffprobe exactly like load_video() does.
	bool ffmpegAvailable()
	{
		return std::system("command -v ffmpeg > /dev/null 2>&1") == 0
			&& std::system("command -v ffprobe > /dev/null 2>&1") == 0;
	}
}

TEST(VideoStreamReader, MatchesLoadVideoFrameForFrame) {
	if (!ffmpegAvailable()) GTEST_SKIP() << "ffmpeg/ffprobe not found on PATH";

	std::stringstream passfail;
	std::string outputFolder = NDL_TEST_OUTPUT_DIR;
	std::filesystem::create_directories(outputFolder);

	std::cout << std::endl << "VIDEO STREAM READER -- MATCHES load_video()'S OWN FULL DECODE" << std::endl;

	// Same deterministic solid-color-per-frame clip video_io_tests.cpp's
	// own SaveLoadRoundTrip test uses -- a round trip that silently
	// scrambled frame order would show up as a color mismatch, not just a
	// count mismatch.
	const int W = 32, H = 32, F = 6;
	OwnedImage<uint8_t, 4> frames({ 3, W, H, F });
	uint8_t palette[3][3] = { { 220, 30, 30 }, { 30, 220, 30 }, { 30, 30, 220 } };
	for (const auto& c : frames.coordinates())
		frames.at(c) = palette[c[3] % 3][c[0]];

	std::string path = outputFolder + "/ndl_video_stream_reader_test.mp4";
	image_io::save_video(frames, path, /*fps*/ 10, /*qp*/ 18);

	// Reference: load_video()'s own full, already-tested decode.
	std::array<int, 4> refExtent;
	std::vector<uint8_t> refData = image_io::load_video(path, refExtent);
	bool refExtentOk = refExtent[0] == 3 && refExtent[1] == W && refExtent[2] == H && refExtent[3] == F;
	passfail << "reference load_video() decode has the expected extent {3," << W << "," << H << "," << F << "}: " << (refExtentOk ? "Pass" : "Fail") << std::endl;

	// VideoStreamReader: read the SAME file one frame at a time, straight
	// into a plain buffer (standing in for RingBufferImage::
	// nextWriteSlot()'s own memory in real use), and compare byte-for-byte
	// against the corresponding slice of load_video()'s own output.
	image_io::VideoStreamReader reader(path);
	bool resolutionOk = reader.width() == W && reader.height() == H;
	passfail << "VideoStreamReader resolves the same width/height load_video() did: " << (resolutionOk ? "Pass" : "Fail") << std::endl;

	std::vector<uint8_t> frameBuf(reader.frameBytes());
	int framesRead = 0;
	bool bytesMatchThroughout = true;
	while (reader.readFrame(frameBuf.data()))
	{
		size_t frameStride = (size_t)W * H * 3;
		const uint8_t* refFrame = refData.data() + (size_t)framesRead * frameStride;
		if (frameBuf.size() != frameStride || !std::equal(frameBuf.begin(), frameBuf.end(), refFrame))
			bytesMatchThroughout = false;
		framesRead++;
	}

	bool countOk = framesRead == F;
	passfail << "VideoStreamReader::readFrame() returns exactly " << F << " frames before a clean EOF (got " << framesRead << "): " << (countOk ? "Pass" : "Fail") << std::endl;
	passfail << "every incrementally-read frame is byte-identical to load_video()'s own corresponding frame: " << (bytesMatchThroughout ? "Pass" : "Fail") << std::endl;

	bool eofStaysFalse = !reader.readFrame(frameBuf.data());
	passfail << "readFrame() keeps returning false after EOF rather than throwing or reading garbage: " << (eofStaysFalse ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(VideoStreamReader, ResolvesAspectRatioAndSpacingLikeLoadVideo) {
	if (!ffmpegAvailable()) GTEST_SKIP() << "ffmpeg/ffprobe not found on PATH";

	std::stringstream passfail;
	std::string outputFolder = NDL_TEST_OUTPUT_DIR;
	std::filesystem::create_directories(outputFolder);

	std::cout << std::endl << "VIDEO STREAM READER -- ASPECT-PRESERVING TARGET SIZE + spacing()" << std::endl;

	// A non-square 3:2 source (48x32), matching the convention
	// video_io_tests.cpp's own aspect-ratio test uses.
	const int W = 48, H = 32, F = 2;
	OwnedImage<uint8_t, 4> frames({ 3, W, H, F });
	for (const auto& c : frames.coordinates()) frames.at(c) = 128;
	std::string path = outputFolder + "/ndl_video_stream_reader_aspect_test.mp4";
	image_io::save_video(frames, path, /*fps*/ 10, /*qp*/ 28);

	// Only targetWidth given (24): expect height 16 (24 * 32 / 48), same
	// as load_video()'s own already-tested resolution logic (they share
	// the exact same detail_video::resolveScale() implementation).
	image_io::VideoStreamReader reader(path, /*targetWidth*/ 24, /*targetHeight*/ 0, /*targetFps*/ 5.0);
	bool aspectOk = reader.width() == 24 && reader.height() == 16;
	passfail << "VideoStreamReader(targetWidth=24, targetHeight=0) computes height=16 from the source's own 3:2 aspect ratio: " << (aspectOk ? "Pass" : "Fail") << " (got " << reader.width() << "x" << reader.height() << ")" << std::endl;

	bool fpsOk = reader.fps() == 5.0;
	passfail << "fps() reflects the requested targetFps (5.0) when one was given: " << (fpsOk ? "Pass" : "Fail") << std::endl;

	VoxelSpacing<4> spacing = reader.spacing();
	bool spacingOk = spacing.unit[0] == "channels" && spacing.unit[1] == "px" && spacing.unit[2] == "px" && spacing.unit[3] == "s"
		&& spacing.spacing[3] == 1.0 / 5.0;
	passfail << "spacing() matches load_video()'s own channels/px/px/s convention: " << (spacingOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(VideoStreamReader, MoveAssignReopensACleanReader) {
	if (!ffmpegAvailable()) GTEST_SKIP() << "ffmpeg/ffprobe not found on PATH";

	std::stringstream passfail;
	std::string outputFolder = NDL_TEST_OUTPUT_DIR;
	std::filesystem::create_directories(outputFolder);

	std::cout << std::endl << "VIDEO STREAM READER -- move-assignment reopen (looping a clip)" << std::endl;

	const int W = 16, H = 16, F = 3;
	OwnedImage<uint8_t, 4> frames({ 3, W, H, F });
	for (const auto& c : frames.coordinates()) frames.at(c) = 64;
	std::string path = outputFolder + "/ndl_video_stream_reader_reopen_test.mp4";
	image_io::save_video(frames, path, /*fps*/ 10, /*qp*/ 28);

	image_io::VideoStreamReader reader(path);
	std::vector<uint8_t> frameBuf(reader.frameBytes());
	int firstPassCount = 0;
	while (reader.readFrame(frameBuf.data())) firstPassCount++;

	// Simulates a live demo looping a finite source file: move-assign a
	// freshly-opened reader over the exhausted one (closing the old
	// pipe), rather than needing a separate "reset" method.
	reader = image_io::VideoStreamReader(path);
	int secondPassCount = 0;
	while (reader.readFrame(frameBuf.data())) secondPassCount++;

	bool ok = firstPassCount == F && secondPassCount == F;
	passfail << "move-assigning a freshly-opened VideoStreamReader re-decodes from the start (" << firstPassCount << " then " << secondPassCount << " frames, expected " << F << " each): " << (ok ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}
