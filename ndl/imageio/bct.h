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


namespace ndl {

#include <bctlib.h>

/*!
Breast CT Image Format Plugin.
To use this plugin, you must type
#define NDL_USE_BCTLIB
before you include ndl.h in your
project. Also, you must
have bctlib.h in your include path.
There are no library dependancies to
use this plugin
*/

template<class VoxelType,int DIM>
class BctPlugin {
public:
    static std::string imagetypename(){ return "bct"; }
    static std::string imageopenextension(){ return ""; }
    static std::string imagesaveextension(){ return "0001"; }
    static bool isfilecompatible(std::string filename,std::string flags,bool sequenceflag=false,bool exactflag=false){
        if (exactflag && getdatatype<VoxelType>()!=G_FLOAT) return false;
        if (DIM<2 || (DIM<3 && sequenceflag)) return false;
        if (fileexists(filename)){
            return isbctfile(filename.c_str());
        }
        return false;
    }
    
    static bool loadfile(Image<VoxelType,DIM>& im,std::vector<std::string>& sequence,bool exactmatchonly,std::string flags,void* headerpntr){
        //check if valid dimensions and type
        int numinsequence = sequence.size();
        int difftype = (getdatatype<VoxelType>()!=G_FLOAT);
        if (exactmatchonly && (difftype)) return false;
        if (DIM<2 || (DIM<3 && numinsequence>1)) return false;
        
        //read the file information and setup the image
        bctheaderinfo* info;
        bctheaderinfo defaultheader;
        if (headerpntr) info = static_cast<bctheaderinfo*>(headerpntr);
        else info = &defaultheader;
        if (!readbctheader(sequence[0].c_str(),info)){
            printf("ERROR READING HEADER FOR '%s'\n",sequence[0].c_str());
            return false;
        }
        im.m_dimarray[0]=info->nxct; 
        im.m_dimarray[1]=info->nyct;
        im.m_voxelsize[0]=info->ctpixel_mm;
        im.m_voxelsize[1]=info->ctpixel_mm;
        im.m_units[0]="mm";
        im.m_units[1]="mm";
        if (DIM>2){
            im.m_dimarray[2]=numinsequence;
            im.m_voxelsize[2]=info->spacebetweenslices;
            im.m_units[2]="mm";
        }
        im.setupdimensions(im.m_dimarray,1); 
        im.setupdata();
        
        //load the image data into the image
        int imagesize = im.m_dimarray[0]*im.m_dimarray[1];
        float* datapntr;
        datapntr = new float[imagesize];
        char text[1000];
        for (unsigned int i = 0;i < numinsequence;i++) {
            sprintf(text,"loading %s: %s",imagetypename().c_str(),printfilename(sequence[i]).c_str());
            im.updateprogress(text,100*i/numinsequence);
            int offset = i*imagesize;
            if (!readbctslice(sequence[i].c_str(),info,datapntr)){
                std::cout << "Error opening file: " << sequence[i] << std::endl;
                delete [] datapntr;
                return false;
            }
            for (int t=0;t<imagesize;t++) im.m_data[offset+t]=datapntr[t];
        }
        im.updateprogress(text,100);
        delete [] datapntr;
            
        //remember the filetype
        im.m_filetype = imagetypename();
        return true;
    }
    static bool savefile(Image<VoxelType,DIM>& im,std::vector<std::string>& sequence,bool exactmatchonly,std::string flags,void* headerpntr){
        //check if valid dimensions and type
        int numinsequence = sequence.size();
        if (DIM<2 || (DIM<3 && numinsequence>1)) return false;
        int difftype = (getdatatype<VoxelType>()!=G_FLOAT);
        if (exactmatchonly && difftype) return false;
        
        //set the file information from the image
        bctheaderinfo* info;
        bctheaderinfo defaultheader;
        if (headerpntr) info = static_cast<bctheaderinfo*>(headerpntr);
        else info = &defaultheader;
        info->nxct = im.m_dimarray[0];
        info->nyct = im.m_dimarray[1];
        info->ctpixel_mm = im.m_voxelsize[0];
        info->xim = numinsequence;
        info->zzz1 = 0;
        info->zzz2 = 0;
        if (DIM>=3){
            info->spacebetweenslices = im.m_voxelsize[2];
            info->zzz1 = info->spacebetweenslices/2;//im.m_worldposition[2] - im.m_voxelsize[0]*im.m_dimarray[2]/2.0;
            info->zzz2 = info->zzz1 + info->spacebetweenslices*(info->xim-1);
        }
        
        //save the image data
        int imagesize = im.m_dimarray[0]*im.m_dimarray[1];
        float* datapntr;
        datapntr = new float[imagesize];
        char text[1000];
        for (unsigned int i = 0;i < numinsequence;i++){
            sprintf(text,"saving %s: %s",imagetypename().c_str(),printfilename(sequence[i]).c_str());

            //update header stuff for this image
            info->islices=i+1;
            float p = float(i-1)/float(numinsequence-1);
            info->zzz=info->zzz1*(1-p) + info->zzz2*p;
            
            //save
            im.updateprogress(text,100*i/numinsequence);
            int offset = i*imagesize*im.m_numcolors;
            for (int t=0;t<imagesize;t++){
                datapntr[t] = im.m_data[offset+t];
                t+=im.m_numcolors-1;
            }
            if (!writebctslice(sequence[i].c_str(),info,datapntr)){
                std::cout << "Error saving file: " << sequence[i] << std::endl;
                delete [] datapntr;
                return false;
            }
        }
        im.updateprogress(text,100);
        
        //save the filetype
        im.m_filetype = imagetypename();
        return true;
    }
    
    static bool checkfilesequence(std::string filename,std::vector<std::string>& sequence,std::string flags){
        bctheaderinfo info;
        readbctheader(filename.c_str(),&info);
        std::vector< std::pair<std::string,int> > labelsizelist;
        for(int i=0;i<sequence.size();i++){
            bctheaderinfo tinfo;
            bool isvalid = true;
            if (isbctfile(sequence[i].c_str())){
                readbctheader(sequence[i].c_str(),&tinfo);
                if (tinfo.series != info.series) isvalid=false;
                if (tinfo.scanid != info.scanid) isvalid=false;
            } else isvalid=false;
            if (isvalid) labelsizelist.push_back( std::pair<std::string,int>(sequence[i],tinfo.xim) );
        }
        
        //reorder images
        std::sort(labelsizelist.begin(), labelsizelist.end(), less_second<std::pair<std::string,int>>());
        
        //rebuild sorted sequence
        sequence.clear();
        for(int i=0;i<labelsizelist.size();i++) sequence.push_back(labelsizelist[i].first);
        limitfilesequence(sequence,flags);
        return true;
    }
};

}