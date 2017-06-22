#pragma once
#include "./ImageIO/bitmap.h";
#include "./ImageIO/jpeg_decoder.h";
#include "./ImageIO/avi.h";
#include "./ImageIO/dicom.h";
#include "./imageIO/NRRD/nrrd.h"
#include <algorithm>

namespace ndl
{
	namespace ImageIO
	{
		Image<uint8_t, 3> Load(std::string fileName)
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

			if (extension == "jpg" || extension == "jpeg")
			{
				size_t size;
				uint8_t *buf;
				FILE *f;
				printf("Opening the input file: %s.\n", fileName.c_str());
				f = fopen(fileName.c_str(), "rb");
				if (!f) {
					printf("Error opening the input file.\n");
					return Image<uint8_t, 3>({ 3, 1, 1 });
				}
				fseek(f, 0, SEEK_END);
				size = ftell(f);
				buf = (unsigned char*)malloc(size);
				fseek(f, 0, SEEK_SET);
				size_t read = fread(buf, 1, size, f);
				fclose(f);
				Decoder decoder(buf, size);
				if (decoder.GetResult() != Decoder::OK)
				{
					printf("Error decoding the input file\n");
					return Image<uint8_t, 3>({ 3, 1, 1 });
				}
				printf("width: %d\r\n", decoder.GetWidth());
				printf("height: %d\r\n", decoder.GetHeight());
				printf("width * height: %d\r\n", decoder.GetWidth() * decoder.GetHeight());
				printf("size: %d\r\n", decoder.GetImageSize());

				Image<uint8_t, 3> im(decoder.GetImage(), { 3, decoder.GetWidth(), decoder.GetHeight() });
				return Image<uint8_t, 3>(im);
			}
			if (extension == "bmp")
			{
				bitmap bmp(fileName.c_str());
				if (bmp.bmih.biBitCount != 32 && bmp.bmih.biBitCount != 24) throw std::runtime_error("unsupported bit count");
				int nColors = bmp.bmih.biBitCount / 8;
				int width = bmp.bmih.biWidth;
				int height = bmp.bmih.biHeight;
				int padding = (4 - ((width * nColors) % 4)) % 4;
				Image<uint16_t, 3> result({ nColors, width, height });
				int x = 0;
				auto i = bmp.data.begin();
				Image<uint16_t, 3> flipped = result({ -1,-1,{ 0,-1,height - 1 } });
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
				return result;
			}
		}

		template<class T, int DIM>
		Image<T, DIM> LoadRaw(std::string rawFileName, std::array<int, DIM> extent, int offsetBytes)
		{
			Image result(extent);
			std::ifstream is;
			is.open(rawFileName, std::ios::binary);
			is.seekg(offsetBytes, std::ios::beg);
			int size = width * height * depth;
			is.read((char*)result.DataArray, size * sizeof(T));
			is.close();
			return result;
		}

		template<class T, int DIM>
		void SaveRaw(Image<T, DIM> source, std::string rawFileName)
		{
			std::cout << "saving output file: " << rawFileName << std::endl;
			std::ofstream ofile(rawFileName, std::ios::binary);
			for (auto it = source.begin(); it != source.end(); ++it) ofile.write((char*)it.Pointer(), sizeof(T));
			ofile.close();
		}

		template<class T> Image<T, 3>
		LoadDicom(std::string filename, bool openAllInFolder = false) 
		{
			dicom image;
			std::replace(filename.begin(), filename.end(), '/', '\\'); // replace all 'x' to 'y'
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
			std::array<int, 3> geo;
			geo[0] = image.width();
			geo[1] = image.height();
			geo[2] = image.number_of_frames();
			if (geo[2] == 0) geo[2] = 1;
			std::cout << "width: " << image.width() << std::endl;
			std::cout << "height: " << image.height() << std::endl;
			std::cout << "number_of_frames: " << image.number_of_frames() << std::endl;

			//get rescale / intercept values
			auto intercept = image.rescale_intercept();
			auto slope = image.rescale_slope();
			std::cout << "rescale_intercept: " << image.rescale_intercept() << std::endl;
			std::cout << "rescale_slope: " << image.rescale_slope() << std::endl;

			//if (geo[2] <= 1 && geo[0] && geo[1]) geo[2] = image.image_size / geo[0] / geo[1] / (image.get_bit_count() / 8);
			//ROWS * COLUMNS * NUMBER_OF_FRAMES *	SAMPLES_PER_PIXEL * (BITS_ALLOCATED / 8)
			Image<T, 3> result(geo);
			T* ptr = result.data();
			long pixel_count = result.size();
			if (sizeof(T) == image.get_bit_count() / 8) image.input_io->read((char*)ptr, pixel_count * sizeof(T));
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
		void Save(Image<T, DIM>& image, std::string fileName)
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
			Image<T, DIM> temp(image);

			//handle each filetype
			if (extension == "nrrd")
			{
				NRRD::save(fileName, temp.data(), DIM, image.Extent.data());
			} 
			else if (extension == "bmp")
			{
				bitmap bmp;
				if (DIM == 3) 
				{
					bmp.bmih.biWidth = image.Extent[1];
					bmp.bmih.biHeight = image.Extent[2];
					if (image.Extent[0] == 3) bmp.bmih.biBitCount = 24;
					else if (image.Extent[0] == 4) bmp.bmih.biBitCount = 32;
					else if (image.Extent[0] == 1) bmp.bmih.biBitCount = 8;
					else throw std::runtime_error("the first dimensions for 3D represents color, an unsupported number of colors was selected");
					auto flipped = image({ -1,-1,{ 0,-1,(int)bmp.bmih.biHeight - 1 } });
					int padding = (4 - ((bmp.bmih.biWidth * image.Extent[0]) % 4)) % 4;
					int x = 0;
					bmp.data = std::vector<unsigned char>((bmp.bmih.biWidth + padding) * bmp.bmih.biHeight * (image.Extent[0]));
					auto i = bmp.data.begin();
					for (auto r = flipped.begin(); r != flipped.end(); ++r)
					{
						*i = *r;
						++i;
						++x;
						if (x >= bmp.bmih.biWidth * image.Extent[0])
						{
							x = 0;
							i += padding;
							continue;
						}
					}
				}
				else if (DIM == 2)
				{
					bmp.bmih.biWidth = image.Extent[0];
					bmp.bmih.biHeight = image.Extent[1];
					bmp.bmih.biBitCount = 8;
					auto flipped = image({ -1,{ 0,-1,(int)bmp.bmih.biHeight - 1 } });
					int padding = (4 - (bmp.bmih.biWidth % 4)) % 4;
					int x = 0;
					bmp.data = std::vector<unsigned char>((bmp.bmih.biWidth + padding) * bmp.bmih.biHeight * (bmp.bmih.biBitCount / 8));
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
			else 
			{
				std::cerr << "couldn't save: " << fileName << "\n";
			}
		}

		template<class T, int DIM>
		void Load(Image<T, DIM>& image, std::string fileName)
		{
			NRRD::load(fileName, image.data(), DIM, &image.Extent.data());
		}
	}
}