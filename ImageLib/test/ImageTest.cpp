#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdint>
#include "../image/Image.h";
#include "../io/imageIO.h";
#include "../common/timer.h";
#include "../image/mathHelpers.h"

using namespace ImageLib;
using namespace timeUtils;

void testImageLibraryDimensions()
{
	int size = 4;
	int i;
	std::cout << std::endl << "1D Image" << std::endl;
	Image<unsigned short, 1> image1D(std::array<int, 1>{ size });
	i = 0;
	for (auto it = image1D.begin(); it != image1D.end(); ++it) *it = ++i;
	std::cout << image1D << std::endl;

	std::cout << std::endl << "2D Image" << std::endl;
	Image<unsigned short, 3> image2D(std::array<int, 3>{ size, size, 1 });
	i = 0;
	for (auto it = image2D.begin(); it != image2D.end(); ++it) *it = ++i;
	std::cout << image2D << std::endl;

	std::cout << std::endl << "3D Image" << std::endl;
	Image<unsigned short, 3> image3D(std::array<int, 3>{ size, size, size });
	i = 0;
	for (auto it = image3D.begin(); it != image3D.end(); ++it) *it = ++i;
	std::cout << image3D << std::endl << image3D.CurrentState() << std::endl;

	std::cout << std::endl << "MirrorX" << std::endl;
	Image<unsigned short, 3> image3DmirrorX(image3D, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ true, false, false });
	std::cout << image3DmirrorX << std::endl;
	std::cout << image3DmirrorX.CurrentState() << std::endl;

	std::cout << std::endl;
	std::cout << "MirrorX" << std::endl;
	Image<unsigned short, 3> image3DmirrorX2(image3DmirrorX, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ true, false, false });
	std::cout << image3DmirrorX2 << std::endl;
	std::cout << image3DmirrorX2.CurrentState() << std::endl;

	std::cout << std::endl;
	std::cout << "MirrorXRoi" << std::endl;
	Image<unsigned short, 3> image3DmirrorX3(image3DmirrorX2, std::array<int, 3>{ 1, 1, 1 }, std::array<int, 3>{ size - 2, size - 2, size - 2 }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ true, false, false });
	std::cout << image3DmirrorX3 << std::endl;
	std::cout << image3DmirrorX3.CurrentState() << std::endl;

	std::cout << std::endl;
	std::cout << "MirrorY" << std::endl;
	Image<unsigned short, 3> image3DmirrorY(image3D, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, true, false });
	std::cout << image3DmirrorY << std::endl;
	std::cout << image3DmirrorY.CurrentState() << std::endl;

	std::cout << std::endl;
	std::cout << "MirrorY" << std::endl;
	Image<unsigned short, 3> image3DmirrorY2(image3DmirrorY, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, true, false });
	std::cout << image3DmirrorY2 << std::endl;
	std::cout << image3DmirrorY2.CurrentState() << std::endl;

	std::cout << std::endl;
	std::cout << "MirrorYRoi" << std::endl;
	Image<unsigned short, 3> image3DmirrorY3(image3DmirrorY2, std::array<int, 3>{ 1, 1, 1 }, std::array<int, 3>{ size - 2, size - 2, size - 2 }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, true, false });
	std::cout << image3DmirrorY3 << std::endl;
	std::cout << image3DmirrorY3.CurrentState() << std::endl;

	std::cout << std::endl;
	std::cout << "MirrorZ" << std::endl;
	Image<unsigned short, 3> image3DmirrorZ(image3D, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, false, true });
	std::cout << image3DmirrorZ << std::endl;
	std::cout << image3DmirrorZ.CurrentState() << std::endl;

	std::cout << std::endl;
	std::cout << "MirrorZ" << std::endl;
	Image<unsigned short, 3> image3DmirrorZ2(image3DmirrorZ, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, false, true });
	std::cout << image3DmirrorZ2 << std::endl;
	std::cout << image3DmirrorZ2.CurrentState() << std::endl;

	std::cout << std::endl;
	std::cout << "MirrorZRoi" << std::endl;
	Image<unsigned short, 3> image3DmirrorZ3(image3DmirrorZ2, std::array<int, 3>{ 1, 1, 1 }, std::array<int, 3>{ size - 2, size - 2, size - 2 }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, false, true });
	std::cout << image3DmirrorZ3 << std::endl;
	std::cout << image3DmirrorZ3.CurrentState() << std::endl;

	 std::cout << std::endl;
	std::cout << "MirrorXYZ" << std::endl;
	Image<unsigned short, 3> image3DmirrorXYZ(image3D, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ true, true, true });
	std::cout << image3DmirrorXYZ << std::endl;
	std::cout << image3DmirrorXYZ.CurrentState() << std::endl;

	 std::cout << std::endl;
	std::cout << "SwapXY" << std::endl;
	Image<unsigned short, 3> image3DswapXY(image3D, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, false, false }, 0, 1);
	std::cout << image3DswapXY << std::endl;
	std::cout << image3DswapXY.CurrentState() << std::endl;

	 std::cout << std::endl;
	std::cout << "SwapXY" << std::endl;
	Image<unsigned short, 3> image3DswapXY2(image3DswapXY, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, false, false }, 0, 1);
	std::cout << image3DswapXY2 << std::endl;
	std::cout << image3DswapXY2.CurrentState() << std::endl;

	 std::cout << std::endl;
	std::cout << "SwapYZ" << std::endl;
	Image<unsigned short, 3> image3DswapYZ(image3D, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, false, false }, 1, 2);
	std::cout << image3DswapYZ << std::endl;
	std::cout << image3DswapYZ.CurrentState() << std::endl;

	std::cout << std::endl;
	std::cout << "SwapYZ" << std::endl;
	Image<unsigned short, 3> image3DswapYZ2(image3DswapYZ, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, false, false }, 1, 2);
	std::cout << image3DswapYZ2 << std::endl;
	std::cout << image3DswapYZ2.CurrentState() << std::endl;

	std::cout << std::endl;
	std::cout << "SwapZX" << std::endl;
	Image<unsigned short, 3> image3DswapZX(image3D, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, false, false }, 0, 2);
	std::cout << image3DswapZX << std::endl;
	std::cout << image3DswapZX.CurrentState() << std::endl;

	std::cout << std::endl;
	std::cout << "SwapZX" << std::endl;
	Image<unsigned short, 3> image3DswapZX2(image3DswapZX, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, false, false }, 0, 2);
	std::cout << image3DswapZX2 << std::endl;
	std::cout << image3DswapZX2.CurrentState() << std::endl;

	Image<unsigned short, 3> image(std::array<int, 3>{ 8, 8, 1 });

	std::cout << std::endl;
	std::cout << "roi1" << std::endl;
	Image<unsigned short, 3> roi1(image, std::array<int, 3>{ 2, 2, 0 }, std::array<int, 3>{ image.Extent[0] - 4, image.Extent[1] - 4, 1 }, std::array<int, 3>{ 2, 2, 1 }, std::array<bool, 3>{ false, false, false });

	for (auto it = roi1.begin(); it != roi1.end(); ++it) *it = (unsigned short)(10 * it.I[0] / 2 + 1 + (it.I[1] + 1));
	std::cout << roi1 << std::endl;
	std::cout << roi1.CurrentState() << std::endl;

	std::cout << std::endl;
	std::cout << "image" << std::endl;
	std::cout << image << std::endl;
}
void testImageLibraryAccuracy()
{
	std::cout << std::endl;
	std::cout << "image" << std::endl;
	Image<double, 3> image(std::array<int, 3>{ 12, 12, 2 });
	std::cout << image << std::endl;

	std::cout << std::endl;
	std::cout << "roi1" << std::endl;
	Image<double, 3> roi1(image, std::array<int, 3>{ 2, 2, 0 }, std::array<int, 3>{ image.Extent[0] - 4, image.Extent[1] - 4, 1 }, std::array<int, 3>{ 2, 2, 1 }, std::array<bool, 3>{ false,false,false });
	for (auto it = roi1.begin(); it != roi1.end(); ++it) *it = it.I[0] / 2 + 1 + (it.I[1] + 1) / 10.0;
	std::cout << roi1 << std::endl;

	std::cout << std::endl;
	std::cout << "roi2" << std::endl;
	Image<double, 3> roi2(image, std::array<int, 3>{ 3, 1, 0 }, std::array<int, 3>{ image.Extent[0] - 4, image.Extent[1] - 2, 1 }, std::array<int, 3>{ 2, 1, 1 }, std::array<bool, 3>{ false, false, false });
	for (auto it = roi2.begin(); it != roi2.end(); ++it) *it = 1;
	std::cout << roi2 << std::endl;

	std::cout << std::endl;
	std::cout << "image after roi updates" << std::endl;
	std::cout << image << std::endl;

	std::cout << std::endl;
	Image<double, 3> result(roi1);
	double basetime = codeTimer("roi1 convolution", [&]() -> void
	{
		auto resultit = result.begin();
		for (auto it = roi1.begin(); it != roi1.end(); ++it)
		{
			*resultit = (it[roi1.StepSize[0]] + it[-roi1.StepSize[0]] + it[roi1.StepSize[1]] + it[-roi1.StepSize[1]]) * 0.25;
			++resultit;
		}
	});
	std::cout << result << std::endl;

	std::cout << std::endl;
	std::cout << "ROI2" << std::endl;
	Image<double, 3> result2(roi2);

	auto result2it = result2.begin();
	for (auto roi2it = roi2.begin(); roi2it != roi2.end(); ++roi2it)
	{
		*result2it = (roi2it[roi2.StepSize[0]] + roi2it[-roi2.StepSize[0]] + roi2it[roi2.StepSize[1]] + roi2it[-roi2.StepSize[1]]) / 4;
		++result2it;
	}
	std::cout << result2 << std::endl;

	std::cout << std::endl;
	int zeros = 0;
	for (const auto& value : roi1)  if (value == 0) zeros++;
	std::cout << "roi1 has " << zeros << " zeros" << std::endl;
	zeros = 0;
	for (const auto& value : image) if (value == 0) zeros++;
	std::cout << "image has " << zeros << " zeros" << std::endl;
	std::cout << std::endl;
}
void ImageLibrarySpeedTest()
{
	int iterations = 100;
	Image<double, 3> image(std::array<int, 3>{ 1000, 1000, 2 });

	Image<double, 3> roi1(image, std::array<int, 3>{ 2, 2, 1 }, std::array<int, 3>{ image.Extent[0] - 4, image.Extent[1] - 4, 1 }, std::array<int, 3>{ 2, 2, 1 }, std::array<bool, 3>{ false, false, false });
	for (auto it = roi1.begin(); it != roi1.end(); ++it) *it = it.I[0] / 2 + 1 + (it.I[1] + 1) / 10.0;

	Image<double, 3> roi2(image, std::array<int, 3>{ 3, 1, 1 }, std::array<int, 3>{ image.Extent[0] - 4, image.Extent[1] - 2, 1 }, std::array<int, 3>{ 2, 1, 1 }, std::array<bool, 3>{ false, false, false });
	for (auto it = roi2.begin(); it != roi2.end(); ++it) *it = 1;

	double basetime;
	{
		Image<double, 3> result(roi1);
		basetime = codeTimer("roi1 convolution index", [&]() -> void
		{
			for (auto it = roi1.begin(), resultit = result.begin(); it != roi1.end(); ++it, ++resultit)
				*resultit = (it[roi1.StepSize[0]] + it[-roi1.StepSize[0]] + it[roi1.StepSize[1]] + it[-roi1.StepSize[1]]) * 0.25;
		}, iterations);
		std::cout << std::endl;
		std::cout << Image<double, 3>(result, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ 4, 4, 1 }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, false, false }) << std::endl;
	}

	{
		Image<double, 3> result(roi1);
		double noAnd = codeTimer("roi1 convolution clamp", [&]() -> void
		{
			for (auto it = roi1.begin(), resultit = result.begin(); it != roi1.end(); ++it, ++resultit)
				*resultit = (it(1, x_clamp) + it(-1, x_clamp) + it(1, y_clamp) + it(-1, y_clamp)) * 0.25;
		}, iterations);
		std::cout << "noAnd is " << (100 * ((basetime / noAnd) - 1)) << "% faster" << std::endl << std::endl;
		std::cout << Image<double, 3>(result, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ 4, 4, 1 }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, false, false }) << std::endl;
	}

	{
		Image<double, 3> result(roi1);
		double noAnd = codeTimer("roi1 convolution wrap", [&]() -> void
		{
			for (auto it = roi1.begin(), resultit = result.begin(); it != roi1.end(); ++it, ++resultit)
				*resultit = (it(1, x_wrap) + it(-1, x_wrap) + it(1, y_wrap) + it(-1, y_wrap)) * 0.25;
		}, iterations);
		std::cout << "noAnd is " << (100 * ((basetime / noAnd) - 1)) << "% faster" << std::endl << std::endl;
		std::cout << Image<double, 3>(result, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ 4, 4, 1 }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, false, false }) << std::endl;
	}
}
void testIntegralImage()
{
	int size = 3;
	std::cout << std::endl << "Original Image" << std::endl;
	Image<unsigned short, 3> image(std::array<int, 3>{ size, size, size });
	int i = 0;
	for (auto it = image.begin(); it != image.end(); ++it) *it = ++i;
	std::cout << image << std::endl;

	std::cout << "Integral Image" << std::endl;
	std::cout << ToIntegralImage<long>(image) << std::endl;
}
void displayBorderTests(Image<unsigned short, 3>& image3D) {
	std::cout << "value: " << image3D(std::array<int, 3>{ 1, 1, 1 }) << std::endl;
	std::cout << "-2 x clamp: " << image3D.begin()(-2, x_clamp) << std::endl;
	std::cout << "-2 x wrap: " << image3D.begin()(-2, x_wrap) << std::endl;
	std::cout << "-2 x reflect: " << image3D.begin()(-2, x_reflect) << std::endl;
	std::cout << "-2 y clamp: " << image3D.begin()(-2, y_clamp) << std::endl;
	std::cout << "-2 y wrap: " << image3D.begin()(-2, y_wrap) << std::endl;
	std::cout << "-2 y reflect: " << image3D.begin()(-2, y_reflect) << std::endl;
	std::cout << "-2 z clamp: " << image3D.begin()(-2, z_clamp) << std::endl;
	std::cout << "-2 z wrap: " << image3D.begin()(-2, z_wrap) << std::endl;
	std::cout << "-2 z reflect: " << image3D.begin()(-2, z_reflect) << std::endl;
}
void testImageLibraryBorders() {
	int size = 4;
	int i;

	std::cout << std::endl << "3D Image" << std::endl;
	Image<unsigned short, 3> image3D(std::array<int, 3>{ size, size, size });
	i = 0;
	for (auto it = image3D.begin(); it != image3D.end(); ++it) *it = ++i;
	std::cout << image3D << std::endl;
	displayBorderTests(image3D);

	std::cout << std::endl << "MirrorX" << std::endl;
	Image<unsigned short, 3> MirrorX(image3D, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ true, false, false });
	std::cout << MirrorX << std::endl;
	displayBorderTests(MirrorX);

	std::cout << std::endl << "MirrorY" << std::endl;
	Image<unsigned short, 3> MirrorY(image3D, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, true, false });
	std::cout << MirrorY << std::endl;
	displayBorderTests(MirrorY);

	std::cout << std::endl << "MirrorZ" << std::endl;
	Image<unsigned short, 3> MirrorZ(image3D, std::array<int, 3>{ 0, 0, 0 }, std::array<int, 3>{ size, size, size }, std::array<int, 3>{ 1, 1, 1 }, std::array<bool, 3>{ false, false, true });
	std::cout << MirrorZ << std::endl;
	displayBorderTests(MirrorZ);
}
void TestImages(std::string inputFolder, std::string outputFolder)
{
	Image<uint8_t, 2> image = ImageIO::LoadJpeg(inputFolder + "\\ng_bwgirl_crop.jpg");
	Image<uint8_t, 2> red(image, std::array<int, 2>{ 0, 0 }, std::array<int, 2>{ image.Extent[0], image.Extent[1] }, std::array<int, 2>{ 3, 1 }, std::array<bool, 2>{ false, false });
	Image<uint8_t, 2> green(image, std::array<int, 2>{ 1, 0 }, std::array<int, 2>{ image.Extent[0], image.Extent[1] }, std::array<int, 2>{ 3, 1 }, std::array<bool, 2>{ false, false });
	Image<uint8_t, 2> blue(image, std::array<int, 2>{ 2, 0 }, std::array<int, 2>{ image.Extent[0], image.Extent[1] }, std::array<int, 2>{ 3, 1 }, std::array<bool, 2>{ false, false });
	ImageIO::SaveRaw(red, outputFolder + std::string("\\red_") + std::to_string(red.Extent[0]) + "x" + std::to_string(red.Extent[1]) + ".raw");
	ImageIO::SaveRaw(green, outputFolder + std::string("\\green_") + std::to_string(green.Extent[0]) + "x" + std::to_string(green.Extent[1]) + ".raw");
	ImageIO::SaveRaw(blue, outputFolder + std::string("\\blue_") + std::to_string(blue.Extent[0]) + "x" + std::to_string(blue.Extent[1]) + ".raw");
	//ImageIO::Save(image, outputFolder + "\\ng_bwgirl_crop.jpg");
	Image<float, 2> fimage(image);
	//fimage.GaussianBlur2D(3);
	ImageIO::SaveRaw(fimage, outputFolder + std::string("\\float_") + std::to_string(fimage.Extent[0]) + "x" + std::to_string(fimage.Extent[1]) + ".raw");
	Image<uint8_t, 2> bmpImage = ImageIO::LoadBmp(inputFolder + "\\marbles.bmp");
	Image<uint8_t, 2> red2(bmpImage, std::array<int, 2>{ 0, 0 }, std::array<int, 2>{ bmpImage.Extent[0], bmpImage.Extent[1] }, std::array<int, 2>{ 3, 1 }, std::array<bool, 2>{ false, false });
	Image<uint8_t, 2> green2(bmpImage, std::array<int, 2>{ 1, 0 }, std::array<int, 2>{ bmpImage.Extent[0], bmpImage.Extent[1] }, std::array<int, 2>{ 3, 1 }, std::array<bool, 2>{ false, false });
	Image<uint8_t, 2> blue2(bmpImage, std::array<int, 2>{ 2, 0 }, std::array<int, 2>{ bmpImage.Extent[0], bmpImage.Extent[1] }, std::array<int, 2>{ 3, 1 }, std::array<bool, 2>{ false, false });
	ImageIO::SaveRaw(red2, outputFolder + std::string("\\red2_") + std::to_string(red2.Extent[0]) + "x" + std::to_string(red2.Extent[1]) + ".raw");
	ImageIO::SaveRaw(green2, outputFolder + std::string("\\green2_") + std::to_string(green2.Extent[0]) + "x" + std::to_string(green2.Extent[1]) + ".raw");
	ImageIO::SaveRaw(blue2, outputFolder + std::string("\\blue2_") + std::to_string(blue2.Extent[0]) + "x" + std::to_string(blue2.Extent[1]) + ".raw");

	//auto dcmImage = ImageIO::LoadDicom<uint16_t>(inputFolder + "\\foot.dcm");
	//ImageIO::SaveRaw(dcmImage, outputFolder + string("\\mri_") + std::to_string(dcmImage.Extent[0]) + "x" + std::to_string(dcmImage.Extent[1]) + ".raw");

	auto dcmImage2 = ImageIO::LoadDicom<int16_t>(inputFolder + "\\foot.dcm");
	ImageIO::SaveRaw(dcmImage2, outputFolder + std::string("\\CT_") + std::to_string(dcmImage2.Extent[0]) + "x" + std::to_string(dcmImage2.Extent[1]) + ".raw");
}
int main()
{
	testImageLibraryDimensions();
	testImageLibraryAccuracy();
	ImageLibrarySpeedTest();
	testIntegralImage();
	testImageLibraryBorders();
	TestImages("C:\\Users\\natha\\Documents\\ndl\\testData", "C:\\Users\\natha\\Desktop");
	return 0;
}