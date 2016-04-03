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
/*!
RAW Image Format Plugin.
This plugin is the default and is always enabled. 
There are no additional dependancies to use this plugin
*/

template<class VoxelType,int DIM>
class RawPlugin {
public:
    static std::string imagetypename(){ return "raw"; }
    static std::string imageopenextension(){ return ""; }
    static std::string imagesaveextension(){ return "raw"; }
    static bool isfilecompatible(std::string filename,std::string flags,bool sequenceflag=false,bool exactflag=false){
        int datatype=0;
        int extradim = (int)sequenceflag;
        std::map<std::string,std::string> flaglist;
        getoptionalparams(flags,flaglist);
        std::map<std::string,std::string>::const_iterator itr;
        for(itr = flaglist.begin(); itr != flaglist.end(); ++itr){
            std::string key = (*itr).first;
            std::string value = (*itr).second;
            if (key=="y" || key=="ysize"){
                if (DIM<2+extradim){ return false; }
            } else
            if (key=="z" || key=="zsize"){
                if (DIM<3+extradim){ return false; }
            } else
            if (key=="datatype"){
                datatype = getdatatype(value.c_str());
            } else
            if (key.substr(0,3) == "dim"){
                int curdim = atoi(key.substr(3).c_str());
                if (DIM<curdim+extradim){ printf("Error opening raw: 5\n"); return false; }
            } else
            if (key.substr(0,4) == "size"){
                int curdim = atoi(key.substr(4).c_str());
                if (DIM<curdim+extradim){ printf("Error opening raw: 6\n"); return false; }
            }
        }
        if (exactflag && datatype!=getdatatype<VoxelType>()) return false;
        return (fileexists(filename.c_str()));
    }
    
    
    static bool loadfile(Image<VoxelType,DIM>& im,std::vector<std::string>& sequence,bool exactmatchonly,std::string flags,void* headerpntr){
        int numinsequence = sequence.size();
        
        //setup the image
        im.m_dimarray[0]=1;
        im.m_voxelsize[0]=1;
        im.m_units[0]="mm";
        int biggestdim=0;
        int headersize=0;
        int datatype=0;
        std::map<std::string,std::string> flaglist;
        getoptionalparams(flags,flaglist);
        std::map<std::string,std::string>::const_iterator itr;
        for(itr = flaglist.begin(); itr != flaglist.end(); ++itr){
            std::string key = (*itr).first;
            std::string value = (*itr).second;
            if (key=="x"){
                im.m_dimarray[0] = atoi(value.c_str());
                biggestdim = std::max(biggestdim,1);
            } else
            if (key=="y"){
                if (DIM<2){ printf("Error opening raw: 1\n"); return false; }
                im.m_dimarray[1] = atoi(value.c_str());
                biggestdim = std::max(biggestdim,2);
            } else
            if (key=="z"){
                if (DIM<3){ printf("Error opening raw: 2\n"); return false; }
                im.m_dimarray[2] = atoi(value.c_str());
                biggestdim = std::max(biggestdim,3);
            } else
            if (key=="xsize"){
                im.m_voxelsize[0] = atof(value.c_str());
            } else
            if (key=="ysize"){
                if (DIM<2){ printf("Error opening raw: 3\n"); return false; }
                im.m_voxelsize[1] = atof(value.c_str());
            } else
            if (key=="zsize"){
                if (DIM<3){ printf("Error opening raw: 4\n"); return false; }
                im.m_voxelsize[2] = atof(value.c_str());
            } else
            if (key=="numcolors"){
                im.m_numcolors = atoi(value.c_str());
            } else
            if (key=="datatype"){
                datatype = getdatatype(value.c_str());
            } else
            if (key=="skipheader"){
                headersize = atoi(value.c_str());
            } else
            if (key.substr(0,3) == "dim"){
                int curdim = atoi(key.substr(3).c_str());
                if (DIM<curdim){ printf("Error opening raw: 5\n"); return false; }
                biggestdim = std::max(biggestdim,curdim);
                im.m_dimarray[curdim-1]=atoi(value.c_str());
            } else
            if (key.substr(0,4) == "size"){
                int curdim = atoi(key.substr(4).c_str());
                if (DIM<curdim){ printf("Error opening raw: 6\n"); return false; }
                im.m_voxelsize[curdim-1]=atof(value.c_str());
            }
        }
        bool difftype = (datatype!=getdatatype<VoxelType>());
        if (exactmatchonly && difftype){
            return false;
        }
        if (numinsequence>1){
            im.m_dimarray[biggestdim]=numinsequence;
            biggestdim++;
        }
        if (biggestdim>DIM){
            printf("Error opening raw: 8\n");
            return false;
        }
        im.setupdimensions(im.m_dimarray,im.m_numcolors);
        im.setupdata();
        
        //load the image data into the image
        int imagesizetimescolor = im.m_numvoxels;
        if (numinsequence>1) imagesizetimescolor/=im.m_dimarray[biggestdim-1];
        VoxelType* datapntr = new VoxelType[imagesizetimescolor];
        char text[1000];
        for (unsigned int i = 0;i < numinsequence;i++){
            sprintf(text,"loading %s: %s",imagetypename().c_str(),printfilename(sequence[i]).c_str());
            im.updateprogress(text,100*i/numinsequence);
            int offset = i*imagesizetimescolor;
            std::ifstream inputfile;
            inputfile.open(sequence[i].c_str(), std::ios::in | std::ios::binary);
            if (inputfile.is_open()){
                //handle header
                if (headersize){
                    if (headersize>0){
                        inputfile.seekg(headersize);
                        //std::cout << "skipping header of size: " << headersize << std::endl;
                    } else {
                        //read from bottom
                        long long filesize=inputfile.rdbuf()->pubseekoff(0,std::ios::end,std::ios::in  | std::ios::binary);
			long long spos = filesize - (imagesizetimescolor*sizeof(VoxelType));
			if (spos<0) spos=0;
                        inputfile.seekg(spos);
                        //std::cout << "reading file from bottom, starting at byte: " << spos << std::endl;
                    }
                }
                inputfile.read((char*)datapntr,imagesizetimescolor*sizeof(VoxelType));
                inputfile.close();
            } else {
                std::cout << "Error opening file: " << sequence[0] << std::endl;
                delete [] datapntr;
                return false;
            }
            for (int t=0;t<imagesizetimescolor;t++) im.m_data[offset+t]=datapntr[t];
        }
        im.updateprogress(text,100);
        delete [] datapntr;
            
        //remember the filetype
        im.m_filetype = imagetypename();
        return true;
    }
    static bool savefile(Image<VoxelType,DIM>& im,std::vector<std::string>& sequence,bool exactmatchonly,std::string flags,void* headerpntr){
        int numinsequence = sequence.size();
        int mindim = getlastimagedimensionindex(im);
        int imagesize = im.m_numvoxels/im.m_numcolors;
        if (numinsequence>1){ imagesize/=im.m_dimarray[mindim]; }
        int imagesizetimescolor = imagesize*im.m_numcolors;
        char text[1000];
        for (unsigned int i = 0;i < numinsequence;i++){
            sprintf(text,"saving %s: %s",imagetypename().c_str(),printfilename(sequence[i]).c_str());
            im.updateprogress(text,100*i/numinsequence);
            int offset = i*imagesizetimescolor;
            std::ofstream outputfile;
            outputfile.open(sequence[i].c_str(), std::ios::out | std::ios::binary);
            if (!outputfile.is_open()) return false;
            outputfile.write((char*)&im.m_data[offset],(long long)imagesizetimescolor*sizeof(VoxelType));
            outputfile.close();
        }
        im.updateprogress(text,100);
        
        //save the filetype
        im.m_filetype = imagetypename();
        return true;
    }
    
};

}