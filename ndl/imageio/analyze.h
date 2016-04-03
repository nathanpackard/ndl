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
    
#include <analyzelib.h>

/*!
Analyze Image Format Plugin.
To use this plugin, you must type
#define NDL_USE_ANALYZELIB
before you include ndl.h in your
project. Also, you must
have analyzelib.h in your include path.
There are no library dependancies to
use this plugin
*/
template<class VoxelType,int DIM>
class AnalyzePlugin {
public:
    static std::string imagetypename(){ return "analyze"; }
    static std::string imageopenextension(){ return "hdr"; }
    static std::string imagesaveextension(){ return "hdr"; }
    static bool isfilecompatible(std::string filename,std::string flags,bool sequenceflag=false,bool exactflag=false){
        if (fileexists(filename)){
            if (isanalyzefile(filename.c_str())){
                analyzeheaderinfo defaultheader;
                analyzeheaderinfo* info = &defaultheader;
                if (!readanalyzeheader(filename.c_str(),info)) return false;
                    
                //check if valid dimensions
                int mindim = info->dim[0]-1;
                if (info->dim[4]==1 && mindim > 2){
                    mindim=2;
                    if (info->dim[3]==1 && mindim > 1){
                        mindim=1;
                        if (info->dim[2]==1 && mindim > 0){
                            mindim=0;
                        }
                    }
                }
                if (mindim>=DIM || (mindim+1>=DIM && sequenceflag)) return false;

                //check if valid type
                int difftype = true;
                if (info->_datatype==DT_UNSIGNED_CHAR){
                    if (getdatatype<VoxelType>()==G_UNSIGNED_CHAR) difftype=false;
                } else
                if (info->_datatype==DT_SIGNED_SHORT){
                    if (getdatatype<VoxelType>()==G_SHORT) difftype=false;
                } else
                if (info->_datatype==DT_SIGNED_INT){
                    if (getdatatype<VoxelType>()==G_INT) difftype=false;
                } else
                if (info->_datatype==DT_FLOAT){
                    if (getdatatype<VoxelType>()==G_FLOAT) difftype=false;
                } else
                if (info->_datatype==DT_COMPLEX){
                    if (getdatatype<VoxelType>()==G_FLOAT) difftype=false;
                } else
                if (info->_datatype==DT_DOUBLE){
                    if (getdatatype<VoxelType>()==G_DOUBLE) difftype=false;
                } else
                if (info->_datatype==DT_RGB){
                    if (getdatatype<VoxelType>()==G_UNSIGNED_CHAR) difftype=false;
                } else {
                    //DT_UNKNOWN;
                    printf("unknown type\n");
                    return false;
                }
                if (exactflag && difftype) return false;
                return true;
            } else return false;
        } else return false;
        return false;
    }
    
    
    //~ static bool isfilecompatible(std::string filename,std::string flags,bool sequenceflag=false,bool exactflag=false){
        //~ if (fileexists(filename)){
            //~ return isanalyzefile(filename.c_str());
        //~ }
        //~ return false;
    //~ }
    
    static bool loadfile(Image<VoxelType,DIM>& im,std::vector<std::string>& sequence,bool exactmatchonly,std::string flags,void* headerpntr){
        //read the file information
        analyzeheaderinfo* info;
        analyzeheaderinfo defaultheader;
        if (headerpntr) info = static_cast<analyzeheaderinfo*>(headerpntr);
        else info = &defaultheader;
        if (!readanalyzeheader(sequence[0].c_str(),info)){
            printf("ERROR READING HEADER FOR '%s'\n",sequence[0].c_str());
            return false;
        }
        
        //check if valid dimensions and type
        int numinsequence = sequence.size();
        int difftype = true;
        int ncolors = 1;
        int mindim = info->dim[0]-1;
        if (info->dim[4]==1 && mindim > 2){
            mindim=2;
            if (info->dim[3]==1 && mindim > 1){
                mindim=1;
                if (info->dim[2]==1 && mindim > 0){
                    mindim=0;
                }
            }
        }
        int imagesize = 1;
        for(int i=1;i<=mindim+1;i++) imagesize*=info->dim[i];
        if (info->_datatype==DT_UNSIGNED_CHAR){
            if (getdatatype<VoxelType>()==G_UNSIGNED_CHAR) difftype=false;
        } else
        if (info->_datatype==DT_SIGNED_SHORT){
            if (getdatatype<VoxelType>()==G_SHORT) difftype=false;
        } else
        if (info->_datatype==DT_SIGNED_INT){
            if (getdatatype<VoxelType>()==G_INT) difftype=false;
        } else
        if (info->_datatype==DT_FLOAT){
            if (getdatatype<VoxelType>()==G_FLOAT) difftype=false;
        } else
        if (info->_datatype==DT_COMPLEX){
            ncolors=2;
            if (getdatatype<VoxelType>()==G_FLOAT) difftype=false;
        } else
        if (info->_datatype==DT_DOUBLE){
            if (getdatatype<VoxelType>()==G_DOUBLE) difftype=false;
        } else
        if (info->_datatype==DT_RGB){
            ncolors=3;
            if (getdatatype<VoxelType>()==G_UNSIGNED_CHAR) difftype=false;
        } else {
            //DT_UNKNOWN;
            printf("unknown type\n");
            return false;
        }
        if (exactmatchonly && difftype) return false;
        if (mindim>=DIM || (mindim+1>=DIM && numinsequence>1)) return false;
        
        //setup the image
        im.m_dimarray[0]=info->dim[1];
        im.m_voxelsize[0]=info->pixdim[1];
        im.m_units[0]=info->vox_units;
        if (DIM>1){
            im.m_dimarray[1]=info->dim[2]; 
            im.m_voxelsize[1]=info->pixdim[2];
            im.m_units[1]=info->vox_units;
        }
        if (DIM>2){
            im.m_dimarray[2]=info->dim[3];
            im.m_voxelsize[2]=info->pixdim[3];
            im.m_units[2]=info->vox_units;
        }
        if (DIM>3){
            im.m_dimarray[3]=info->dim[4];
            im.m_voxelsize[3]=info->pixdim[4];
            im.m_units[3]="ms";
        }
        if (numinsequence>1){
            im.m_dimarray[mindim+1]=numinsequence;
            im.m_voxelsize[mindim+1]=1;
            im.m_units[mindim+1]="vols";
        }
        im.setupdimensions(im.m_dimarray,ncolors);
        im.setupdata();
        
        // printf("mindim: %d\n",mindim);
        // printf("imagesize: %d\n",imagesize);
        // printf("ncolors: %d\n",ncolors);
        // printf("numinsequence: %d\n",numinsequence);
        
        //load the image data into the image
        VoxelType* datapntr = new VoxelType[imagesize*ncolors];
        char text[1000];
        for (unsigned int i = 0;i < numinsequence;i++){
            sprintf(text,"loading %s: %s",imagetypename().c_str(),printfilename(sequence[i]).c_str());
            im.updateprogress(text,100*i/numinsequence);
            int offset = i*imagesize*ncolors;
            if (!openanalyzefile(sequence[i].c_str(),datapntr,info)){
                std::cout << "Error opening file: " << sequence[i] << std::endl;
                delete [] datapntr;
                return false;
            }
            for (int t=0;t<imagesize*ncolors;t++) im.m_data[offset+t]=datapntr[t];
        }
        im.updateprogress(text,100);
        delete [] datapntr;
            
        //remember the filetype
        im.m_filetype = imagetypename();
        return true;
    }
    static bool savefile(Image<VoxelType,DIM>& im,std::vector<std::string>& sequence,bool exactmatchonly,std::string flags,void* headerpntr){
        //check if valid dimensions
        int numinsequence = sequence.size();
        if (DIM<2 || (DIM<3 && numinsequence>1)) return false;
            
        //check if valid type
        int difftype = true;
        analyzeheaderinfo* info;
        analyzeheaderinfo defaultheader;
        if (headerpntr) info = static_cast<analyzeheaderinfo*>(headerpntr);
        else info = &defaultheader;
        info->bitpix=sizeof(VoxelType)*8;
        if (im.m_numcolors==1){
            info->_datatype=DT_FLOAT; //default is float
            if (getdatatype<VoxelType>()==G_UNSIGNED_CHAR){
                info->_datatype=DT_UNSIGNED_CHAR;
                difftype = false;
            } else
            if (getdatatype<VoxelType>()==G_SHORT){
                info->_datatype=DT_SIGNED_SHORT;
                difftype = false;
            } else
            if (getdatatype<VoxelType>()==G_INT){
                info->_datatype=DT_SIGNED_INT;
                difftype = false;
            } else
            if (getdatatype<VoxelType>()==G_FLOAT){
                info->_datatype=DT_FLOAT;
                difftype = false;
            } else
            if (getdatatype<VoxelType>()==G_DOUBLE){
                info->_datatype=DT_DOUBLE;
                difftype = false;
            }
        } else
        if (im.m_numcolors==2){
            info->_datatype=DT_COMPLEX; //default is complex (float)
            if (getdatatype<VoxelType>()==G_FLOAT){
                difftype = false;
            }
        } else
        if (im.m_numcolors>=3){
            info->_datatype=DT_RGB; //default is RGB (unsigned char)
            if (getdatatype<VoxelType>()==G_UNSIGNED_CHAR) difftype = false;
        }
        if (exactmatchonly && difftype) return false;
        
        //set the file information from the image
        info->dim[1]=im.m_dimarray[0]; 
        info->pixdim[1]=im.m_voxelsize[0];
        strncpy(info->vox_units,im.m_units[0].c_str(),4);
        if (DIM>1){
            info->dim[2]=im.m_dimarray[1]; 
            info->pixdim[2]=im.m_voxelsize[1];
        } else info->dim[2]=1;
        if (DIM>2){
            info->dim[3]=im.m_dimarray[2];
            info->pixdim[3]=im.m_voxelsize[2];
        } else info->dim[3]=1;
        if (DIM>3){
            info->dim[4]=im.m_dimarray[3];
            info->pixdim[4]=im.m_voxelsize[3];
        } else info->dim[4]=1;
        
        //save the image data
        //find min dimension that is not size 1 (that is also bigger than 2D)
        int mindim = getlastimagedimensionindex(im);
        int imagesize = im.m_numvoxels/im.m_numcolors;
        if (numinsequence>1){
            info->dim[mindim+1]=1;
            imagesize/=im.m_dimarray[mindim];
        }
        int ncolor=im.m_numcolors;
        if (ncolor>3) ncolor=3;
        VoxelType* datapntr = new VoxelType[imagesize*ncolor];
        char text[1000];
        for (unsigned int i = 0;i < numinsequence;i++){
            sprintf(text,"saving %s: %s",imagetypename().c_str(),printfilename(sequence[i]).c_str());
            im.updateprogress(text,100*i/numinsequence);
            //update header stuff for this image
            // ... nothing to do here
            
            //save
            int offset = i*imagesize*im.m_numcolors;
            int c=0;
            for (int t=0;t<imagesize;t++){
                if (ncolor==1){
                    datapntr[t] = im.m_data[offset+c++];
                } else
                if (ncolor==2){
                    datapntr[t*2] = im.m_data[offset+c++];
                    datapntr[t*2+1] = im.m_data[offset+c++];
                } else
                if (ncolor==3){
                    datapntr[t*3] = im.m_data[offset+c++];
                    datapntr[t*3+1] = im.m_data[offset+c++];
                    datapntr[t*3+2] = im.m_data[offset+c++];
                    c+=im.m_numcolors-3;
                }
            }
            if (!writeanalyzeslice(sequence[i].c_str(),info,datapntr)){
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
};

}