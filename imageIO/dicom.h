#pragma once
// Copyright Fang-Cheng Yeh 2010
// Distributed under the BSD License
//
/*
Copyright (c) 2010, Fang-Cheng Yeh
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <iomanip>
#include <map>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <algorithm>
#include <memory>
#include <locale>
#include "../utility.h"
//---------------------------------------------------------------------------
namespace ndl
{
	enum transfer_syntax_type { lee, bee, lei };
	//---------------------------------------------------------------------------
	const char dicom_long_flag[] = "OBUNOWSQ";
	const char dicom_short_flag[] = "AEASATCSDADSDTFLFDISLOLTPNSHSLSSSTTMUIULUS";
	//---------------------------------------------------------------------------
	class dicom_group_element
	{
	public:
		union
		{
			char gel[8];
			struct
			{
				unsigned short group;
				unsigned short element;
				union
				{
					unsigned int length;
					struct
					{
						union
						{
							unsigned short vr;
							struct
							{
								char lt0;
								char lt1;
							};
						};
						union
						{
							unsigned short new_length;
							struct
							{
								char lt2;
								char lt3;
							};
						};
					};
				};
			};
		};
		std::vector<unsigned char> data;
	private:
		void assign(const dicom_group_element& rhs)
		{
			std::copy(rhs.gel, rhs.gel + 8, gel);
			data = rhs.data;
		}
		bool flag_contains(const char* flag, unsigned int flag_size)
		{
			for (unsigned int index = 0; index < flag_size; ++index)
			{
				char lb = flag[index << 1];
				char hb = flag[(index << 1) + 1];
				if (lt0 == lb && lt1 == hb)
					return true;
			}
			return false;
		}

	public:
		dicom_group_element(void) {}
		dicom_group_element(const dicom_group_element& rhs)
		{
			assign(rhs);
		}
		const dicom_group_element& operator=(const dicom_group_element& rhs)
		{
			assign(rhs);
			return *this;
		}

		bool read(std::ifstream& in, transfer_syntax_type transfer_syntax)
		{
			if (!in.read(gel, 8))
				return false;
			if (transfer_syntax == bee)
			{
				if (group == 0x0002)
					transfer_syntax = lee;
				else
				{
					change_endian(group);
					change_endian(element);
				}
			}
			unsigned int read_length = length;
			if (flag_contains(dicom_long_flag, 4))
			{
				if (!in.read((char*)&read_length, 4))
					return false;
				if (transfer_syntax == bee)
					change_endian(read_length);
			}
			else
				if (flag_contains(dicom_short_flag, 21))
				{
					if (transfer_syntax == bee)
						change_endian(new_length);
					read_length = new_length;
				}
			if (read_length == 0xFFFFFFFF)
				read_length = 0;
			if (read_length)
			{
				if (group == 0x7FE0 && element == 0x0010)
				{
					length = read_length;
					return false;
				}
				data.resize(read_length);
				in.read((char*)&*(data.begin()), read_length);
				if (transfer_syntax == bee)
				{
					if (is_float()) // float
						change_endian((float*)&*data.begin(), data.size() / sizeof(float));
					if (is_double()) // double
						change_endian((double*)&*data.begin(), data.size() / sizeof(double));
					if (is_int16()) // uint16type
						change_endian((short*)&*data.begin(), data.size() / sizeof(short));
					if (is_int32() && data.size() >= 4)
						change_endian((int*)&*data.begin(), data.size() / sizeof(int));
				}
			}
			return !(!in);
		}

		unsigned int get_order(void) const
		{
			unsigned int order = group;
			order <<= 16;
			order |= element;
			return order;
		}
		const std::vector<unsigned char>& get(void) const
		{
			return data;
		}
		unsigned short get_vr(void) const
		{
			return vr;
		}

		bool is_string(void) const
		{
			return (lt0 == 'D' ||  // DA DS DT
				lt0 == 'P' ||  // PN
				lt0 == 'T' ||  // TM
				lt0 == 'L' ||  // LO LT
				lt1 == 'I' ||  // UI
				lt1 == 'H' ||  // SH
				(lt0 != 'A' && lt1 == 'T') || // ST UT LT
				(lt0 == 'A' && lt1 == 'E') || // AE
				((lt0 == 'A' || lt0 == 'C' || lt0 == 'I') && lt1 == 'S'));//AS CS IS
		}
		bool is_int16(void) const
		{
			return (lt0 == 'A' && lt1 == 'T') ||
				(lt0 == 'O' && lt1 == 'W') ||
				(lt0 == 'S' && lt1 == 'S') ||
				(lt0 == 'U' && lt1 == 'S');
		}
		bool is_int32(void) const
		{
			return (lt0 == 'S' && lt1 == 'L') ||
				(lt0 == 'U' && lt1 == 'L');
		}
		bool is_float(void) const
		{
			//FL
			return (lt0 == 'F' && lt1 == 'L') || (lt0 == 'O' && lt1 == 'F');
		}
		bool is_double(void) const
		{
			//FD
			return (lt0 == 'F' && lt1 == 'D');
		}

		template<typename value_type>
		void get_value(value_type& value) const
		{
			if (data.empty())
				return;
			if (is_float() && data.size() >= 4) // float
			{
				value = value_type(*(const float*)&*data.begin());
				return;
			}
			if (is_double() && data.size() >= 8) // double
			{
				value = value_type(*(const double*)&*data.begin());
				return;
			}
			if (is_int16() && data.size() >= 2) // uint16type
			{
				value = value_type(*(const short*)&*data.begin());
				return;
			}
			if (is_int32() && data.size() >= 4)
			{
				value = value_type(*(const int*)&*data.begin());
				return;
			}
			bool is_ascii = true;
			if (!is_string())
				for (unsigned int index = 0;index < data.size() && (data[index] || index <= 2);++index)
					if (!::isprint(data[index]))
					{
						is_ascii = false;
						break;
					}
			if (is_ascii)
			{
				std::string str(data.begin(), data.end());
				str.push_back(0);
				std::istringstream in(str);
				in >> value;
				return;
			}
			if (data.size() == 2) // uint16type
			{
				value = value_type(*(const short*)&*data.begin());
				return;
			}
			if (data.size() == 4)
			{
				value = value_type(*(const int*)&*data.begin());
				return;
			}
			if (data.size() == 8)
			{
				value = value_type(*(const double*)&*data.begin());
				return;
			}
		}

		template<typename stream_type>
		void operator>> (stream_type& out) const
		{
			if (data.empty())
			{
				out << "(null)";
				return;
			}
			if (is_float() && data.size() >= 4) // float
			{
				const float* iter = (const float*)&*data.begin();
				for (unsigned int index = 3;index < data.size();index += 4, ++iter)
					out << *iter << " ";
				return;
			}
			if (is_double() && data.size() >= 8) // double
			{
				const double* iter = (const double*)&*data.begin();
				for (unsigned int index = 7;index < data.size();index += 8, ++iter)
					out << *iter << " ";
				return;
			}
			if (is_int16() && data.size() >= 2)
			{
				for (unsigned int index = 1;index < data.size();index += 2)
					out << *(const short*)&*(data.begin() + index - 1) << " ";
				return;
			}
			if (is_int32() && data.size() == 4)
			{
				for (unsigned int index = 3;index < data.size();index += 4)
					out << *(const int*)&*(data.begin() + index - 3) << " ";
				return;
			}
			bool is_ascii = true;
			if (!is_string()) // String
				for (unsigned int index = 0;index < data.size() && (data[index] || index <= 2);++index)
					if (!::isprint(data[index]))
					{
						is_ascii = false;
						break;
					}
			if (is_ascii)
			{
				for (unsigned int index = 0;index < data.size();++index)
				{
					char ch = data[index];
					if (!ch)
						break;
					out << ch;
				}
				return;
			}
			out << data.size() << " bytes";
			if (data.size() == 8)
				out << ", double=" << *(double*)&*data.begin() << " ";
			if (data.size() == 4)
				out << ", int=" << *(int*)&*data.begin() << ", float=" << *(float*)&*data.begin() << " ";
			if (data.size() == 2)
				out << ", short=" << *(short*)&*data.begin() << " ";
			return;
		}

	};

	class dicom
	{
	private:
		std::map<unsigned int, unsigned int> ge_map;
		std::vector<dicom_group_element*> data;
		void assign(const dicom& rhs)
		{
			ge_map = rhs.ge_map;
			for (unsigned int index = 0;index < rhs.data.size();index++) data.push_back(new dicom_group_element(*rhs.data[index]));
		}
	public:
		unsigned int image_size;
		std::auto_ptr<std::ifstream> input_io;
		transfer_syntax_type transfer_syntax;

		dicom(void) :transfer_syntax(lee) {}
		dicom(const dicom& rhs)
		{
			assign(rhs);
		}
		const dicom& operator=(const dicom& rhs)
		{
			assign(rhs);
			return *this;
		}
		~dicom(void)
		{
			for (unsigned int index = 0; index < data.size(); ++index) delete data[index];
		}
	public:
		bool load_from_file(const std::string& file_name)
		{
			return load_from_file(file_name.c_str());
		}
		template<typename char_type> bool load_from_file(const char_type* file_name)
		{
			ge_map.clear();
			data.clear();
			input_io.reset(new std::ifstream(file_name, std::ios::binary));
			if (!(*input_io)) return false;
			input_io->seekg(128);
			unsigned int dicom_mark = 0;
			input_io->read((char*)&dicom_mark, 4);
			if (dicom_mark != 0x4d434944) //DICM
			{
				// switch to another DICOM format
				input_io->seekg(0, std::ios::beg);
				input_io->read((char*)&dicom_mark, 4);
				if (dicom_mark != 0x00050008 && dicom_mark != 0x00000008) return false;
				input_io->seekg(0, std::ios::beg);
			}
			while (*input_io)
			{
				std::auto_ptr<dicom_group_element> ge(new dicom_group_element);
				if (!ge->read(*input_io, transfer_syntax))
				{
					if (!(*input_io)) return false;
					image_size = ge->length;
					std::string image_type;
					return true;
				}
				// detect transfer syntax at 0x0002,0x0010
				if (ge->group == 0x0002 && ge->element == 0x0010)
				{
					if (std::string((char*)&*ge->data.begin()) == std::string("1.2.840.10008.1.2")) transfer_syntax = lei;//Little Endian Implicit
					if (std::string((char*)&*ge->data.begin()) == std::string("1.2.840.10008.1.2.1")) transfer_syntax = lee;//Little Endian Explicit
					if (std::string((char*)&*ge->data.begin()) == std::string("1.2.840.10008.1.2.2")) transfer_syntax = bee;//Big Endian Explicit
				}
				ge_map[ge->get_order()] = data.size();
				data.push_back(ge.release());
			}
			return false;
		}
		const unsigned char* get_data(unsigned short group, unsigned short element, unsigned int& length) const
		{
			std::map<unsigned int, unsigned int>::const_iterator iter =
				ge_map.find(((unsigned int)group << 16) | (unsigned int)element);
			if (iter == ge_map.end())
			{
				length = 0;
				return 0;
			}
			length = (unsigned int)data[iter->second]->get().size();
			if (!length)
				return 0;
			return (const unsigned char*)&*data[iter->second]->get().begin();
		}
		bool get_text(unsigned short group, unsigned short element, std::string& result) const
		{
			unsigned int length = 0;
			const char* text = (const char*)get_data(group, element, length);
			if (!text)
				return false;
			result = std::string(text, text + length);
			return true;
		}
		template<typename value_type>
		bool get_value(unsigned short group, unsigned short element, value_type& value) const
		{
			std::map<unsigned int, unsigned int>::const_iterator iter =
				ge_map.find(((unsigned int)group << 16) | (unsigned int)element);
			if (iter == ge_map.end())
				return false;
			data[iter->second]->get_value(value);
			return true;
		}
		unsigned int get_int(unsigned short group, unsigned short element) const
		{
			unsigned int value = 0;
			get_value(group, element, value);
			return value;
		}
		float get_float(unsigned short group, unsigned short element) const
		{
			float value = 0.0;
			get_value(group, element, value);
			return value;
		}
		double get_double(unsigned short group, unsigned short element) const
		{
			double value = 0.0;
			get_value(group, element, value);
			return value;
		}
		template<typename voxel_size_type>
		void get_voxel_size(voxel_size_type voxel_size) const
		{
			std::string slice_dis;
			if (get_text(0x0018, 0x0088, slice_dis) || get_text(0x0018, 0x0050, slice_dis))
				std::istringstream(slice_dis) >> voxel_size[2];
			else
				voxel_size[2] = 1.0;

			std::string pixel_spacing;
			if (get_text(0x0028, 0x0030, pixel_spacing))
			{
				std::replace(pixel_spacing.begin(), pixel_spacing.end(), '\\', ' ');
				std::istringstream(pixel_spacing) >> voxel_size[0] >> voxel_size[1];
			}
			else
				voxel_size[0] = voxel_size[1] = voxel_size[2];
		}

		/**
		The DICOM attribute (0020,0037) "Image Orientation (Patient)" gives the
		orientation of the x- and y-axes of the image data in terms of 2 3-vectors.
		The first vector is a unit vector along the x-axis, and the second is
		along the y-axis.
		*/
		template<typename vector_type> void get_image_row_orientation(vector_type image_row_orientation) const
		{
			//float image_row_orientation[3];
			std::string image_orientation;
			if (!get_text(0x0020, 0x0037, image_orientation) &&
				!get_text(0x0020, 0x0035, image_orientation))
				return;
			std::replace(image_orientation.begin(), image_orientation.end(), '\\', ' ');
			std::istringstream(image_orientation)
				>> image_row_orientation[0]
				>> image_row_orientation[1]
				>> image_row_orientation[2];
		}
		template<typename vector_type> void get_image_col_orientation(vector_type image_col_orientation) const
		{
			//float image_col_orientation[3];
			float temp;
			std::string image_orientation;
			if (!get_text(0x0020, 0x0037, image_orientation) &&
				!get_text(0x0020, 0x0035, image_orientation))
				return;
			std::replace(image_orientation.begin(), image_orientation.end(), '\\', ' ');
			std::istringstream(image_orientation)
				>> temp >> temp >> temp
				>> image_col_orientation[0]
				>> image_col_orientation[1]
				>> image_col_orientation[2];
		}
		template<typename vector_type> void get_image_orientation(vector_type orientation_matrix) const
		{
			get_image_row_orientation(orientation_matrix);
			get_image_col_orientation(orientation_matrix + 3);
			// get the slice direction
			orientation_matrix[6] =
				(orientation_matrix[1] * orientation_matrix[5]) -
				(orientation_matrix[2] * orientation_matrix[4]);
			orientation_matrix[7] =
				(orientation_matrix[2] * orientation_matrix[3]) -
				(orientation_matrix[0] * orientation_matrix[5]);
			orientation_matrix[8] =
				(orientation_matrix[0] * orientation_matrix[4]) -
				(orientation_matrix[1] * orientation_matrix[3]);

			// the slice ordering is always increamental
			if (orientation_matrix[6] + orientation_matrix[7] + orientation_matrix[8] < 0) // no flip needed
			{
				orientation_matrix[6] = -orientation_matrix[6];
				orientation_matrix[7] = -orientation_matrix[7];
				orientation_matrix[8] = -orientation_matrix[8];
			}
		}
		float get_slice_location(void) const
		{
			std::string slice_location;
			if (!get_text(0x0020, 0x1041, slice_location))
				return 0.0;
			float data;
			std::istringstream(slice_location) >> data;
			return data;
		}
		void get_patient(std::string& info)
		{
			std::string date, gender, age, id;
			date = gender = age = id = "_";
			get_text(0x0008, 0x0022, date);
			get_text(0x0010, 0x0040, gender);
			get_text(0x0010, 0x1010, age);
			get_text(0x0010, 0x0010, id);
			using namespace std;
			gender.erase(remove(gender.begin(), gender.end(), ' '), gender.end());
			id.erase(remove(id.begin(), id.end(), ' '), id.end());
			std::replace(id.begin(), id.end(), '-', '_');
			std::replace(id.begin(), id.end(), '/', '_');
			info = date;
			info += "_";
			info += gender;
			info += age;
			info += "_";
			info += id;
		}
		void get_sequence_id(std::string& seq)
		{
			get_text(0x0008, 0x103E, seq);
			using namespace std;
			seq.erase(remove(seq.begin(), seq.end(), ' '), seq.end());
			std::replace(seq.begin(), seq.end(), '-', '_');
		}
		void get_sequence(std::string& info)
		{
			std::string series_num, series_des;
			series_num = series_des = "_";
			get_text(0x0020, 0x0011, series_num);
			get_sequence_id(series_des);
			using namespace std;
			series_num.erase(remove(series_num.begin(), series_num.end(), ' '), series_num.end());
			if (series_num.size() == 1)
			{
				info = std::string("0");
				info += series_num;
			}
			else
				info = series_num;

			info += "_";
			info += series_des;
		}
		std::string get_image_num(void)
		{
			std::string image_num;
			get_text(0x0020, 0x0013, image_num);
			using namespace std;
			if (!image_num.empty())
				image_num.erase(remove(image_num.begin(), image_num.end(), ' '), image_num.end());
			return image_num;
		}
		void get_image_name(std::string& info)
		{
			std::string series_des;
			series_des = "_";
			get_sequence_id(series_des);
			info = series_des;
			info += "_i";
			info += get_image_num();
			info += ".dcm";
		}
		unsigned int width(void) const
		{
			return get_int(0x0028, 0x0011);
		}
		unsigned int height(void) const
		{
			return get_int(0x0028, 0x0010);
		}
		unsigned int frame_num(void) const
		{
			return get_int(0x0028, 0x0008);
		}
		unsigned int get_bit_count(void) const
		{
			return get_int(0x0028, 0x0100);
		}
		unsigned int get_samples_per_pixel(void) const
		{
			return get_int(0x0028, 0x0002);
		}
		unsigned int pixel_representation(void) const
		{
			return get_int(0x0028, 0x0103);
		}
		unsigned int number_of_frames(void) const
		{
			return get_int(0x0028, 0x0008);
		}
		double rescale_intercept(void) const
		{
			return get_double(0x0028, 0x1052);
		}
		double rescale_slope(void) const
		{
			return get_double(0x0028, 0x1053);
		}
		const dicom& operator>>(std::string& report) const
		{
			std::ostringstream out;
			std::map<unsigned int, unsigned int>::const_iterator iter = ge_map.begin();
			std::map<unsigned int, unsigned int>::const_iterator end = ge_map.end();
			for (;iter != end;++iter)
			{
				out << std::setw(8) << std::setfill('0') << std::hex << std::uppercase <<
					iter->first << "=";
				out << std::dec;
				if (data[iter->second]->data.empty())
				{
					out << std::setw(8) << std::setfill('0') << std::hex << std::uppercase <<
						data[iter->second]->length << " ";
					out << std::dec;
				}
				else
				{
					unsigned short vr = data[iter->second]->vr;
					if ((vr & 0xFF) && (vr >> 8))
						out << (char)(vr & 0xFF) << (char)(vr >> 8) << " ";
					else
						out << "   ";
					*(data[iter->second]) >> out;
				}
				out << std::endl;
			}
			report = out.str();
			return *this;
		}
	};
}
