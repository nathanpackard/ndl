#pragma once
#include <utility>
namespace ImageLib
{
	template<typename type>
	void change_endian(type& value)
	{
		type data = value;
		unsigned char temp[sizeof(type)];
		unsigned char* pdata = ((unsigned char*)&data) + sizeof(type) - 1;
		for (char i = 0; i < sizeof(type); ++i, --pdata)
			temp[i] = *pdata;
		value = *(type*)temp;
	}
	inline void change_endian(unsigned short& data)
	{
		unsigned char* h = (unsigned char*)&data;
		std::swap(*h, *(h + 1));
	}

	inline void change_endian(short& data)
	{
		unsigned char* h = (unsigned char*)&data;
		std::swap(*h, *(h + 1));
	}


	inline void change_endian(unsigned int& data)
	{
		unsigned char* h = (unsigned char*)&data;
		std::swap(*h, *(h + 3));
		std::swap(*(h + 1), *(h + 2));
	}

	inline void change_endian(int& data)
	{
		unsigned char* h = (unsigned char*)&data;
		std::swap(*h, *(h + 3));
		std::swap(*(h + 1), *(h + 2));
	}

	inline void change_endian(float& data)
	{
		change_endian(*(int*)&data);
	}

	template<typename datatype, typename size_type>
	void change_endian(datatype* data, size_type count)
	{
		for (unsigned int index = 0; index < count; ++index)
			change_endian(data[index]);
	}
}