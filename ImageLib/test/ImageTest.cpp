#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdint>
#include "../image/Image.h";
#include "../io/imageIO.h";
#include "../common/timer.h";
#include "../image/mathHelpers.h"
#include <iostream>

using namespace std;
using namespace ImageLib;
using namespace timeUtils;

void testImageLibraryDimensions()
{
	int size = 4;
	int i;
	cout << endl << "1D Image" << endl;
	Image<unsigned short> image1D(size);
	i = 0;
	for (auto it = image1D.begin(); it != image1D.end(); ++it) *it = ++i;
	cout << image1D << endl;

	cout << endl << "2D Image" << endl;
	Image<unsigned short> image2D(size, size);
	i = 0;
	for (auto it = image2D.begin(); it != image2D.end(); ++it) *it = ++i;
	cout << image2D << endl;

	cout << endl << "3D Image" << endl;
	Image<unsigned short> image3D(size, size, size);
	i = 0;
	for (auto it = image3D.begin(); it != image3D.end(); ++it) *it = ++i;
	cout << image3D << endl << image3D.CurrentState() << endl;

	cout << endl << "MirrorX" << endl;
	Image<unsigned short> image3DmirrorX(image3D, 0, 0, 0, size, size, size, 1, 1, 1, true);
	cout << image3DmirrorX << endl;
	cout << image3DmirrorX.CurrentState() << endl;

	cout << endl;
	cout << "MirrorX" << endl;
	Image<unsigned short> image3DmirrorX2(image3DmirrorX, 0, 0, 0, size, size, size, 1, 1, 1, true);
	cout << image3DmirrorX2 << endl;
	cout << image3DmirrorX2.CurrentState() << endl;

	cout << endl;
	cout << "MirrorXRoi" << endl;
	Image<unsigned short> image3DmirrorX3(image3DmirrorX2, 1, 1, 1, size - 2, size - 2, size - 2, 1, 1, 1, true);
	cout << image3DmirrorX3 << endl;
	cout << image3DmirrorX3.CurrentState() << endl;

	cout << endl;
	cout << "MirrorY" << endl;
	Image<unsigned short> image3DmirrorY(image3D, 0, 0, 0, size, size, size, 1, 1, 1, false, true);
	cout << image3DmirrorY << endl;
	cout << image3DmirrorY.CurrentState() << endl;

	cout << endl;
	cout << "MirrorY" << endl;
	Image<unsigned short> image3DmirrorY2(image3DmirrorY, 0, 0, 0, size, size, size, 1, 1, 1, false, true);
	cout << image3DmirrorY2 << endl;
	cout << image3DmirrorY2.CurrentState() << endl;

	cout << endl;
	cout << "MirrorYRoi" << endl;
	Image<unsigned short> image3DmirrorY3(image3DmirrorY2, 1, 1, 1, size - 2, size - 2, size - 2, 1, 1, 1, false, true);
	cout << image3DmirrorY3 << endl;
	cout << image3DmirrorY3.CurrentState() << endl;

	cout << endl;
	cout << "MirrorZ" << endl;
	Image<unsigned short> image3DmirrorZ(image3D, 0, 0, 0, size, size, size, 1, 1, 1, false, false, true);
	cout << image3DmirrorZ << endl;
	cout << image3DmirrorZ.CurrentState() << endl;

	cout << endl;
	cout << "MirrorZ" << endl;
	Image<unsigned short> image3DmirrorZ2(image3DmirrorZ, 0, 0, 0, size, size, size, 1, 1, 1, false, false, true);
	cout << image3DmirrorZ2 << endl;
	cout << image3DmirrorZ2.CurrentState() << endl;

	cout << endl;
	cout << "MirrorZRoi" << endl;
	Image<unsigned short> image3DmirrorZ3(image3DmirrorZ2, 1, 1, 1, size - 2, size - 2, size - 2, 1, 1, 1, false, false, true);
	cout << image3DmirrorZ3 << endl;
	cout << image3DmirrorZ3.CurrentState() << endl;

	 cout << endl;
	cout << "MirrorXYZ" << endl;
	Image<unsigned short> image3DmirrorXYZ(image3D, 0, 0, 0, size, size, size, 1, 1, 1, true, true, true);
	cout << image3DmirrorXYZ << endl;
	cout << image3DmirrorXYZ.CurrentState() << endl;

	 cout << endl;
	cout << "SwapXY" << endl;
	Image<unsigned short> image3DswapXY(image3D, 0, 0, 0, size, size, size, 1, 1, 1, false, false, false, true);
	cout << image3DswapXY << endl;
	cout << image3DswapXY.CurrentState() << endl;

	 cout << endl;
	cout << "SwapXY" << endl;
	Image<unsigned short> image3DswapXY2(image3DswapXY, 0, 0, 0, size, size, size, 1, 1, 1, false, false, false, true);
	cout << image3DswapXY2 << endl;
	cout << image3DswapXY2.CurrentState() << endl;

	 cout << endl;
	cout << "SwapYZ" << endl;
	Image<unsigned short> image3DswapYZ(image3D, 0, 0, 0, size, size, size, 1, 1, 1, false, false, false, false, true);
	cout << image3DswapYZ << endl;
	cout << image3DswapYZ.CurrentState() << endl;

	cout << endl;
	cout << "SwapYZ" << endl;
	Image<unsigned short> image3DswapYZ2(image3DswapYZ, 0, 0, 0, size, size, size, 1, 1, 1, false, false, false, false, true);
	cout << image3DswapYZ2 << endl;
	cout << image3DswapYZ2.CurrentState() << endl;

	cout << endl;
	cout << "SwapZX" << endl;
	Image<unsigned short> image3DswapZX(image3D, 0, 0, 0, size, size, size, 1, 1, 1, false, false, false, false, false, true);
	cout << image3DswapZX << endl;
	cout << image3DswapZX.CurrentState() << endl;

	cout << endl;
	cout << "SwapZX" << endl;
	Image<unsigned short> image3DswapZX2(image3DswapZX, 0, 0, 0, size, size, size, 1, 1, 1, false, false, false, false, false, true);
	cout << image3DswapZX2 << endl;
	cout << image3DswapZX2.CurrentState() << endl;

	Image<unsigned short> image(8, 8);

	cout << endl;
	cout << "roi1" << endl;
	Image<unsigned short> roi1(image, 2, 2, image.Width - 4, image.Height - 4, 2, 2);

	for (auto it = roi1.begin(); it != roi1.end(); ++it) *it = (unsigned short)(10 * it.X / 2 + 1 + (it.Y + 1));
	cout << roi1 << endl;
	cout << roi1.CurrentState() << endl;

	cout << endl;
	cout << "image" << endl;
	cout << image << endl;
}
void testImageLibraryAccuracy()
{
	cout << endl;
	cout << "image" << endl;
	Image<double> image(12, 12, 2);
	cout << image << endl;

	cout << endl;
	cout << "roi1" << endl;
	Image<double> roi1(image, 2, 2, image.Width - 4, image.Height - 4, 2, 2);
	for (auto it = roi1.begin(); it != roi1.end(); ++it) *it = it.X / 2 + 1 + (it.Y + 1) / 10.0;
	cout << roi1 << endl;

	cout << endl;
	cout << "roi2" << endl;
	Image<double> roi2(image, 3, 1, image.Width - 4, image.Height - 2, 2, 1);
	for (auto it = roi2.begin(); it != roi2.end(); ++it) *it = 1;
	cout << roi2 << endl;

	cout << endl;
	cout << "image after roi updates" << endl;
	cout << image << endl;

	cout << endl;
	Image<double> result(roi1);
	double basetime = codeTimer("roi1 convolution", [&]() -> void
	{
		auto resultit = result.begin();
		for (auto it = roi1.begin(); it != roi1.end(); ++it)
		{
			*resultit = (it[roi1.DX] + it[-roi1.DX] + it[roi1.DY] + it[-roi1.DY]) * 0.25;
			++resultit;
		}
	});
	cout << result << endl;

	cout << endl;
	cout << "ROI2" << endl;
	Image<double> result2(roi2);

	auto result2it = result2.begin();
	for (auto roi2it = roi2.begin(); roi2it != roi2.end(); ++roi2it)
	{
		*result2it = (roi2it[roi2.DX] + roi2it[-roi2.DX] + roi2it[roi2.DY] + roi2it[-roi2.DY]) / 4;
		++result2it;
	}
	cout << result2 << endl;

	cout << endl;
	int zeros = 0;
	for (const auto& value : roi1)  if (value == 0) zeros++;
	cout << "roi1 has " << zeros << " zeros" << endl;
	zeros = 0;
	for (const auto& value : image) if (value == 0) zeros++;
	cout << "image has " << zeros << " zeros" << endl;
	cout << endl;
}
void ImageLibrarySpeedTest()
{
	int iterations = 100;
	Image<double> image(1000, 1000, 2);

	Image<double> roi1(image, 2, 2, image.Width - 4, image.Height - 4, 2, 2);
	for (auto it = roi1.begin(); it != roi1.end(); ++it) *it = it.X / 2 + 1 + (it.Y + 1) / 10.0;

	Image<double> roi2(image, 3, 1, image.Width - 4, image.Height - 2, 2, 1);
	for (auto it = roi2.begin(); it != roi2.end(); ++it) *it = 1;

	double basetime;
	{
		Image<double> result(roi1);
		basetime = codeTimer("roi1 convolution", [&]() -> void
		{
			for (auto it = roi1.begin(), resultit = result.begin(); it != roi1.end(); ++it, ++resultit)
				*resultit = (it[roi1.DX] + it[-roi1.DX] + it[roi1.DY] + it[-roi1.DY]) * 0.25;
		}, iterations);
		cout << endl;
		cout << Image<double>(result, 0, 0, 0, 4, 4, 1, 1, 1, 1) << endl;
	}

	{
		Image<double> result(roi1);
		double noAnd = codeTimer("roi1 convolution no &", [&]() -> void
		{
			for (auto it = roi1.begin(), resultit = result.begin(); it != roi1.end(); ++it, ++resultit)
				*resultit = (it(1, x_clamp) + it(-1, x_clamp) + it(1, y_clamp) + it(-1, y_clamp)) * 0.25;
		}, iterations);
		cout << "noAnd is " << (100 * ((basetime / noAnd) - 1)) << "% faster" << endl << endl;
		cout << Image<double>(result, 0, 0, 0, 4, 4, 1, 1, 1, 1) << endl;
	}

	{
		Image<double> result(roi1);
		double noAndPointer = codeTimer("roi1 convolution no & pointer", [&]() -> void
		{
			for (auto it = roi1.begin(), resultit = result.begin(); it != roi1.end(); ++it, ++resultit)
				*resultit = (it[roi1.DX] + it[-roi1.DX] + it[roi1.DY] + it[-roi1.DY]) * 0.25;
		}, iterations);
		cout << "noAndPointer is " << (100 * (basetime / noAndPointer) - 1) << "% faster" << endl << endl;
		cout << Image<double>(result, 0, 0, 0, 4, 4, 1, 1, 1, 1) << endl;
	}
}
void testIntegralImage()
{
	int size = 3;
	cout << endl << "Original Image" << endl;
	Image<unsigned short> image(size, size, size);
	int i = 0;
	for (auto it = image.begin(); it != image.end(); ++it) *it = ++i;
	cout << image << endl;

	cout << "Integral Image" << endl;
	cout << ToIntegralImage<long>(image) << endl;
}
void displayBorderTests(Image<unsigned short>& image3D) {
	cout << "value: " << image3D(1, 1, 1) << endl;
	cout << "-2 x clamp: " << image3D.begin()(-2, x_clamp) << endl;
	cout << "-2 x wrap: " << image3D.begin()(-2, x_wrap) << endl;
	cout << "-2 x reflect: " << image3D.begin()(-2, x_reflect) << endl;
	cout << "-2 y clamp: " << image3D.begin()(-2, y_clamp) << endl;
	cout << "-2 y wrap: " << image3D.begin()(-2, y_wrap) << endl;
	cout << "-2 y reflect: " << image3D.begin()(-2, y_reflect) << endl;
	cout << "-2 z clamp: " << image3D.begin()(-2, z_clamp) << endl;
	cout << "-2 z wrap: " << image3D.begin()(-2, z_wrap) << endl;
	cout << "-2 z reflect: " << image3D.begin()(-2, z_reflect) << endl;
}
void testImageLibraryBorders() {
	int size = 4;
	int i;

	cout << endl << "3D Image" << endl;
	Image<unsigned short> image3D(size, size, size);
	i = 0;
	for (auto it = image3D.begin(); it != image3D.end(); ++it) *it = ++i;
	cout << image3D << endl;
	displayBorderTests(image3D);

	cout << endl << "MirrorX" << endl;
	Image<unsigned short> MirrorX(image3D, 0, 0, 0, size, size, size, 1, 1, 1, true);
	cout << MirrorX << endl;
	displayBorderTests(MirrorX);

	cout << endl << "MirrorY" << endl;
	Image<unsigned short> MirrorY(image3D, 0, 0, 0, size, size, size, 1, 1, 1, false, true);
	cout << MirrorY << endl;
	displayBorderTests(MirrorY);

	cout << endl << "MirrorZ" << endl;
	Image<unsigned short> MirrorZ(image3D, 0, 0, 0, size, size, size, 1, 1, 1, false, false, true);
	cout << MirrorZ << endl;
	displayBorderTests(MirrorZ);
}
void TestImages(string inputFolder, string outputFolder)
{
	Image<uint8_t> image = ImageIO::LoadJpeg(inputFolder + "\\ng_bwgirl_crop.jpg");
	Image<uint8_t> red(image, 0, 0, image.Width, image.Height, 3, 1);
	Image<uint8_t> green(image, 1, 0, image.Width, image.Height, 3, 1);
	Image<uint8_t> blue(image, 2, 0, image.Width, image.Height, 3, 1);
	ImageIO::SaveRaw(red, outputFolder + string("\\red_") + std::to_string(red.Width) + "x" + std::to_string(red.Height) + ".raw");
	ImageIO::SaveRaw(green, outputFolder + string("\\green_") + std::to_string(green.Width) + "x" + std::to_string(green.Height) + ".raw");
	ImageIO::SaveRaw(blue, outputFolder + string("\\blue_") + std::to_string(blue.Width) + "x" + std::to_string(blue.Height) + ".raw");
	//ImageIO::Save(image, outputFolder + "\\ng_bwgirl_crop.jpg");
	Image<float> fimage(image);
	//fimage.GaussianBlur2D(3);
	ImageIO::SaveRaw(fimage, outputFolder + string("\\float_") + std::to_string(fimage.Width) + "x" + std::to_string(fimage.Height) + ".raw");
	Image<uint8_t> bmpImage = ImageIO::LoadBmp(inputFolder + "\\marbles.bmp");
	Image<uint8_t> red2(bmpImage, 0, 0, bmpImage.Width, bmpImage.Height, 3, 1);
	Image<uint8_t> green2(bmpImage, 1, 0, bmpImage.Width, bmpImage.Height, 3, 1);
	Image<uint8_t> blue2(bmpImage, 2, 0, bmpImage.Width, bmpImage.Height, 3, 1);
	ImageIO::SaveRaw(red2, outputFolder + string("\\red2_") + std::to_string(red2.Width) + "x" + std::to_string(red2.Height) + ".raw");
	ImageIO::SaveRaw(green2, outputFolder + string("\\green2_") + std::to_string(green2.Width) + "x" + std::to_string(green2.Height) + ".raw");
	ImageIO::SaveRaw(blue2, outputFolder + string("\\blue2_") + std::to_string(blue2.Width) + "x" + std::to_string(blue2.Height) + ".raw");

	//auto dcmImage = ImageIO::LoadDicom<uint16_t>(inputFolder + "\\foot.dcm");
	//ImageIO::SaveRaw(dcmImage, outputFolder + string("\\mri_") + std::to_string(dcmImage.Width) + "x" + std::to_string(dcmImage.Height) + ".raw");

	auto dcmImage2 = ImageIO::LoadDicom<int16_t>(inputFolder + "\\foot.dcm");
	ImageIO::SaveRaw(dcmImage2, outputFolder + string("\\CT_") + std::to_string(dcmImage2.Width) + "x" + std::to_string(dcmImage2.Height) + ".raw");
}
int main()
{
	testImageLibraryDimensions();
	testImageLibraryAccuracy();
	ImageLibrarySpeedTest();
	testIntegralImage();
	testImageLibraryBorders();
	TestImages("C:\\workspace\\ndl\\testData", "C:\\Users\\Nathan\\Desktop");
	return 0;
}