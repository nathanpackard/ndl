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

TEST(ImageIO, ImagePngRoundtrip) {
	std::stringstream passfail;
	std::string outputFolder = NDL_TEST_OUTPUT_DIR;
	std::filesystem::create_directories(outputFolder);

	std::cout << std::endl << "IMAGE PNG ROUND TRIP" << std::endl;

	const int W = 6, H = 5;
	std::vector<uint8_t> rgba(W * H * 4);
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
			for (int c = 0; c < 4; c++)
				rgba[(y * W + x) * 4 + c] = (uint8_t)((x * 37 + y * 53 + c * 19) % 251);

	std::array<int, 3> extent{ 4, W, H };
	Image<uint8_t, 3> img(rgba.data(), extent);

	std::string path = outputFolder + "/ndl_png_roundtrip_test.png";
	image_io::save(img, path);

	std::array<int, 3> loadedExtent;
	std::vector<uint8_t> loaded = image_io::load(path, loadedExtent);

	passfail << "png save/load round trip preserves extent: " << (loadedExtent == extent ? "Pass" : "Fail") << std::endl;
	bool pixelsMatch = loaded.size() == rgba.size() && std::equal(loaded.begin(), loaded.end(), rgba.begin());
	passfail << "png save/load round trip preserves pixel data exactly: " << (pixelsMatch ? "Pass" : "Fail") << std::endl;

	// Also exercise the RGB (3-channel) and greyscale (1-channel) encode
	// paths, since the decoder/encoder both branch on channel count
	// internally and always expand back out to RGBA on load.
	std::vector<uint8_t> rgbData(W * H * 3);
	for (size_t i = 0; i < rgbData.size(); i++) rgbData[i] = (uint8_t)(i * 7 % 251);
	std::array<int, 3> rgbExtent{ 3, W, H };
	Image<uint8_t, 3> rgbImg(rgbData.data(), rgbExtent);
	image_io::save(rgbImg, outputFolder + "/ndl_png_roundtrip_rgb_test.png");
	std::array<int, 3> rgbLoadedExtent;
	std::vector<uint8_t> rgbLoaded = image_io::load(outputFolder + "/ndl_png_roundtrip_rgb_test.png", rgbLoadedExtent);
	bool rgbOk = rgbLoadedExtent[0] == 4 && rgbLoadedExtent[1] == W && rgbLoadedExtent[2] == H;
	for (int y = 0; y < H && rgbOk; y++)
		for (int x = 0; x < W && rgbOk; x++)
			for (int c = 0; c < 3 && rgbOk; c++)
				if (rgbLoaded[(y * W + x) * 4 + c] != rgbData[(y * W + x) * 3 + c]) rgbOk = false;
	passfail << "png round trip preserves RGB color data (3-channel save, RGBA load): " << (rgbOk ? "Pass" : "Fail") << std::endl;

	std::vector<uint8_t> greyData(W * H);
	for (size_t i = 0; i < greyData.size(); i++) greyData[i] = (uint8_t)(i * 11 % 251);
	std::array<int, 3> greyExtent{ 1, W, H };
	Image<uint8_t, 3> greyImg(greyData.data(), greyExtent);
	image_io::save(greyImg, outputFolder + "/ndl_png_roundtrip_grey_test.png");
	std::array<int, 3> greyLoadedExtent;
	std::vector<uint8_t> greyLoaded = image_io::load(outputFolder + "/ndl_png_roundtrip_grey_test.png", greyLoadedExtent);
	bool greyOk = greyLoadedExtent[0] == 4 && greyLoadedExtent[1] == W && greyLoadedExtent[2] == H;
	for (int y = 0; y < H && greyOk; y++)
		for (int x = 0; x < W && greyOk; x++)
		{
			uint8_t v = greyData[y * W + x];
			size_t o = (y * W + x) * 4;
			// Greyscale PNG expands to R=G=B=v, A=255 on decode.
			if (greyLoaded[o] != v || greyLoaded[o + 1] != v || greyLoaded[o + 2] != v || greyLoaded[o + 3] != 255) greyOk = false;
		}
	passfail << "png round trip preserves greyscale data (1-channel save, RGBA load): " << (greyOk ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(ImageIO, TestImages) {
	std::stringstream passfail;
	std::string inputFolder = NDL_TEST_DATA_DIR;
	std::string outputFolder = NDL_TEST_OUTPUT_DIR;
	std::filesystem::create_directories(outputFolder);

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
	reportPassFail(passfail);
}

