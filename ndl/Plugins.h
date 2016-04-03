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

#ifndef PLUGINS_H
#define PLUGINS_H

#ifdef NDL_USE_BMP
#include "imageio/bmp.h"
#endif

#ifdef NDL_USE_JPEG
#include "imageio/jpeg.h"
#endif

#ifdef NDL_USE_BCTLIB
#include "imageio/bct.h"
#endif

#ifdef NDL_USE_ANALYZELIB
#include "imageio/analyze.h"
#endif

#ifdef NDL_USE_DCMTK
#include "imageio/dcm.h"
#endif

#include "imageio/raw.h"
#include "imageio/xml.h"

#include "renders/render1D.h"
#include "renders/orthorenderND.h"
#include <list>

namespace ndl {

//**********************
//HELPER FUNCTIONS
//**********************
void limitfilesequence(std::vector<std::string>& sequence,std::string flags){
    std::map<std::string,std::string> flaglist;
    getoptionalparams(flags,flaglist);
    if (flaglist.find("maxseqnum") != flaglist.end()){
        int v = atoi(flaglist["maxseqnum"].c_str());
        if (v<sequence.size()) sequence.erase(sequence.begin()+v,sequence.end());
    }
    if (flaglist.find("minseqnum") != flaglist.end()){
        int v = atoi(flaglist["minseqnum"].c_str());
        if (v>0) sequence.erase(sequence.begin(),sequence.begin()+v-1);
    }
}
bool checkfilesequence_default(std::string filename,std::vector<std::string>& sequence,std::string flags){
    std::vector< std::pair<std::string,int> > labelsizelist;
    for(int i=0;i<sequence.size();i++){
        std::string extension = "";
        std::string prefix = "";
        std::string thenumber;
        int pos = filename.find_last_of('.');
        if (pos != std::string::npos) extension = filename.substr(pos+1);
        int numdigits = 1;
        bool numericflag = false;
        if (isnumeric(extension)){
            numericflag = true;
            extension = "";
            prefix = filename.substr(0,pos);
        } else {
            int s = pos - 1;
            while(s>=0 && isdigit(filename[s])) s--;
            s++;
            prefix = filename.substr(0,s);
        }
        
        if (sequence[i].substr(0,prefix.size())!=prefix) continue;
        if (sequence[i].substr(sequence[i].size()-extension.size())!=extension) continue;
        if (pos == std::string::npos) thenumber = sequence[i].substr(prefix.size(),sequence[i].size() - prefix.size());
        else thenumber = sequence[i].substr(prefix.size(),sequence[i].size() - prefix.size() - extension.size() - 1);

        labelsizelist.push_back( std::pair<std::string,int>(sequence[i],atoi(thenumber.c_str())) );
    }
    
    //reorder images
    std::sort(labelsizelist.begin(), labelsizelist.end(), less_second<std::pair<std::string,int>>());
    
    //rebuild sorted sequence
    sequence.clear();
    for(int i=0;i<labelsizelist.size();i++) sequence.push_back(labelsizelist[i].first);
    limitfilesequence(sequence,flags);
    return true;
}

std::string generatesequencefilename(std::string filename,std::string flags,int filenum){
    std::string extension = "";
    std::string prefix = "";
    int pos = filename.find_last_of('.');
    extension = filename.substr(pos+1);
    int numdigits = 1;
    bool numericflag = false;
    if (isnumeric(extension)){
        numericflag = true;
        extension = "";
        prefix = filename.substr(0,pos);
    } else {
        int s = pos - 1;
        while(s>=0 && isdigit(filename[s])) s--;
        s++;
        prefix = filename.substr(0,s);
    }
    
    numdigits = filename.size() - prefix.size() - extension.size() - 1;
    char formatstr[100];
    char fnumber[100];
    sprintf(formatstr,"%%0%dd",numdigits); //cool trick
    sprintf(fnumber,formatstr,filenum+1);
    
    if (numericflag) return prefix + "." + fnumber;
    else return prefix + fnumber + "." + extension;
}

//find min dimension that is not size 1 (that is also bigger than 2D)
template<class VoxelType,int DIM>
int getlastimagedimensionindex(Image<VoxelType,DIM>& im){
    int mindim=DIM-1;
    while (im.m_dimarray[mindim]==1 && mindim>1) mindim--;
    return mindim;
}


//**********************
//LOADING AND SAVING IS DONE THROUGH THIS FUNCTION
//**********************
std::vector<std::string> NDL_getimagetypes(){
    const int DIM=2;
    std::vector<std::string> result;
    #ifdef NDL_USE_BMP
    result.push_back(BmpPlugin<int,DIM>::imagetypename());
    #endif
    #ifdef NDL_USE_JPEG
    result.push_back(JpegPlugin<int,DIM>::imagetypename());
    #endif
    #ifdef NDL_USE_BCTLIB
    result.push_back(BctPlugin<int,DIM>::imagetypename());
    #endif
    #ifdef NDL_USE_ANALYZELIB
    result.push_back(AnalyzePlugin<int,DIM>::imagetypename());
    #endif
    #ifdef NDL_USE_DCMTK
    result.push_back(DicomPlugin<int,DIM>::imagetypename());
    #endif
    result.push_back(XmlPlugin<int,DIM>::imagetypename());
    result.push_back(RawPlugin<int,DIM>::imagetypename());
    return result;
}

std::vector<std::string> NDL_getimageopenextensions(){
    const int DIM=2;
    std::vector<std::string> result;
    #ifdef NDL_USE_BMP
    result.push_back(BmpPlugin<int,DIM>::imageopenextension());
    #endif
    #ifdef NDL_USE_JPEG
    result.push_back(JpegPlugin<int,DIM>::imageopenextension());
    #endif
    #ifdef NDL_USE_BCTLIB
    result.push_back(BctPlugin<int,DIM>::imageopenextension());
    #endif
    #ifdef NDL_USE_ANALYZELIB
    result.push_back(AnalyzePlugin<int,DIM>::imageopenextension());
    #endif
    #ifdef NDL_USE_DCMTK
    result.push_back(DicomPlugin<int,DIM>::imageopenextension());
    #endif
    result.push_back(XmlPlugin<int,DIM>::imageopenextension());
    result.push_back(RawPlugin<int,DIM>::imageopenextension());
    return result;
}

std::vector<std::string> NDL_getimagesaveextensions(){
    const int DIM=2;
    std::vector<std::string> result;
    #ifdef NDL_USE_BMP
    result.push_back(BmpPlugin<int,DIM>::imagesaveextension());
    #endif
    #ifdef NDL_USE_JPEG
    result.push_back(JpegPlugin<int,DIM>::imagesaveextension());
    #endif
    #ifdef NDL_USE_BCTLIB
    result.push_back(BctPlugin<int,DIM>::imagesaveextension());
    #endif
    #ifdef NDL_USE_ANALYZELIB
    result.push_back(AnalyzePlugin<int,DIM>::imagesaveextension());
    #endif
    #ifdef NDL_USE_DCMTK
    result.push_back(DicomPlugin<int,DIM>::imagesaveextension());
    #endif
    result.push_back(XmlPlugin<int,DIM>::imagesaveextension());
    result.push_back(RawPlugin<int,DIM>::imagesaveextension());
    return result;
}

template<class VoxelType,int DIM>
bool NDL_loadsave(Image<VoxelType,DIM>& im,std::string filename,bool loadflag,std::string flags,void* headerpntr){
    //loadflag==true loads
    //loadflag==false saves
    
    //process some flags
    std::map<std::string,std::string> flaglist;
    getoptionalparams(flags,flaglist);
    std::string type="";
    bool exactmatchonly = false; //exact match is if we have the correct data type
    if (flaglist.find("filetype") != flaglist.end()){
        type = flaglist["filetype"];
    }
    if (flaglist.find("exactmatchonly") != flaglist.end()) exactmatchonly=true;

    //get the filenames of the sequence to load or save
    std::vector<std::string> sequence,tsequence;
    if (loadflag){
        if (im.m_sequenceflag){
            std::string dir = std::string(getfolderfromfile(filename));
            getfiles(dir,sequence);
            for (unsigned int i = 0;i < sequence.size();i++) {
                sequence[i] = dir + "/" + sequence[i];
            }
        } else sequence.push_back(filename);
    } else {
        if (im.m_sequenceflag){
            int mindim = getlastimagedimensionindex(im);
            for (unsigned int i = 0;i < im.m_dimarray[mindim];i++) {
                sequence.push_back(generatesequencefilename(filename,flags,i));
            }
        } else sequence.push_back(filename);
    }
    
    //if the file is compabible, load or save it
    #ifdef NDL_USE_BMP
    if (type==BmpPlugin<VoxelType,DIM>::imagetypename() || type==""){
        //printf("bmp\n");
        if (loadflag){
            if (BmpPlugin<VoxelType,DIM>::isfilecompatible(filename,flags,im.m_sequenceflag,exactmatchonly)){
                tsequence=sequence;
                if (im.m_sequenceflag){ checkfilesequence_default(filename,tsequence,flags); }
                if (BmpPlugin<VoxelType,DIM>::loadfile(im,tsequence,exactmatchonly,flags,headerpntr)) return true;
            }
        } else if (BmpPlugin<VoxelType,DIM>::savefile(im,sequence,exactmatchonly,flags,headerpntr)) return true;
    }
    #endif
    #ifdef NDL_USE_JPEG
    if (type==JpegPlugin<VoxelType,DIM>::imagetypename() || type==""){
        //printf("jpeg\n");
        if (loadflag){
            if (JpegPlugin<VoxelType,DIM>::isfilecompatible(filename,flags,im.m_sequenceflag,exactmatchonly)){
                tsequence=sequence;
                if (im.m_sequenceflag){ checkfilesequence_default(filename,tsequence,flags); }
                if (JpegPlugin<VoxelType,DIM>::loadfile(im,tsequence,exactmatchonly,flags,headerpntr)) return true;
            }
        } else if (JpegPlugin<VoxelType,DIM>::savefile(im,sequence,exactmatchonly,flags,headerpntr)) return true;
    }
    #endif
    #ifdef NDL_USE_BCTLIB
    if (type==BctPlugin<VoxelType,DIM>::imagetypename() || type==""){
        //printf("bct\n");
        if (loadflag){
            if (BctPlugin<VoxelType,DIM>::isfilecompatible(filename,flags,im.m_sequenceflag,exactmatchonly)){
                tsequence=sequence;
                if (im.m_sequenceflag){ BctPlugin<VoxelType,DIM>::checkfilesequence(filename,tsequence,flags); }
                if (BctPlugin<VoxelType,DIM>::loadfile(im,tsequence,exactmatchonly,flags,headerpntr)) return true;
            }
        } else if (BctPlugin<VoxelType,DIM>::savefile(im,sequence,exactmatchonly,flags,headerpntr)) return true;
    }
    #endif
    #ifdef NDL_USE_ANALYZELIB
    if (type==AnalyzePlugin<VoxelType,DIM>::imagetypename() || type==""){
        //printf("analyze\n");
        if (loadflag){
            if (AnalyzePlugin<VoxelType,DIM>::isfilecompatible(filename,flags,im.m_sequenceflag,exactmatchonly)){
                tsequence=sequence;
                if (im.m_sequenceflag){ checkfilesequence_default(filename,tsequence,flags); }
                if (AnalyzePlugin<VoxelType,DIM>::loadfile(im,tsequence,exactmatchonly,flags,headerpntr)) return true;
            }
        } else if (AnalyzePlugin<VoxelType,DIM>::savefile(im,sequence,exactmatchonly,flags,headerpntr)) return true;
    }
    #endif
    #ifdef NDL_USE_DCMTK
    if (type==DicomPlugin<VoxelType,DIM>::imagetypename() || type==""){
        //printf("dcm\n");
        if (loadflag){
            if (DicomPlugin<VoxelType,DIM>::isfilecompatible(filename,flags,im.m_sequenceflag,exactmatchonly)){
                tsequence=sequence;
                if (im.m_sequenceflag){ DicomPlugin<VoxelType,DIM>::checkfilesequence(filename,tsequence,flags); }
                if (DicomPlugin<VoxelType,DIM>::loadfile(im,tsequence,exactmatchonly,flags,headerpntr)) return true;
            }
        } else if (DicomPlugin<VoxelType,DIM>::savefile(im,sequence,exactmatchonly,flags,headerpntr)) return true;
    }
    #endif
    if (type==XmlPlugin<VoxelType,DIM>::imagetypename() || type==""){
        //printf("xml\n");
        if (loadflag){
            if (XmlPlugin<VoxelType,DIM>::isfilecompatible(filename,flags,im.m_sequenceflag,exactmatchonly)){
                tsequence=sequence;
                if (im.m_sequenceflag){ XmlPlugin<VoxelType,DIM>::checkfilesequence(filename,tsequence,flags); }
                if (XmlPlugin<VoxelType,DIM>::loadfile(im,tsequence,exactmatchonly,flags,headerpntr)) return true;
            }
        } else if (XmlPlugin<VoxelType,DIM>::savefile(im,sequence,exactmatchonly,flags,headerpntr)) return true;
    }
    if (type==RawPlugin<VoxelType,DIM>::imagetypename() || type==""){
        //printf("raw\n");
        if (loadflag){
            if (RawPlugin<VoxelType,DIM>::isfilecompatible(filename,flags,im.m_sequenceflag,exactmatchonly)){
                tsequence=sequence;
                if (im.m_sequenceflag){ checkfilesequence_default(filename,tsequence,flags); }
                if (RawPlugin<VoxelType,DIM>::loadfile(im,tsequence,exactmatchonly,flags,headerpntr)) return true;
            }
        } else if (RawPlugin<VoxelType,DIM>::savefile(im,sequence,exactmatchonly,flags,headerpntr)) return true;
    }
    
    return false;
}    


/*
each render looks at the data 
and projects it onto a place on a 2D image
taking into account:

1) 2D image coords: (x,y,width,height)
2) zoom
3) ave/MIP/Surface Render
5) OrthoAxisAligned w/any 2 axis to define a plane plus a thickness axis 
   / Arbitrary angle w/any projection matrix
*/


//list all seleciton types here
enum { NDL_POINT_SELECT=1, NDL_LINE_SELECT, NDL_BOX_SELECT, NDL_SPHERE_SELECT };
enum { NDL_SELECTIONNEW, NDL_SELECTIONMOVEEDGE, NDL_SELECTIONMOVE };
template<class VoxelType,int DIM>
class SelectionInfo {
    public:
    int point1[DIM];
    int point2[DIM];
    int selectiontype;
    int selectionstate;
    int selectionactivedimension;
    int selectionactiveside;
    
    //constructor
    SelectionInfo(){ Init(); }
    void Init(){
        memset((void*)this,0,sizeof(SelectionInfo<VoxelType,DIM>));
        selectiontype=NDL_POINT_SELECT;
        selectionstate=NDL_SELECTIONNEW;
        selectionactivedimension=0;
        selectionactiveside=0;
    }
};

//list all rendertypes here, to add a new render type define a new symbol here
//then edit all places in this file where these symbols are found to include
//the new symbol.
enum { NDL_View1D=1, NDL_ViewND };
template<class VoxelType,int DIM>
class RenderInfo {
    public:
    
    //for ND image data (initialized on creation)
    char dataname[100];
    int dim[DIM];
    double voxelsize[DIM];
    char units[DIM][10];
    int orgin[DIM];
    int regionsize[DIM];
    int ncolors;
    int nvoxels;
    float min; //for windowlevel
    float max; //for windowlevel
    
    //render identifier
    int rendertype;
    
    //render parameters
    //OrthoRenderND parameters
    int axis1;
    int axis2;
    int axis3;
    int axis3pos;
    int axis3thickness;
    int displaymode; //NDL_AVE, NDL_MIP, NDL_GEOMAVE, ...
    unsigned char LUT[768]; //256 RGB values
    bool LUTflag;
    unsigned char rendercolor[3];
    
    //for the 2D displayimage data
    int xpos; //pixel coordinate
    int ypos; //pixel coordinate
    int width; //in pixels
    int height; //in pixels
    bool needsrefresh;
    
    //pointer to an image data, only used on viewer process
    Image<VoxelType,DIM>* imagepointer;
    
    //constructor
    RenderInfo(){
        memset((void*)this,0,sizeof(RenderInfo<VoxelType,DIM>));
        memcpy(rendercolor,randomswatch,3);
    }
};

template<class VoxelType,int DIM>
class ViewInfo {
    public:
        
    //**********************
    //VARIABLES
    //**********************    
    //configuration to be set by parent process before starting the viewer 
    //process, it must be statically allocated
    int pid;
    bool winlevelcontrols;
    bool LUTcontrols;
    bool showcontrols;
    bool showcrossheirs;
    char filename[500];
    char settingfilename[500];
    
    //Used by child process only, can be static or dynamically allocated
    int mousex;
    int mousey;
    int leftbutton;
    int middlebutton;
    int rightbutton;
    int leftmousedownx;
    int leftmousedowny;
    int middlemousedownx;
    int middlemousedowny;
    int rightmousedownx;
    int rightmousedowny;
    int margin;
    double window;
    double level;
    int tooltype;
    RenderInfo<VoxelType,DIM>* activerenderinfopointer;
    std::list< RenderInfo<VoxelType,DIM> >* renderinfolistpointer;
    std::list< SelectionInfo<VoxelType,DIM> >* annotationlistpointer;
    
    //Commands to be set by parent process after starting the viewer process
    //and read by the child process. All data that is read by both processes
    //must be statically allocated
    volatile bool closecommand;
    volatile bool refreshcommand;
    volatile bool resizecommand;
        int totalwidth;
        int totalheight;
    volatile bool movecommand;
        int xpos;
        int ypos;
    volatile bool updateLUTcommand;
    volatile bool addrendertoviewcommand;
        RenderInfo<VoxelType,DIM> renderinfo;
    volatile bool selectioncommand;
        SelectionInfo<VoxelType,DIM> selectioninfo;
        
    //Event handlers to be called by child process if they are defined by parent code
    bool (*closeevent)(ViewInfo<VoxelType,DIM>* view);
    
    //**********************
    //these functions are to be called from viewer process
    //**********************
    void renderimage(RenderInfo<VoxelType,DIM>& ri,Image<unsigned char,2>& displayimage){
        switch(ri.rendertype){
            case NDL_View1D: { Render1D<VoxelType,DIM>::renderimage(this,ri,displayimage); break; }
            case NDL_ViewND: { OrthoRenderND<VoxelType,DIM>::renderimage(this,ri,displayimage); break; }
        }
    }

    void renderannotation(RenderInfo<VoxelType,DIM>& ri,Image<unsigned char,2>& displayimage){
        switch(ri.rendertype){
            case NDL_View1D: { Render1D<VoxelType,DIM>::renderannotation(this,ri,displayimage); break; }
            case NDL_ViewND: { OrthoRenderND<VoxelType,DIM>::renderannotation(this,ri,displayimage); break; }
        }
    }

    void renderselection(RenderInfo<VoxelType,DIM>& ri,Image<unsigned char,2>& displayimage){
        switch(ri.rendertype){
            case NDL_View1D: { Render1D<VoxelType,DIM>::renderselection(this,ri,displayimage); break; }
            case NDL_ViewND: { OrthoRenderND<VoxelType,DIM>::renderselection(this,ri,displayimage); break; }
        }
    }
    
    void onmousewheel(int delta){
        if (activerenderinfopointer==0) return;
        switch(activerenderinfopointer->rendertype){
            case NDL_View1D: { Render1D<VoxelType,DIM>::onmousewheel(this,delta); break; }
            case NDL_ViewND: { OrthoRenderND<VoxelType,DIM>::onmousewheel(this,delta); break; }
        }
    }

    void onmousemove(){
        if (activerenderinfopointer==0) return;
        switch(activerenderinfopointer->rendertype){
            case NDL_View1D: { Render1D<VoxelType,DIM>::onmousemove(this); break; }
            case NDL_ViewND: { OrthoRenderND<VoxelType,DIM>::onmousemove(this); break; }
        }
    }
    
    void onmousebutton(int buttonnum){ //Left: buttonnum=0, Middle: buttonnum=1, Right: buttonnum=2,
        if (activerenderinfopointer==0) return;
        switch(activerenderinfopointer->rendertype){
            case NDL_View1D: { Render1D<VoxelType,DIM>::onmousebutton(this,buttonnum); break; }
            case NDL_ViewND: { OrthoRenderND<VoxelType,DIM>::onmousebutton(this,buttonnum); break; }
        }
    }
    
    void getwindowlevel(double* window,double* level){
        #if defined NDL_USE_WXWIDGETS
        wxgetwindowlevel<VoxelType,DIM>(window,level);
        #elif defined NDL_USE_QT
        #else
        #endif
    }

    void setwindowlevel(double window,double level){
        #if defined NDL_USE_WXWIDGETS
        wxsetwindowlevel<VoxelType,DIM>(window,level);
        #elif defined NDL_USE_QT
        #else
        #endif
    }
    
    //...
    //~ void addtocontextmenu(std::string text, function){
        //~ #if defined NDL_USE_WXWIDGETS
        
        //~ //...
        
        //~ #elif defined NDL_USE_QT
        //~ #else
        //~ #endif
    //~ }
    
    //**********************
    //FUNCTIONS TO BE CALLED BY PARENT PROCESS
    //**********************
    static ViewInfo<VoxelType,DIM>* openview(Image<VoxelType,DIM>& im,int rendertype,std::string flags=""){
        //setup params
        std::map<std::string,std::string> flaglist;
        getoptionalparams(flags,flaglist);
        //setup viewinfo, NOTE, there is no constructor, EVERYTHING must be initialized HERE
        ViewInfo<VoxelType,DIM>* vi = (ViewInfo<VoxelType,DIM>*)allocate_sharedmemory(sizeof(ViewInfo<VoxelType,DIM>));

        //assign the vars to the viewinfo
        vi->pid=0;
        vi->mousex=-1;
        vi->mousey=-1;
        vi->leftbutton=0;
        vi->middlebutton=0;
        vi->rightbutton=0;
        vi->leftmousedownx=-1;
        vi->leftmousedowny=-1;
        vi->middlemousedownx=-1;
        vi->middlemousedowny=-1;
        vi->rightmousedownx=-1;
        vi->rightmousedowny=-1;
        vi->margin=0;
        vi->activerenderinfopointer=0;
        vi->renderinfolistpointer=0;
        vi->annotationlistpointer=0;
        vi->window=255;
        vi->level=128;
        vi->tooltype=NDL_POINT_SELECT;
        vi->closecommand = false;
        vi->refreshcommand = false;
        vi->resizecommand = false;
        vi->movecommand = false;
        vi->totalwidth = 0;
        vi->totalheight = 0;
        vi->xpos = -1;
        vi->ypos = -1;
        vi->updateLUTcommand = false;
        vi->addrendertoviewcommand = false;
        vi->selectioncommand = false;
        strcpy(vi->filename,im.m_filename.c_str());
        sprintf(vi->settingfilename,"%s.ini",im.m_filename.c_str());
        
        //handle flags
        if (flaglist.find("hidewinlevel") != flaglist.end()){
            vi->winlevelcontrols=false;
        } else vi->winlevelcontrols=true;
        if (flaglist.find("hideLUT") != flaglist.end()){
            vi->LUTcontrols=false;
        } else vi->LUTcontrols=true;
        if (flaglist.find("hideall") != flaglist.end()){
            vi->showcontrols=false;
        } else vi->showcontrols=true;
        if (flaglist.find("showcrossheirs") != flaglist.end()){
            vi->showcrossheirs=true;
        } else vi->showcrossheirs=false;
        
        //~ SaveSetting("test1","value",23);
        //~ double value;
        //~ if (LoadSetting("test1","value",value)) printf("\n!RESULTS: %f\n",value);
                
        //
        vi->selectioninfo.Init();
        rendertype = vi->setuprenderinfofromimage(im,rendertype);
        vi->closeevent = 0;

        //start the process and send it the viewinfo
        #if defined NDL_USE_WXWIDGETS
        //launch a wxwidgets viewer
        vi->pid = launchprocess(wxmakeview<VoxelType,DIM>,get_sharedmemoryname((void*)vi));
        #elif defined NDL_USE_QT
        //launch a qt viewer
        //...
        #else
        printf("ERROR, NO VIEWER ENABLED. DON'T FORGET TO DEFINE ONE (e.g. #define NDL_USE_WXWIDGETS)\n");
        return 0;
        #endif
        
        //setup the renderinfo info from the image
        switch(rendertype){
            case NDL_View1D: { Render1D<VoxelType,DIM>::openview(im,vi); break; }
            case NDL_ViewND: { OrthoRenderND<VoxelType,DIM>::openview(im,vi); break; }
        }
            
        im.m_viewlist.push_back(vi);
        vi->refresh();
        
        return vi;
    }
    
    void resizeview(int width,int height){
        //for any value, -1 means don't change it
        totalwidth = width;
        totalheight = height;
        resizecommand = true;
    }
    
    int setuprenderinfofromimage(Image<VoxelType,DIM>& im,int rendertype){
        //handle default rendertype
        if (rendertype==0){
            if (DIM==1) rendertype = NDL_View1D;
            if (DIM>=2) rendertype = NDL_ViewND;
        }
        renderinfo.rendertype = rendertype;
        
        renderinfo.axis3thickness=1;
        renderinfo.displaymode = NDL_AVE;
        for(int i=0;i<DIM;i++){
            renderinfo.dim[i] = im.m_dimarray[i];
            renderinfo.voxelsize[i] = im.m_voxelsize[i];
            strncpy(renderinfo.units[i], im.m_units[i].c_str(), sizeof(renderinfo.units[i])-1);
            renderinfo.orgin[i] = im.m_orgin[i];
            renderinfo.regionsize[i] = im.m_regionsize[i];
        }
        renderinfo.min=im.m_min;
        renderinfo.max=im.m_max;
        renderinfo.ncolors = im.m_numcolors;
        renderinfo.nvoxels = im.m_numvoxels;
        strcpy(renderinfo.dataname,get_sharedmemoryname(im.m_data));
        return rendertype;
    }

    void addrendertoview(Image<VoxelType,DIM>& im,int rendertype){
        //if the image has changed, update the image info
        if (strcmp(renderinfo.dataname,get_sharedmemoryname(im.m_data))!=0){
            setuprenderinfofromimage(im,rendertype);
        }
        
        //add the render
        addrendertoviewcommand = true; 
        while (addrendertoviewcommand){ }
    }
    
    //NOT DONE YET!!
    void setselection(){
        //setup selectioninfo
        //...
        
        //add the selection
        selectioncommand = true; 
        while (selectioncommand){ }
    }
        
    void closeview(std::string flags=""){
        //reqest for the view to close
        closecommand = true;
        
        //wait for view process to finish
        waitforprocess(pid);
        
        //deallocate memory
        delete_sharedmemory((void*)this);
    }

    void refresh(){
        refreshcommand = true;
    }
    
    void updateLUT(){
        updateLUTcommand = true;
    }
    
    bool isviewclosed(){
        return !isprocessrunning(pid);
    }
    
    int isleftbuttondown(){
        return leftbutton;
    }

    int ismiddlebuttondown(){
        return middlebutton;
    }

    int isrightbuttondown(){
        return rightbutton;
    }
    
    int getmousex(){
        return mousex;
    }
    
    int getmousey(){
        return mousey;
    }
    
    void movewindow(int x,int y){
        //for any value, -1 means don't change it
        xpos = x;
        ypos = y;
        movecommand = true;
    }
    
    //Functions to Alter Settings
    template<class T>
    bool SaveSetting(std::string key, std::string attribute, T value){
        std::ostringstream strout;
        strout << value;
        return SaveSetting(key,attribute,strout.str());
    }
    bool SaveSetting(std::string key, std::string attribute, std::string value){
        TiXmlDocument doc(settingfilename);
        TiXmlNode* node = doc.FirstChild( key );
        if (node){
            TiXmlElement* item = node->ToElement();
    		item->SetAttribute( attribute.c_str(), value.c_str() );
        } else {
            TiXmlElement item( key.c_str() );
            item.SetAttribute( attribute.c_str(), value.c_str() );
            doc.InsertEndChild( item );
        }
        doc.SaveFile();
        return true;
    }
    template<class T>
    bool LoadSetting(std::string key, std::string attribute, T& value){
        std::string strvalue;
        bool result = LoadSetting(key,attribute,strvalue);
        if (result){
            istringstream strin(strvalue);
            strin >> value;
            return true;
        } else return false;
    }
    bool LoadSetting(std::string key,std::string attribute, std::string& value ){
        TiXmlDocument doc(settingfilename);
        TiXmlNode* node = doc.FirstChild( key.c_str() );
        if (!node) false;
        TiXmlElement* todoElement = node->ToElement();
        if (todoElement==0 || todoElement->Attribute(attribute.c_str())==0) return false;
        value = todoElement->Attribute(attribute.c_str());
        return true;
    }
    
    
};

} //end namespace

//openMP
//#ifdef NDL_USE_OMP
//#include "omp.h" //(this might not be required)
//#endif

//select the viewer system
#if defined NDL_USE_WXWIDGETS
    #include "displays/wxviewer.h"
#endif

#endif
