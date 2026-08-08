#pragma once
#include "imageIO/bitmap.h"
#include "imageIO/jpeg_decoder.h"
#include "imageIO/avi.h"
#include "imageIO/dicom.h"
#include "imageIO/NRRD/nrrd.h"
#include "imageIO/png/yspng.h"
#include "imageIO/png/yspngenc.h"
#include <algorithm>
#include <array>
#include <exception>

namespace ndl
{
	namespace image_io
	{
		std::vector<uint8_t> load(std::string fileName, std::array<int, 3>& extent)
		{
			extent = { 0, 0, 0 };
			std::string extension = "";
			size_t pos = fileName.find_last_of(".");
			if (pos != std::string::npos) extension = fileName.substr(pos + 1);

			//assume nrrd if no extension is provided
			if (extension == "")
			{
				extension = "nrrd";
				fileName += "." + extension;
			}

			if (extension == "jpg" || extension == "jpeg")
			{
				std::cout << "loading file: " << fileName << std::endl;
				size_t size;
				uint8_t *buf;
				FILE *f;
				printf("Opening the input file: %s.\n", fileName.c_str());
				f = fopen(fileName.c_str(), "rb");
				if (!f) throw std::runtime_error("failed to open input file: " + fileName);
				fseek(f, 0, SEEK_END);
				size = ftell(f);
				buf = (unsigned char*)malloc(size);
				fseek(f, 0, SEEK_SET);
				size_t bytesRead = fread(buf, 1, size, f);
				fclose(f);
				if (bytesRead != size)
				{
					free(buf);
					throw std::runtime_error("failed to read input file: " + fileName);
				}
				Decoder decoder(buf, size);
				if (decoder.GetResult() != Decoder::OK)
				{
					free(buf);
					throw std::runtime_error("failed to decode jpeg: " + fileName);
				}
				printf("width: %d\r\n", decoder.GetWidth());
				printf("height: %d\r\n", decoder.GetHeight());
				printf("width * height: %d\r\n", decoder.GetWidth() * decoder.GetHeight());
				printf("size: %zu\r\n", decoder.GetImageSize());

				extent = { 3, decoder.GetWidth(), decoder.GetHeight() };
				unsigned char* data = (unsigned char*)decoder.GetImage();
				std::vector<uint8_t> result(data, data + std::accumulate(extent.begin(), extent.end(), 1, std::multiplies<int>()));
				free(buf);
				return result;
			}
			if (extension == "bmp")
			{
				std::cout << "loading file: " << fileName << std::endl;
				bitmap bmp(fileName.c_str());
				if (bmp.bmih.biBitCount != 32 && bmp.bmih.biBitCount != 24) throw std::runtime_error("unsupported bit count");
				int nColors = bmp.bmih.biBitCount / 8;
				int width = bmp.bmih.biWidth;
				int height = bmp.bmih.biHeight;
				int padding = (4 - ((width * nColors) % 4)) % 4;
				std::vector<uint8_t> resultData(nColors * width * height);
				extent = { nColors, width, height };
				Image<uint8_t, 3> result(resultData.data(), extent);
				int x = 0;
				auto i = bmp.data.begin();
				Image<uint8_t, 3> flipped = result.view({}, {}, {1, 1, -1});
				for (auto r = flipped.begin(); r != flipped.end(); ++r)
				{
					*r = *i;
					++i;
					++x;
					if (x >= width * nColors)
					{
						x = 0;
						i += padding;
						continue;
					}
				}
				return resultData;
			}
			if (extension == "png")
			{
				std::cout << "loading file: " << fileName << std::endl;
				YsGenericPngDecoder::verboseMode = YSFALSE;
				YsRawPngDecoder decoder;
				if (decoder.Decode(fileName.c_str()) != YSOK) throw std::runtime_error("failed to decode png: " + fileName);
				// YsRawPngDecoder always expands to interleaved RGBA regardless of the source color type.
				extent = { 4, decoder.wid, decoder.hei };
				size_t size = (size_t)decoder.wid * decoder.hei * 4;
				std::vector<uint8_t> result(decoder.rgba, decoder.rgba + size);
				return result;
			}
			throw std::runtime_error("unknown data type!!");
		}

		template<class T, int DIM>
		std::vector<T> load_raw(std::string rawFileName, std::array<int, DIM> extent, int offsetBytes)
		{
			std::vector<T> result(std::accumulate(extent.begin(), extent.end(), 1, std::multiplies<int>()));
			std::ifstream is;
			is.open(rawFileName, std::ios::binary);
			is.seekg(offsetBytes, std::ios::beg);
			int size = std::accumulate(extent.begin(), extent.end(), 1, std::multiplies<int>());
			is.read((char*)result.data(), size * sizeof(T));
			is.close();
			return result;
		}

		template<class T, int DIM>
		void save_raw(const Image<T, DIM>& source, std::string rawFileName)
		{
			std::cout << "saving output file: " << rawFileName << std::endl;
			std::ofstream ofile(rawFileName, std::ios::binary);
			for (auto it = source.begin(); it != source.end(); ++it) ofile.write((const char*)it.get(), sizeof(T));
			ofile.close();
		}

		template<class T>
		std::vector<T> load_dicom(std::string filename, std::array<int, 3>& extent, bool openAllInFolder = false)
		{
			dicom image;
			std::replace(filename.begin(), filename.end(), '\\', '/'); // replace all 'x' to 'y'
			image.load_from_file(filename);

			//parse path
			auto lastSlashIndex = filename.find_last_of("\\");
			std::string dir = filename.substr(0, lastSlashIndex);
			std::string file = filename.substr(lastSlashIndex + 1, filename.length() - 1);

			//parse file
			std::string firstPart = file;
			std::string lastPart = "";
			//...

			std::cout << "filename: " << filename << std::endl;
			std::cout << "dir: " << dir << std::endl;
			std::cout << "file: " << file << std::endl;

			//get image dimensions
			extent = std::array<int, 3>{ (int)image.width(), (int)image.height(), (int)image.number_of_frames() };
			if (extent[2] == 0) extent[2] = 1;
			std::cout << "width: " << image.width() << std::endl;
			std::cout << "height: " << image.height() << std::endl;
			std::cout << "number_of_frames: " << image.number_of_frames() << std::endl;

			//get rescale / intercept values
			auto intercept = image.rescale_intercept();
			auto slope = image.rescale_slope();
			std::cout << "rescale_intercept: " << image.rescale_intercept() << std::endl;
			std::cout << "rescale_slope: " << image.rescale_slope() << std::endl;

			//if (extent[2] <= 1 && extent[0] && extent[1]) extent[2] = image.image_size / extent[0] / extent[1] / (image.get_bit_count() / 8);
			//ROWS * COLUMNS * NUMBER_OF_FRAMES *	SAMPLES_PER_PIXEL * (BITS_ALLOCATED / 8)
			std::vector<T> result(std::accumulate(extent.begin(), extent.end(), 1, std::multiplies<int>()));
			T* ptr = result.data();
			long pixel_count = result.size();
			if (sizeof(T) == image.get_bit_count() / 8)
			{
				image.input_io->read((char*)ptr, pixel_count * sizeof(T));
			}
			else
			{
				std::vector<char> data(pixel_count*image.get_bit_count() / 8);
				image.input_io->read((char*)&(data[0]), data.size());
				switch (image.get_bit_count()) //bit count
				{
				case 8://DT_UNSIGNED_CHAR 2
					std::copy((const unsigned char*)&(data[0]), (const unsigned char*)&(data[0]) + pixel_count, ptr);
					break;
				case 16://DT_SIGNED_SHORT 4
					std::copy((const short*)&(data[0]), (const short*)&(data[0]) + pixel_count, ptr);
					break;
				case 32://DT_SIGNED_INT 8
					std::copy((const int*)&(data[0]), (const int*)&(data[0]) + pixel_count, ptr);
					break;
				case 64://DT_DOUBLE 64
					std::copy((const double*)&(data[0]), (const double*)&(data[0]) + pixel_count, ptr);
					break;
				}
			}
			return result;
		}

		template<class T, int DIM>
		void save(const Image<T, DIM>& image, std::string fileName)
		{
			std::string extension = "";
			size_t pos = fileName.find_last_of(".");
			if (pos != std::string::npos) extension = fileName.substr(pos + 1);

			//assume nrrd if no extension is provided
			if (extension == "")
			{
				extension = "nrrd";
				fileName += "." + extension;
			}

			//make a local copy because the pointer needs to be contiguous and it may not be
			//contiguous within the provided image
			std::vector<T> data(image.size());
			Image<T, DIM> temp(data.data(), image);

			//handle each filetype
			if (extension == "nrrd")
			{
				std::cout << "saving output file: " << fileName << std::endl;
				NRRD::save(fileName, data.data(), DIM, image.extent().data());
			}
			else if (extension == "bmp")
			{
				std::cout << "saving output file: " << fileName << std::endl;
				bitmap bmp;
				if (DIM == 3)
				{
					bmp.bmih.biWidth = image.extent()[1];
					bmp.bmih.biHeight = image.extent()[2];
					if (image.extent()[0] == 3) bmp.bmih.biBitCount = 24;
					else if (image.extent()[0] == 4) bmp.bmih.biBitCount = 32;
					else if (image.extent()[0] == 1) bmp.bmih.biBitCount = 8;
					else throw std::runtime_error("the first dimensions for 3D represents color, an unsupported number of colors was selected");
					auto flipped = image.view({}, {}, {1, 1, -1});
					int padding = (4 - ((bmp.bmih.biWidth * image.extent()[0]) % 4)) % 4;
					int x = 0;
					bmp.data = std::vector<unsigned char>((bmp.bmih.biWidth + padding) * bmp.bmih.biHeight * (image.extent()[0]));
					auto i = bmp.data.begin();
					for (auto r = flipped.begin(); r != flipped.end(); ++r)
					{
						*i = *r;
						++i;
						++x;
						if (x >= bmp.bmih.biWidth * image.extent()[0])
						{
							x = 0;
							i += padding;
							continue;
						}
					}
				}
				else if (DIM == 2)
				{
					bmp.bmih.biWidth = image.extent()[0];
					bmp.bmih.biHeight = image.extent()[1];
					bmp.bmih.biBitCount = 8;
					auto flipped = image.view({}, {}, {1, -1});
					int padding = (4 - (bmp.bmih.biWidth % 4)) % 4;
					int x = 0;
					bmp.data = std::vector<unsigned char>((bmp.bmih.biWidth + padding) * bmp.bmih.biHeight);
					auto i = bmp.data.begin();
					for (auto r = flipped.begin(); r != flipped.end(); ++r)
					{
						*i = *r;
						++i;
						++x;
						if (x >= bmp.bmih.biWidth)
						{
							x = 0;
							i += padding;
							continue;
						}
					}
				}
				else throw std::runtime_error("unsupported dimension");
				bmp.save_to_file(fileName.c_str());
			}
			else if (extension == "png")
			{
				std::cout << "saving output file: " << fileName << std::endl;
				if (sizeof(T) != 1) throw std::runtime_error("png save requires an 8-bit-per-channel image");
				if (DIM != 3) throw std::runtime_error("png save requires a 3D image (channel,width,height)");
				int nColors = image.extent()[0];
				int width = image.extent()[1];
				int height = image.extent()[2];
				int colorType;
				if (nColors == 1) colorType = 0;      // Greyscale
				else if (nColors == 3) colorType = 2; // Truecolor
				else if (nColors == 4) colorType = 6; // Truecolor with alpha
				else throw std::runtime_error("the first dimension for 3D represents color, an unsupported number of colors was selected");
				// data is already contiguous with dim0 (channel) fastest and top-to-bottom row order,
				// exactly what PNG expects -- unlike bmp, no vertical flip is needed here.
				YsRawPngEncoder encoder;
				encoder.verboseMode = YSFALSE;
				if (encoder.EncodeToFile(fileName.c_str(), width, height, 8, colorType, (const unsigned char*)data.data()) != YSOK)
					throw std::runtime_error("failed to encode png: " + fileName);
			}
			else
			{
				std::cerr << "couldn't save: " << fileName << "\n";
			}
		}

		template<class T, int DIM>
		std::vector<T> load_nrrd(std::array<int, DIM>& extent, std::string fileName)
		{
			T* data;
			std::vector<int> extentVector(DIM);
			NRRD::load(fileName, &data, &extentVector);
			std::copy(extentVector.begin(), extentVector.end(), extent.begin());
			return std::vector<T>(data, data + std::accumulate(extent.begin(), extent.end(), 1, std::multiplies<int>()));
		}
	}
}
