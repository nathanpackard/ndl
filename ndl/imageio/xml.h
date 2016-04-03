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
class XmlPlugin {
public:
    static std::string imagetypename(){ return "xml"; }
    static std::string imageopenextension(){ return "xml"; }
    static std::string imagesaveextension(){ return "xml"; }
    static bool isfilecompatible(std::string filename,std::string flags,bool sequenceflag=false,bool exactflag=false){
        if (getextension(filename)!="xml") return false;
        TiXmlDocument doc( filename.c_str() );
        bool loadOkay = doc.LoadFile();
        return loadOkay;
    }
    static bool loadfile(Image<VoxelType,DIM>& im,std::vector<std::string>& sequence,bool exactmatchonly,std::string flags,void* headerpntr){
        printf("loadxml\n");
        return im.loadfile("C:\\IM\\MPRAGE\\0001.dcm",flags,im.m_sequenceflag);
    }
    static bool savefile(Image<VoxelType,DIM>& im,std::vector<std::string>& sequence,bool exactmatchonly,std::string flags,void* headerpntr){
        printf("savexml\n");
        return true;
    }
    static bool checkfilesequence(std::string filename,std::vector<std::string>& sequence,std::string flags){
        //force sequence to be only a single file
        sequence.empty();
        sequence.push_back(filename);
        return true;
    }

};

}