#pragma once
#include <stdexcept>
#include <fstream>
#include <vector>
namespace ndl
{

struct bitmap_file_header
{
    unsigned int bfSize;
    bool read(std::istream& in)
    {
        unsigned short bfType;
        in.read((char*)&bfType,2);
        if (bfType !=  0x4D42) // BM
            return false;
        in.read((char*)&bfSize,4);
        unsigned int dummy;
        in.read((char*)&dummy,4);
        in.read((char*)&dummy,4);
        return in && dummy== 54;//bfOffBits
    }
    bool write(std::ostream& out) const
    {
        unsigned short bfType = 0x4D42;
        out.write((const char*)&bfType,2);
        out.write((const char*)&bfSize,4);
        unsigned int dummy = 0;
        out.write((const char*)&dummy,4);
        dummy = 54;
        out.write((const char*)&dummy,4);//bfOffBits
        return !(!out);
    }
};


struct bitmap_info_header
{
    unsigned int biSize;
    unsigned int biWidth;
    unsigned int biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned int biCompression;
    unsigned int biSizeImage;
    unsigned int biXPelsPerMeter;
    unsigned int biYPelsPerMeter;
    unsigned int biClrUsed;
    unsigned int biClrImportant;
};
class bitmap
{
private:
    bitmap_file_header bmfh;
public:
	bitmap_info_header bmih;
	std::vector<unsigned char> data;
	bitmap(void)
    {
        std::fill((char*)&bmih,(char*)&bmih+sizeof(bmih),0);
        bmih.biSize = sizeof(bitmap_info_header);
    }
    template<typename char_type>
    bitmap(const char_type* file_name)
    {
        if (!load_from_file(file_name))
            throw std::runtime_error("failed to open bitmap file");
    }
    template<typename char_type>
    bool save_to_file(const char_type* file_name)
    {
        std::ofstream out(file_name,std::ios::binary);
        if (!bmfh.write(out))
            return false;
        out.write((const char*)&bmih,sizeof(bitmap_info_header));
        out.write((const char*)&*data.begin(),data.size());
        return true;
    }
    template<typename char_type>
    bool load_from_file(const char_type* file_name)
    {
        std::ifstream in(file_name,std::ios::binary);
		if (!in || !bmfh.read(in)) return false;
        in.read((char*)&bmih,sizeof(bitmap_info_header));
        if (!in || bmih.biWidth <= 0 || bmih.biHeight <= 0 || bmih.biCompression != 0) return false;
        try
        {
			auto pos = in.tellg();
			in.seekg(0, std::ios::end);
			auto fsize = in.tellg() - pos;
			in.seekg(pos, std::ios::beg);
			if (bmih.biSizeImage == 0) bmih.biSizeImage = fsize;
			
			data.resize(bmih.biSizeImage);
		}
        catch (...)
        {
            return false;
        }
        in.read((char*)&*data.begin(),data.size());
        if (!in)
            return false;
        return true;
    }
};


}
