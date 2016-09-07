#pragma once
#include "bitmap.h";
#include "jpeg_decoder.h";
#include "avi.h";
#include "dicom.h";
#include <algorithm>

namespace ImageLib
{
	namespace ImageIO {
		Image<uint8_t> LoadJpeg(std::string filename) {
			size_t size;
			uint8_t *buf;
			FILE *f;
			printf("Opening the input file: %s.\n", filename.c_str());
			f = fopen(filename.c_str(), "rb");
			if (!f) {
				printf("Error opening the input file.\n");
				return 1;
			}
			fseek(f, 0, SEEK_END);
			size = ftell(f);
			buf = (unsigned char*)malloc(size);
			fseek(f, 0, SEEK_SET);
			size_t read = fread(buf, 1, size, f);
			fclose(f);
			Jpeg::Decoder decoder(buf, size);
			if (decoder.GetResult() != Jpeg::Decoder::OK)
			{
				printf("Error decoding the input file\n");
				return 1;
			}
			printf("width: %d\r\n", decoder.GetWidth());
			printf("height: %d\r\n", decoder.GetHeight());
			printf("width * height: %d\r\n", decoder.GetWidth() * decoder.GetHeight());
			printf("size: %d\r\n", decoder.GetImageSize());

			Image<uint8_t> im(decoder.GetImage(), decoder.GetWidth() * 3, decoder.GetHeight());
			return Image<uint8_t>(im);
		}
		Image<uint16_t> LoadBmp(std::string filename) {
			bitmap bmp(filename.c_str());
			if (bmp.bmih.biBitCount != 32 && bmp.bmih.biBitCount != 24) throw std::runtime_error("unsupported bit count");
			unsigned int nColors = bmp.bmih.biBitCount / 8;
			unsigned int width = bmp.bmih.biWidth;
			unsigned int height = bmp.bmih.biHeight;
			unsigned int padding = 4 - (width % 4);
			Image<uint16_t> result(width * nColors, height);
			int x = 0;
			auto i = bmp.data.begin();
			for (auto r = result.begin();r != result.end();++r)
			{
				*r = *i;
				++i;
				++x;
				if (x >= width)
				{
					x = 0;
					i += padding;
					continue;
				}
			}
			return result;
		}
		template<class T> Image<T> LoadRaw(std::string rawFileName, int width, int height, int depth, int offsetBytes)
		{
			Image result(width, height, depth);
			std::ifstream is;
			is.open(rawFileName, std::ios::binary);
			is.seekg(offsetBytes, std::ios::beg);
			int size = width * height * depth;
			is.read((char*)result.DataArray, size * sizeof(T));
			is.close();
			return result;
		}
		template<class T> void SaveRaw(Image<T> source, std::string rawFileName)
		{
			std::cout << "saving output file: " << rawFileName << std::endl;
			std::ofstream ofile(rawFileName, std::ios::binary);
			for (auto it = source.begin(); it != source.end(); ++it) ofile.write((char*)it.Pointer(), sizeof(T));
			ofile.close();
		}
		template<class T> Image<T> LoadDicom(std::string filename, bool openAllInFolder = false) {
			dicom image;
			image.load_from_file(filename);

			//parse path
			auto lastSlashIndex = filename.find_last_of("/");
			string dir = filename.substr(0, lastSlashIndex);
			string file = filename.substr(lastSlashIndex + 1, filename.length() - 1);

			//parse file
			string firstPart = file;
			string lastPart = "";
			//...
			
			cout << "filename: " << filename << endl;
			cout << "dir: " << dir << endl;
			cout << "file: " << file << endl;
						
			//get image dimensions
			int geo[3];
			geo[0] = image.width();
			geo[1] = image.height();
			geo[2] = image.number_of_frames();
			if (geo[2] == 0) geo[2] = 1;
			cout << "width: " << image.width() << endl;
			cout << "height: " << image.height() << endl;
			cout << "number_of_frames: " << image.number_of_frames() << endl;

			//get rescale / intercept values
			auto intercept = image.rescale_intercept();
			auto slope = image.rescale_slope();
			cout << "rescale_intercept: " << image.rescale_intercept() << endl;
			cout << "rescale_slope: " << image.rescale_slope() << endl;

			//if (geo[2] <= 1 && geo[0] && geo[1]) geo[2] = image.image_size / geo[0] / geo[1] / (image.get_bit_count() / 8);
			//ROWS * COLUMNS * NUMBER_OF_FRAMES *	SAMPLES_PER_PIXEL * (BITS_ALLOCATED / 8)
			Image<T> result(geo[0], geo[1], geo[2]);
			T* ptr = result.Data();
			long pixel_count = result.size();
			if (sizeof(T) == image.get_bit_count() / 8) image.input_io->read((char*)ptr, pixel_count*sizeof(T));
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
	}
}