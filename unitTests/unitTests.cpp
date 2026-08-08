#include <vector>
#include <algorithm>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdint>
#include <complex>
#include <type_traits>
#include <initializer_list>
#include <cmath>

#include <ndl/image.h>
#include <ndl/imageIO.h>
#include <ndl/utility.h>
#include <ndl/mathHelpers.h>
#include <ndl/fft.h>
#include <ndl/matrix.h>


using namespace ndl;
using namespace ndl::fft;

std::vector<int> genLinVec(int size) {
    std::vector<int> v(size);
    std::iota(v.begin(), v.end(), int(1));
    return v;
}

std::vector<int> generateFlattenedArray(const std::initializer_list<int>& sizes) {
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
void passFailCheck(std::stringstream& passfail, const Image<T, DIM>& image, const std::vector<int>& refVec, const std::string testName) {
    bool testPassed = true;
    int total = 0;
    for (const auto &index : image.getCoordinates()) 
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
		std::cout << image.state() << std::endl;
		total = 0;
		for (const auto &index : image.getCoordinates()) 
		{
			passfail << "    img:ref (" << image.at(index) << ":" << refVec[total] << ")" << std::endl;
			total++;
		}
	}
}

void testImageLibraryDimensions(std::stringstream& passfail)
{
	//setup variables
	int size = 4;
	int i;

	//create a 1D image with increasing values
	std::cout << std::endl << "1D Image" << std::endl;
	std::vector<unsigned short> image1Ddata(size);
	Image<unsigned short, 1> image1D(image1Ddata.data(), { size });
	i = 0;
	for (auto it = image1D.begin(); it != image1D.end(); ++it) 
		*it = ++i;
	std::cout << image1D;
    passFailCheck(passfail, image1D, generateFlattenedArray({size}), "1D Image Test");


	//create a 2D image with increasing values
	std::cout << std::endl << "2D Image" << std::endl;
	std::vector<unsigned short> image2Ddata(size*size);
	Image<unsigned short, 2> image2D(image2Ddata.data(), { size, size });
	i = 0;
	for (auto it = image2D.begin(); it != image2D.end(); ++it) 
		*it = ++i;
	std::cout << image2D;
    passFailCheck(passfail, image2D, generateFlattenedArray({size, size}), "2D Image Test");

	//create a 3D image with increasing values
	std::cout << std::endl << "3D Image" << std::endl;
	std::vector<unsigned short> image3Ddata( size*size*size );
	Image<unsigned short, 3> image3D(image3Ddata.data(), { size, size, size });
	i = 0;
	for (auto it = image3D.begin(); it != image3D.end(); ++it) 
		*it = ++i;
	std::cout << image3D;
    passFailCheck(passfail, image3D, generateFlattenedArray({size, size, size}), "3D Image Test");

	//from the 3D image extract an ROI
	std::cout << std::endl;
	std::cout << "ExtractRoi" << std::endl;
	Image<unsigned short, 3> image3DextractRoi = image3D({1,1,1}, {-2,-2,-2});

	std::cout << image3DextractRoi;

	//mirror the 3D image along X direction
	std::cout << std::endl << "MirrorX" << std::endl;
	Image<unsigned short, 3> image3DmirrorX = image3D({}, {}, {-1, 1, 1});
	std::cout << image3DmirrorX;
    passFailCheck(passfail, image3DmirrorX, generateFlattenedArray({-size, size, size}), "MirrorX 3D Image Test 1");

	//mirror the 3D image along X direction again
	std::cout << std::endl;
	std::cout << "MirrorX" << std::endl;
	Image<unsigned short, 3> image3DmirrorX2 = image3DmirrorX({}, {}, {-1, 1, 1});
	std::cout << image3DmirrorX2;
    passFailCheck(passfail, image3DmirrorX2, generateFlattenedArray({size, size, size}), "MirrorX 3D Image Test 2");

	//mirror the 3D image along X direction within an ROI
	std::cout << std::endl;
	std::cout << "MirrorXRoi" << std::endl;
	Image<unsigned short, 3> image3DmirrorX3 = image3DmirrorX2({ 1, 1, 1 },{ -2,-2,-2 },{ -1,1,1 });
	std::cout << image3DmirrorX3;

	//mirror the 3D image along Y direction
	std::cout << std::endl;
	std::cout << "MirrorY" << std::endl;
	Image<unsigned short, 3> image3DmirrorY = image3D({}, {}, {1, -1, 1});
	std::cout << image3DmirrorY;
    passFailCheck(passfail, image3DmirrorY, generateFlattenedArray({size, -size, size}), "MirrorY 3D Image Test 1");

	//mirror the 3D image along Y direction again
	std::cout << std::endl;
	std::cout << "MirrorY" << std::endl;
	Image<unsigned short, 3> image3DmirrorY2 = image3DmirrorY({}, {}, {1, -1, 1});
	std::cout << image3DmirrorY2;
    passFailCheck(passfail, image3DmirrorY2, generateFlattenedArray({size, size, size}), "MirrorY 3D Image Test 2");

	//mirror the 3D image along Y direction within an ROI
	std::cout << std::endl;
	std::cout << "MirrorYRoi" << std::endl;
	Image<unsigned short, 3> image3DmirrorY3 = image3DmirrorY2({ 1, 1, 1 },{ -2,-2,-2 },{ 1,-1,1 });
	std::cout << image3DmirrorY3;

	//mirror the 3D image along Z direction
	std::cout << std::endl;
	std::cout << "MirrorZ" << std::endl;
	Image<unsigned short, 3> image3DmirrorZ = image3D({}, {}, {1, 1, -1});
	std::cout << image3DmirrorZ;
    passFailCheck(passfail, image3DmirrorZ, generateFlattenedArray({size, size, -size}), "MirrorZ 3D Image Test 1");

	//mirror the 3D image along Z direction again
	std::cout << std::endl;
	std::cout << "MirrorZ" << std::endl;
	Image<unsigned short, 3> image3DmirrorZ2 = image3DmirrorZ({}, {}, {1, 1, -1});
	std::cout << image3DmirrorZ2;
    passFailCheck(passfail, image3DmirrorZ2, generateFlattenedArray({size, size, size}), "MirrorZ 3D Image Test 2");

	//mirror the 3D image along Z direction within an ROI
	std::cout << std::endl;
	std::cout << "MirrorZRoi" << std::endl;
	Image<unsigned short, 3> image3DmirrorZ3 = image3DmirrorZ2({ 1, 1, 1 },{ -2, -2, -2 },{ 1, 1, -1 });
	std::cout << image3DmirrorZ3;

	//mirror the 3D image along X,Y, and Z directions
	std::cout << std::endl;
	std::cout << "MirrorXYZ" << std::endl;
	Image<unsigned short, 3> image3DmirrorXYZ = image3D({}, {}, {-1, -1, -1});
	std::cout << image3DmirrorXYZ;
    passFailCheck(passfail, image3DmirrorXYZ, generateFlattenedArray({-size, -size, -size}), "MirrorXYZ 3D Image");

	//swap X and Y dimensions
	std::cout << std::endl;
	std::cout << "SwapXY" << std::endl;
	Image<unsigned short, 3> image3DswapXY = image3D.swap(0,1);
	std::cout << image3DswapXY;

	//swap X and Y dimensions again
	std::cout << std::endl;
	std::cout << "SwapXY" << std::endl;
	Image<unsigned short, 3> image3DswapXY2 = image3DswapXY.swap(0, 1);
	std::cout << image3DswapXY2;

	//swap Y and Z dimensions
	std::cout << std::endl;
	std::cout << "SwapYZ" << std::endl;
	Image<unsigned short, 3> image3DswapYZ = image3D.swap(1, 2);
	std::cout << image3DswapYZ;

	//swap Y and Z dimensions again
	std::cout << std::endl;
	std::cout << "SwapYZ" << std::endl;
	Image<unsigned short, 3> image3DswapYZ2 = image3DswapYZ.swap(1, 2);
	std::cout << image3DswapYZ2;

	//swap Z and X dimensions
	std::cout << std::endl;
	std::cout << "SwapZX" << std::endl;
	Image<unsigned short, 3> image3DswapZX = image3D.swap(0, 2);
	std::cout << image3DswapZX;

	//swap Z and X dimensions again
	std::cout << std::endl;
	std::cout << "SwapZX" << std::endl;
	Image<unsigned short, 3> image3DswapZX2 = image3DswapZX.swap(0, 2);
	std::cout << image3DswapZX2;

	//get a sub-image while mirroring along y and skipping
	std::cout << std::endl;
	std::cout << "SubImage1" << std::endl;
	Image<unsigned short, 3> subImage1 = image3D({},{ -1, 3, -1 }, { 1, -2, 1 });
	std::cout << subImage1;
	
	//get a sub-image
	std::cout << std::endl;
	std::cout << "SubImage2" << std::endl;
	Image<unsigned short, 3> subImage2 = image3D({0,2,0}, {-1,3,-1}, {});
	std::cout << subImage2;

	//combination test
	std::vector<unsigned short> imagedata(8*8*1);
	Image<unsigned short, 3> image(imagedata.data(), { 8, 8, 1 });
	std::cout << std::endl;
	std::cout << "roi1" << std::endl;
	Image<unsigned short, 3> roi1 = image({2,2,0}, {-4,-4,0}, {2,2,1});
	for (auto it = roi1.begin(); it != roi1.end(); ++it)
		*it = (unsigned short)(10 * it.I[0] / 2 + 1 + (it.I[1] + 1));
	std::cout << roi1 << std::endl;
	std::cout << "image" << std::endl;
	std::cout << image;
}
void testImageLibraryAccuracy(std::stringstream& passfail)
{
	std::cout << std::endl;
	std::cout << "image" << std::endl;
	std::vector<double> imagedata(12*12*2);
	Image<double, 3> image(imagedata.data(), { 12, 12, 2 });
	std::cout << image;

	std::cout << std::endl;
	std::cout << "roi1" << std::endl;
	Image<double, 3> roi1 = image({ 2,2,0 },{ -4,-4,0 },{ 2,2,1 });
	for (auto it = roi1.begin(); it != roi1.end(); ++it)
		*it = it.I[0] / 2 + 1 + (it.I[1] + 1) / 10.0;
	std::cout << roi1;

	std::cout << std::endl;
	std::cout << "roi2" << std::endl;
	Image<double, 3> roi2 = image({ 3,1,0 },{ -4,-2,0 },{ 2,1,1 });
	for (auto it = roi2.begin(); it != roi2.end(); ++it)
		*it = 1;
	std::cout << roi2;

	std::cout << std::endl;
	std::cout << "image after roi updates" << std::endl;
	std::cout << image;

	std::cout << std::endl;
	Image<double, 3> result(roi1);
	double basetime = codeTimer("roi1 convolution", [&]() -> void
	{
		auto resultit = result.begin();
		for (auto it = roi1.begin(); it != roi1.end(); ++it)
		{
			*resultit = (it[roi1.Stride[0]] + it[-roi1.Stride[0]] + it[roi1.Stride[1]] + it[-roi1.Stride[1]]) * 0.25;
			++resultit;
		}
	});
	std::cout << result;

	std::cout << std::endl;
	std::cout << "ROI2" << std::endl;
	std::vector<double> result2Data(std::accumulate(roi2.Extent.begin(), roi2.Extent.end(), 1, std::multiplies<int>()));
	Image<double, 3> result2(result2Data.data(), roi2);

	auto result2it = result2.begin();
	for (auto roi2it = roi2.begin(); roi2it != roi2.end(); ++roi2it)
	{
		*result2it = (roi2it[roi2.Stride[0]] + roi2it[-roi2.Stride[0]] + roi2it[roi2.Stride[1]] + roi2it[-roi2.Stride[1]]) / 4;
		++result2it;
	}
	std::cout << result2;

	std::cout << std::endl;
	int zeros = 0;
	for (auto value : roi1)  if (value == 0) zeros++;
	std::cout << "roi1 has " << zeros << " zeros" << std::endl;
	zeros = 0;
	for (auto value : image) if (value == 0) zeros++;
	std::cout << "image has " << zeros << " zeros" << std::endl;
	std::cout << std::endl;
}
void ImageLibrarySpeedTest(std::stringstream& passfail)
{
	int iterations = 100;
	std::vector<double> imagedata(1000 * 1000 * 2);
	Image<double, 3> image(imagedata.data(), { 1000, 1000, 2 });

	Image<double, 3> roi1 = image({ 2,2,1 },{ -4,-4,1 },{ 2,2,1 });
	for (auto it = roi1.begin(); it != roi1.end(); ++it) 
		*it = it.I[0] / 2 + 1 + (it.I[1] + 1) / 10.0;
	Image<double, 3> roi2 = image({ 3,1,1 },{ -4,-2,1 },{ 2,1,1 });
	for (auto it = roi2.begin(); it != roi2.end(); ++it)
		*it = 1;

	double basetime;
	std::vector<double> roi1Data(std::accumulate(roi1.Extent.begin(), roi1.Extent.end(), 1, std::multiplies<int>()));
	{
		std::fill(roi1Data.begin(), roi1Data.end(), 0);
		Image<double, 3> result(roi1Data.data(), roi1);
		basetime = codeTimer("roi1 convolution index", [&]() -> void
		{
			for (auto it = roi1.begin(), resultit = result.begin(); it != roi1.end(); ++it, ++resultit)
				*resultit = (it[roi1.Stride[0]] + it[-roi1.Stride[0]] + it[roi1.Stride[1]] + it[-roi1.Stride[1]]) * 0.25;
		}, iterations);
		std::cout << std::endl;
		std::cout << result({}, { 3,3,0 }, {});
	}

	{
		std::fill(roi1Data.begin(), roi1Data.end(), 0);
		Image<double, 3> result(roi1Data.data(), roi1);
		double noAnd = codeTimer("roi1 convolution clamp", [&]() -> void
		{
			for (auto it = roi1.begin(), resultit = result.begin(); it != roi1.end(); ++it, ++resultit)
				*resultit = (it.clamp(1, 0) + it.clamp(-1, 0) + it.clamp(1, 1) + it.clamp(-1, 1)) * 0.25;
		}, iterations);
		std::cout << "noAnd is " << (100 * ((basetime / noAnd) - 1)) << "% faster" << std::endl << std::endl;
		std::cout << result({},{ 3,3,0 },{});
	}

	{
		std::fill(roi1Data.begin(), roi1Data.end(), 0);
		Image<double, 3> result(roi1Data.data(), roi1);
		double noAnd = codeTimer("roi1 convolution wrap", [&]() -> void
		{
			for (auto it = roi1.begin(), resultit = result.begin(); it != roi1.end(); ++it, ++resultit)
				*resultit = (it.wrap(1, 0) + it.wrap(-1, 0) + it.wrap(1, 1) + it.wrap(-1, 1)) * 0.25;
		}, iterations);
		std::cout << "noAnd is " << (100 * ((basetime / noAnd) - 1)) << "% faster" << std::endl << std::endl;
		std::cout << result({},{ 3,3,0 },{});
	}
}
void displayBorderTests(Image<unsigned short, 3>& image3D) {
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
void testImageLibraryBorders(std::stringstream& passfail) {
	int size = 4;
	int i;

	std::cout << std::endl << "3D Image" << std::endl;
	std::vector<unsigned short> image3Ddata(size*size*size);
	Image<unsigned short, 3> image3D(image3Ddata.data(), { size, size, size });
	i = 0;
	for (auto it = image3D.begin(); it != image3D.end(); ++it) 
		*it = ++i;
	std::cout << image3D;
	displayBorderTests(image3D);

	std::cout << std::endl << "MirrorX" << std::endl;
	Image<unsigned short, 3> MirrorX = image3D({},{ -1,-1,-1 },{ -1,1,1 });
	std::cout << MirrorX;
	displayBorderTests(MirrorX);

	std::cout << std::endl << "MirrorY" << std::endl;
	Image<unsigned short, 3> MirrorY = image3D({},{ -1,-1,-1 },{ 1,-1,1 });
	std::cout << MirrorY;
	displayBorderTests(MirrorY);

	std::cout << std::endl << "MirrorZ" << std::endl;
	Image<unsigned short, 3> MirrorZ = image3D({},{ -1,-1,-1 },{ 1,1,-1 });
	std::cout << MirrorZ;
	displayBorderTests(MirrorZ);
}
void TestImages(std::stringstream& passfail, std::string inputFolder, std::string outputFolder)
{
	std::array<int, 3> imageExtent;
	std::vector<uint8_t> imageData = ImageIO::Load(inputFolder + "/ng_bwgirl_crop.jpg", imageExtent);
	Image<uint8_t, 3> image(imageData.data(), imageExtent);
	Image<uint8_t, 2> red = image.slice(0, 0);
	Image<uint8_t, 2> green = image.slice(0, 1);
	Image<uint8_t, 2> blue = image.slice(0, 2);
	ImageIO::SaveRaw(red, outputFolder + std::string("/red_") + std::to_string(red.Extent[0]) + "x" + std::to_string(red.Extent[1]) + ".raw");
	ImageIO::SaveRaw(green, outputFolder + std::string("/green_") + std::to_string(green.Extent[0]) + "x" + std::to_string(green.Extent[1]) + ".raw");
	ImageIO::SaveRaw(blue, outputFolder + std::string("/blue_") + std::to_string(blue.Extent[0]) + "x" + std::to_string(blue.Extent[1]) + ".raw");
	ImageIO::Save(image, outputFolder + "/ng_bwgirl_crop.nrrd");
	ImageIO::Save(image, outputFolder + "/ng_bwgirl_crop.bmp");
	ImageIO::Save(red, outputFolder + "/ng_bwgirl_crop_red.nrrd");

	std::array<int, 3> extentNrrd;
	std::vector<uint8_t> reloadVector = ImageIO::LoadNrrd<uint8_t, 3>(extentNrrd, outputFolder + "/ng_bwgirl_crop.nrrd");
	Image<uint8_t, 3> reload(reloadVector.data(), extentNrrd);
	ImageIO::Save(reload, outputFolder + "/ng_bwgirl_crop_RESAVE.bmp");

	std::vector<float> fimagedata(image.size());
	Image<float, 3> fimage(fimagedata.data(), image);
	ImageIO::SaveRaw(fimage, outputFolder + std::string("/float_") + std::to_string(fimage.Extent[0]) + "x" + std::to_string(fimage.Extent[1]) + "x" + std::to_string(fimage.Extent[2]) + ".raw");

	std::array<int, 3> bmpImageExtent;
	std::vector<uint8_t> bmpImageData = ImageIO::Load(inputFolder + "/marbles.bmp", bmpImageExtent);
	Image<uint8_t, 3> bmpImage(bmpImageData.data(), bmpImageExtent);
	ImageIO::Save(bmpImage, outputFolder + "/marbles_output.bmp");

	Image<uint8_t, 2> red2 = bmpImage.slice(0, 0);
	Image<uint8_t, 2> green2 = bmpImage.slice(0, 1);
	Image<uint8_t, 2> blue2 = bmpImage.slice(0, 2);

	ImageIO::Save(red2, outputFolder + "/marbles_red2_output.bmp");
	ImageIO::Save(green2, outputFolder + "/marbles_green2_output.bmp");
	ImageIO::Save(blue2, outputFolder + "/marbles_blue2_output.bmp");

	ImageIO::SaveRaw(red2, outputFolder + std::string("/red2_") + std::to_string(red2.Extent[0]) + "x" + std::to_string(red2.Extent[1]) + ".raw");
	ImageIO::SaveRaw(green2, outputFolder + std::string("/green2_") + std::to_string(green2.Extent[0]) + "x" + std::to_string(green2.Extent[1]) + ".raw");
	ImageIO::SaveRaw(blue2, outputFolder + std::string("/blue2_") + std::to_string(blue2.Extent[0]) + "x" + std::to_string(blue2.Extent[1]) + ".raw");
	std::array<int, 3> dcmImage2Extent;
	std::vector<int16_t> dcmImage2data = ImageIO::LoadDicom<int16_t>(inputFolder + "/foot.dcm", dcmImage2Extent);
	Image<int16_t, 3> dcmImage2(dcmImage2data.data(), dcmImage2Extent);
	ImageIO::SaveRaw(dcmImage2, outputFolder + std::string("/CT_") + std::to_string(dcmImage2Extent[0]) + "x" + std::to_string(dcmImage2Extent[1]) + ".raw");
}
void testcomplex(std::stringstream& passfail) {
	const int length = 1024;
	const int maxprint = 16;
	//setup a complex input vector
	std::cout << "\nTESTCOMPLEX";
	std::vector<std::complex<double>> time(length);
	for (int i = 0; i<length; i++)
		time[i] = std::min(i + 1, 10);
	std::cout << "\n========================\nORIGINAL\n========================\n";
	for (int i = 0; i<maxprint; i++)
		std::cout << time[i] << "\n";

	//setup an FFT object
	std::vector<std::complex<double>> freq(length);
	std::vector<double> scratch(length * 4);
	FFT<double, length> fft(scratch.data());

	//run the fft a bunch of times
	clock_t start = clock();
	for (int i = 0; i<128; i++) {
		fft.fft(length, time.data(), freq.data());
	}
	double ellapsed = double(clock() - start) / double(CLOCKS_PER_SEC);
	std::cout << "==========================\n Ellapsed time: " << ellapsed << " sec\n";

	//display frequency data
	std::cout << "\n========================\nFREQ\n========================\n";
	for (int i = 0; i<maxprint; i++)
		std::cout << freq[i] << "\n";

	//display the time data again
	fft.ifft(length, freq.data(), time.data());
	std::cout << "\n========================\nAND BACK\n========================\n";
	for (int i = 0; i<maxprint; i++)
		std::cout << time[i] << "\n";
	std::cout << "==========================\n Ellapsed time: " << std::scientific << ellapsed << " sec\n";
}
void testreal(std::stringstream& passfail) {
	const int length = 1024;
	const int maxprint = 16;
	std::cout << "\nTESTREAL";
	std::vector<double> input(length);
	std::vector<std::complex<double>> output(length);
	std::vector<double> scratch(length * 5);
	FFTReal<double, length> fft(scratch.data());
	for (int i = 0; i < length; i++)
		input[i] = std::min(i + 1, 10);

	std::cout << "\n========================\nORIGINAL\n========================\n";
	for (int i = 0; i<maxprint; i++) std::cout << input[i] << "\n";

	std::cout << "\n========================\nFREQ\n========================\n";
	clock_t start = clock();
	for (int i = 0; i<768; i++)
		fft.fft(length, input.data(), output.data());
	double ellapsed = double(clock() - start) / double(CLOCKS_PER_SEC);
	for (int i = 0; i<maxprint; i++)
		std::cout << output[i] << "\n";

	std::cout << "\n========================\nAND BACK\n========================\n";
	fft.ifft(length, output.data(), input.data());
	for (int i = 0; i<maxprint; i++)
		std::cout << input[i] << "\n";
	std::cout << "==========================\n Ellapsed time: " << std::scientific  << ellapsed << " sec\n";
}


void testMatrixLibrary(std::stringstream& passfail) {
	std::cout << std::endl << "MATRIX" << std::endl;

	Matrix<double, 3> diag3;
	diag3.ElementAt(0, 0) = 2; diag3.ElementAt(1, 1) = 3; diag3.ElementAt(2, 2) = 4;
	passfail << "Matrix Determinant (3x3): " << (std::abs(diag3.Determinant() - 24.0) < 1e-9 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 5> diag5;
	for (int i = 0; i < 5; i++) diag5.ElementAt(i, i) = i + 1;
	passfail << "Matrix Determinant (5x5, Laplace expansion): " << (std::abs(diag5.Determinant() - 120.0) < 1e-9 ? "Pass" : "Fail") << std::endl;

	// SVD-based inverse should satisfy A * inverse(A) == identity
	Matrix<double, 3> rot;
	rot.SetRotate(M_PI / 4, 0, 1);
	Matrix<double, 3> inv = rot.GetInverse();
	Matrix<double, 3> product = rot * inv;
	Matrix<double, 3> identity;
	bool isIdentity = true;
	for (int i = 0; i < 3 && isIdentity; i++)
		for (int j = 0; j < 3 && isIdentity; j++)
			if (std::abs(product.ElementAt(i, j) - identity.ElementAt(i, j)) > 1e-9) isIdentity = false;
	passfail << "Matrix GetInverse (A * inverse(A) == I): " << (isIdentity ? "Pass" : "Fail") << std::endl;

	// a rotation matrix is orthogonal: its inverse equals its transpose
	Matrix<double, 3> transpose = rot.GetTranspose();
	bool matchesTranspose = true;
	for (int i = 0; i < 3 && matchesTranspose; i++)
		for (int j = 0; j < 3 && matchesTranspose; j++)
			if (std::abs(inv.ElementAt(i, j) - transpose.ElementAt(i, j)) > 1e-9) matchesTranspose = false;
	passfail << "Matrix rotation inverse == transpose: " << (matchesTranspose ? "Pass" : "Fail") << std::endl;
}

// Regression test for a bug where composing a further view (roi/mirror/swap via
// operator()) on top of a slice()-derived image silently lost the slice's
// channel offset, making every channel of a sliced-then-viewed image alias
// channel 0. See the RootDataArray comment on the slice constructor in image.h.
void testSliceComposition(std::stringstream& passfail) {
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
		Image<int, 2> mirroredY = channelSlice({}, {}, { 1, -1 });
		for (int y = 0; y < h && allMatch; y++)
			for (int x = 0; x < w && allMatch; x++)
			{
				int expected = c * 1000 + (h - 1 - y) * 10 + x;
				if (mirroredY.at({ x, y }) != expected) allMatch = false;
			}
	}
	passfail << "Slice then mirror reads correct channel: " << (allMatch ? "Pass" : "Fail") << std::endl;
}

#include <filesystem>
namespace fs = std::filesystem;

#ifndef NDL_TEST_DATA_DIR
#define NDL_TEST_DATA_DIR "."
#endif

#ifndef NDL_TEST_OUTPUT_DIR
#define NDL_TEST_OUTPUT_DIR "output"
#endif

int main()
{
    std::stringstream passfail;

    // Both directories are baked in at configure time (see CMakeLists.txt) so this
    // works regardless of where the build directory lives relative to the source tree.
    fs::path dataPath = NDL_TEST_DATA_DIR;
    fs::path tmpPath  = NDL_TEST_OUTPUT_DIR;
    fs::create_directories(tmpPath);

    testImageLibraryDimensions(passfail);
    testImageLibraryAccuracy(passfail);
    ImageLibrarySpeedTest(passfail);
    testImageLibraryBorders(passfail);
    testSliceComposition(passfail);
    testMatrixLibrary(passfail);
    TestImages(passfail, dataPath.string(), tmpPath.string());
    testreal(passfail);
    testcomplex(passfail);

    std::cout << "\n\nPass/Fail Results:\n" << passfail.str() << "\n";
    return 0;
}
