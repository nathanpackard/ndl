// Copyright (C) 2009   Nathan Packard   <nathanpackard@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as 
// published by the Free Software Foundation; either version 3 of the 
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public 
// License along with this program; if not, see 
// <http://www.gnu.org/licenses/>.

#include <EasyBMP.h>

namespace ndl {

/*!
BMP Image Format Plugin.
To use this plugin, you must type
#define NDL_USE_BMP
before you include ndl.h in your
project. Also, you must
have bmplib.h in your include path.
The bmplib library is also 
requried when using this plugin, be sure
to link your project to the following .lib file:
- libbmp.lib
*/
template<class VoxelType,int DIM>
class BmpPlugin {
public:
    static std::string imagetypename(){ return "bmp"; }
    static std::string imageopenextension(){ return "bmp"; }
    static std::string imagesaveextension(){ return "bmp"; }
    static bool isfilecompatible(std::string filename,std::string flags,bool sequenceflag=false,bool exactflag=false){
        //check if valid dimensions
        if (DIM<2 || (DIM<3 && sequenceflag)) return false;
        //check if valid type
        if (exactflag && getdatatype<VoxelType>()!=G_UNSIGNED_CHAR) return false;
        BMP theimage;
        SetEasyBMPwarningsOff();
        return theimage.ReadFromFile(filename.c_str());
    }
    
    static bool loadfile(Image<VoxelType,DIM>& im,std::vector<std::string>& sequence,bool exactmatchonly,std::string flags,void* headerpntr){
        //check if valid dimensions and type
        int numinsequence = sequence.size();
        int difftype = (getdatatype<VoxelType>()!=G_UNSIGNED_CHAR);
        if (exactmatchonly && (difftype)) return false;
        if (DIM<2 || (DIM<3 && numinsequence>1)) return false;
        
        //read the file information and setup the image
        BMP* info;
        BMP defaultheader;
        if (headerpntr) info = static_cast<BMP*>(headerpntr);
        else info = &defaultheader;
        if (!info->ReadFromFile((char*)sequence[0].c_str())){
            printf("ERROR READING HEADER FOR '%s'\n",sequence[0].c_str());
            return false;
        }
        im.m_dimarray[0]=info->TellWidth();
        im.m_dimarray[1]=info->TellHeight();
        if (DIM>2) im.m_dimarray[2]=numinsequence;
        im.setupdimensions(im.m_dimarray,3); 
        im.setupdata();
        
        //load the image data into the image
        int imagesize = im.m_dimarray[0]*im.m_dimarray[1];
        char text[1000];
        for (unsigned int i = 0;i < numinsequence;i++) {
            sprintf(text,"loading %s: %s",imagetypename().c_str(),printfilename(sequence[i]).c_str());
            im.updateprogress(text,100*i/numinsequence);
            int offset = i*imagesize*3;
            
            //if i==0, we've allready read the file, don't do it again
            if (i>0){
                if (!info->ReadFromFile((char*)sequence[i].c_str())){
                    std::cout << "Error opening file: " << sequence[i] << std::endl;
                    return false;
                }
            }

            int c=offset;
            for(int j=0;j<im.m_dimarray[1];j++){
            for(int i=0;i<im.m_dimarray[0];i++){
                im.m_data[c++] = info->GetPixel(i,j).Red;
                im.m_data[c++] = info->GetPixel(i,j).Green;
                im.m_data[c++] = info->GetPixel(i,j).Blue;
            }}
        }
        im.updateprogress(text,100);
        
        //remember the filetype
        im.m_filetype = imagetypename();
        return true;
    }
    
    static bool savefile(Image<VoxelType,DIM>& im,std::vector<std::string>& sequence,bool exactmatchonly,std::string flags,void* headerpntr){
        //check if valid dimensions and type
        int numinsequence = sequence.size();
        if (DIM<2 || (DIM<3 && numinsequence>1)) return false;
        int difftype = (getdatatype<VoxelType>()!=G_UNSIGNED_CHAR);
        if (exactmatchonly && difftype) return false;
        
        //set the file information from the image
        BMP* info;
        BMP defaultheader;
        if (headerpntr) info = static_cast<BMP*>(headerpntr);
        else info = &defaultheader;
        info->SetSize( im.m_dimarray[0], im.m_dimarray[1] );
        info->SetBitDepth( 32 );
        
        //save the image data
        int imagesize = im.m_dimarray[0]*im.m_dimarray[1];
        char text[1000];
        for (unsigned int i = 0;i < numinsequence;i++) {
            sprintf(text,"saving %s: %s",imagetypename().c_str(),printfilename(sequence[i]).c_str());
            im.updateprogress(text,100*i/numinsequence);

            //update header stuff for this image
            //... nothing to do
            
            //save
            int offset = i*imagesize*im.m_numcolors;
            
            //scale/bias the data to fit within the range of 0..255
            double minvalue=im.m_min;
            double maxvalue=im.m_max;
            if (getdatatype<VoxelType>()==G_UNSIGNED_CHAR){ minvalue = 0; maxvalue=255; }
            if (getdatatype<VoxelType>()==G_CHAR){ minvalue = -128; maxvalue=127;}
            double bias = -minvalue;
            double scale = 255.0/(maxvalue-minvalue);
            
            //printf("bias: %f, scale: %f\n",bias,scale);
            //printf("minvalue: %f, maxvalue: %f\n",minvalue,maxvalue);
            
            RGBApixel NewPixel;
            int c=offset;
            for(int j=0;j<im.m_dimarray[1];j++){
            for(int i=0;i<im.m_dimarray[0];i++){
                if (im.m_numcolors==1){
                    float value = (im.m_data[c++]+bias)*scale + 0.5;
                    NewPixel.Red = NewPixel.Green = NewPixel.Blue = CLAMP(value,minvalue,maxvalue);
                    NewPixel.Alpha = 0;
                }
                if (im.m_numcolors==2){
                    float value1 = (im.m_data[c++]+bias)*scale + 0.5;
                    float value2 = (im.m_data[c++]+bias)*scale + 0.5;
                    NewPixel.Red = NewPixel.Green = NewPixel.Blue = CLAMP(value1,minvalue,maxvalue);
                    NewPixel.Alpha = CLAMP(value2,minvalue,maxvalue);
                }
                if (im.m_numcolors==3){
                    float value1 = (im.m_data[c++]+bias)*scale + 0.5;
                    float value2 = (im.m_data[c++]+bias)*scale + 0.5;
                    float value3 = (im.m_data[c++]+bias)*scale + 0.5;
                    NewPixel.Red = CLAMP(value1,minvalue,maxvalue);
                    NewPixel.Green = CLAMP(value2,minvalue,maxvalue);
                    NewPixel.Blue = CLAMP(value3,minvalue,maxvalue);
                    NewPixel.Alpha = 0;
                }
                if (im.m_numcolors>=4){
                    float value1 = (im.m_data[c++]+bias)*scale + 0.5;
                    float value2 = (im.m_data[c++]+bias)*scale + 0.5;
                    float value3 = (im.m_data[c++]+bias)*scale + 0.5;
                    float value4 = (im.m_data[c++]+bias)*scale + 0.5;
                    NewPixel.Red = CLAMP(value1,minvalue,maxvalue);
                    NewPixel.Green = CLAMP(value2,minvalue,maxvalue);
                    NewPixel.Blue = CLAMP(value3,minvalue,maxvalue);
                    NewPixel.Alpha = CLAMP(value4,minvalue,maxvalue);
                    c+=im.m_numcolors-4;
                }
                info->SetPixel(i,j,NewPixel);
            }}
            if (!info->WriteToFile( sequence[i].c_str() )){
                std::cout << "Error saving file: " << sequence[i] << std::endl;
                return false;
            }
        }
        im.updateprogress(text,100);
        
        //save the filetype
        im.m_filetype = imagetypename();
        return true;
    }
    
};

}