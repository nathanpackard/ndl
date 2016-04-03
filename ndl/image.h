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

#ifndef IMAGE_H
#define IMAGE_H

//standard include files
#include <map>
#include <queue>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <fstream>
#include <vector>
#include <iostream>
#include <utility>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <sstream>
#include <cmath>
#include <utility>
#include <algorithm>
#include <functional>
#include <cctype>

//other required include files
#include <defs.h>
#include <hlib/helpers/types.h>
#include <hlib/helpers/mathhelpers.h>
#include <hlib/helpers/fontdefs.h>
#include <hlib/helpers/shapelib.h>
#include <hlib/fft/fft.h>
#include <hlib/linalg/Vector.h> //Vector class (for use with Matrix class)
#include <hlib/linalg/Matrix.h> //Matrix class
#include <hlib/IPC/IPC.h>
#include <hlib/IPC/sharedmem.h>
#include <hlib/helpers/consoleIO.h>
#include <hlib/helpers/dirIO.h>
#include <hlib/tinyxml/tinyxml.h>

namespace ndl {

//declare some classes
template<class VoxelType,int DIM>class Image;
template<class VoxelType,int DIM,int CDIM>class ImageIndexer;
template<class VoxelType,int DIM>class watershednode;
template<class VoxelType,int DIM>class ViewInfo;
template<class VoxelType,int DIM>struct RenderInfo;
template<class VoxelType,int DIM>struct SelectionInfo;

//define some helper functions
template <class VoxelType,int DIM> bool operator< (const watershednode<VoxelType,DIM>& node1, const watershednode<VoxelType,DIM> &node2){	return node1.value > node2.value;	}
template <class VoxelType,int DIM> bool operator> (const watershednode<VoxelType,DIM>& node1, const watershednode<VoxelType,DIM> &node2){	return node1.value < node2.value;	}
template<class VoxelType> void assignvalue(VoxelType& dest,VoxelType value){ dest = value; }
template<class VoxelType> void subvalue(VoxelType& dest,VoxelType value){ dest -= value; }
template<class VoxelType> void addvalue(VoxelType& dest,VoxelType value){ dest += value; }
template<class VoxelType> void avevalue(VoxelType& dest,VoxelType value){ dest += value; dest/=2; }
template<class VoxelType> void multvalue(VoxelType& dest,VoxelType value){ dest *= value; }
template<class VoxelType> void minvalue(VoxelType& dest,VoxelType value){ dest = (std::min)(dest,value); }
template<class VoxelType> void maxvalue(VoxelType& dest,VoxelType value){ dest = (std::max)(dest,value); }
template<class VoxelType> void normvalue(VoxelType& dest,VoxelType value){ value *= dest; if (value<0) dest=0; else dest = sqrt(value); }
bool dummymask(int index){ return true; }
void stdoutprogressstart(){ }
void stdoutprogressupdate(char* text,int i){ printf("%d%%) %s\r",i,text); }
void stdoutprogressend(){ printf("\n"); }

/*!
//This class handles ND-image processing in NDL.
*/
template<class VoxelType,int DIM>
class Image {
public:
    static const int MYDIM = DIM;

    /**
    print image properties to stdout
    */
    void printinfo(char* toptext = 0){
        std::cout << "*****************" << std::endl;
        if (toptext) std::cout << toptext << std::endl << "*****************" << std::endl;
        std::cout << "DIM: " << DIM << std::endl;
        std::cout << "datatype: " << getdatatypestring(getdatatype<VoxelType>()) << std::endl;
        std::cout << "numcolors: " << m_numcolors << std::endl;
        std::cout << "dimensions: ";
        for(int i=0;i<DIM;i++) std::cout << m_dimarray[i] << " ";
        std::cout << std::endl;
        std::cout << "orgin: ";
        for(int i=0;i<DIM;i++) std::cout << m_orgin[i] << " ";
        std::cout << std::endl;
        std::cout << "regionsize: ";
        for(int i=0;i<DIM;i++) std::cout << m_regionsize[i] << " ";
        std::cout << std::endl;
        std::cout << "voxelsize: ";
        for(int i=0;i<DIM;i++) std::cout << m_voxelsize[i] << " " << m_units[i] << " ";
        std::cout << std::endl;
        std::cout << "rotorgin: ";
        for(int i=0;i<DIM;i++) std::cout << m_rotorgin[i] << " ";
        std::cout << std::endl;
        std::cout << "dimPROD: "; for (int i=0;i<DIM;i++) std::cout << m_dimPROD[i] << " "; std::cout << std::endl;
        std::cout << "dimPRODregion: "; for (int i=0;i<DIM;i++) std::cout << m_dimPRODregion[i] << " "; std::cout << std::endl;
        std::cout << "m_worldposition: " << std::endl; m_worldposition.print();
        std::cout << "m_voxelsizematrix: " << std::endl; m_voxelsizematrix.print();
        std::cout << "m_rotateorginmatrix: " << std::endl; m_rotateorginmatrix.print();
        std::cout << "m_rotationmatrix: " << std::endl; m_rotationmatrix.print();
        std::cout << "m_worldmatrix: " << std::endl; m_worldmatrix.print();
        std::cout << "m_projectionmatrix: " << std::endl; m_projectionmatrix.print();
        std::cout << "m_orientationmatrix: " << std::endl; m_orientationmatrix.print();
        std::cout << "m_iorientationmatrix: " << std::endl; m_iorientationmatrix.print();
        std::cout << "m_finalmatrix: " << std::endl; m_finalmatrix.print();
        std::cout << "m_ifinalmatrix: " << std::endl; m_ifinalmatrix.print();
        std::cout << "min: " << (double)m_min << std::endl;
        std::cout << "max: " << (double)m_max << std::endl;
    }
    
    /**
    print the image to stdout (could take a long time for a large image)
    */
    void print(char* toptext = 0){
        std::cout << "*****************" << std::endl;
        if (toptext) std::cout << toptext << std::endl << "*****************" << std::endl;
        std::cout << "position\t";
        for(int n=0;n<DIM;n++){
            std::cout << m_units[n];
            if(n!=DIM-1) std::cout << ",";
        }
        std::cout << "\t";
        std::cout << "value" << std::endl;
        float tempvector[DIM];
        NDL_FOREACH(*this){
            int index;
            NDL_GETCOORD(index,tempvector);
            for(int n=0;n<DIM;n++){
                std::cout << tempvector[n]*m_voxelsize[n] - m_rotorgin[n];
                if(n!=DIM-1) std::cout << ",";
            }
            std::cout << "\t";
            for(int n=0;n<DIM;n++){
                std::cout << tempvector[n];
                if(n!=DIM-1) std::cout << ",";
            }
            std::cout << "\t";
            std::cout << m_data[index] << std::endl;
        } NDL_ENDFOREACH
    }
    
    /**
    determine if the image empty (uninitialized)
    */
    bool isempty(){
        return (m_numvoxels==0);
    }

    /**
    For a given dimarray and numcolors, computes dimension products (prodarray) and the total number of voxels (return value)
    prodarray[0] will be set to the number of colors (prodarray[0]=numcolors), and prodarray[n]=dimarray[n-1]*prodarray[n-1]
    */
    int calculatenumvoxels(int* prodarray,int* dimarray,int numcolors){
        prodarray[0]=numcolors;
        for(int i=1;i<DIM;i++) prodarray[i]=dimarray[i-1]*prodarray[i-1];
        return dimarray[DIM-1]*prodarray[DIM-1];
    }
    
    /**
    Setup internal variables to represent an image of the specified dimension and number of colors
    */
    void setupdimensions(int dim[DIM],int numcolors){
        m_numcolors=numcolors;
        for(int i=0;i<DIM;i++){
            m_orgin[i]=0;
            m_dimarray[i]=m_regionsize[i]=dim[i];
        }
        m_numvoxels=calculatenumvoxels(m_dimPROD,m_dimarray,m_numcolors);
        m_numregionvoxels=calculatenumvoxels(m_dimPRODregion,m_regionsize,m_numcolors);
        resetvoxelsize();
        resetrotateorgin();
    }
    
    /**
    Allocate shared memory (useful for sharing memory between processes as is done when viewing image data)
    */
    template<class T> T* allocate(int num,int line){
        try {
            return (T*)allocate_sharedmemory(num*sizeof(T));
            //return new T[num];
        } catch (std::exception& e){
            printf("Memory Error: %s\n",e.what());
            printf("The error is in %s on line %d\n", __FILE__, line);
            exit(1);
        }
    }
    
    /**
    ///deallocate shared memory
    */
    template<class T> void deallocate(T* pntr){
        delete_sharedmemory((void*)pntr);
        //delete [] pntr;
    }
    
    /**
    Deallocate any memory being used from this image (used when destroying an image)
    */
    void cleandata(){
        if (m_data){
            if (*m_sharedcounter==1){
                if (!m_externaldataflag) deallocate(m_data);
                deallocate(m_sharedcounter);
            } else (*m_sharedcounter)--;
        }
    }
    
    /**
    Allocate memory for this image (to be used after setupdimensions)
    */
    void setupdata(){
        cleandata();
        m_data = allocate<VoxelType>(m_numvoxels,__LINE__);
        m_sharedcounter=allocate<int>(1,__LINE__);
        *m_sharedcounter = 1;
        m_externaldataflag=false;
    }
    
    /**
    Attach external buffer to be used for this image (to be used as an alternative to setupdata)
    */
    void setupshareddata(VoxelType* shareddata){
        cleandata();
        m_data=shareddata;
        m_sharedcounter=allocate<int>(1,__LINE__);
        *m_sharedcounter = 1;
        m_externaldataflag=true;
    }
        
    /**
    Initilize all variables (called by all constructors)
    */
    bool Image<VoxelType,DIM>::InitImage(){
        //init some vars
        for(int i=0;i<DIM;i++){
            m_dimarray[i]=1;
            m_dimPROD[i]=1;
            m_orgin[i]=0;
            m_regionsize[i]=0;
            m_dimPRODregion[i]=0;
            m_voxelsize[i]=1;
            m_scalevector[i]=1;
            m_rotorgin[i]=0;
            m_units[i]="px";
        }
        m_numregionvoxels=0;
        m_numvoxels=0;
        m_numcolors=1;
        m_data=0;
        m_sharedcounter=0;
        m_externaldataflag=false;
        m_min=m_max=0;
        m_issaved=true;
        m_isloaded=true;
        m_matrixrecalculateflag=false;

        //init some file sequence vars
        m_filename="";
        m_sequenceflag=false;

        //flag used by save and load functions for aborting
        m_attemptabort=false;
        m_progress=0;
        setprogressupdatefunction(&stdoutprogressupdate);
        setprogressstartfunction(&stdoutprogressstart);
        setprogressendfunction(&stdoutprogressend);
        
        m_filetype="";
        return true;
    }
        
    /**
    Make the current image a copy of the source image. If shared=true, the images will share memory. Otherwise
    A copy of will be created with the size of the copy specified by dim.
    */
    template<class T,int CDIM> void imagecopy(Image<T,CDIM>& source,int dim[DIM],bool shared=false){
        if ((void*)&source==(void*)this) return;

        //copy some vectors/matricies
        for (int i = 0; i<DIM; i++) m_units[i] = source.m_units[i];
        m_voxelsize = source.m_voxelsize;
        m_scalevector = source.m_scalevector;
        m_rotorgin = source.m_rotorgin;
        m_worldposition = source.m_worldposition;
        m_rotationmatrix = source.m_rotationmatrix;
        m_projectionmatrix = source.m_projectionmatrix;
        m_voxelsizematrix = source.m_voxelsizematrix;
        m_rotateorginmatrix = source.m_rotateorginmatrix;
        m_orientationmatrix = source.m_orientationmatrix;
        m_iorientationmatrix = source.m_iorientationmatrix;
        m_worldmatrix = source.m_worldmatrix;
        m_finalmatrix = source.m_finalmatrix;
        m_ifinalmatrix = source.m_ifinalmatrix;
        
        //setup dimensions
        setupdimensions(dim,source.m_numcolors);
        setrotateorgin(source.m_rotorgin); //look at this more...

        //setup data, then copy the data
        if (shared){
            cleandata();
            m_data=(VoxelType*)source.m_data;
            m_sharedcounter=source.m_sharedcounter;
            m_externaldataflag=source.m_externaldataflag;
            (*m_sharedcounter)++;
        } else {
            setupdata();
            setvalue(0); //is this needed?
                        
            //copy data according to selection
            T* newdata = source.m_data;
            NDL_FOREACH2PL(*this,source){
                int index,imindex,validflag;
                NDL_GETINDEXPAIR(validflag,index,imindex);
                if (validflag) m_data[index] = VoxelType(newdata[imindex]);
            } NDL_ENDFOREACH
        }
        
        //save some vars
        m_issaved = source.m_issaved;
        m_isloaded = source.m_isloaded;
        m_min = source.m_min;
        m_max = source.m_max;
        m_attemptabort = source.m_attemptabort;
        m_progress = source.m_progress;
        setprogressupdatefunction(source.updatefunction);
        setprogressstartfunction(source.startfunction);
        setprogressendfunction(source.endfunction);
        m_filetype = source.m_filetype;
        
        m_sequenceflag=source.m_sequenceflag;
    }

    //*********************
    //Constructors
    //*********************
    Image(){ InitImage(); };
    Image(int dim[DIM]){ InitImage(); setupdimensions(dim,1); setupdata(); } //create a new image
    Image(int dim[DIM],int numcolors){ InitImage(); setupdimensions(dim,numcolors); setupdata(); }
    Image(int dim[DIM],int numcolors,VoxelType* data){ InitImage(); setupdimensions(dim,numcolors); setupshareddata(data); } //create a new image that shares data from an external source
    Image(std::string filename,std::string flags="",bool loadsequence=false,void* headerpntr=0){ InitImage(); loadfile(filename,flags,loadsequence,headerpntr); } //load a predefined filetype
    Image(char* filename,char* flags="",bool loadsequence=false,void* headerpntr=0){ InitImage(); loadfile(filename,flags,loadsequence,headerpntr); } //load a predefined filetype
    
    //Convienince Constructors for 1->4 dimensions
    Image(int width,int numcolors){ InitImage(); assert(DIM>=1); int dim[DIM]; dim[0]=width; for(int i=1;i<DIM;i++) dim[i]=1; setupdimensions(dim,numcolors); setupdata(); } //create a new image
    Image(int width,int height,int numcolors){ InitImage(); assert(DIM>=2); int dim[DIM]; dim[0]=width; dim[1]=height; for(int i=2;i<DIM;i++) dim[i]=1; setupdimensions(dim,numcolors); setupdata(); } //create a new image
    Image(int width,int height,int depth,int numcolors){ InitImage(); assert(DIM>=3); int dim[DIM]; dim[0]=width; dim[1]=height; dim[2]=depth; for(int i=3;i<DIM;i++) dim[i]=1; setupdimensions(dim,numcolors); setupdata(); } //create a new image
    Image(int width,int height,int depth,int timesteps,int numcolors){ InitImage(); assert(DIM>=4); int dim[DIM]; dim[0]=width; dim[1]=height; dim[2]=depth; dim[3]=timesteps; for(int i=4;i<DIM;i++) dim[i]=1; setupdimensions(dim,numcolors); setupdata(); } //create a new image
        
    //default copy constructor
    Image(Image<VoxelType,DIM>& im,bool shared=false){
        InitImage();
        imagecopy(im,im.m_dimarray,shared); 
    }

    //copy constructor for other image types with different dimension
    template<class T,int CDIM> Image(Image<T,CDIM>& im,bool shared=false){
        InitImage();
        //std::cout << "IMAGE.copyconstructor (other type, other dim)\n"; 
        int dim[DIM];
        if (DIM>=CDIM){
            for(int i=0;i<CDIM;i++) dim[i]=im.m_dimarray[i];
            for(int i=CDIM;i<DIM;i++) dim[i]=1;
        } else {
            for(int i=0;i<DIM;i++) dim[i]=im.m_dimarray[i];
            for(int i=DIM;i<CDIM;i++) dim[DIM-1]*=im.m_dimarray[i]; //just add the data to the last dimension of this image
        }
        imagecopy(im,dim,shared);
    }
    
    //copy constructor for other image types
    //same dimension
    template<class T> Image(Image<T,DIM>& im){
        InitImage(); 
        //std::cout << "IMAGE.copyconstructor (other type)\n"; 
        imagecopy(im,im.m_dimarray); 
    }

    //destructor
    ~Image(){
        removeviews();
        if (m_numvoxels){
            cleandata();
        }
    }
    
    //*********************
    //progress functions
    //*********************
    void setprogressstartfunction(void (*newstartfunction)()){
        startfunction = newstartfunction;
    }
    void setprogressupdatefunction(void (*newupdatefunction)(char* text,int i)){
        updatefunction = newupdatefunction;
    }
    void setprogressendfunction(void (*newendfunction)()){
        endfunction = newendfunction;
    }
    void updateprogress(std::string text,int i){
        m_progress = i;
        if (m_progress==0 && startfunction) startfunction();
        if (updatefunction) updatefunction((char*)text.c_str(),i);
        if (m_progress>=100 && endfunction){
            m_progress=0;
            endfunction();
        }
    }
    
    //*********************
    //matrix functions
    //*********************
    void setupworldmatricies(){
        //Transformation to move from voxel to world coords
        //~ m_irotateorginmatrix
        m_orientationmatrix = m_worldmatrix * m_rotationmatrix * m_rotateorginmatrix * m_voxelsizematrix;
        m_ifinalmatrix = m_iorientationmatrix = m_orientationmatrix.GetInverse();
        m_finalmatrix = m_projectionmatrix * m_orientationmatrix;
        m_matrixrecalculateflag = false;
    }
    
    void worldtoimagematricies(Image<VoxelType,DIM> &im){
        //Transformation to move world to new image voxel coords
        m_finalmatrix = im.m_iorientationmatrix * m_finalmatrix;
        //~ m_ifinalmatrix = m_finalmatrix.GetInverse();
        m_ifinalmatrix = m_iorientationmatrix * im.m_orientationmatrix;
        //~ m_finalmatrix = m_ifinalmatrix.GetInverse();
        m_matrixrecalculateflag = false;
    }
        
    //*********************
    //Scale/Zoom functions
    //*********************
    void resetvoxelsize(){ 
        m_voxelsizematrix.SetScale(m_voxelsize);
        m_matrixrecalculateflag=true;
    }
    void setscale(Vector<double,DIM>& scalevector){
        m_scalevector = scalevector;
        m_matrixrecalculateflag=true;
    }
    bool isisotropic(){
        float s = m_voxelsize[0];
        for(int i=1;i<DIM;i++) if (m_voxelsize[i]!=s) return false;
        return true;
    }
    
    void scaleisotropic(int fixeddimension=0){ 
        for(int i=0;i<DIM;i++) m_scalevector[i] *= m_voxelsize[i]/m_voxelsize[fixeddimension];
        m_matrixrecalculateflag=true;
    }
    void scale(Vector<double,DIM>& scalevector){
        m_scalevector *= scalevector;
        m_matrixrecalculateflag=true;
    }
    void zoom(double factor){
        Vector<double,DIM> tvector;
        for(int i=0;i<DIM;i++) tvector[i] = factor;
        m_scalevector *= tvector;
        m_matrixrecalculateflag=true;
    }
    
    //*********************
    //Translation functions
    //*********************
    void resetworldposition(){
        for(int i=0;i<DIM;i++) m_worldposition[i] = 0;
        m_worldmatrix.SetTranslate(m_worldposition);
        m_matrixrecalculateflag=true; 
    }
    void setworldposition(Vector<double,DIM> &worldposition){ 
        m_worldposition = worldposition; 
        m_worldmatrix.SetTranslate(m_worldposition);
        m_matrixrecalculateflag=true;
    }
    void translate(Vector<double,DIM> &translatevector){ 
        Matrix<double,DIM+1> tempmatrix;
        tempmatrix.SetTranslate(translatevector);
        m_worldposition += translatevector;
        m_worldmatrix = tempmatrix * m_worldmatrix;
        m_matrixrecalculateflag=true;
    }
    
    //*********************
    //Rotation functions
    //*********************
    void resetrotateorgin(){ 
        for(int i=0;i<DIM;i++){
            m_rotorgin[i]=(m_orgin[i] + (double)m_regionsize[i]/2.0)*m_voxelsize[i];
        }
        m_rotateorginmatrix.SetTranslate(-m_rotorgin);
        m_matrixrecalculateflag=true;
    }
    void setrotateorgin(Vector<double,DIM>& rotorgin){
        m_rotorgin = rotorgin; 
        m_rotateorginmatrix.SetTranslate(-m_rotorgin);
        m_matrixrecalculateflag=true;
    }
    void resetrotate(){
        m_rotationmatrix.SetIdentity(); 
        m_matrixrecalculateflag=true; 
    }
    void setrotate(double theta=0,int axis1=0,int axis2=1){
        m_rotationmatrix.SetRotate(theta,axis1,axis2);
        m_matrixrecalculateflag = true;
    }
    void rotate(double theta=0,int axis1=0,int axis2=1){
        Matrix<double,DIM+1> tempmatrix;
        tempmatrix.SetRotate(theta,axis1,axis2);
        m_rotationmatrix=tempmatrix*m_rotationmatrix;
        m_matrixrecalculateflag = true;
    }
    void shear(double amount=0,int axis1=0,int axis2=1){
        Matrix<double,DIM+1> tempmatrix;
        tempmatrix.SetShear(amount,axis1,axis2);
        m_rotationmatrix=tempmatrix*m_rotationmatrix;
        m_matrixrecalculateflag = true;
    }
    
    //*********************
    //Projection functions
    //*********************
    void setorthoprojection(int axis1=0,int axis2=1){
        m_projectionmatrix.SetIdentity(); 
        m_matrixrecalculateflag = true;
    }
    void setprojection(VoxelType Dist,int Axis=0){
        m_projectionmatrix.SetProjection(Dist,Axis);
        m_matrixrecalculateflag = true;
    }
    
    //*******************************************************************
    // Assignment, Casting, and Element Selection Operator Overloads
    //*******************************************************************
    
    //indexing
    typename ImageIndexer<VoxelType,DIM,DIM>::NEXTELEMENTTYPE operator[](const unsigned int i){ return ImageIndexer<VoxelType,DIM,DIM>(0,this).operator[](i*m_dimPROD[0]); }
    VoxelType& operator()(const unsigned int i){ return m_data[i]; }
    template<class T> VoxelType& operator()(T* coord){
        int index = 0;
        for(int i=0;i<DIM;i++){
            index+=m_dimPROD[i]*(int)coord[i];
        }
        return m_data[index];
    }
    
    //assignment overload
    template<class T>
    Image<VoxelType,DIM>& operator=(Image<T,DIM>& source){
        imagecopy(source,source.m_dimarray);
        return *this;
    }
        
    //*********************************************************************
    // Viewer functions
    //*********************************************************************
    ViewInfo<VoxelType,DIM>* addview(std::string flags="",int rendertype=0){
        return ViewInfo<VoxelType,DIM>::openview(*this,rendertype,flags);
    }
    
    void refreshviews(){
        for (std::vector<ViewInfo<VoxelType,DIM>*>::iterator it = m_viewlist.begin();it != m_viewlist.end(); it++){
            (*it)->refresh();
        }
    }
    
    void removeview(ViewInfo<VoxelType,DIM>* view,std::string flags=""){
        std::vector<ViewInfo<VoxelType,DIM>*>::iterator it = std::find( m_viewlist.begin(), m_viewlist.end(), view );
        if (it==m_viewlist.end()) return;
        m_viewlist.erase(it); //remove from the list of views for this instance of this class
        view->closeview(flags); //close the view
    }
    
    void removeviews(){
        std::vector<ViewInfo<VoxelType,DIM>*>::iterator it = m_viewlist.begin();
        while(it != m_viewlist.end()){
            removeview(*it);
            it = m_viewlist.begin();
        }
    }
    
    void viewloop(){
        int numopen;
        do {
            numopen=0;
            for (std::vector<ViewInfo<VoxelType,DIM>*>::iterator it = m_viewlist.begin();it != m_viewlist.end(); it++){
                ViewInfo<VoxelType,DIM>* view = *it;
                if (!view->isviewclosed()) numopen++;
            }
            SleepInMilliseconds(200);
        } while (numopen>0);
    }
    
    void addviewloop(std::string flags="",int rendertype=0){
        addview(flags,rendertype);
        viewloop();
    }
        
    //**********************************************************************
    //selection functions
    //**********************************************************************
    void select(int x[],int regionsize[]){
        for(int n=0;n<DIM;n++){
            x[n] = m_orgin[n] = CLAMP(x[n],0,m_dimarray[n]-1);
            if (regionsize[n]<=0) regionsize[n] = m_regionsize[n] = m_dimarray[n]-m_orgin[n];
            else regionsize[n] = m_regionsize[n] = (std::min)(regionsize[n],m_dimarray[n]-m_orgin[n]);
        }
        m_numregionvoxels=calculatenumvoxels(m_dimPRODregion,m_regionsize,m_numcolors);
    }
    
    //convienince function for DIM 1-4
    void select(int x1=0,int x2=0,int x3=0,int x4=0,int x5=0,int x6=0,int x7=0,int x8=0){
        if (DIM==1){
            m_orgin[0]=x1;
            if (x2>0) m_regionsize[0]=x2; else m_regionsize[0]=m_dimarray[0]-m_orgin[0];
        }
        if (DIM==2){
            m_orgin[0]=x1;
            m_orgin[1]=x2;
            if (x3>0) m_regionsize[0]=x3; else m_regionsize[0]=m_dimarray[0]-m_orgin[0];
            if (x4>0) m_regionsize[1]=x4; else m_regionsize[1]=m_dimarray[1]-m_orgin[1];
        }
        if (DIM==3){
            m_orgin[0]=x1;
            m_orgin[1]=x2;
            m_orgin[2]=x3;
            if (x4>0) m_regionsize[0]=x4; else m_regionsize[0]=m_dimarray[0]-m_orgin[0];
            if (x5>0) m_regionsize[1]=x5; else m_regionsize[1]=m_dimarray[1]-m_orgin[1];
            if (x6>0) m_regionsize[2]=x6; else m_regionsize[2]=m_dimarray[2]-m_orgin[2];
        }
        if (DIM==4){
            m_orgin[0]=x1;
            m_orgin[1]=x2;
            m_orgin[2]=x3;
            m_orgin[2]=x4;
            if (x5>0) m_regionsize[0]=x5; else m_regionsize[0]=m_dimarray[0]-m_orgin[0];
            if (x6>0) m_regionsize[1]=x6; else m_regionsize[1]=m_dimarray[1]-m_orgin[1];
            if (x7>0) m_regionsize[2]=x7; else m_regionsize[2]=m_dimarray[2]-m_orgin[2];
            if (x8>0) m_regionsize[3]=x7; else m_regionsize[3]=m_dimarray[3]-m_orgin[3];
        }
        select(m_orgin,m_regionsize);
    }
    
    void saveselection(int savedorgin[DIM],int savedregion[DIM]){
        for(int i=0;i<DIM;i++){
            savedorgin[i]=m_orgin[i];
            savedregion[i]=m_regionsize[i];
        }
    }

    //************************************************
    // SAMPLING FUNCTIONS
    //************************************************
    template<class T,int CDIM>
    void copyregion(Image<T,CDIM> &im){
        T* imdata = im.m_data;
        NDL_FOREACH2PL(*this,im){
            int index,imindex,validflag;
            NDL_GETINDEXPAIR(validflag,index,imindex);
            if (validflag) m_data[index] = (VoxelType)imdata[imindex];
        } NDL_ENDFOREACH
    }
    
    void getcorners(VoxelType corners[Power<2,DIM>::value]){
        for(int a=0;a<Power<2,DIM>::value;a++){
            int index = 0;
            for(int n=0;n<DIM;n++){
                int i = ((a >> n) & 1);
                index+=(i*(m_regionsize[n]-1)+m_orgin[n])*m_dimPROD[n];
            }
            corners[a] = m_data[index];
        }
    }
    
    //***********************
    // STATISTICAL FUNCTIONS
    //***********************
    //get the number of voxels within a given bound (lower,upper)
    bool isinrange(VoxelType lower,VoxelType upper){ return isinrange(lower,upper,*this,(VoxelType)0,(VoxelType)-1); }
    template<class T> int isinrange(VoxelType lower,VoxelType upper,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        bool dothreshold = (threshold2>=threshold1);
        T* maskdata = maskimage.m_data;
        NDL_FOREACH(*this){
            int index;
            NDL_GETINDEX(index);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            if (m_data[index]<lower || m_data[index]>upper) return false;
        } NDL_ENDFOREACH
        return true;
    }

    //get the number of voxels within a given bound (lower,upper)
    int numinrange(VoxelType lower,VoxelType upper){ return numinrange(lower,upper,*this,(VoxelType)0,(VoxelType)-1); }
    template<class T> int numinrange(VoxelType lower,VoxelType upper,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        bool dothreshold = (threshold2>=threshold1);
        T* maskdata = maskimage.m_data;
        int inrange=0;
        //~ NDL_FOREACHPL(*this){ //has errors
        NDL_FOREACH(*this){
            int index;
            NDL_GETINDEX(index);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            if (m_data[index]>=lower && m_data[index]<=upper) inrange++;
        } NDL_ENDFOREACH
        return inrange;
    }
    
    //calculate min/max
    void minmax(){ VoxelType tmin,tmax; minmax(tmin,tmax); }
    void minmax(VoxelType &minvalue,VoxelType& maxvalue){ minmax(minvalue,maxvalue,*this,(VoxelType)0,(VoxelType)-1); }
    template<class T> void minmax(VoxelType &minvalue,VoxelType& maxvalue,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        bool dothreshold = (threshold2>=threshold1);
        bool firstflag=true;
        minvalue=maxvalue=0; //set default value
        T* maskdata = maskimage.m_data;
        //~ NDL_FOREACHPL(*this){ //has errors?
        NDL_FOREACH(*this){
            int index;
            NDL_GETINDEX(index);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            VoxelType temp = m_data[index];
            if (firstflag){ minvalue=maxvalue=temp; firstflag=false; }
            if (temp <minvalue) minvalue = temp;
            if (temp >maxvalue) maxvalue = temp;
        } NDL_ENDFOREACH
        m_min=minvalue;
        m_max=maxvalue;
    }
    
    //calculate median/percentile (between a range if desired)
    void minmax99(float percentile=0.99){ m_min=getpercentile(1 - percentile); m_max=getpercentile(percentile); }
    VoxelType getmedian(){ return getpercentile(0.5); }
    template<class T> VoxelType getmedian(Image<T,DIM>& maskimage,T threshold1,T threshold2){ return getpercentile(0.5,maskimage,threshold1,threshold2); }
    VoxelType getpercentile(float percentile,VoxelType threshold1=0,VoxelType threshold2=-1){ return getpercentile(percentile,*this,threshold1,threshold2); }
    template<class T> VoxelType getpercentile(float percentile,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        VoxelType* imdata = allocate<VoxelType>(m_numregionvoxels,__LINE__);
        T* maskdata = maskimage.m_data;
        bool dothreshold = (threshold2>=threshold1);
        
        int num=0;
        NDL_FOREACH(*this){
            int index;
            NDL_GETINDEX(index);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            imdata[num++]=m_data[index];
        } NDL_ENDFOREACH

        VoxelType result;
        if (num==0){ result=0; }
        else if (num==1){ result=imdata[0]; }
        else {
            VoxelType* begin = imdata;
            VoxelType* tresult = &imdata[(int)((num-1)*CLAMP(percentile,0,1))];
            VoxelType* end = &imdata[num-1];
            std::nth_element(begin,tresult,end);
            result = *tresult;
        }
        deallocate(imdata);
        return result;
    }
    
    //calculate MAD - median absolute deviation  (between a range if desired)
    VoxelType getmad(VoxelType median,VoxelType threshold1=0,VoxelType threshold2=-1){ return getmad(median,*this,threshold1,threshold2); }
    template<class T> VoxelType getmad(VoxelType median,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        VoxelType* imdata = allocate<VoxelType>(m_numregionvoxels,__LINE__);
        T* maskdata = maskimage.m_data;
        bool dothreshold = (threshold2>=threshold1);
        
        int num=0;
        NDL_FOREACH(*this){
            int index;
            NDL_GETINDEX(index);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            imdata[num++]=abs(m_data[index]-median);
        } NDL_ENDFOREACH

        VoxelType result;
        if (num==0){ result=0; }
        else if (num==1){ result=imdata[0]; }
        else {
            VoxelType* begin = imdata;
            VoxelType* tresult = &imdata[(int)((num-1)*0.5)];
            VoxelType* end = &imdata[num-1];
            std::nth_element(begin,tresult,end);
            result = *tresult;
        }
        deallocate(imdata);
        return result;
    }
    
    
    //calculate mean/mean_wcount (between a range if desired)
    VoxelType getmean(VoxelType threshold1=0,VoxelType threshold2=-1){ int count; return getmean_wcount(count,*this,threshold1,threshold2); }
    template<class T> VoxelType getmean(Image<T,DIM>& maskimage,T threshold1,T threshold2){ int count; return getmean_wcount(count,maskimage,threshold1,threshold2); }
    VoxelType getmean_wcount(int& count,VoxelType threshold1=0,VoxelType threshold2=-1){ return getmean_wcount(count,*this,threshold1,threshold2); }
    template<class T> VoxelType getmean_wcount(int& count,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        VoxelType value = 0;
        count = 0;
        bool dothreshold = (threshold2>=threshold1);
        T* maskdata = maskimage.m_data;
        //~ NDL_FOREACHPL(*this){
        NDL_FOREACH(*this){
            int index;
            NDL_GETINDEX(index);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            value += m_data[index];
            count++;
        } NDL_ENDFOREACH
        if (count){
            value = value/count;
        } else value=0;
        return value;
    }
    
    //calculate mean/mean_wcount (between a range if desired)
    VoxelType getave(VoxelType threshold1=0,VoxelType threshold2=-1){ int count; float sum = getsum_wcount(count,*this,threshold1,threshold2); if (count) return sum/count; else return 0; }
    VoxelType getsum(VoxelType threshold1=0,VoxelType threshold2=-1){ int count; return getsum_wcount(count,*this,threshold1,threshold2); }
    template<class T> VoxelType getsum(Image<T,DIM>& maskimage,T threshold1,T threshold2){ int count; return getsum_wcount(count,maskimage,threshold1,threshold2); }
    VoxelType getsum_wcount(int& count,VoxelType threshold1=0,VoxelType threshold2=-1){ return getsum_wcount(count,*this,threshold1,threshold2); }
    template<class T> VoxelType getsum_wcount(int& count,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        VoxelType value = 0;
        count = 0;
        bool dothreshold = (threshold2>=threshold1);
        T* maskdata = maskimage.m_data;
        //~ NDL_FOREACHPL(*this){
        NDL_FOREACH(*this){
            int index;
            NDL_GETINDEX(index);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            value += m_data[index];
            count++;
        } NDL_ENDFOREACH
        return value;
    }    
    
    //calculate stdev (between a range if desired)
    VoxelType getstdev(VoxelType mean,VoxelType threshold1=0,VoxelType threshold2=-1){ return getstdev(mean,*this,threshold1,threshold2); }
    template<class T> VoxelType getstdev(VoxelType mean,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        VoxelType value = 0;
        VoxelType delta;
        int count = 0;
        bool dothreshold = (threshold2>=threshold1);
        T* maskdata = maskimage.m_data;
        //~ NDL_FOREACHPL(*this){
        NDL_FOREACH(*this){
            int index;
            NDL_GETINDEX(index);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            delta=(m_data[index]-mean);
            value += delta*delta;
            count++;
        } NDL_ENDFOREACH
        if (count>1){
            value = sqrt(value/(count-1));
        } else value=0;
        return value;
    }
    
    
    //calculate stdev (between a range if desired)
    VoxelType getnorm(VoxelType threshold1=0,VoxelType threshold2=-1){ return getnorm(*this,threshold1,threshold2); }
    template<class T> VoxelType getnorm(Image<T,DIM>& maskimage,T threshold1,T threshold2){
        VoxelType value = 0;
        VoxelType delta;
        bool dothreshold = (threshold2>=threshold1);
        T* maskdata = maskimage.m_data;
        //~ NDL_FOREACHPL(*this){
        NDL_FOREACH(*this){
            int index;
            NDL_GETINDEX(index);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            delta=m_data[index];
            value += delta*delta;
        } NDL_ENDFOREACH
        value = sqrt(value);
        return value;
    }
    
    //calculate AAD - average absolute deviation (between a range if desired)
    VoxelType getaad(VoxelType mean,VoxelType threshold1=0,VoxelType threshold2=-1){ return getaad(mean,*this,threshold1,threshold2); }
    template<class T> VoxelType getaad(VoxelType mean,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        VoxelType value = 0;
        VoxelType delta;
        int count = 0;
        bool dothreshold = (threshold2>=threshold1);
        T* maskdata = maskimage.m_data;
        //~ NDL_FOREACHPL(*this){
        NDL_FOREACH(*this){
            int index;
            NDL_GETINDEX(index);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            delta=(m_data[index]-mean);
            value += abs(delta);
            count++;
        } NDL_ENDFOREACH
        if (count>1){
            value /= (count-1);
        } else value=0;
        return value;
    }
    
    //calculate skewness (between a range if desired)
    VoxelType getskewness(VoxelType mean,VoxelType stdev,VoxelType threshold1=0,VoxelType threshold2=-1){ return getskewness(mean,stdev,*this,threshold1,threshold2); }
    template<class T> VoxelType getskewness(VoxelType mean,VoxelType stdev,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        VoxelType value = 0;
        VoxelType delta;
        int count = 0;
        bool dothreshold = (threshold2>=threshold1);
        VoxelType stdev3 = stdev*stdev*stdev;
        T* maskdata = maskimage.m_data;
        NDL_FOREACH(*this){
        //~ NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            delta=(m_data[index]-mean);
            value += delta*delta*delta;
            count++;
        } NDL_ENDFOREACH
        if (count>1){
            value = value/(stdev3*(count-1));
        } else value=0;
        return value;
    }
    
    //calculate kurtosis (between a range if desired)
    VoxelType getkurtosis(VoxelType mean,VoxelType stdev,VoxelType threshold1=0,VoxelType threshold2=-1){ return getkurtosis(mean,stdev,*this,threshold1,threshold2); }
    template<class T> VoxelType getkurtosis(VoxelType mean,VoxelType stdev,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        VoxelType value = 0;
        VoxelType delta;
        int count = 0;
        bool dothreshold = (threshold2>=threshold1);
        VoxelType stdev4 = stdev*stdev*stdev*stdev;
        T* maskdata = maskimage.m_data;
        NDL_FOREACH(*this){
        //~ NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            delta=(m_data[index]-mean);
            value += delta*delta*delta*delta;
            count++;
        } NDL_ENDFOREACH
        if (count>1){
            value = value/(stdev4*(count-1));
        } else value=0;
        return value;
    }
        
    //calculate spatial mean (center of mass)
    Vector<float,DIM> getspatialmean(VoxelType threshold1=0,VoxelType threshold2=-1){ return getspatialmean(*this,threshold1,threshold2); }
    template<class T> Vector<float,DIM> getspatialmean(Image<T,DIM>& maskimage,T threshold1,T threshold2){
        float value[DIM];
        for(int i=0;i<DIM;i++) value[i] = 0;
        int count = 0;
        bool dothreshold = (threshold2>=threshold1);
        T* maskdata = maskimage.m_data;
        NDL_FOREACH(*this){
        //~ NDL_FOREACHPL(*this){
            float tempvector[DIM];
            int index;
            NDL_GETCOORD(index,tempvector);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            for(int i=0;i<DIM;i++) value[i] += tempvector[i];
            count++;
        } NDL_ENDFOREACH
        if (count){
            for(int i=0;i<DIM;i++) value[i] /= count;
        }
        return value;
    }
    
    //calculate spatial stdev (stdev from center of mass)
    Vector<float,DIM> getspatialstdev(Vector<float,DIM> spatialmean,VoxelType threshold1=0,VoxelType threshold2=-1){ return getspatialstdev(spatialmean,*this,threshold1,threshold2); }
    template<class T> Vector<float,DIM> getspatialstdev(Vector<float,DIM> spatialmean,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        Vector<float,DIM> value;
        Vector<float,DIM> tempvector;
        Vector<float,DIM> delta;
        int count = 0;
        bool dothreshold = (threshold2>=threshold1);
        T* maskdata = maskimage.m_data;
        NDL_FOREACH(*this){
        //~ NDL_FOREACHPL(*this){
            int index;
            NDL_GETCOORD(index,tempvector);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            delta=(tempvector-spatialmean);
            for(int i=0;i<DIM;i++){ value[i] += delta[i]*delta[i]; }
            count++;
        } NDL_ENDFOREACH
        if (count>1){
            for(int i=0;i<DIM;i++){ value[i] = sqrt(value[i]/(count-1)); }
        }
        return value;
    }

    //calculate spatial skewness (skewness from center of mass)
    Vector<float,DIM> getspatialskewness(Vector<float,DIM> spatialmean,Vector<float,DIM> spatialstdev,VoxelType threshold1=0,VoxelType threshold2=-1){ return getspatialskewness(spatialmean,spatialstdev,*this,threshold1,threshold2); }
    template<class T> VoxelType getspatialskewness(Vector<float,DIM> spatialmean,Vector<float,DIM> spatialstdev,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        Vector<float,DIM> value;
        Vector<float,DIM> tempvector;
        Vector<float,DIM> delta;
        int count = 0;
        bool dothreshold = (threshold2>=threshold1);
        Vector<float,DIM> spatialstdev3 = spatialstdev*spatialstdev*spatialstdev;
        T* maskdata = maskimage.m_data;
        NDL_FOREACH(*this){
        //~ NDL_FOREACHPL(*this){
            int index;
            NDL_GETCOORD(index,tempvector);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            delta=(tempvector-spatialmean);
            for(int i=0;i<DIM;i++){ value[i] += delta[i]*delta[i]*delta[i]; }
            count++;
        } NDL_ENDFOREACH
        if (count>1){
            for(int i=0;i<DIM;i++){ float den=spatialstdev3*(count-1); value[i]/=den; }
        }
        return value;
    }

    //calculate spatial kurtosis (kurtosis from center of mass)
    Vector<float,DIM> getspatialkurtosis(Vector<float,DIM> spatialmean,Vector<float,DIM> spatialstdev,VoxelType threshold1=0,VoxelType threshold2=-1){ return getspatialkurtosis(spatialmean,spatialstdev,*this,threshold1,threshold2); }
    template<class T> VoxelType getspatialkurtosis(Vector<float,DIM> spatialmean,Vector<float,DIM> spatialstdev,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        Vector<float,DIM> value;
        Vector<float,DIM> tempvector;
        Vector<float,DIM> delta;
        int count = 0;
        bool dothreshold = (threshold2>=threshold1);
        Vector<float,DIM> spatialstdev4 = spatialstdev*spatialstdev*spatialstdev*spatialstdev;
        T* maskdata = maskimage.m_data;
        NDL_FOREACH(*this){
        //~ NDL_FOREACHPL(*this){
            int index;
            NDL_GETCOORD(index,tempvector);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            delta=(tempvector-spatialmean);
            for(int i=0;i<DIM;i++){ value[i] += delta[i]*delta[i]*delta[i]*delta[i]; }
            count++;
        } NDL_ENDFOREACH
        if (count>1){
            for(int i=0;i<DIM;i++){ float den=spatialstdev4*(count-1); value[i]/=den; }
        }
        return value;
    }
    
    //make a histogram
    void gethistogram(Image<unsigned int,1> &histogram,int numbins=100,VoxelType threshold1=0,VoxelType threshold2=-1){ gethistogram(histogram,numbins,*this,threshold1,threshold2); }
    template<class T> void gethistogram(Image<unsigned int,1> &histogram,int numbins,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        //init stuff
        if (numbins<=0) return;

        if (histogram.isempty() || numbins!=histogram.m_numvoxels){ histogram.setupdimensions(&numbins,1); histogram.setupdata(); }

        //init the histogram
        histogram.setvalue(0);
        unsigned int* res = histogram.data();
        bool dothreshold = (threshold2>=threshold1);
        T* maskdata = maskimage.m_data;
        
        VoxelType vmin,vmax,vrange;
        minmax(vmin,vmax);
        vrange = vmax - vmin;
        if (vmin<vmax){
            NDL_FOREACHPL(*this){
                int index;
                NDL_GETINDEX(index);
                if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
                int pos = (int)((m_data[index]-vmin)*(numbins-1)/vrange);
                if (pos>=0 && pos<(int)numbins) ++res[pos];
            } NDL_ENDFOREACH
        } else res[0]+=m_numvoxels;
    }
    
    //calculate k means (output to outputmeans)
    //optionally get the (k-1) thresholds as output as well (output to outputthresholds)
    //the calculation is histogram based, numbins specifies the number of histogram bins
    //NOTE: should we add threshold1 and threshold2 for limit scope of operator
    
    //NOTE: WE CAN ALSO DO KMEANS SPATIALLY
    bool kmeans(int k,VoxelType* outputmeans,VoxelType* outputthresholds=0,int numbins=100){
        //only valid for k>=2
        if (k<2) return false;
        
        //generate a histogram
        Image<unsigned int,1> histogram;
        gethistogram(histogram,numbins); //calculate histogram
        unsigned int* histdata = histogram.m_data;
        float total=0;
        for (int i=0;i<numbins;i++) total+=histdata[i];

        //declare vars
        VoxelType *thresholdvalue;
        if (outputthresholds==0) thresholdvalue = allocate<VoxelType>(k-1,__LINE__);
        else thresholdvalue = outputthresholds;
        int *threshold = allocate<int>(k-1,__LINE__);
        int *count = allocate<int>(k,__LINE__);
        int *mean = allocate<int>(k,__LINE__);
        VoxelType *meanvalue;
        meanvalue = outputmeans;
        
        //histogram spacing to seed the k-means thresholds
        float t=0;
        for (int i=0,c=0;i<numbins;i++){
            t+=histdata[i];
            while(t/total > (float)c/(float)k){
                threshold[c] = i;
                c++;
            }
            if (c>=k-1) break;
        }
        
        //do k-means on the histogram
        while(1){
            //calulate means
            for(int c=0;c<k;c++){ count[c]=0; mean[c]=0; }
            for (int i=0,c=0;i<numbins;i++){
                count[c]+=histdata[i];
                mean[c]+=i*histdata[i];
                if (c<k-1 && i>threshold[c]) c++;
            }
            for(int c=0;c<k;c++){
                if (count[c]) mean[c]/=(float)count[c];
                else {
                    if (c==0) mean[c]=(threshold[c]+0)*0.5;
                    else if (c==k-1) mean[c]=(numbins-1+threshold[c-1])*0.5;
                    else mean[c] = (threshold[c]+threshold[c-1])*0.5;
                }
            }
            
            //update thresholds
            bool breakout=true;
            for(int c=0;c<k-1;c++){
                int pthreshold=threshold[c];
                threshold[c]=0.5*(mean[c]+mean[c+1]);
                if (threshold[c]!=pthreshold) breakout=false;
            }
            
            //breakout when converged
            if (breakout) break;
        }
        
        //now calculate the intensity of each threshold
        for(int c=0;c<k-1;c++){
            thresholdvalue[c]=(((VoxelType)threshold[c])/((VoxelType)(numbins-1)))*(m_max - m_min) + m_min;
        }
        
        //now calculate the mean value of each region
        for(int c=0;c<k;c++){
            meanvalue[c] = (((VoxelType)mean[c])/((VoxelType)(numbins-1)))*(m_max - m_min) + m_min;
        }
        
        deallocate(threshold);
        deallocate(count);
        deallocate(mean);
        if (outputthresholds==0) deallocate(thresholdvalue);

        return true;
    }
    
    void orthoproject(int axis,Image<VoxelType,DIM> &im,VoxelType threshold1=0,VoxelType threshold2=-1){ _orthoproject(axis,im,0,*this,threshold1,threshold2); }
    template<class T> void orthoproject(int axis,Image<VoxelType,DIM> &im,Image<T,DIM>& maskimage,T threshold1=0,T threshold2=-1){ _orthoproject(axis,im,0,maskimage,threshold1,threshold2); }
    template<class T> void orthoproject(int axis,Image<VoxelType,DIM> &im,Image<int,DIM> &imlength,Image<T,DIM>& maskimage,T threshold1=0,T threshold2=-1){ _orthoproject(axis,im,&imlength,maskimage,threshold1,threshold2); }
    template<class T> void _orthoproject(int axis,Image<VoxelType,DIM> &im,Image<int,DIM>* imlength,Image<T,DIM>& maskimage,T threshold1=0,T threshold2=-1){
        //Create the projection Image if need be
        if (im.isempty()){
            int tarray[DIM];
            for(int i=0;i<DIM;i++){
                tarray[i]=m_regionsize[i];
            }
            tarray[axis]=1;
            im.setupdimensions(tarray,m_numcolors);
            im.setupdata();
        }
        im.setvalue(0);

        //init both images to 0
        if (imlength){
            *imlength = im;
            imlength->setvalue(0);
        }
        
        //setup some vars
        VoxelType* imdata = im.m_data;
        T* maskdata = maskimage.m_data;
        bool dothreshold = (threshold2>=threshold1);
        int* lengtharray = 0;
        if (imlength) lengtharray = imlength->m_data;
        int dimprod = m_dimPROD[axis];
        int endposdimprod = m_regionsize[axis]*dimprod;

        //ITERATE OVER THE PROJECTION IMAGE, 
        //THEN ADD UP VALUES ALONG THE LINE
        NDL_FOREACHLINE2PL(im,*this,axis){
            int index,imindex,validflag;
            NDL_GETINDEXPAIR(validflag,imindex,index);
            if (!validflag) continue;
            for (int t=0; t<endposdimprod; t+=dimprod){
                if (dothreshold && (maskdata[index+t]<threshold1 || maskdata[index+t]>threshold2)) continue;
                imdata[imindex]+=m_data[index + t];
                if (lengtharray) lengtharray[imindex]++;
            }
        } NDL_ENDFOREACH
    }
    
    template<int KERNELSIZE>
    void precomputekernalindecies(int indexarray[Power<KERNELSIZE,DIM>::value],int coordarray[DIM*Power<KERNELSIZE,DIM>::value],int levelarray[Power<KERNELSIZE,DIM>::value],int indexarray_LINE[NumLines<KERNELSIZE,DIM>::value],int indexarray_DELTA[NumLines<KERNELSIZE,DIM>::value],int dimensionarray[NumLines<KERNELSIZE,DIM>::value]){
        //compute index for each voxel in the kernel
        int count=0;
        for(int a=0;a<Power<KERNELSIZE,DIM>::value;a++){
            int dindex=0;
            int level=0;
            for(int n=0;n<DIM;n++){
                int D = powx(KERNELSIZE,n);
                int i = (a / D) % KERNELSIZE - (KERNELSIZE-1)/2; //the subtraction term centers the kernel
                level+=!i;
                coordarray[count++]=i;
                dindex += i*m_dimPROD[n];
            }
            indexarray[a]=dindex;
            levelarray[a]=level;
        }
    
        //compute index of first voxel in each line going through the kernel
        //also save indexarray_DELTA, the delta value to step through the line
        count=0;
        for(int m=0;m<DIM;m++){
            int D = powx(KERNELSIZE,m);
            for(int a=0;a<Power<KERNELSIZE,DIM>::value;a++){
                int dindex=0;
                for(int n=0;n<DIM;n++){
                    int t = powx(KERNELSIZE,n);
                    int i = (a / t) % KERNELSIZE;
                    if (i && m>=n) goto skip;
                    dindex += i * t;
                }
                indexarray_LINE[count]=dindex;
                indexarray_DELTA[count]=D;
                dimensionarray[count]=m;
                count++;
                skip: ;
            }
        }
    }

    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    //**************************************
    //LOOK AT THESE FUNCTIONS
    //**************************************
    void setuptransformtargetimage(Image<VoxelType,DIM> &im){
        //convert each corner to world coords
        Vector<double,DIM+1> corner[Power<2,DIM>::value];
        for(int a=0;a<Power<2,DIM>::value;a++){
            for(int n=0;n<DIM;n++){
                int i = ((a >> n) & 1);

                //SHOULD THIS CHANGE TO MINUS 1?? eg: corner[a][n]=i*(m_regionsize[n]-1) + m_orgin[n];
                corner[a][n]=i*m_regionsize[n] + m_orgin[n];
                
            }
            corner[a][DIM] = 1;
            //std::cout << "oldcorner:"; corner[a].print();
            corner[a] = m_orientationmatrix * corner[a];
            corner[a].HomoNormalize();
        }
        
        //find min and max corner value for each dimension
        double tmin[DIM];
        double tmax[DIM];
        for(int i=0;i<DIM;i++){ tmin[i]=corner[0][i]; tmax[i]=corner[0][i]; }
        for(int a=0;a<Power<2,DIM>::value;a++){
            //std::cout << "newcorner:"; corner[a].print();
            for(int i=0;i<DIM;i++){
                if (corner[a][i]>tmax[i]) tmax[i] = corner[a][i];
                if (corner[a][i]<tmin[i]) tmin[i] = corner[a][i];
            }
        }

        ////LOOK AT THIS!!!
        ////update selection according to zoom
        //int savedorgin[DIM];
        //int savedregionsize[DIM];
        //for(int i=0;i<DIM;i++){
            //savedorgin[i] = m_orgin[i];
            //savedregionsize[i] = m_regionsize[i];
        //}
                
        //calculate some info about the new image
        int tarray[DIM]; 
        for(int i=0;i<DIM;i++){
            int d1 = 1;
            int d2 = (tmax[i] - tmin[i])*m_scalevector[i] / m_voxelsize[i] + 0.5;
            tarray[i] = (std::max)(d1,d2); //get dimensions of new volume, + 0.5 makes rounding work out
            im.m_voxelsize[i] = m_voxelsize[i] / m_scalevector[i]; //new voxel size
            im.m_units[i] = m_units[i]; //just use previous units
        }
        
        //int torgin[DIM],tregion[DIM];
        //for(int i=0;i<DIM;i++){
            //double temporgin = m_orgin[i]*m_scalevector[i];
            //double tempregion = m_regionsize[i]*m_scalevector[i];
            //double orginmax = tarray[i]-1;
            //double regionmax = tarray[i]-torgin[i];
            //torgin[i] = CLAMP(temporgin,0,orginmax);// + 0.5;
            //tregion[i] = CLAMP(tempregion,1,regionmax);//  + 0.5;
        //}
        
        //printf("T_ORGIN: (%d,%d,%d)\n",torgin[0],torgin[1],torgin[2]);
        //printf("T_REGION: (%d,%d,%d)\n",tregion[0],tregion[1],tregion[2]);
        
        //select(torgin,tregion);
        
        
        ////update selection according to zoom
        //int torgin[DIM],tregion[DIM];
        //for(int i=0;i<DIM;i++){
            //torgin[i] = CLAMP(m_orgin[i]*m_scalevector[i],0,tarray[i]-1);// + 0.5;
            //tregion[i] = CLAMP(m_regionsize[i]*m_scalevector[i],1,tarray[i]);//  + 0.5;
        //}
        //select(torgin,tregion);
        
        
        
        //init the image
        im.setupdimensions(tarray,m_numcolors);
        im.setupdata();
        im.resetvoxelsize(); //calculate new voxel size
        
        
        
        im.resetrotateorgin(); //calculate new image rotation orgin
        //for(int i=0;i<DIM;i++){
            //im.m_rotorgin[i]=(m_orgin[i] + (double)m_regionsize[i]/2.0)*m_voxelsize[i];
        //}
        //im.m_rotateorginmatrix.SetTranslate(-im.m_rotorgin);
        
        
        
        //Vector<double,DIM> translateamount;
        //for(int i=0;i<DIM;i++) translateamount[i] = (im.m_rotorgin[i] - m_rotorgin[i]);
        //im.translate(translateamount); //calculate new world position
        //im.translate(im.m_rotorgin - m_rotorgin); //calculate new world position
        //im.setworldposition(im.m_rotorgin - m_rotorgin); //calculate new world position
        
        Vector<double,DIM> translateamount;
        for(int i=0;i<DIM;i++) translateamount[i] = (im.m_rotorgin[i] - m_rotorgin[i]);
        im.translate(translateamount); //calculate new world position
        
        
        
        
        
        im.setupworldmatricies();//calculate new image matrices
        im.worldtoimagematricies(*this);//calculate new image matrices
        
        m_matrixrecalculateflag=true;
    }
    
    void setuptargetimage_and_matrices(Image<VoxelType,DIM> &im){
        if (!m_matrixrecalculateflag && !im.isempty()) return;
        setupworldmatricies();
        if (im.isempty()) setuptransformtargetimage(im);
        worldtoimagematricies(im);
    }
    
    void resize(Image<VoxelType,DIM> &result,int interpolationtype=NDL_NN){
        Vector<double,DIM> scalevector;
        for(int i=0;i<DIM;i++){
            scalevector[i] = (double)result.m_regionsize[i]/(double)m_regionsize[i];
        }
        setscale(scalevector);
        transform(result,m_min,m_max,interpolationtype);
    }

    void resize(Image<VoxelType,DIM> &result,int newsize[DIM],int interpolationtype=NDL_NN){
        Vector<double,DIM> scalevector;
        for(int i=0;i<DIM;i++){
            scalevector[i] = (double)newsize[i]/(double)m_regionsize[i];
        }
        setscale(scalevector);
        transform(result,m_min,m_max,interpolationtype);
    }

    void constrainedresize(Image<VoxelType,DIM> &result,int dim,int newsize,int interpolationtype=NDL_NN){
        Vector<double,DIM> scalevector;
        for(int i=0;i<DIM;i++){
            scalevector[i] = (double)newsize/(double)m_regionsize[dim];
        }
        setscale(scalevector);
        transform(result,m_min,m_max,interpolationtype);
    }
    
    void transform(Image<VoxelType,DIM> &im,VoxelType clampmin,VoxelType clampmax,int interpolationtype=NDL_LINEAR){
        if (interpolationtype==NDL_NN) _transform<NDL_NN,assignvalue<VoxelType>>(im,clampmin,clampmax); //nearest neighbor
        if (interpolationtype==NDL_LINEAR) _transform<NDL_LINEAR,assignvalue<VoxelType>>(im,clampmin,clampmax); //linear
        if (interpolationtype==NDL_CUBIC) _transform<NDL_CUBIC,assignvalue<VoxelType>>(im,clampmin,clampmax); //cubic (hermite)
    }
    
    template<int INTERPOLATIONTYPE,void T_function(VoxelType&,VoxelType)>
    void _transform(Image<VoxelType,DIM> &im,VoxelType clampmin,VoxelType clampmax){
        setuptargetimage_and_matrices(im);
        //~ if (m_matrixrecalculateflag){
            //~ setupworldmatricies();
            //~ worldtoimagematricies(im);
        //~ }
        
        VoxelType* imdata = im.m_data;
        
        //****************************************
        //precompute indices used in interpolation
        //****************************************
        int indexarray[Power<INTERPOLATIONTYPE,DIM>::value];
        int coordarray[DIM*Power<INTERPOLATIONTYPE,DIM>::value];
        int levelarray[Power<INTERPOLATIONTYPE,DIM>::value];
        int indexarray_LINE[NumLines<INTERPOLATIONTYPE,DIM>::value];
        int indexarray_DELTA[NumLines<INTERPOLATIONTYPE,DIM>::value];
        int dimensionarray[NumLines<INTERPOLATIONTYPE,DIM>::value];
        precomputekernalindecies<INTERPOLATIONTYPE>(indexarray,coordarray,levelarray,indexarray_LINE,indexarray_DELTA,dimensionarray);
        
        //****************************************
        //Iterate over all voxels
        //****************************************
        NDL_FOREACH2PL(im,*this){
            double tempvector[DIM+1];
            int tempindex,imindex;
            NDL_GETINDEX(imindex);
            NDL_GETCOORD_FOR_TRANSFORM(tempindex,tempvector);
            tempvector[DIM]=1;
            
            m_ifinalmatrix.transformpoint(tempvector);

            //load kernel values from current position
            VoxelType values[Power<INTERPOLATIONTYPE,DIM>::value];
            if (INTERPOLATIONTYPE==NDL_NN){
                int index=0;
                for(int n=0;n<DIM;n++){
                    tempvector[n]+=0.5; //add half a pixel so that nn interpolation works properly
                    if (tempvector[n] >= m_dimarray[n]) goto skip;
                    if (tempvector[n] >=0 ) index += (int)tempvector[n] * m_dimPROD[n];
                }
                values[0] = m_data[index];
            } else {
                //calculate index
                int index=0;
                for(int n=0;n<DIM;n++){
                    if (tempvector[n] >= m_dimarray[n]) goto skip;
                    if (tempvector[n] >=0 ) index += (int)tempvector[n] * m_dimPROD[n];
                }
                
                //load the data into an array using precomputed indicies
                for(int a=0;a<Power<INTERPOLATIONTYPE,DIM>::value;++a){
                    int dindex=index+indexarray[a];
                    values[a] = m_data[CLAMP(dindex,0,m_numvoxels-1)];
                }
                
                switch(INTERPOLATIONTYPE){
                    case NDL_LINEAR:{
                        //use the precomputed indecies to do linear interpolation in each dimension
                        for(int q=0;q<NumLines<2,DIM>::value;++q){
                            int m = dimensionarray[q];
                            if (m_dimarray[m]==1){
                                //just pick the one value
                                continue; 
                            }
                            int index1 = indexarray_LINE[q];
                            int index2 = index1+indexarray_DELTA[q];
                            values[index1] = linear_interpolation(tempvector[m]-(int)tempvector[m],values[index1],values[index2]);
                        }
                        
                        //clamp the result
                        values[0] = CLAMP(values[0],clampmin,clampmax);
                        break;
                    }
                    case NDL_CUBIC: {                        
                        //use the precomputed indecies to do cubic interpolation in each dimension
                        for(int q=0;q<NumLines<4,DIM>::value;++q){
                            int m = dimensionarray[q];
                            int index0 = indexarray_LINE[q];
                            int index1 = index0+indexarray_DELTA[q];
                            int index2 = index1+indexarray_DELTA[q];
                            int index3 = index2+indexarray_DELTA[q];
                            if (m_dimarray[m]==1){
                                //just pick the one value
                                values[index0] = values[index1];
                                continue;
                            } else
                            if (m_dimarray[m]<4){
                                //do linear interpolation
                                values[index0] = linear_interpolation(tempvector[m]-(int)tempvector[m],values[index1],values[index2]);
                                continue;
                            }
                            VoxelType tvalue = cubic_interpolation(tempvector[m]-(int)tempvector[m],values[index0],values[index1],values[index2],values[index3]);
                            values[index0] = CLAMP(tvalue,clampmin,clampmax);
                        }
                        
                        values[0] = CLAMP(values[0],clampmin,clampmax);
                        break;
                    }
                }
            }

            //do something with the result
            T_function(imdata[imindex],values[0]);
            continue;
            
            skip: ;
            imdata[imindex] = clampmin;
            
        } NDL_ENDFOREACH
    }
    //****************************************************
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    

    void transformdiff(Image<VoxelType,DIM> &im,VoxelType clampmin,VoxelType clampmax,int interpolationtype=NDL_LINEAR){
        if (interpolationtype==NDL_NN) _backtransform<NDL_NN,subvalue<VoxelType>,dummymask>(im,clampmin,clampmax); //nearest neighbor
        if (interpolationtype==NDL_LINEAR) _backtransform<NDL_LINEAR,subvalue<VoxelType>,dummymask>(im,clampmin,clampmax); //linear
        if (interpolationtype==NDL_CUBIC) _backtransform<NDL_CUBIC,subvalue<VoxelType>,dummymask>(im,clampmin,clampmax); //cubic (hermite)
    }
    template<bool mask_function(int)>
    void backtransform(Image<VoxelType,DIM> &im,VoxelType clampmin,VoxelType clampmax,int interpolationtype=NDL_LINEAR){
        if (interpolationtype==NDL_NN) _backtransform<NDL_NN,addvalue<VoxelType>,mask_function>(im,clampmin,clampmax); //nearest neighbor
        if (interpolationtype==NDL_LINEAR) _backtransform<NDL_LINEAR,addvalue<VoxelType>,mask_function>(im,clampmin,clampmax); //linear
        if (interpolationtype==NDL_CUBIC) _backtransform<NDL_CUBIC,addvalue<VoxelType>,mask_function>(im,clampmin,clampmax); //cubic (hermite)
    }
    void backtransform(Image<VoxelType,DIM> &im,VoxelType clampmin,VoxelType clampmax,int interpolationtype=NDL_LINEAR){
        if (interpolationtype==NDL_NN) _backtransform<NDL_NN,addvalue<VoxelType>,dummymask>(im,clampmin,clampmax); //nearest neighbor
        if (interpolationtype==NDL_LINEAR) _backtransform<NDL_LINEAR,addvalue<VoxelType>,dummymask>(im,clampmin,clampmax); //linear
        if (interpolationtype==NDL_CUBIC) _backtransform<NDL_CUBIC,addvalue<VoxelType>,dummymask>(im,clampmin,clampmax); //cubic (hermite)
    }
    template<int INTERPOLATIONTYPE,void T_function(VoxelType&,VoxelType),bool mask_function(int)>
    void _backtransform(Image<VoxelType,DIM> &im,VoxelType clampmin,VoxelType clampmax){
        im.setupworldmatricies(); //is this needed?
        if (m_matrixrecalculateflag){
            setupworldmatricies();
            worldtoimagematricies(im);
        }
        
        VoxelType* imdata = im.m_data;
        
        //****************************************
        //precompute indices used in interpolation
        //****************************************
        int indexarray[Power<INTERPOLATIONTYPE,im.MYDIM>::value];
        int coordarray[im.MYDIM*Power<INTERPOLATIONTYPE,im.MYDIM>::value];
        int levelarray[Power<INTERPOLATIONTYPE,im.MYDIM>::value];
        int indexarray_LINE[NumLines<INTERPOLATIONTYPE,im.MYDIM>::value];
        int indexarray_DELTA[NumLines<INTERPOLATIONTYPE,im.MYDIM>::value];
        int dimensionarray[NumLines<INTERPOLATIONTYPE,im.MYDIM>::value];
        im.precomputekernalindecies<INTERPOLATIONTYPE>(indexarray,coordarray,levelarray,indexarray_LINE,indexarray_DELTA,dimensionarray);
                    
        //****************************************
        //Iterate over all voxels
        //****************************************
        NDL_FOREACHPL(*this){
            double tempvector[DIM+1];
            int index;
            NDL_GETCOORD(index,tempvector);
            tempvector[DIM]=1;
            
            if (mask_function(index)==0) continue;
            m_finalmatrix.transformpoint(tempvector);

            //load kernel values from current position
            VoxelType values[Power<INTERPOLATIONTYPE,DIM>::value];
            if (INTERPOLATIONTYPE==NDL_NN){
                int imindex=0;
                for(int n=0;n<DIM;n++){
                    tempvector[n]+=0.5; //add half a pixel so that nn interpolation works properly
                    if (tempvector[n] >= im.m_regionsize[n] || tempvector[n]<im.m_orgin[n]){ goto skip; }
                    if (tempvector[n] >=0 ){ imindex += (int)tempvector[n] * im.m_dimPROD[n]; }
                }
                values[0] = imdata[imindex];
            }
            else {
                //calculate index
                int imindex=0;
                for(int n=0;n<DIM;n++){
                    if (im.m_dimarray[n]==1) tempvector[n]+=0.5; //add half a pixel so that nn interpolation works properly
                    if (tempvector[n] >= im.m_regionsize[n] || tempvector[n]<im.m_orgin[n]){ goto skip; }
                    if (tempvector[n] >=0 ){ imindex += (int)tempvector[n] * im.m_dimPROD[n]; }
                }
                
                //load the data into an array using precomputed indicies
                for(int a=0;a<Power<INTERPOLATIONTYPE,DIM>::value;++a){
                    int dindex=imindex+indexarray[a];
                    values[a] = imdata[CLAMP(dindex,0,im.m_numvoxels-1)];
                }
                
                switch(INTERPOLATIONTYPE){
                    case NDL_LINEAR:{
                        //use the precomputed indecies to do linear interpolation in each dimension
                        for(int q=0;q<NumLines<2,DIM>::value;++q){
                            int m = dimensionarray[q];
                            if (m_dimarray[m]==1) continue;
                            int index1 = indexarray_LINE[q];
                            int index2 = index1+indexarray_DELTA[q];
                            values[index1] = linear_interpolation(tempvector[m]-(int)tempvector[m],values[index1],values[index2]);
                        }
                        
                        break;
                    }
                    case NDL_CUBIC: {
                        //use the precomputed indecies to do cubic interpolation in each dimension
                        for(int q=0;q<NumLines<4,DIM>::value;++q){
                            int m = dimensionarray[q];
                            int index0 = indexarray_LINE[q];
                            int index1 = index0+indexarray_DELTA[q];
                            int index2 = index1+indexarray_DELTA[q];
                            int index3 = index2+indexarray_DELTA[q];
                            if (m_dimarray[m]==1){
                                //just pick the one value
                                values[index0] = values[index1];
                                continue;
                            } else
                            if (m_dimarray[m]<4){
                                //do linear interpolation
                                values[index0] = linear_interpolation(tempvector[m]-(int)tempvector[m],values[index1],values[index2]);
                                continue;
                            }
                            VoxelType tvalue = cubic_interpolation(tempvector[m]-(int)tempvector[m],values[index0],values[index1],values[index2],values[index3]);
                            values[index0] = CLAMP(tvalue,clampmin,clampmax);
                        }
                        
                        values[0] = CLAMP(values[0],clampmin,clampmax);
                        break;
                    }
                }
            }

            //do something with the result
            T_function(m_data[index],values[0]);
            continue;
            
            skip: ;
            
        } NDL_ENDFOREACH
        
    }

    template<bool mask_function(int)>
    void project(Image<VoxelType,DIM> &im,VoxelType clampmin,VoxelType clampmax,int interpolationtype=NDL_LINEAR,Image<VoxelType,DIM>* raylengthimage=0){
        if (interpolationtype==NDL_NN) _project<NDL_NN,addvalue<VoxelType>,mask_function>(im,clampmin,clampmax,raylengthimage); //nearest neighbor
        if (interpolationtype==NDL_LINEAR) _project<NDL_LINEAR,addvalue<VoxelType>,mask_function>(im,clampmin,clampmax,raylengthimage); //linear
        if (interpolationtype==NDL_CUBIC) _project<NDL_CUBIC,addvalue<VoxelType>,mask_function>(im,clampmin,clampmax,raylengthimage); //cubic (hermite)
    }

    void project(Image<VoxelType,DIM> &im,VoxelType clampmin,VoxelType clampmax,int interpolationtype=NDL_LINEAR,Image<VoxelType,DIM>* raylengthimage=0){
        if (interpolationtype==NDL_NN) _project<NDL_NN,addvalue<VoxelType>,dummymask>(im,clampmin,clampmax,raylengthimage); //nearest neighbor
        if (interpolationtype==NDL_LINEAR) _project<NDL_LINEAR,addvalue<VoxelType>,dummymask>(im,clampmin,clampmax,raylengthimage); //linear
        if (interpolationtype==NDL_CUBIC) _project<NDL_CUBIC,addvalue<VoxelType>,dummymask>(im,clampmin,clampmax,raylengthimage); //cubic (hermite)
    }
    template<int INTERPOLATIONTYPE,void T_function(VoxelType&,VoxelType),bool mask_function(int)>
    void _project(Image<VoxelType,DIM> &im,VoxelType clampmin,VoxelType clampmax,Image<VoxelType,DIM>* raylengthimage=0){
        im.setupworldmatricies(); //is this needed?
        if (m_matrixrecalculateflag){
            setupworldmatricies();
            worldtoimagematricies(im);
        }
        VoxelType* imdata = im.m_data;
        VoxelType* raylengthdata = 0;
        
        if (raylengthimage){
            *raylengthimage = im;
            raylengthimage->setvalue(0);
            raylengthdata = raylengthimage->m_data;
        }
        
        //****************************************
        //precompute indices used in interpolation
        //****************************************
        int indexarray[Power<INTERPOLATIONTYPE,DIM>::value];
        int coordarray[DIM*Power<INTERPOLATIONTYPE,DIM>::value];
        int levelarray[Power<INTERPOLATIONTYPE,DIM>::value];
        int indexarray_LINE[NumLines<INTERPOLATIONTYPE,DIM>::value];
        int indexarray_DELTA[NumLines<INTERPOLATIONTYPE,DIM>::value];
        int dimensionarray[NumLines<INTERPOLATIONTYPE,DIM>::value];
        precomputekernalindecies<INTERPOLATIONTYPE>(indexarray,coordarray,levelarray,indexarray_LINE,indexarray_DELTA,dimensionarray);
        
        //****************************************
        //Iterate over all voxels
        //****************************************

        //calculate position of start ray
        Vector<double,DIM+1> p0;
        for(int i=0;i<DIM;i++) p0[i]=0;
        p0[DIM]=1;
        Vector<double,DIM+1> pstart;
        pstart = m_iorientationmatrix*p0;
        
        NDL_FOREACHPL(im){
            Vector<double,DIM+1> tempvector;
            Vector<double,DIM+1> pend;
            Vector<double,DIM+1> dp;
            Vector<double,DIM+1> stepper;
            int imindex;
            NDL_GETCOORD(imindex,pend);
            
            pend[1]-=0.5;//NOTE: MAGIC NUMBER, THIS WAS EMPERICALLY ADDED,
                          //SHOULD LOOK AT MORE!!!
            
            pend[DIM]=1;
            
            //calculate position of end ray and delta (dp)
            pend = m_ifinalmatrix*pend;
            
            dp = pend - pstart;
            double norm = dp.Norm();
            if (norm) dp/=norm;

            VoxelType values[Power<INTERPOLATIONTYPE,DIM>::value];
            VoxelType finalvalue = 0;
            int raylength=0;
            stepper = pstart;
            for(int i=0;i<norm;i++){
                tempvector = stepper;
                
                if (INTERPOLATIONTYPE==NDL_NN){
                    int index=0;
                    for(int n=0;n<DIM;n++){
                        tempvector[n]+=0.5; //add half a pixel so that nn interpolation works properly
                        if ((int)tempvector[n] >= m_regionsize[n] || (int)tempvector[n] < m_orgin[n]) goto OUTOFBOUNDS;
                        index += (int)tempvector[n] * m_dimPROD[n];
                    }
                    if (mask_function(index)==false){ stepper += dp; continue; }
                    raylength++;
                    finalvalue += m_data[index];
                } else {
                    //calculate index
                    int index=0;
                    for(int n=0;n<DIM;n++){
                        if (m_dimarray[n]==1) tempvector[n]+=0.5; //add half a pixel so that nn interpolation works properly
                        if (tempvector[n] >= m_regionsize[n] || tempvector[n] < m_orgin[n]) goto OUTOFBOUNDS;
                        index += (int)tempvector[n] * m_dimPROD[n];
                    }
                    if (mask_function(index)==false){ stepper += dp; continue; }
                    raylength++;
                    
                    //load the data into an array using precomputed indicies
                    for(int a=0;a<Power<INTERPOLATIONTYPE,DIM>::value;++a){
                        int dindex=index+indexarray[a];
                        values[a] = m_data[CLAMP(dindex,0,m_numvoxels-1)];
                    }
                    
                    switch(INTERPOLATIONTYPE){
                        case NDL_LINEAR:{
                            //use the precomputed indecies to do linear interpolation in each dimension
                            for(int q=0;q<NumLines<2,DIM>::value;++q){
                                int m = dimensionarray[q];
                                if (m_dimarray[m]==1){
                                    //just pick the one value
                                    continue; 
                                }
                                int index1 = indexarray_LINE[q];
                                int index2 = index1+indexarray_DELTA[q];
                                values[index1] = linear_interpolation(tempvector[m]-(int)tempvector[m],values[index1],values[index2]);
                            }
                            
                            //clamp the result
                            finalvalue += CLAMP(values[0],clampmin,clampmax);
                            break;
                        }
                        case NDL_CUBIC: {                        
                            //use the precomputed indecies to do cubic interpolation in each dimension
                            for(int q=0;q<NumLines<4,DIM>::value;++q){
                                int m = dimensionarray[q];
                                int index0 = indexarray_LINE[q];
                                int index1 = index0+indexarray_DELTA[q];
                                int index2 = index1+indexarray_DELTA[q];
                                int index3 = index2+indexarray_DELTA[q];
                                if (m_dimarray[m]==1){
                                    //just pick the one value
                                    values[index0] = values[index1];
                                    continue;
                                } else
                                if (m_dimarray[m]<4){
                                    //do linear interpolation
                                    values[index0] = linear_interpolation(tempvector[m]-(int)tempvector[m],values[index1],values[index2]);
                                    continue;
                                }
                                VoxelType tvalue = cubic_interpolation(tempvector[m]-(int)tempvector[m],values[index0],values[index1],values[index2],values[index3]);
                                values[index0] = CLAMP(tvalue,clampmin,clampmax);
                            }
                            
                            finalvalue += CLAMP(values[0],clampmin,clampmax);
                            break;
                        }
                    }
                }
                stepper += dp;
                continue;
                
                OUTOFBOUNDS: ;
                //if we've already traced out some legal voxels, 
                //and now are out of bounds, we know we are done with the ray
                if (raylength>0) break; 
                stepper += dp;
            }
            
            //do something with the result
            if (raylengthdata) raylengthdata[imindex] = raylength;
            if (raylength) finalvalue/=(float)raylength;
            T_function(imdata[imindex],finalvalue);
        } NDL_ENDFOREACH
    }

    //***********************
    // DRAWING FUNCTIONS
    //***********************
    void DrawRectangle(int origin[DIM],int size[DIM],VoxelType value,bool filled=false){ 
        VoxelType* tvalue = new VoxelType[m_numcolors];
        for(int i=0;i<m_numcolors;i++) tvalue[i] = value;
        DrawRectangle(origin,size,tvalue,filled);
        delete [] tvalue;
    }
    void DrawRectangle(int origin[DIM],int size[DIM],VoxelType* value,bool filled=false){
        int savedorgin[DIM],savedregion[DIM];
        saveselection(savedorgin,savedregion);
        select(origin,size);
        if (filled){
            NDL_FOREACHPL(*this){
                int index,colorindex;
                NDL_GETCOLORINDEX(index,colorindex);
                m_data[index] = value[colorindex];
            } NDL_ENDFOREACH
        } else {
            for(int axis=0;axis<DIM;axis++){
                int endoffset = (m_regionsize[axis] - 1)*m_dimPROD[axis];
                NDL_FOREACHLINE(*this,axis){ //step 1 is for the x axis only
                    int index,colorindex;
                    NDL_GETCOLORINDEX(index,colorindex);
                    m_data[index] = value[colorindex];
                    m_data[index + endoffset] = value[colorindex];
                } NDL_ENDFOREACH
            }
        }
        select(savedorgin,savedregion);
    }

    void DrawLine(int point1[DIM],int point2[DIM],VoxelType value){
        VoxelType* tvalue = new VoxelType[m_numcolors];
        for(int i=0;i<m_numcolors;i++) tvalue[i] = value;
        DrawLine(point1,point2,tvalue);
        delete [] tvalue;
    }
    void DrawLine(int point1[DIM],int point2[DIM],VoxelType* value){
        //calculate delta
        float delta[DIM];
        for(int i=0;i<DIM;i++) delta[i]=point2[i]-point1[i];
        
        //calculate norm and normalize delta
        float norm=0;
        for(int i=0;i<DIM;i++) norm+=delta[i]*delta[i];
        norm = sqrt(norm);
        if (norm) for(int i=0;i<DIM;i++) delta[i]/=norm;

        //step through line
        float pos[DIM];
        for(int i=0;i<DIM;i++) pos[i] = point1[i]+0.5;
        for(int step=0;step<norm;step++){
            bool valid=true;
            for(int i=0;i<DIM;i++) if (pos[i]<m_orgin[i] || pos[i]>=m_regionsize[i]+m_orgin[i]) valid=false;
            if (valid){
                //set the color
                VoxelType* tdata = &(*this)(pos);
                for(int t=0;t<m_numcolors;t++) tdata[t] = value[t];
            }
            
            //take step
            for(int i=0;i<DIM;i++) pos[i] += delta[i];
        }
    }
    
    //just draw on first two dimensions for now, could make more flexable later
    void DrawText(int origin[DIM],char* text,VoxelType value,int linenumber=0,int flags=0){
        VoxelType* tvalue = new VoxelType[m_numcolors];
        for(int i=0;i<m_numcolors;i++) tvalue[i] = value;
        DrawText(origin,text,tvalue,linenumber,flags);
        delete [] tvalue;
    }
    void DrawText(int origin[DIM],char* text,VoxelType* value,int linenumber=0,int flags=0){
        if (DIM<2) return; //Only Works for 2D or greater
        int textlen = strlen(text);
        int savedorgin[DIM],savedregion[DIM];
        saveselection(savedorgin,savedregion);
        
        int xspacing = 3;
        int char_width = font_char_width-xspacing*2;
        
        //setup selection
        int neworigin[DIM];
        int size[DIM];
        for(int i=0;i<DIM;i++){
            neworigin[i]=origin[i];
            size[i]=1;
        }
        neworigin[1] += linenumber*font_char_height;
        size[0] = char_width * textlen;
        size[1] = font_char_height;
        select(neworigin,size);
        
        NDL_FOREACHPL(*this){
            float coord[DIM];
            int index,colorindex;
            NDL_GETCOLORCOORD(index,colorindex,coord);
            int coordx = ((int)coord[0]-m_orgin[0]);
            int coordy = ((int)coord[1]-m_orgin[1]);
            int x = coordx/char_width;
            int y = coordy/font_char_height;
            int ttx = coordx%char_width + xspacing;
            int tty = coordy%font_char_height;
            
            bool valid = true;
            for(int i=0;i<DIM;i++){
                if (coord[i]<savedorgin[i] || coord[i]>=savedregion[i]+savedorgin[i]) valid=false;
            }

            if (valid && x<textlen && y<1){
                unsigned int currchar = text[x];
                if (currchar){
                    float thevalue = value[colorindex]*(float)font[font_char_origin[currchar] + ttx*font_dx + tty*font_dy]/(float)font_max_value;
                    if (thevalue) m_data[index] = thevalue;
                }
            }
        } NDL_ENDFOREACH
        
        select(savedorgin,savedregion);
    }

    void DrawEllipse(int origin[DIM],int size[DIM],VoxelType value,bool filled=false){
        VoxelType* tvalue = new VoxelType[m_numcolors];
        for(int i=0;i<m_numcolors;i++) tvalue[i] = value;
        DrawEllipse(origin,size,tvalue,filled);
        delete [] tvalue;
    }
    void DrawEllipse(int origin[DIM],int size[DIM],VoxelType* value,bool filled=false){
        bool dims[DIM];
        for(int i=0;i<DIM;i++) dims[i]=true;
        DrawEllipse(origin,size,dims,value,filled);
    }
    void DrawEllipse(int origin[DIM],int size[DIM],bool dims[DIM],VoxelType value,bool filled=false){
        VoxelType* tvalue = new VoxelType[m_numcolors];
        for(int i=0;i<m_numcolors;i++) tvalue[i] = value;
        DrawEllipse(origin,size,dims,tvalue,filled);
        delete [] tvalue;
    }
    void DrawEllipse(int origin[DIM],int size[DIM],bool dims[DIM],VoxelType* value,bool filled=false){
        int temp_savedorgin[DIM],temp_savedregion[DIM];
        for(int i=0;i<DIM;i++){
            temp_savedorgin[i]=origin[i];
            temp_savedregion[i]=size[i];
        }
        int savedorgin[DIM],savedregion[DIM];
        saveselection(savedorgin,savedregion);
        select(origin,size);

        float oneoverradiussquared[DIM];
        float centervalue[DIM];
        float oneoverradiussquared2[DIM];
        float centervalue2[DIM];
        for(int i=0;i<DIM;i++){
            if (!dims[i] || m_dimarray[i]==1) continue;
            centervalue[i] = temp_savedregion[i]/2.0;
            oneoverradiussquared[i] = 1.0/(centervalue[i]*centervalue[i]);
            centervalue2[i] = centervalue[i]-1;
            oneoverradiussquared2[i] = 1.0/(centervalue2[i]*centervalue2[i]);
        }
        NDL_FOREACHPL(*this){
            float coord[DIM];
            int index,colorindex;
            NDL_GETCOLORCOORD(index,colorindex,coord);
            float result=0;
            float result2=0;
            for(int i=0;i<DIM;i++){
                if (!dims[i] || m_dimarray[i]==1) continue;
                float temp = coord[i]-temp_savedorgin[i]-centervalue[i];
                float temp2 = coord[i]-temp_savedorgin[i]-centervalue2[i]-1;
                result+=temp*temp*oneoverradiussquared[i];
                result2+=temp2*temp2*oneoverradiussquared2[i];
            }
            
            bool valid = true;
            for(int i=0;i<DIM;i++){
                if (coord[i]<savedorgin[i] || coord[i]>=savedregion[i]+savedorgin[i]) valid=false;
            }
            
            if (valid && result<=1 && (filled || result2>=1)){
                m_data[index] = value[colorindex];
            }
        } NDL_ENDFOREACH
        
        select(savedorgin,savedregion);
    }

    //*****************************************************
    // MATH FUNCTIONS
    //*****************************************************
    //operators
    Image<VoxelType,DIM> operator-(){
        return Image<VoxelType,DIM>(*this)*=-1;
    }
    
    Image<VoxelType,DIM> operator+(VoxelType value){ return Image<VoxelType,DIM>(*this)+=value; }
    Image<VoxelType,DIM>& operator+=(VoxelType value){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            m_data[index] += value;
        } NDL_ENDFOREACH
        return *this;
    }

    Image<VoxelType,DIM> operator-(VoxelType value){ return Image<VoxelType,DIM>(*this)-=value; }
    Image<VoxelType,DIM>& operator-=(VoxelType value){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            m_data[index] -= value;
        } NDL_ENDFOREACH
        return *this;
    }

    Image<VoxelType,DIM> operator*(VoxelType value){ return Image<VoxelType,DIM>(*this)*=value; }
    Image<VoxelType,DIM>& operator*=(VoxelType value){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            m_data[index] *= value;
        } NDL_ENDFOREACH
        return *this;
    }

    Image<VoxelType,DIM> operator/(VoxelType value){ return Image<VoxelType,DIM>(*this)/=value; }
    Image<VoxelType,DIM>& operator/=(VoxelType value){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            m_data[index] /= value;
        } NDL_ENDFOREACH
        return *this;
    }    

    Image<VoxelType,DIM> operator&(VoxelType value){ return Image<VoxelType,DIM>(*this)&=value; }
    Image<VoxelType,DIM>& operator&=(VoxelType value){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            m_data[index] &= value;
        } NDL_ENDFOREACH
        return *this;
    }    

    Image<VoxelType,DIM> operator|(VoxelType value){ return Image<VoxelType,DIM>(*this)|=value; }
    Image<VoxelType,DIM>& operator|=(VoxelType value){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            m_data[index] |= value;
        } NDL_ENDFOREACH
        return *this;
    }    

    Image<VoxelType,DIM> operator^(VoxelType value){ return Image<VoxelType,DIM>(*this)^=value; }
    Image<VoxelType,DIM>& operator^=(VoxelType value){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            m_data[index] ^= value;
        } NDL_ENDFOREACH
        return *this;
    }
    
    template<class T> Image<VoxelType,DIM> operator+(Image<T,DIM> &im){ return Image<VoxelType,DIM>(*this)+=im; }
    template<class T> Image<VoxelType,DIM>& operator+=(Image<T,DIM> &im){
        T* imdata = im.m_data;
        NDL_FOREACH2PL(im,*this){
            int index,imindex,validflag;
            NDL_GETINDEXPAIR(validflag,imindex,index);
            if (validflag) m_data[index] += imdata[imindex];
        } NDL_ENDFOREACH
        return *this;
    }

    template<class T> Image<VoxelType,DIM> operator-(Image<T,DIM> &im){ return Image<VoxelType,DIM>(*this)-=im; }
    template<class T> Image<VoxelType,DIM>& operator-=(Image<T,DIM> &im){
        T* imdata = im.m_data;
        NDL_FOREACH2PL(im,*this){
            int index,imindex,validflag;
            NDL_GETINDEXPAIR(validflag,imindex,index);
            if (validflag) m_data[index] -= imdata[imindex];
        } NDL_ENDFOREACH
        return *this;
    }

    template<class T> Image<VoxelType,DIM> operator*(Image<T,DIM> &im){ return Image<VoxelType,DIM>(*this)*=im; }
    template<class T> Image<VoxelType,DIM>& operator*=(Image<T,DIM> &im){
        T* imdata = im.m_data;
        NDL_FOREACH2PL(im,*this){
            int index,imindex,validflag;
            NDL_GETINDEXPAIR(validflag,imindex,index);
            if (validflag) m_data[index] *= imdata[imindex];
        } NDL_ENDFOREACH
        return *this;
    }

    template<class T> Image<VoxelType,DIM> operator/(Image<T,DIM> &im){ return Image<VoxelType,DIM>(*this)/=im; }
    template<class T> Image<VoxelType,DIM>& operator/=(Image<T,DIM> &im){
        T* imdata = im.m_data;
        NDL_FOREACH2PL(im,*this){
            int index,imindex,validflag;
            NDL_GETINDEXPAIR(validflag,imindex,index);
            if (validflag){
                //handle div by 0 by just setting to 0
                if (imdata[imindex]==0) m_data[index]=0;
                else m_data[index] /= imdata[imindex];
            }
        } NDL_ENDFOREACH
        return *this;
    }    

    template<class T> Image<VoxelType,DIM> operator&(Image<T,DIM> &im){ return Image<VoxelType,DIM>(*this)&=im; }
    template<class T> Image<VoxelType,DIM>& operator&=(Image<T,DIM> &im){
        T* imdata = im.m_data;
        NDL_FOREACH2PL(im,*this){
            int index,imindex,validflag;
            NDL_GETINDEXPAIR(validflag,imindex,index);
            if (validflag) m_data[index] &= imdata[imindex];
        } NDL_ENDFOREACH
        return *this;
    }    

    template<class T> Image<VoxelType,DIM> operator|(Image<T,DIM> &im){ return Image<VoxelType,DIM>(*this)|=im; }
    template<class T> Image<VoxelType,DIM>& operator|=(Image<T,DIM> &im){
        T* imdata = im.m_data;
        NDL_FOREACH2PL(im,*this){
            int index,imindex,validflag;
            NDL_GETINDEXPAIR(validflag,imindex,index);
            if (validflag) m_data[index] |= imdata[imindex];
        } NDL_ENDFOREACH
        return *this;
    }    

    template<class T> Image<VoxelType,DIM> operator^(Image<T,DIM> &im){ return Image<VoxelType,DIM>(*this)^=im; }
    template<class T> Image<VoxelType,DIM>& operator^=(Image<T,DIM> &im){
        T* imdata = im.m_data;
        NDL_FOREACH2PL(im,*this){
            int index,imindex,validflag;
            NDL_GETINDEXPAIR(validflag,imindex,index);
            if (validflag) m_data[index] ^= imdata[imindex];
        } NDL_ENDFOREACH
        return *this;
    }

    void Abs(){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            m_data[index] = abs(m_data[index]);
        } NDL_ENDFOREACH
    }

    void Positive(){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            if (m_data[index]<0) m_data[index] = 0;
        } NDL_ENDFOREACH
    }

    void Reciprocal(VoxelType numerator=1){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            if (m_data[index]!=0) m_data[index] = numerator/m_data[index];
        } NDL_ENDFOREACH
    }
    
    void Sqrt(){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            if (m_data[index]<0) m_data[index]=0;
            else m_data[index] = sqrt((double)m_data[index]);
        } NDL_ENDFOREACH
    }

    void Square(bool keepsign=false){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            int bit = 1;
            if (keepsign && m_data[index]<0) bit=-1;
            m_data[index] = bit*m_data[index]*m_data[index];
        } NDL_ENDFOREACH
    }
    
    void Log(float base=M_E){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            if (m_data[index]<=0) m_data[index]=FLT_EPSILON;
            m_data[index] = log((float)m_data[index]);
            if (base!=M_E) m_data[index]/=log(base);
        } NDL_ENDFOREACH
        return;
    }

    void Exp(float base=M_E){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            m_data[index] = pow(base,(float)m_data[index]);
        } NDL_ENDFOREACH
        return;
    }
    
    void Pow(float exp){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            m_data[index] = pow((float)m_data[index],exp);
        } NDL_ENDFOREACH
        return;
    }
    
    /**
    set an image to a value

    sets the image selection to the specified value, 
    you can optionally limit operation to pixels whose 
    value is between threshold1 and threshold2
    
    eg: To create a cool box pattern
    
    @code  
    Image<unsigned char,2> myimage(500,500,1);
    myimage.setvalue(0);
    myimage.select(100,100,300,300)
    myimage.setvalue(10);
    myimage.select(200,200,100,100)
    myimage.setvalue(20);
    myimage.select();
    myimage.addviewloop();
    @endcode 
    
    @param value  the value to set to the image
    @param threshold1  Lower bound for operating range, default=0
    @param threshold2  Upper bound for operating range, default=-1 (if threshold2 < threshold1)
    @returns         a reference to the current image (*this)
    */
    template<class T> Image<VoxelType,DIM>& setvalue(VoxelType value,Image<T,DIM>& maskimage,T threshold1=0,T threshold2=-1){
        bool dothreshold = (threshold2>=threshold1);
        T* maskdata = maskimage.m_data;
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            m_data[index] = value;
        } NDL_ENDFOREACH
        return *this;
    }
    Image<VoxelType,DIM>& setvalue(VoxelType value,VoxelType threshold1=0,VoxelType threshold2=-1){ return setvalue(value,*this,threshold1,threshold2); }

    void Clamp(VoxelType themin,VoxelType themax){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            m_data[index] = (std::max)((std::min)(m_data[index],themax),themin);
        } NDL_ENDFOREACH
    }

    void ClampMin(VoxelType themin){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            m_data[index] = (std::max)(m_data[index],themin);
        } NDL_ENDFOREACH
    }
    
    void ClampMax(VoxelType themax){
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            m_data[index] = (std::min)(m_data[index],themax);
        } NDL_ENDFOREACH
    }

    //*****************************************************
    // IMAGE FILTERING FUNCTIONS (LINEAR, AND NON-LINEAR (eg: MORPHALOGICAL))
    //*****************************************************
    void fillholes(VoxelType threshold1,VoxelType threshold2){ Image<VoxelType,DIM> im; fillholes(im,*this,threshold1,threshold2); this->copyregion(im); }
    template<class T> void fillholes(Image<T,DIM>& maskimage,T threshold1,T threshold2){ Image<VoxelType,DIM> im; fillholes(im,maskimage,threshold1,threshold2); this->copyregion(im); }
    template<class T> void fillholes(Image<VoxelType,DIM> &im,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        VoxelType markervalue = -10000000;
        im.imagecopy(*this,this->m_regionsize);
        im.setvalue(markervalue,maskimage,threshold1,threshold2);
        
        VoxelType* imdata = im.m_data;
        T* maskdata = maskimage.m_data;
        
        int numbadpixels;
        do {
            numbadpixels=0;
            NDL_FOREACH2(im,*this){
                int tarray[DIM];
                int imtarray[DIM];
                int index,imindex,validflag;
                NDL_GETCOORDPAIR(validflag,imindex,imtarray,index,tarray);
                
                if (!validflag || (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
                
                VoxelType varray[NDL_NUMCROSSVALUES];
                NDL_GETCROSSVALUES(imindex,imtarray,im.m_dimPROD,imdata,varray);
                
                VoxelType newvalue=0;
                float count = 0;
                for(int i=1;i<NDL_NUMCROSSVALUES;i++){
                    if (varray[i]!=markervalue){
                        newvalue+=varray[i];
                        count++;
                    }
                }
                if (count) imdata[imindex] = newvalue/count;
                else numbadpixels++;
            } NDL_ENDFOREACH
        } while (numbadpixels>0);
    }
    
    //NEEDS MORE WORK!
    void bluranisotropic(){ Image<VoxelType,DIM> im; bluranisotropic(im); this->copyregion(im); }
    void bluranisotropic(Image<VoxelType,DIM> &im){
        const float tao = 0.5/DIM; //tao: 0 -> 0.5/DIM
        const float K=0.2; // K: 0 -> 1
        const float oneoverK=1/K;

        if (im.isempty()) im.imagecopy(*this,this->m_regionsize);
        VoxelType* imdata = im.m_data;
        int tarray[DIM];
        Vector<VoxelType,DIM> gradientvector;
        Matrix<VoxelType,DIM> tmatrix;
        Matrix<VoxelType,DIM> diffusiontensor;
        Vector<VoxelType,DIM> eigenvalues;
        Matrix<VoxelType,DIM> eigenvectors;
        
        NDL_FOREACH2(im,*this){ //change to parallel later
            int tarray[DIM];
            int imtarray[DIM];
            int index,imindex,validflag;
            NDL_GETCOORDPAIR(validflag,imindex,imtarray,index,tarray);
            if (!validflag) continue;
            if (_c % 1000 == 0) printf("%10d/%10d, %f%%\r",_c,_numvoxels,100.0*(float)_c/(float)_numvoxels);

            //calculate gradient vector
            VoxelType value = m_data[index]; //add center point
            bool skipflag=false;
            for(int n=0;n<DIM;n++){
                int t=tarray[n] - 1;
                int t2=tarray[n] + 1;
                if (t < m_dimarray[n] && t>=0 && t2 < m_dimarray[n] && t2>=0){
                    gradientvector[n] = m_data[index - m_dimPROD[n]] + m_data[index + m_dimPROD[n]] - 2*value;
                    //gradientvector[n] = m_data[index - m_dimPROD[n]] - value;
                } else { skipflag=true; break; }
            }
            if (skipflag==false){
                //calculate and solve structure tensor
                tmatrix.OuterProduct(gradientvector,gradientvector);
                tmatrix.EigenDecomposition(eigenvalues,eigenvectors);
                
                //calculate diffusion tensor
                float result=0;
                for (int i=0;i<DIM;i++) result+=gradientvector[i]*gradientvector[i];
                
                float den = result*oneoverK;
                float den2 = den*den;
                float den4 = den2*den2;
                
                //POSSIBLE OPTIONS
                eigenvalues.m_data[0]=1-M_E*(-M_PI/(den4*den4));
                //~ eigenvalues.m_data[0]*=1-M_E*(-M_PI/(den4*den4));
                //~ if (eigenvalues.m_data[0]>.1) eigenvalues.m_data[0]=1-M_E*(-M_PI/(den4*den4));
                //~ if (eigenvalues.m_data[0]>K) eigenvalues.m_data[0]=1-M_E*(-M_PI/(den4*den4));
                
                for(int i=1;i<DIM;i++) eigenvalues.m_data[i]=1;
                    
                tmatrix.SetZero();
                tmatrix.SetDiagonal(eigenvalues.m_data);
                diffusiontensor = eigenvectors * tmatrix * eigenvectors.GetTranspose();
                
                //solve diffusion equation
                eigenvalues = diffusiontensor*gradientvector;
                VoxelType total = 0;
                for(int n=0;n<DIM;n++) total+=eigenvalues[n];
                total*=tao;
                total+=m_data[index];
                
                //handle strange cases
                if (total<m_min) total = value;
                if (total>m_max) total = value;
                    
                //update the image
                imdata[imindex] = total;
            }
        } NDL_ENDFOREACH
    }
    
    //blur
    void blur(VoxelType threshold1=0,VoxelType threshold2=-1){ Image<VoxelType,DIM> im; blur(im,*this,threshold1,threshold2); this->copyregion(im); }
    template<class T> void blur(Image<VoxelType,DIM> &im,Image<T,DIM>& maskimage,T threshold1=0,T threshold2=-1){
        if (im.isempty()) im.imagecopy(*this,this->m_regionsize);
        VoxelType* imdata = im.m_data;
        bool dothreshold = (threshold2>=threshold1);
        T* maskdata = maskimage.m_data;
        
        NDL_FOREACH2PL(im,*this){
            int tarray[DIM];
            int imtarray[DIM];
            int index,imindex,validflag;
            NDL_GETCOORDPAIR(validflag,imindex,imtarray,index,tarray);
            
            if (!validflag || (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2))) continue;

            VoxelType varray[NDL_NUMCROSSVALUES];
            NDL_GETCROSSVALUES(index,tarray,m_dimPROD,m_data,varray);
            
            VoxelType newvalue=varray[0];
            for(int i=1;i<NDL_NUMCROSSVALUES;i++) newvalue+=varray[i];

            imdata[imindex] = newvalue*(1.0/NDL_NUMCROSSVALUES);
        } NDL_ENDFOREACH
    }
    
    //im is output image
    void median(VoxelType threshold1=0,VoxelType threshold2=-1){ Image<VoxelType,DIM> im; median(im,*this,threshold1,threshold2); this->copyregion(im); }
    template<class T> void median(Image<VoxelType,DIM> &im,Image<T,DIM>& maskimage,T threshold1=0,T threshold2=-1){
        if (im.isempty()) im.imagecopy(*this,this->m_regionsize);
        VoxelType* imdata = im.m_data;
        bool dothreshold = (threshold2>=threshold1);
        T* maskdata = maskimage.m_data;
        
        NDL_FOREACH2PL(im,*this){
            int tarray[DIM];
            int imtarray[DIM];
            int index,imindex,validflag;
            NDL_GETCOORDPAIR(validflag,imindex,imtarray,index,tarray);
            
            if (!validflag || (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2))) continue;

            VoxelType varray[NDL_NUMCROSSVALUES];
            NDL_GETCROSSVALUES(index,tarray,m_dimPROD,m_data,varray);
            
            VoxelType* begin = varray;
            VoxelType* middle = &varray[(NDL_NUMCROSSVALUES-1)/2];
            VoxelType* end = &varray[NDL_NUMCROSSVALUES-1];
            std::nth_element(begin,middle,end);
            
            imdata[imindex] = *middle; //normalize
        } NDL_ENDFOREACH
    }
    
    //edge and gradient descent/ascent (could add mask versions for descent ascent)
    VoxelType gradientdescent(float stepsize,VoxelType threshold1=0,VoxelType threshold2=-1){ Image<VoxelType,DIM> im; edge(im,*this,threshold1,threshold2); VoxelType retval = im.getnorm(); im*=stepsize; *this -= im; return retval; }
    VoxelType gradientascent(float stepsize,VoxelType threshold1=0,VoxelType threshold2=-1){ Image<VoxelType,DIM> im; edge(im,*this,threshold1,threshold2); VoxelType retval = im.getnorm(); im*=stepsize; *this += im; return retval; }
    void edge(VoxelType threshold1=0,VoxelType threshold2=-1){ Image<VoxelType,DIM> im; edge(im,*this,threshold1,threshold2); this->copyregion(im); }
    void edge(Image<VoxelType,DIM>& im){ edge(im,*this); }
    /// edge will do a (2N+1)-point laplacian gradient and return it in the image im
    template<class T> void edge(Image<VoxelType,DIM> &im,Image<T,DIM>& maskimage,T threshold1=0,T threshold2=-1){
        if (im.isempty()) im.imagecopy(*this,this->m_regionsize);
        VoxelType* imdata = im.m_data;
        bool dothreshold = (threshold2>=threshold1);
        T* maskdata = maskimage.m_data;
        
        NDL_FOREACH2PL(im,*this){
            int tarray[DIM];
            int imtarray[DIM];
            int index,imindex,validflag;
            NDL_GETCOORDPAIR(validflag,imindex,imtarray,index,tarray);
            
            if (!validflag || (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2))) continue;

            VoxelType varray[NDL_NUMCROSSVALUES];
            NDL_GETCROSSVALUES(index,tarray,m_dimPROD,m_data,varray);
            VoxelType newvalue=(NDL_NUMCROSSVALUES-1)*varray[0]; //add center point
            for(int i=1;i<NDL_NUMCROSSVALUES;i++) newvalue-=varray[i];

            imdata[imindex] = newvalue;
        } NDL_ENDFOREACH
    }

    //edgesobel
    void edgesobel(){ Image<VoxelType,DIM> im; edgesobel(im); this->copyregion(im); }
    void edgesobel(Image<VoxelType,DIM> &im){
        im.imagecopy(*this,this->m_regionsize);
        im.setvalue(0);
        for(int direction=0;direction<DIM;direction++){
            Image<VoxelType,DIM> tempim;
            for(int i=0;i<DIM;i++){
                Image<VoxelType,1> kernel(3,1);
                if (i==direction){ kernel.m_data[0]=-1; kernel.m_data[1]=0; kernel.m_data[2]=1; }
                else { kernel.m_data[0]=1; kernel.m_data[1]=1.41; kernel.m_data[2]=1; }
                if (i==0) convolve(tempim,kernel,i);
                else tempim.convolve(kernel,i);
            }
            tempim.Square();
            im+=tempim;
        }
        im.Sqrt();
    }

    //edgesobel (old version, need to do comparison with new version)
    void edgesobel_old(){ Image<VoxelType,DIM> im; edgesobel_old(im); this->copyregion(im); }
    void edgesobel_old(Image<VoxelType,DIM> &im){
        int indexarray[Power<3,DIM>::value];
        int coordarray[DIM*Power<3,DIM>::value];
        int levelarray[Power<3,DIM>::value];
        int indexarray_LINE[NumLines<3,DIM>::value];
        int indexarray_DELTA[NumLines<3,DIM>::value];
        int dimensionarray[NumLines<3,DIM>::value];
        precomputekernalindecies<3>(indexarray,coordarray,levelarray,indexarray_LINE,indexarray_DELTA,dimensionarray);
        im.imagecopy(*this,this->m_regionsize);
        im.setvalue(0);
        VoxelType* imdata = im.m_data;
        for(int iter=0;iter<DIM;iter++){
            NDL_FOREACH2PL(im,*this){
                int index,imindex,validflag;
                NDL_GETINDEXPAIR(validflag,imindex,index);            
                VoxelType newvalue = 0;
                for(int a=0,tempa=0;a<Power<3,DIM>::value;a++,tempa+=DIM){
                    int klevel = levelarray[a];
                    int kmultiplier = coordarray[tempa + iter];
                    if (!kmultiplier) continue;
                    int myindex = index+indexarray[a];
                    //handle boundaries
                    if(myindex<0 || myindex>=m_numvoxels) newvalue+=(DIM+1-klevel)*kmultiplier*m_data[index];
                    else newvalue+=(DIM+1-klevel)*kmultiplier*m_data[myindex]; //sobelfunction = (DIM-klevel+1)*kmultiplier , it could be tweaked
                }
                imdata[imindex] += newvalue*newvalue;
            } NDL_ENDFOREACH
        }
        im.Sqrt();
    }

    //**********************************
    //morphalogical operations 
    //count specifies the number of times to dialate or erode
    //lower->upper limits the range of values that will be spread
    //threshold1->threshold2 limits the range of values that will be replaced
    //**********************************
    //dialate
    void dialate(int count=1,VoxelType lower=0,VoxelType upper=-1,VoxelType threshold1=0,VoxelType threshold2=-1){ Image<VoxelType,DIM> im; for(int i=0;i<count;i++){ dialate(im,*this,lower,upper,threshold1,threshold2); this->copyregion(im); } }
    template<class T> void dialate(Image<VoxelType,DIM> &im,Image<T,DIM>& maskimage,VoxelType lower=0,VoxelType upper=-1,VoxelType threshold1=0,VoxelType threshold2=-1){ //default threshold is invalid so as to signal, don't use thresholds
        if (im.isempty()) im.imagecopy(*this,this->m_regionsize);
        VoxelType* imdata = im.m_data;
        bool dothreshold2 = (upper>=lower);
        bool dothreshold = (threshold2>=threshold1);
        
        NDL_FOREACH2PL(im,*this){
            int tarray[DIM];
            int imtarray[DIM];
            int index,imindex,validflag;
            NDL_GETCOORDPAIR(validflag,imindex,imtarray,index,tarray);
            if (!validflag || (dothreshold && (maskimage[index]<threshold1 || maskimage[index]>threshold2))) continue;
            VoxelType newvalue = m_data[index]; //start with center point
            for(int n=0;n<DIM;n++){
                int t=tarray[n] - 1;
                if (t < m_dimarray[n] && t>=0 && t<m_regionsize[n]){ newvalue = (std::max)(newvalue,m_data[index - m_dimPROD[n]]); }
                t+=2;
                if (t < m_dimarray[n] && t>=0 && t<m_regionsize[n]){ newvalue = (std::max)(newvalue,m_data[index + m_dimPROD[n]]); }
            }
            if (dothreshold2 && (newvalue<lower || newvalue>upper)) continue;
            imdata[imindex] = newvalue; //normalize
        } NDL_ENDFOREACH
    }
    
    //erode
    void erode(int count=1,VoxelType lower=0,VoxelType upper=-1,VoxelType threshold1=0,VoxelType threshold2=-1){ Image<VoxelType,DIM> im; for(int i=0;i<count;i++){ erode(im,*this,lower,upper,threshold1,threshold2); this->copyregion(im); } }
    template<class T> void erode(Image<VoxelType,DIM> &im,Image<T,DIM>& maskimage,VoxelType lower=0,VoxelType upper=-1,VoxelType threshold1=0,VoxelType threshold2=-1){ //default threshold is invalid so as to signal, don't use thresholds
        if (im.isempty()) im.imagecopy(*this,this->m_regionsize);
        VoxelType* imdata = im.m_data;
        bool dothreshold2 = (upper>=lower);
        bool dothreshold = (threshold2>=threshold1);
        
        NDL_FOREACH2PL(im,*this){
            int tarray[DIM];
            int imtarray[DIM];
            int index,imindex,validflag;
            NDL_GETCOORDPAIR(validflag,imindex,imtarray,index,tarray);
            if (!validflag || (dothreshold && (maskimage[index]<threshold1 || maskimage[index]>threshold2))) continue;
            VoxelType newvalue = m_data[index]; //start with center point
            for(int n=0;n<DIM;n++){
                int t=tarray[n] - 1;
                if (t < m_dimarray[n] && t>=0 && t<m_regionsize[n]){ newvalue = (std::min)(newvalue,m_data[index - m_dimPROD[n]]); }
                t+=2;
                if (t < m_dimarray[n] && t>=0 && t<m_regionsize[n]){ newvalue = (std::min)(newvalue,m_data[index + m_dimPROD[n]]); }
            }
            if (dothreshold2 && (newvalue<lower || newvalue>upper)) continue;
            imdata[imindex] = newvalue; //normalize
        } NDL_ENDFOREACH
    }
        
    //opening / closing
    void opening(int count=1,VoxelType lower=0,VoxelType upper=-1,VoxelType threshold1=0,VoxelType threshold2=-1){ Image<VoxelType,DIM> im; opening(im,count,lower,upper,threshold1,threshold2); this->copyregion(im); }
    void opening(Image<VoxelType,DIM> &im,int count=1,VoxelType lower=0,VoxelType upper=-1,VoxelType threshold1=0,VoxelType threshold2=-1){
        if (im.isempty()) im.imagecopy(*this,this->m_regionsize);
        im.erode(count,lower,upper,threshold1,threshold2);
        im.dialate(count,lower,upper,threshold1,threshold2);
    }
    void closing(int count=1,VoxelType lower=0,VoxelType upper=-1,VoxelType threshold1=0,VoxelType threshold2=-1){ Image<VoxelType,DIM> im; closing(im,count,lower,upper,threshold1,threshold2); this->copyregion(im); }
    void closing(Image<VoxelType,DIM> &im,int count=1,VoxelType lower=0,VoxelType upper=-1,VoxelType threshold1=0,VoxelType threshold2=-1){
        if (im.isempty()) im.imagecopy(*this,this->m_regionsize);
        im.dialate(count,lower,upper,threshold1,threshold2);
        im.erode(count,lower,upper,threshold1,threshold2);
    }
    
    //tophat / bottomhat (top: f - opening, bottom: closing - f)
    void tophat(int count=1,VoxelType lower=0,VoxelType upper=-1,VoxelType threshold1=0,VoxelType threshold2=-1){ Image<VoxelType,DIM> im; tophat(im,count,lower,upper,threshold1,threshold2); this->copyregion(im); }
    void tophat(Image<VoxelType,DIM> &im,int count=1,VoxelType lower=0,VoxelType upper=-1,VoxelType threshold1=0,VoxelType threshold2=-1){
        if (im.isempty()) im.imagecopy(*this,this->m_regionsize);
        Image<VoxelType,DIM> im2(im);
        im2.opening(count,lower,upper,threshold1,threshold2);
        im -= im2;
    }
    void bottomhat(int count=1,VoxelType lower=0,VoxelType upper=-1,VoxelType threshold1=0,VoxelType threshold2=-1){ Image<VoxelType,DIM> im; bottomhat(im,count,lower,upper,threshold1,threshold2); this->copyregion(im); }
    void bottomhat(Image<VoxelType,DIM> &im,int count=1,VoxelType lower=0,VoxelType upper=-1,VoxelType threshold1=0,VoxelType threshold2=-1){
        if (im.isempty()) im.imagecopy(*this,this->m_regionsize);
        Image<VoxelType,DIM> im2(im);
        im.closing(count,lower,upper,threshold1,threshold2);
        im -= im2;
    }
    
    //morphalogical edge detector
    void morphedge(int count=1,VoxelType lower=0,VoxelType upper=-1,VoxelType threshold1=0,VoxelType threshold2=-1){ Image<VoxelType,DIM> im; morphedge(im,count,lower,upper,threshold1,threshold2); this->copyregion(im); }
    void morphedge(Image<VoxelType,DIM> &im,int count=1,VoxelType lower=0,VoxelType upper=-1,VoxelType threshold1=0,VoxelType threshold2=-1){
        if (im.isempty()) im.imagecopy(*this,this->m_regionsize);
        Image<VoxelType,DIM> im2(im);
        im.dialate(count,lower,upper,threshold1,threshold2);
        im2.erode(count,lower,upper,threshold1,threshold2);
        im -= im2;
    }
        
    //do a signed distance transform calculating the signed distance of every pixel
    //to the edge of the region defined by (lower,upper)
    void signeddistancetransform(VoxelType lower,VoxelType upper){Image<float,DIM> im; signeddistancetransform(im,lower,upper); this->copyregion(im); }
    void signeddistancetransform(Image<float,DIM> &result,VoxelType lower,VoxelType upper){
        result.imagecopy(*this,this->m_regionsize);
        float* imdata = result.m_data;
        NDL_FOREACH2PL(result,*this){
            int index,imindex,validflag;
            NDL_GETINDEXPAIR(validflag,imindex,index);
            if (!validflag) continue;
            if (m_data[index]>=lower && m_data[index]<=upper) imdata[imindex]=1;
            else imdata[imindex]=0;
        } NDL_ENDFOREACH
        result.edge();
        result.Abs();
        result.distancetransform(result,1,FLT_MAX);
        
        //negate interior values (and update min/max)
        bool firstflag=true;
        float minvalue=0;
        float maxvalue=0; //set default value
        NDL_FOREACH2PL(result,*this){
            int index,imindex,validflag;
            NDL_GETINDEXPAIR(validflag,imindex,index);
            if (!validflag) continue;
            if (m_data[index]>=lower && m_data[index]<=upper) imdata[imindex]*=-1;
            
            //min/max calc
            float temp = imdata[imindex];
            if (firstflag){ minvalue=maxvalue=temp; firstflag=false; }
            if (temp <minvalue) minvalue = temp;
            if (temp >maxvalue) maxvalue = temp;
        } NDL_ENDFOREACH
        result.m_min=minvalue;
        result.m_max=maxvalue;
    }
    
    //do a distance transform calculating the positive distance of every pixel
    //to the region defined by (lower,upper)
    void distancetransform(VoxelType lower,VoxelType upper){Image<float,DIM> im; distancetransform(im,lower,upper); this->copyregion(im); }
    void distancetransform(Image<float,DIM> &result,VoxelType lower,VoxelType upper){
        // based on:
        // sedt : SEDT  in linear time
        // David Coeurjolly (david.coeurjolly@liris.cnrs.fr) - Sept. 2004
        // Version 0.3 : Feb. 2005
        if ((void*)&result!=(void*)this) result.imagecopy(*this,this->m_regionsize);
        float* imdata = result.m_data;
        Image<float,DIM> tempim(result);
        float* tempimdata = tempim.m_data;
        
        //ITERATE OVER ALL LINES
        int dimprod = m_dimPROD[0];
        int endpos = m_regionsize[0];
        int imdimprod = result.m_dimPROD[0];
        int imendpos = result.m_regionsize[0];
        NDL_FOREACHLINE2PL(result,*this,0){ //step 1 is for the x axis only
            
            int index,imindex,validflag;
            NDL_GETINDEXPAIR(validflag,imindex,index);
            if (!validflag) continue;

            //init the first value
            if (m_data[index] >= lower && m_data[index] <= upper) imdata[imindex]=0;
            else imdata[imindex]=I_INFTY;

            //scan one way
            int p_imindex = 0;
            for (int t=1; t<imendpos; ++t){
                int tindex = index + t*dimprod;
                int t_imindex = imindex + t*imdimprod;
                if (m_data[tindex] >= lower && m_data[tindex] <= upper) imdata[t_imindex]=0;
                else {
                    int temp = inf_sum(1, imdata[p_imindex]);
                    imdata[t_imindex] = temp;
                }
                p_imindex = t_imindex;
            }
            //and then the other
            p_imindex = imindex + (imendpos-1)*imdimprod;
            for (int t=imendpos-2; t>=0; --t){
                int t_imindex = imindex + t*imdimprod;
                if (imdata[p_imindex] < imdata[t_imindex]){
                    int temp = inf_sum(1, imdata[p_imindex]);
                    imdata[t_imindex] = temp;
                }
                p_imindex = t_imindex;
            }
        } NDL_ENDFOREACH
        
        //square distances
        NDL_FOREACHPL(result){
            imdata[_c] = inf_prod(imdata[_c],imdata[_c]);
        } NDL_ENDFOREACH
        
        //now handle each additional axis
        for(int axis=1;axis<DIM;axis++){
            int imdimprod = result.m_dimPROD[axis];
            int imregionsize = result.m_regionsize[axis];
            int* tarray=allocate<int>(imregionsize,__LINE__);
            int* sarray=allocate<int>(imregionsize,__LINE__);
            int q,w;

            NDL_FOREACHLINE(result,axis){
                int imindex;
                NDL_GETINDEX(imindex);
                //scan one way
                tarray[0]=sarray[0]=q=0;
                for (int t=0; t<imregionsize; ++t){
                    int t_imindex = imindex + t*imdimprod;
                    while ((q >= 0) && (SDT_F(tarray[q],sarray[q], imdata[imindex + sarray[q]*imdimprod]) > SDT_F(tarray[q],t,imdata[t_imindex]))){
                        q--;
                    }
                    if(q<0){
                        q=0;
                        sarray[0]=t;
                    } else {
                        w = 1 + SDT_Sep(sarray[q], t, imdata[imindex + sarray[q]*imdimprod], imdata[t_imindex]);
                        if (w < imregionsize){
                            q++;
                            sarray[q]=t;
                            tarray[q]=w;
                        }
                    }
                }
                
                //and then the other
                for (int t=imregionsize-1; t>=0; --t){
                    int t_imindex = imindex + t*imdimprod;
                    tempimdata[t_imindex] = SDT_F(t,sarray[q],imdata[imindex + sarray[q]*imdimprod]);
                    if (t==tarray[q]) q--;
                }
            } NDL_ENDFOREACH
            deallocate(tarray);
            deallocate(sarray);
            
            result = tempim;
            tempimdata = tempim.m_data;
        }
        
        //squareroot distances (and update min/max)
        bool firstflag=true;
        float minvalue=0;
        float maxvalue=0; //set default value
        NDL_FOREACHPL(result){
            imdata[_c] = sqrt((float)imdata[_c]);
            
            //min/max calc
            float temp = imdata[_c];
            if (firstflag){ minvalue=maxvalue=temp; firstflag=false; }
            if (temp <minvalue) minvalue = temp;
            if (temp >maxvalue) maxvalue = temp;
        } NDL_ENDFOREACH
        
        result.m_min=minvalue;
        result.m_max=maxvalue;
    }

    void thin(float agressiveness=1){ Image<float,DIM> im; thin(im,agressiveness); this->copyregion(im); }
    void thin(Image<float,DIM> &result,float agressiveness=1){
        result.imagecopy(*this,this->m_regionsize);
        float* imdata = result.m_data;

        int indexarray[Power<3,DIM>::value];
        int coordarray[DIM*Power<3,DIM>::value];
        int levelarray[Power<3,DIM>::value];
        int indexarray_LINE[NumLines<3,DIM>::value];
        int indexarray_DELTA[NumLines<3,DIM>::value];
        int dimensionarray[NumLines<3,DIM>::value];
        precomputekernalindecies<3>(indexarray,coordarray,levelarray,indexarray_LINE,indexarray_DELTA,dimensionarray);
        NDL_FOREACH2PL(result,*this){
            int index,imindex;
            NDL_GETINDEX(imindex);
            NDL_GETINDEX2(index);
                        
            //load the data into an array using precomputed indicies
            VoxelType values[Power<3,DIM>::value];
            for(int a=0;a<Power<3,DIM>::value;++a){
                int dindex=index+indexarray[a];
                if (dindex<0 || dindex >=m_numvoxels) dindex=index;
                //~ CLAMP(dindex,0,m_numvoxels-1)
                values[a] = m_data[dindex];
            }
            
            //get the top third value
            VoxelType* begin = values;
            VoxelType* topthird = &values[2*Power<3,DIM>::value/3 - 1];
            VoxelType* end = &values[Power<3,DIM>::value-1];
            std::nth_element(begin,topthird,end);
            VoxelType comparevalue = *topthird;
            
            if (m_data[index] < comparevalue*agressiveness) imdata[imindex] = 0;
        } NDL_ENDFOREACH
    }

    void variancefilter(){ Image<float,DIM> im; variancefilter(im); this->copyregion(im); }
    void variancefilter(Image<float,DIM> &result){
        result.imagecopy(*this,this->m_regionsize);
        float* imdata = result.m_data;

        int indexarray[Power<3,DIM>::value];
        int coordarray[DIM*Power<3,DIM>::value];
        int levelarray[Power<3,DIM>::value];
        int indexarray_LINE[NumLines<3,DIM>::value];
        int indexarray_DELTA[NumLines<3,DIM>::value];
        int dimensionarray[NumLines<3,DIM>::value];
        precomputekernalindecies<3>(indexarray,coordarray,levelarray,indexarray_LINE,indexarray_DELTA,dimensionarray);
        NDL_FOREACH2PL(result,*this){
            int index,imindex;
            NDL_GETINDEX(imindex);
            NDL_GETINDEX2(index);
                        
            //load the data into an array using precomputed indicies
            VoxelType values[Power<3,DIM>::value];
            VoxelType meanvalue=0;
            for(int a=0;a<Power<3,DIM>::value;++a){
                int dindex=index+indexarray[a];
                if (dindex<0 || dindex>=m_numvoxels) dindex=index;
                values[a] = m_data[dindex];
                meanvalue += values[a];
            }
            meanvalue/=Power<3,DIM>::value;
            
            VoxelType variance=0;
            for(int a=0;a<Power<3,DIM>::value;++a){
                VoxelType diff = values[a]-meanvalue;
                variance += diff*diff;
            }
            
            imdata[imindex] = variance;
        } NDL_ENDFOREACH
    }

    
/*
From wikipedia for connected components:
On the first pass:

   1. Iterate through each element of the data
   2. If the element is not the background
         1. Get the neighboring elements of the current element
         2. If there are no neighbors, uniquely label the current element and continue
         3. Otherwise, find the neighbor with the smallest label and assign it to the current element
         4. Store the equivalence between neighboring labels

On the second pass:

   1. Iterate through each element of the data by column, then by row
   2. If the element is not the background
         1. Relabel the element with the lowest equivalent label
*/
    template<class T>int connectedcomponents(Image<T,DIM> &result,VoxelType threshold1,VoxelType threshold2,unsigned int numrequiredneighbors=1,unsigned int numrequiredelements=1){std::vector<std::pair<int,int>> labelsizelist; return connectedcomponents(result,labelsizelist,threshold1,threshold2,numrequiredneighbors,numrequiredelements); }
    template<class T>int connectedcomponents(Image<T,DIM> &result,std::vector< std::pair<int,int> >& labelsizelist,VoxelType threshold1,VoxelType threshold2,unsigned int numrequiredneighbors=1,unsigned int numrequiredelements=1){
        int currentlabel=0;
        std::vector<unsigned int> equivalencelabels;
        std::vector<unsigned int> nvoxelsperlabel;
        equivalencelabels.push_back(0);
        nvoxelsperlabel.push_back(0);
        result.imagecopy(*this,this->m_regionsize);
        T* imdata = result.m_data;
        int* imdimprod = result.m_dimPROD;
        int* imdimarray = result.m_dimarray;
        result.setvalue(0);
        
        //first pass - mark each region, make label tree for saving 
        //regions to be combined
        int tarray[DIM];
        NDL_FOREACH2(result,*this){ //change to parallel later
            int tarray[DIM];
            int imtarray[DIM];
            int index,imindex,validflag;
            NDL_GETCOORDPAIR(validflag,imindex,imtarray,index,tarray);
            
            //if background, skip to next voxel
            if (m_data[index]<threshold1 || m_data[index]>threshold2) continue;
            
            //find neighbors
            int numneighbors=0;
            T neighbors[DIM];
            for(int n=0;n<DIM;n++){
                neighbors[n] = 0;
                int t=imtarray[n] - 1; //t is previous neighbor for this dimension
                if (t>=0 && t<imdimarray[n]){ //make sure we are in the bounds of the image
                    neighbors[n]=imdata[imindex - imdimprod[n]];
                    if (neighbors[n]!=0) numneighbors++;
                }
            }

            //find neighbors roots and the minimum root
            T minroot=0;
            for(int n=0;n<DIM;n++){
                //find root of neighbor
                int root=neighbors[n];
                if (root){
                    while (root!=equivalencelabels[root]) root = equivalencelabels[root];
                    neighbors[n]=root;
                    if (minroot) minroot = (std::min<T>)(minroot,root);
                    else minroot=root;
                }
            }
            
            if (numneighbors>=numrequiredneighbors){
                //store equvalance info
                for(int n=0;n<DIM;n++){
                    if (neighbors[n] && neighbors[n]!=minroot){
                        equivalencelabels[neighbors[n]] = minroot;
                    }
                }
                //assign the label
                imdata[imindex]=minroot;
                nvoxelsperlabel[minroot]++;
            } else {
                //create a new label
                currentlabel++;
                equivalencelabels.push_back(currentlabel);
                nvoxelsperlabel.push_back(1);
                imdata[imindex] = currentlabel;
            }
        } NDL_ENDFOREACH
       
        //flatten labels
        int s=equivalencelabels.size();
        for(int i=s-1;i>0;i--){
            //find root
            int root=i;
            while (root!=equivalencelabels[root]) root = equivalencelabels[root];
            //update labels to root
            int t=i;
            int prevt=0;
            while (t!=equivalencelabels[t]){
                if (prevt){
                    equivalencelabels[prevt] = root;
                    nvoxelsperlabel[root]+=nvoxelsperlabel[prevt];
                    nvoxelsperlabel[prevt]=0;
                }
                prevt=t;
                t = equivalencelabels[t];
            }
        }

        //build vector of final regions, then sort by size
        for(int i=1;i<s;i++) labelsizelist.push_back( std::pair<int,int>(equivalencelabels[i],nvoxelsperlabel[equivalencelabels[i]]));
        std::stable_sort(labelsizelist.begin(), labelsizelist.end(), less_second<std::pair<int,int>>());
        
        //relabel so that biggest regions are the brightest
        int count=0;
        std::map<int,int> region_label_map;
        for (std::vector< std::pair<int,int> >::iterator i = labelsizelist.begin(); i != labelsizelist.end(); i++){
            if (i->second >= numrequiredelements){
                if (region_label_map.find(i->first) == region_label_map.end()) count++;
                region_label_map[i->first]=count;
            }
            else region_label_map[i->first]=0;
        }
        for(int i=1;i<s;i++) equivalencelabels[i] = region_label_map[equivalencelabels[i]];
        region_label_map.clear();
                
        //update the image
        NDL_FOREACHPL(result){
            int imindex;
            NDL_GETINDEX(imindex);
            imdata[imindex] = equivalencelabels[imdata[imindex]];//update the image
        } NDL_ENDFOREACH
        
        return count;
    }
    
    void watershed(VoxelType threshold1,VoxelType threshold2){
        //threshold1 and threshold2 define range of marker values

        std::priority_queue<watershednode<VoxelType,DIM>, std::vector<watershednode<VoxelType,DIM> >,std::less<std::vector<watershednode<VoxelType,DIM> >::value_type > > pq; 	//The priorities are in Ascending Order of value
        //~ priority_queue<watershednode<VoxelType,DIM>, vector<watershednode<VoxelType,DIM> >,greater<vector<watershednode<VoxelType,DIM> >::value_type > > pq; 	//The priorities are in the Descending Order of value
        
        //populate priority queue
        NDL_FOREACH(*this){
            int coord[DIM];
            int index;
            NDL_GETCOORD(index,coord);
            if (m_data[index]<threshold1 || m_data[index]>threshold2) continue;
            for(int n=0;n<DIM;n++){
                //look at previous voxel
                coord[n]-=1; 
                if (coord[n]>=m_orgin[n] && coord[n]<m_orgin[n]+m_regionsize[n]){ //make sure we are in the bounds of the selection
                    int tindex = index - m_dimPROD[n];
                    if (m_data[tindex]<threshold1 || m_data[tindex]>threshold2){
                        pq.push(watershednode<VoxelType,DIM>(m_data[tindex], tindex, coord, m_data[index]));
                    }
                }
                //look at next voxel
                coord[n]+=2; 
                if (coord[n]>=m_orgin[n] && coord[n]<m_orgin[n]+m_regionsize[n]){ //make sure we are in the bounds of the selection
                    int tindex = index + m_dimPROD[n];
                    if (m_data[tindex]<threshold1 || m_data[tindex]>threshold2){
                        pq.push(watershednode<VoxelType,DIM>(m_data[tindex], tindex, coord, m_data[index]));
                    }
                }
            }
            
        } NDL_ENDFOREACH
        
        //process queue
        while ( !pq.empty() ) {
            int* coord = pq.top().coord;
            int index = pq.top().index;
            VoxelType label = pq.top().label;

            if (m_data[index]>=threshold1 && m_data[index]<=threshold2){
                pq.pop();
                continue;
            }
                
            //add neighbors to queue
            printf(".");
            for(int n=0;n<DIM;n++){
                //look at previous voxel
                printf("&");
                coord[n]-=1;
                printf("=");
                if (coord[n]>=m_orgin[n] && coord[n]<m_orgin[n]+m_regionsize[n]){ //make sure we are in the bounds of the selection
                    int tindex = index - m_dimPROD[n];
                    if (m_data[tindex]<threshold1 || m_data[tindex]>threshold2){
                        printf("1");
                        pq.push(watershednode<VoxelType,DIM>(m_data[tindex], tindex, coord, label));
                        printf("2");
                    }
                }
                //look at next voxel
                coord[n]+=2; 
                if (coord[n]>=m_orgin[n] && coord[n]<m_orgin[n]+m_regionsize[n]){ //make sure we are in the bounds of the selection
                    int tindex = index + m_dimPROD[n];
                    if (m_data[tindex]<threshold1 || m_data[tindex]>threshold2){
                        printf("3");
                        pq.push(watershednode<VoxelType,DIM>(m_data[tindex], tindex, coord, label));
                        printf("4");
                    }
                }
            }
            printf("!");
            
            //mark, then remove self from queue
            m_data[index] = label;
            pq.pop();
        }
    }

    template<class T> void segmentbackground(Image<T,DIM> &result,float newvalue,float threshold1,float threshold2,int minimumacceptablesize=1,int connectedness=1){
        //setup result if need be
        if (result.isempty()) result.imagecopy(*this,this->m_regionsize);
        
        //segment largest chunck of background
        Image<int,DIM> backgroundresult;
        int largestlabel = connectedcomponents(backgroundresult,threshold1,threshold2,connectedness);
        
        //set background on result to min-1
        result.minmax(); //should we do this??
        int minvalue = result.m_min;
        int maxvalue = result.m_max;
        result.setvalue(minvalue-1,backgroundresult,largestlabel,largestlabel);

        //segment foreground object and eliminate objects that are too small
        if (minimumacceptablesize>1){
            largestlabel = result.connectedcomponents(backgroundresult,minvalue,maxvalue,connectedness,minimumacceptablesize);
            result.setvalue(minvalue-1,backgroundresult,0,0);
        }
        result.setvalue(newvalue,minvalue-1,minvalue-1);
    }
    
    //given a distance image (from the 'distancetransform' method)
    //make a plot of ave intensity at each distance
    void makedistanceplot(Image<VoxelType,1> &meanresult,Image<int,1>& countresult,Image<VoxelType,1> &stdevresult,Image<float,DIM> &distancetransform,VoxelType threshold1=0,VoxelType threshold2=-1){ makedistanceplot(meanresult,countresult,stdevresult,distancetransform,*this,threshold1,threshold2); }
    template<class T> void makedistanceplot(Image<VoxelType,1> &meanresult,Image<int,1>& countresult,Image<VoxelType,1> &stdevresult,Image<float,DIM> &distancetransform,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        float tmin,tmax;
        distancetransform.minmax(tmin,tmax,maskimage,threshold1,threshold2);
        int dmin = (tmin+0.5);
        int dmax = (tmax+0.5);
        int drange = dmax-dmin + 1;
        //printf("dmin: %d, dmax: %d, drange: %d\n",dmin,dmax,drange);
        
        //setup images
        meanresult.setupdimensions(&drange,m_numcolors);
        meanresult.setupdata();
        meanresult.setvalue(0);
        countresult = meanresult; //count the number of points per value
        stdevresult = meanresult; //count the number of points per value
            
        //setup data vars
        float* dtdata = distancetransform.m_data;
        VoxelType* dpdata = meanresult.m_data;
        VoxelType* stdevdata = stdevresult.m_data;
        int* cpdata = countresult.m_data;
        T* maskdata = maskimage.m_data;
        bool dothreshold = (threshold2>=threshold1);

        //get mean and count values
        NDL_FOREACH(*this){
            int index;
            NDL_GETINDEX(index);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            int dpindex = CLAMP(dtdata[index] - dmin,0,drange-1);
            dpdata[dpindex] += m_data[index];
            cpdata[dpindex]++;
        } NDL_ENDFOREACH
        for(int i=0;i<drange;i++) if (cpdata[i]) dpdata[i]/=cpdata[i];
        
        //now get stdev values
        NDL_FOREACH(*this){
            int index;
            NDL_GETINDEX(index);
            if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
            int dpindex = CLAMP(dtdata[index] - dmin,0,drange-1);
            float diff = m_data[index]-dpdata[dpindex];
            //~ printf("data - mean: %f - %f = %f\n",m_data[index],dpdata[dpindex],m_data[index]-dpdata[dpindex]);
            stdevdata[dpindex] += diff*diff;
        } NDL_ENDFOREACH
        for(int i=0;i<drange;i++) if (cpdata[i]) stdevdata[i]=sqrt(stdevdata[i]/cpdata[i]);
            
        //change rotation center to coorespond to the edge distance
        meanresult.m_rotorgin[0]=-dmin;
        countresult.m_rotorgin[0]=-dmin;
        stdevresult.m_rotorgin[0]=-dmin;
    }


    //make a histogram and save the mean for each bin too
    void getdistancehistogram(Image<VoxelType,1> &result,Image<unsigned int,1> &histogram,int numbins,Image<float,DIM> &distancetransform,VoxelType threshold1=0,VoxelType threshold2=-1){ getdistancehistogram(result,histogram,numbins,distancetransform,*this,threshold1,threshold2); }
    template<class T> void getdistancehistogram(Image<VoxelType,1> &result,Image<unsigned int,1> &histogram,int numbins,Image<float,DIM> &distancetransform,Image<T,DIM>& maskimage,T threshold1,T threshold2){
        //init stuff
        if (numbins<=0) return;
        if (histogram.isempty() || numbins!=histogram.m_numvoxels){ histogram.setupdimensions(&numbins,1); histogram.setupdata(); }
        if (result.isempty() || numbins!=result.m_numvoxels){ result.setupdimensions(&numbins,1); result.setupdata(); }

        //init the histogram
        histogram.setvalue(0);
        result.setvalue(0);
        unsigned int* res = histogram.data();
        float* resmean = result.data();
        T* maskdata = maskimage.m_data;
        VoxelType* distancedata = distancetransform.m_data;
        bool dothreshold = (threshold2>=threshold1);
        
        VoxelType vmin,vmax,vrange;
        minmax(vmin,vmax);
        vrange = vmax - vmin;
        if (vmin<vmax){
            NDL_FOREACHPL(*this){
                int index;
                NDL_GETINDEX(index);
                if (dothreshold && (maskdata[index]<threshold1 || maskdata[index]>threshold2)) continue;
                int pos = (int)((distancedata[index]-vmin)*(numbins-1)/vrange);
                if (pos>=0 && pos<(int)numbins){ ++res[pos]; resmean[pos]+=m_data[index]; }
            } NDL_ENDFOREACH
        } else res[0]+=m_numvoxels;

        //normalize
        printf("normalize\n");
        for(int i=0;i<histogram.m_numvoxels;i++){
            if (res[i]) resmean[i]/=res[i];
            else resmean[i]=0;
        }
        
    }
    
    //Make a gaussian
    void gaussianwindow(float stdev,bool centeredflag=true){
        Image<VoxelType,DIM> im;
        im.imagecopy(*this,this->m_regionsize);
        im.makegaussian(stdev,centeredflag);
        *this *= im;
    }
    void gaussianwindow(int dimlist[DIM],float meanlist[DIM],float stdevlist[DIM]){
        Image<VoxelType,DIM> im;
        im.imagecopy(*this,this->m_regionsize);
        im.makegaussian(dimlist,meanlist,stdevlist);
        *this *= im;
    }
    void makegaussian(float stdev,bool centeredflag=true){
        int dimlist[DIM];
        float meanlist[DIM];
        float stdevlist[DIM];
        for(int i=0;i<DIM;i++){
            dimlist[i]=i;
            if (centeredflag) meanlist[i]=m_orgin[i] + m_regionsize[i]/2;
            else meanlist[i]=m_orgin[i];
            stdevlist[i]=stdev;
        }
        makegaussian(dimlist,meanlist,stdevlist);
    }
    void makegaussian(int dimlist[DIM],float meanlist[DIM],float stdevlist[DIM]){
        float oneovertwotimesvarlist[DIM];
        for(int i=0;i<DIM;i++) oneovertwotimesvarlist[i] = 1.0/(2.0*stdevlist[i]*stdevlist[i]);
        NDL_FOREACHPL(*this){
            int tarray[DIM];
            int index;
            NDL_GETCOORD(index,tarray);
            float expon=0;
            for(int i=0;i<DIM;i++){
                int axis = dimlist[i];
                if (axis!=-1){
                    float value = tarray[axis]-meanlist[i];
                    if (value>=m_regionsize[i]/2) value -= m_regionsize[axis];
                    if (value<-m_regionsize[i]/2) value += m_regionsize[axis];
                    value *= value;
                    value *= oneovertwotimesvarlist[i];
                    expon -= value;
                }
            }
            m_data[index]=exp((double)expon);
        } NDL_ENDFOREACH
    }
    
    //stdev in units
    void unsharp(float stdev){ Image<VoxelType,DIM> im(*this); im.gaussianblur(stdev); *this *= 2; *this -= im; }
    void gaussianblur(float stdev,int freqflag=false){
        if (stdev==0) return;
        int dimlist[DIM];
        float stdevlist[DIM];
        for(int i=0;i<DIM;i++){
            dimlist[i]=i;
            stdevlist[i]=stdev;
        }
        if (freqflag) gaussianblur_freq(dimlist,stdevlist);
        else gaussianblur(dimlist,stdevlist);
    }
    
    //stdev in units for the given dimensions, if dimlist[i]==-1, then skip that dimension
    void gaussianblur(int dimlist[DIM],float stdevlist[DIM]){
        //create a gaussian image using a seperable filter
        for(int i=0;i<DIM;i++){
            if (stdevlist[i]!=-1){
                Image<VoxelType,1> kernel(3*stdevlist[i]/m_voxelsize[i],1);
                kernel.makegaussian(stdevlist[i]/m_voxelsize[i]);
                kernel/=kernel.getsum();
                convolve(kernel,i);
            }
        }
    }
        
    void gaussianblur_freq(int dimlist[DIM],float stdevlist[DIM]){
        //do fft
        Image<VoxelType,DIM> real;
        Image<VoxelType,DIM> imag;
        fft(real,imag);
        
        //create a gaussian image for freqency domain multiplication
        Image<VoxelType,DIM> gauss;
        float meanlist[DIM];
        float freq_stdevlist[DIM];
        for(int i=0;i<DIM;i++){
            if (stdevlist[i]!=-1){
                freq_stdevlist[i]=real.m_regionsize[i]*m_voxelsize[i]/(2*stdevlist[i]);
                meanlist[i]=0;
            } else freq_stdevlist[i]=-1;
        }
        real.gaussianwindow(dimlist,meanlist,freq_stdevlist);
        imag.gaussianwindow(dimlist,meanlist,freq_stdevlist);
        
        //ifft
        ifft(real,imag);
    }

    void TVdenoise(int numsteps=10,float epsilon=1,float lambda=1,Image<VoxelType,DIM>* lambdaimage=0,float stdev=0){
        //Guy Gilboa, Nir Sochen, Yehoshua Y. Zeevi, "Texture Preserving Variational Denoising Using an Adaptive Fidelity Term", to appear in Proc. VLSM 2003, Nice, France, Oct. 2003.
        float epsilon2 = epsilon*epsilon;
        float dt = epsilon/5;
        float mymean = getmean();
        Image<VoxelType,DIM> originalimage(*this);
        for(int i=0;i<numsteps;i++){
            //calcualte gradiant image
            Image<VoxelType,DIM> TempImage;
            edge(TempImage);
            
            VoxelType* blurdata = m_data;
            Image<VoxelType,DIM> BlurImage;
            if (stdev!=0){
                BlurImage = *this;
                BlurImage.gaussianblur(stdev);
                blurdata = BlurImage.m_data;
            }
                        
            //update image
            NDL_FOREACHPL(*this){
                int index;
                NDL_GETINDEX(index);
                //~ float tvterm = TempImage(index)/sqrt((float)(epsilon2 + TempImage(index)*TempImage(index)));
                //~ float fidelityterm = (blurdata[index] - originalimage.m_data[index]);
                //~ float lambdavalue = lambda;
                //~ if (lambdaimage) lambdavalue *= lambdaimage->m_data[index];
                //~ float update = lambdavalue*tvterm + (1-lambdavalue)*fidelityterm;
                
                float tvterm = TempImage(index);
                float fidelityterm = (blurdata[index] - originalimage.m_data[index]);
                fidelityterm*=fidelityterm;
                float lambdavalue = lambda;
                float update = lambdavalue*tvterm + (1-lambdavalue)*fidelityterm;
                
                m_data[index] -= dt*update;
            } NDL_ENDFOREACH
            
            //refresh views
            printf("%d ",i);
            refreshviews();
        }
        
        //normalize to original mean
        *this/=getave();
        *this*=originalimage.getave();
        
        printf("\n");
    }

    //stdev in units
    void gaussiandeblur(float stdev){
        if (stdev==0) return;
        int dimlist[DIM];
        float stdevlist[DIM];
        for(int i=0;i<DIM;i++){
            dimlist[i]=i;
            stdevlist[i]=stdev;
        }
        gaussiandeblur_freq(dimlist,stdevlist);
    }

    void fftmirror(Image<VoxelType,DIM>& tempimage){
        int dimlist[DIM];
        int dim[DIM];
        for(int i=0;i<DIM;i++){
            dimlist[i]=i;
            if (m_regionsize[i]>1){
                dimlist[i]=i;
                dim[i] = nextpowof2(m_regionsize[i]*2);
            } else {
                dimlist[i]=-1;
                dim[i]=m_regionsize[i];
            }
        }
        mirror(tempimage,dimlist,dim);
    }

    //create mirror image
    void mirror(Image<VoxelType,DIM>& tempimage,int dimlist[DIM],int dim[DIM]){
        tempimage.imagecopy(*this,dim);
        VoxelType* tdata = tempimage.m_data;
        tempimage.select();
        for(int c=0;c<DIM;c++){
            if (dimlist[c]==-1 || dim[c]<=1) continue;
            int axis = dimlist[c];
            int dimprod = tempimage.m_dimPROD[axis];
            int originalregionsize = m_regionsize[axis];
            int regionsize = tempimage.m_regionsize[axis];
            int halfregionsize = regionsize/2;
            NDL_FOREACHLINE(tempimage,axis){
                int index;
                NDL_GETINDEX(index);
                int t2 = regionsize-1;
                for(int t=0;t<halfregionsize;t++){
                    int temp = originalregionsize - t;
                    if (temp<=0) tdata[index + t*dimprod] = tdata[index + (t+2*temp-1)*dimprod];
                    tdata[index + t2*dimprod]=tdata[index + t*dimprod];
                    t2--;
                }
            } NDL_ENDFOREACH
        }
    }
    
    void gaussiandeblur_freq(int dimlist[DIM],float stdevlist[DIM]){
        Image<VoxelType,DIM> tempimage;
        fftmirror(tempimage);
                
        //do fft
        Image<VoxelType,DIM> real;
        Image<VoxelType,DIM> imag;
        tempimage.fft(real,imag);
        
        //create a gaussian image for freqency domain multiplication
        float meanlist[DIM];
        float freq_stdevlist[DIM];
        for(int i=0;i<DIM;i++){
            if (stdevlist[i]!=-1){
                freq_stdevlist[i]=real.m_regionsize[i]*m_voxelsize[i]/(2*stdevlist[i]);
                meanlist[i]=0;
            } else freq_stdevlist[i]=-1;
        }
                
        //create a gaussian image
        Image<VoxelType,DIM> gaussian(real);
        gaussian.makegaussian(dimlist,meanlist,freq_stdevlist);
                
        //divide the real and imaginary parts by the gaussian
        real/=gaussian;
        imag/=gaussian;
        
        //ifft
        ifft(real,imag);
    }
    
    void HanningWindow(bool centeredflag=true){
        Image<VoxelType,DIM> im;
        im.imagecopy(*this,this->m_regionsize);
        im.MakeHanning(centeredflag);
        *this *= im;
    }
    void HanningWindow(int dimlist[DIM],int meanlist[DIM]){
        Image<VoxelType,DIM> im;
        im.imagecopy(*this,this->m_regionsize);
        im.MakeHanning(dimlist,meanlist);
        *this *= im;
    }
    void MakeHanning(bool centeredflag=true){
        int dimlist[DIM];
        int meanlist[DIM];
        for(int i=0;i<DIM;i++){
            dimlist[i]=i;
            if (centeredflag) meanlist[i]=m_orgin[i] + m_regionsize[i]/2;
            else meanlist[i]=m_orgin[i];
        }
        MakeHanning(dimlist,meanlist);
    }
    void MakeHanning(int dimlist[DIM],int meanlist[DIM]){
        //for 1D, hanning(x) = cos^2(pi*x/2a)
        bool firstflag=true;
        int naxis=0;
        for(int a=0;a<DIM;a++) if (dimlist[a]>=0) naxis++;
        for(int axisindex=0;axisindex<DIM;axisindex++){
            int axis = dimlist[axisindex];
            int meanvalue = meanlist[axisindex] - m_orgin[axis];
            if (axis==-1) continue;
            
            //setup vars
            int dimprod = m_dimPROD[axis];
            int regionsize = m_regionsize[axis];
            int halfregionsize = regionsize/2.0;

            //ITERATE OVER ALL LINES
            NDL_FOREACHLINE(*this,axis){
                int index;
                NDL_GETINDEX(index);

                for (int t=0; t<regionsize; ++t){
                    int tindex = index + t*dimprod;
                    float value = t - meanvalue;
                    
                    if (value>=halfregionsize) value -= regionsize;
                    if (value<-halfregionsize) value += regionsize;
                    
                    value = cos(0.5*M_PI*value/(std::max)(1,halfregionsize));
                    if (firstflag) m_data[tindex] = value;
                    else m_data[tindex]*=value;
                }
                
            } NDL_ENDFOREACH
            firstflag=false;
        }
        
        //normalize
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            m_data[index] = pow((float)m_data[index],(float)(2/(float)naxis));
        } NDL_ENDFOREACH
    }

    void SincWindow(float cycles,bool centeredflag=true){
        Image<VoxelType,DIM> im;
        im.imagecopy(*this,this->m_regionsize);
        im.MakeSinc(cycles,centeredflag);
        *this *= im;
    }
    void SincWindow(int dimlist[DIM],int meanlist[DIM],float cycles){
        Image<VoxelType,DIM> im;
        im.imagecopy(*this,this->m_regionsize);
        im.MakeSinc(dimlist,meanlist,cycles);
        *this *= im;
    }
    void MakeSinc(float cycles,bool centeredflag=true){
        int dimlist[DIM];
        int meanlist[DIM];
        for(int i=0;i<DIM;i++){
            dimlist[i]=i;
            if (centeredflag) meanlist[i]=m_orgin[i] + m_regionsize[i]/2;
            else meanlist[i]=m_orgin[i];
        }
        MakeSinc(dimlist,meanlist,cycles);
    }
    void MakeSinc(int dimlist[DIM],int meanlist[DIM],float cycles){
        bool firstflag=true;
        int naxis=0;
        for(int a=0;a<DIM;a++) if (dimlist[a]>=0) naxis++;
        for(int axisindex=0;axisindex<DIM;axisindex++){
            int axis = dimlist[axisindex];
            int meanvalue = meanlist[axisindex] - m_orgin[axis];
            if (axis==-1) continue;
            
            //setup vars
            int dimprod = m_dimPROD[axis];
            int regionsize = m_regionsize[axis];
            int halfregionsize = regionsize/2.0;

            //ITERATE OVER ALL LINES
            NDL_FOREACHLINE(*this,axis){
                int index;
                NDL_GETINDEX(index);

                for (int t=0; t<regionsize; ++t){
                    int tindex = index + t*dimprod;
                    float value = t - meanvalue;
                    
                    if (value>=halfregionsize) value -= regionsize;
                    if (value<-halfregionsize) value += regionsize;
                    
                    value = M_PI*cycles*value/(std::max)(1,halfregionsize);
                    if (value) value = sin(value)/value; 
                    else value = 1;
                    
                    if (firstflag) m_data[tindex] = value;
                    else m_data[tindex]*=value;
                }
                
            } NDL_ENDFOREACH
            firstflag=false;
        }
    }
    
    
    
    
    void RampWindow(bool centeredflag=true){
        Image<VoxelType,DIM> im;
        im.imagecopy(*this,this->m_regionsize);
        im.MakeRamp(centeredflag);
        *this *= im;
    }
    void RampWindow(int dimlist[DIM],int meanlist[DIM]){
        Image<VoxelType,DIM> im;
        im.imagecopy(*this,this->m_regionsize);
        im.MakeRamp(dimlist,meanlist);
        *this *= im;
    }
    void MakeRamp(bool centeredflag=true){
        int dimlist[DIM];
        int meanlist[DIM];
        for(int i=0;i<DIM;i++){
            dimlist[i]=i;
            if (centeredflag) meanlist[i]=m_orgin[i] + m_regionsize[i]/2;
            else meanlist[i]=m_orgin[i];
        }
        MakeRamp(dimlist,meanlist);
    }
    void MakeRamp(int dimlist[DIM],int meanlist[DIM]){
        bool firstflag=true;
        int naxis=0;
        for(int axisindex=0;axisindex<DIM;axisindex++){
            int axis = dimlist[axisindex];
            int meanvalue = meanlist[axisindex] - m_orgin[axis];
            if (axis==-1) continue;
            
            //setup vars
            int dimprod = m_dimPROD[axis];
            int regionsize = m_regionsize[axis];
            int halfregionsize = regionsize/2.0;
            float a = 1.0;

            //ITERATE OVER ALL LINES
            NDL_FOREACHLINE(*this,axis){
                int index;
                NDL_GETINDEX(index);
                
                for (int t=0; t<regionsize; ++t){
                    int tindex = index + t*dimprod;
                    int dindex = t - meanvalue;
                    
                    if (dindex>=halfregionsize) dindex -= regionsize;
                    if (dindex<-halfregionsize) dindex += regionsize;
                    
                    float value;
                    if( dindex == 0 ) value = 1.0 / (4.0*a*a);
                    else {
                        if( dindex % 2 == 0 ) value = 0.0;
                        else value = -1 / (dindex*dindex*M_PI*M_PI*a*a);
                    }
                        
                    if (firstflag) m_data[tindex] = value;
                    else m_data[tindex]*=value;
                }
                
            } NDL_ENDFOREACH
            firstflag=false;
        }
        
        //fft, then take magnitude
        Image<float,DIM> real;
        Image<float,DIM> imag;
        fft(real,imag);
        real.tomag(imag);
        imagecopy(real,real.m_regionsize);
    }
    
    //almost done, but not quite
    void cannyedge(float stdev=1,float agressiveness=1){
        gaussianblur(stdev);
        edgesobel();
        thin(agressiveness);
        //do histeresis ... (two thresholds with region growing)
    }
    
    //convolve a 1D function (for use with seperable kernels like gaussian)
    void convolve(Image<VoxelType,1> &kernel,int dim=0){ Image<VoxelType,DIM> im; convolve(im,kernel,dim); this->copyregion(im); }
    void convolve(Image<VoxelType,DIM> &im,Image<VoxelType,1> &kernel,int dim=0){
        if (im.isempty()) im.imagecopy(*this,this->m_regionsize);
        VoxelType* imdata = im.m_data;
        int delta = m_dimPROD[dim];
        int length = kernel.m_dimarray[0];
        VoxelType* kerneldata = kernel.m_data;
        int offset = -length/2;
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            VoxelType value = 0;
            int pos = offset*delta;
            for(int i=0;i<length;i++){
                int dindex = index + pos;
                if (dindex>=0 && dindex<m_numvoxels){
                    value += m_data[dindex]*kerneldata[i];
                } else value += m_data[index]*kerneldata[i];
                pos+=delta;
            }
            imdata[index] = value;
        } NDL_ENDFOREACH
    }
    
    //im is output image, kernel is an input image (NOT DONE YET)
    void convolve(Image<VoxelType,DIM> &kernel,int dim1=-1,int dim2=-1,int dim3=-1,int dim4=-1){
        //do fft
        Image<VoxelType,DIM> real;
        Image<VoxelType,DIM> imag;
        fft(real,imag,dim1,dim2,dim3,dim4);

        //resize the kernel if need be
        //...
        
        //fft the kernel
        Image<VoxelType,DIM> kernelreal;
        Image<VoxelType,DIM> kernelimag;
        kernel.fft(kernelreal,kernelimag,dim1,dim2,dim3,dim4);

        //complex multiply FT(kernel) by the FT(image), then unshift and ifft
        real *= kernelreal;
        real -= imag*kernelimag;
        imag *= kernelreal;
        imag += real*kernelimag;
        ifft(real,imag,dim1,dim2,dim3,dim4);
    }
    
    //convert a complex image (real/imaginary) to mag/phase
    void tomagphase(Image<VoxelType,DIM> &im_imag){
        VoxelType* imdata = im_imag.m_data;
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            VoxelType imag = imdata[index];
            VoxelType real = m_data[index];
            VoxelType phase = 0;
            VoxelType mag = 0;
            if (imag || real){
                mag = sqrt(imag*imag + real*real);
                phase = atan2(imag,real);
            }
            m_data[index] = mag;
            imdata[index] = phase;
        } NDL_ENDFOREACH
        return;
    }

    void tomag(Image<VoxelType,DIM> &im_imag){
        VoxelType* imdata = im_imag.m_data;
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            VoxelType imag = imdata[index];
            VoxelType real = m_data[index];
            VoxelType mag = 0;
            if (imag || real){
                mag = sqrt(imag*imag + real*real);
            }
            m_data[index] = mag;
        } NDL_ENDFOREACH
        return;
    }
    
    //convert a mag/phase image to complex
    void tocomplex(Image<VoxelType,DIM> &im_phase){
        VoxelType* imdata = im_phase.m_data;
        NDL_FOREACHPL(*this){
            int index;
            NDL_GETINDEX(index);
            VoxelType phase = imdata[index];
            VoxelType mag = m_data[index];
            VoxelType real = mag*cos(phase);
            VoxelType imag = mag*sin(phase);                
            m_data[index] = real;
            imdata[index] = imag;
        } NDL_ENDFOREACH
        return;
    }
    
    //im_result is output
    //dim specifies the dimension along which to do the transform, -1 cooresponds to all dimensions
    void fft(Image<VoxelType,DIM> &im_real,Image<VoxelType,DIM> &im_imag,int dim1=-1,int dim2=-1,int dim3=-1,int dim4=-1){
        //setup dimlist
        int dimlist[4];
        int dimlistsize=0;
        if (dim1!=-1) dimlist[dimlistsize++]=dim1;
        if (dim2!=-1) dimlist[dimlistsize++]=dim2;
        if (dim3!=-1) dimlist[dimlistsize++]=dim3;
        if (dim4!=-1) dimlist[dimlistsize++]=dim4;
        
        //get power of 2 dimensions for fft
        int dim[DIM];
        
        if (dimlistsize==0){
            for(int i=0;i<DIM;i++) dim[i]=nextpowof2(m_regionsize[i]);
        } else {
            for(int i=0;i<DIM;i++) dim[i]=m_regionsize[i];
            for(int c=0;c<dimlistsize;c++) dim[dimlist[c]]=nextpowof2(dim[dimlist[c]]);
        }
        im_real.imagecopy(*this,dim);
        
        //do fft
        im_real._fft(im_imag,dimlist,dimlistsize,false);
    }
    
    //im_result is output
    //dim specifies the dimension along which to do the transform, -1 cooresponds to all dimensions
    void fft_padded(Image<VoxelType,DIM> &im_real,Image<VoxelType,DIM> &im_imag,int dim1=-1,int dim2=-1,int dim3=-1,int dim4=-1){
        //setup dimlist
        int dimlist[DIM];
        int dimlistsize=0;
        for(int i=0;i<DIM;i++) dimlist[i]=-1;
        if (dim1!=-1 && dimlistsize<DIM) dimlist[dimlistsize++]=dim1;
        if (dim2!=-1 && dimlistsize<DIM) dimlist[dimlistsize++]=dim2;
        if (dim3!=-1 && dimlistsize<DIM) dimlist[dimlistsize++]=dim3;
        if (dim4!=-1 && dimlistsize<DIM) dimlist[dimlistsize++]=dim4;
            
        //get power of 2 dimensions for fft
        int dim[DIM];
        if (dimlistsize==0){
            for(int i=0;i<DIM;i++) dim[i]=2*nextpowof2(m_regionsize[i]);
        } else {
            for(int i=0;i<DIM;i++) dim[i]=m_regionsize[i];
            for(int c=0;c<dimlistsize;c++) dim[dimlist[c]]=2*nextpowof2(dim[dimlist[c]]);
        }
                
        //take mirror image for best fft results 
        //(to avoid aliasing from high frequency edges)
        mirror(im_real,dimlist,dim);
        
        //do fft
        im_real._fft(im_imag,dimlist,dimlistsize,false);
    }
    
    void ifft(Image<VoxelType,DIM> &im_real,Image<VoxelType,DIM> &im_imag,int dim1=-1,int dim2=-1,int dim3=-1,int dim4=-1){
        //setup dimlist
        int dimlist[4];
        int dimlistsize=0;
        if (dim1!=-1) dimlist[dimlistsize++]=dim1;
        if (dim2!=-1) dimlist[dimlistsize++]=dim2;
        if (dim3!=-1) dimlist[dimlistsize++]=dim3;
        if (dim4!=-1) dimlist[dimlistsize++]=dim4;
            
        //do ifft (assume im_real and im_imag are pow of 2)
        im_real._fft(im_imag,dimlist,dimlistsize,true);
        
        //copy result back to image
        this->copyregion(im_real);
    }
    
    void _fft(Image<VoxelType,DIM> &im_imag,int* dimlist,int dimlistsize,bool ifftflag){

        //Setup imaginary storage if need be
        bool hasimaginaryflag=true;
        if (im_imag.isempty()){
            im_imag.imagecopy(*this,m_dimarray);
            im_imag.setvalue(0);
            hasimaginaryflag=false;
        }
        
        //setup dimensions
        int* thedimlist = dimlist;
        bool deletelistflag=false;
        if (dimlistsize==0 || dimlist==0){
            thedimlist = allocate<int>(DIM,__LINE__);
            for(int i=0;i<DIM;i++) thedimlist[i]=i;
            dimlistsize=DIM;
            deletelistflag=true;
        }
        
        int maxN=0;
        for(int axisindex=0;axisindex<dimlistsize;axisindex++){
            int axis = thedimlist[axisindex];
            int length = m_regionsize[axis];
            int pow2len = nextpowof2(length);
            maxN=(std::max)(maxN,pow2len);
        }
        FFTOBJ<VoxelType> fftobj(maxN);
        
        //iterate over all dimensions
        for(int axisindex=0;axisindex<dimlistsize;axisindex++){
            int axis = thedimlist[axisindex];
            //setup vars
            int dimprod = m_dimPROD[axis];
            int length = m_regionsize[axis];
            if (length==1) continue;
            int halflength = length/2;
            int pow2len = nextpowof2(length);
            int p = floor(log((float)pow2len)/log((float)2) + 0.5); //add 0.5 and take floor to round to nearest int
            //printf("axis: %d, length: %d, p: %d, pow2len:%d\r",axis,length,p,pow2len);
            
            VoxelType* tdata = fftobj.fft_input;
            VoxelType* tdata2 = fftobj.fft_output;
            
            //ITERATE OVER ALL LINES
            NDL_FOREACHLINE(*this,axis){
                int index;
                NDL_GETINDEX(index);
                
                //1D fft
                for (int i=0; i<length; i++){
                    int t=i;
                    if (!ifftflag){ //center the fft
                        t+=halflength;
                        if (t>=length) t-=length;
                    }
                    int tindex = index + t*dimprod;
                    //~ if (hasimaginaryflag && ifftflag==0){
                        tdata[i << 1] = m_data[tindex];
                        tdata[(i << 1)+1] = im_imag.m_data[tindex];
                    //~ } else tdata[i] = m_data[tindex];
                }
                
                for (int i=length;i<pow2len;i++){
                    tdata[i << 1] = 0;
                    tdata[(i << 1)+1] = 0;
                }
                
                if (ifftflag) fftobj.ifft(pow2len);
                else {
                    //~ if (hasimaginaryflag){
                        fftobj.fft(pow2len);
                    //~ } else fftobj.realfft(pow2len);
                }

                //load data from array
                for (int i=0; i<length; i++){
                    int t=i;
                    if (ifftflag){ //center the image
                        t+=halflength;
                        if (t>=length) t-=length;
                    }
                    int tindex = index + t*dimprod;
                    m_data[tindex] = tdata2[i << 1];
                    im_imag.m_data[tindex] = tdata2[(i << 1)+1];
                }
                
            } NDL_ENDFOREACH
        }
        if (deletelistflag) deallocate(thedimlist);
    }
    
    //do the fftshift of the specified dimensions (or all if none specified) 
    void fftshift(int dim1=-1,int dim2=-1,int dim3=-1,int dim4=-1){
        int dimlist[4];
        int dimlistsize=0;
        if (dim1!=-1) dimlist[dimlistsize++]=dim1;
        if (dim2!=-1) dimlist[dimlistsize++]=dim2;
        if (dim3!=-1) dimlist[dimlistsize++]=dim3;
        if (dim4!=-1) dimlist[dimlistsize++]=dim4;
        fftshift(dimlist,dimlistsize);
    }
    void fftshift(int* dimlist,int dimlistsize){
        int* thedimlist = dimlist;
        bool deletelistflag=false;
        if (dimlistsize==0 || dimlist==0){
            thedimlist = allocate<int>(DIM,__LINE__);
            for(int i=0;i<DIM;i++) thedimlist[i]=i;
            dimlistsize=DIM;
            deletelistflag=true;
        }
        for(int axisindex=0;axisindex<dimlistsize;axisindex++){
            int axis = thedimlist[axisindex];
            //setup vars
            int dimprod = m_dimPROD[axis];
            int regionsize = m_regionsize[axis];
            int halfregionsize = regionsize/2;

            //ITERATE OVER ALL LINES
            NDL_FOREACHLINE(*this,axis){
                int index;
                NDL_GETINDEX(index);
                //fftshift
                for (int t=0; t<halfregionsize; ++t){
                    int tindex = index + t*dimprod;
                    int t2index = index + (t+halfregionsize)*dimprod;
                    VoxelType temp = m_data[tindex];
                    m_data[tindex] = m_data[t2index];
                    m_data[t2index] = temp;
                }
            } NDL_ENDFOREACH
        }
        if (deletelistflag) deallocate(thedimlist);
    }
    
    //***********************************************************
	// IO methods
    //***********************************************************
    bool Image<VoxelType,DIM>::loadfile(std::string filename,std::string flags,int loadsequence=0,void* headerpntr=0){
        fixslashes(filename);
        m_sequenceflag = loadsequence;
        m_isloaded = NDL_loadsave(*this,filename,true,flags,headerpntr);
        if (m_isloaded){ 
            m_filename = filename;
            minmax(); 
        }
        return m_isloaded;
    }

    bool Image<VoxelType,DIM>::savefile(std::string filename,std::string flags,int sequenceflag=-1,void* headerpntr=0){
        fixslashes(filename);

        //check flags for a specified filetype
        std::map<std::string,std::string> flaglist;
        getoptionalparams(flags,flaglist);
        std::string type;
        if (flaglist.find("filetype") != flaglist.end()){
            type = flaglist["filetype"];
        } else type = m_filetype;
        if (sequenceflag!=-1) m_sequenceflag=sequenceflag;
        return NDL_loadsave(*this,filename,false,flags,headerpntr);
    }

    bool save(){ return savefile(m_filename,m_flags); }
    bool savefile(std::string filename){ return savefile(filename,m_flags); }
    bool loaded(){ return m_isloaded; }
    VoxelType* data() { return m_data; }
    int size(){ return m_numvoxels; }
    
//********************************
//MEMBER VARIABLES
//********************************
    //1) file variables
    bool m_issaved; //does the image have unsaved changes?
	bool m_isloaded; //was the image loaded correctly?
    std::string m_flags;
    std::string m_filename;
    bool m_sequenceflag;        //for use with file sequences
    //~ headerinfobase* m_headerinfo; //pointer to header data
    //~ int m_imagepluginindex;
        
    //2) data variables
	VoxelType* m_data;	        //holds the volume data
    VoxelType m_min,m_max;	     //min and max voxel values of the image

    int* m_sharedcounter;      //keeps track of how many images share the volume data (needed for destructor)
    int m_externaldataflag;    //if data was from external source, remember so we don't destruct the data

    //3) dimension variables
    int m_numcolors;
    int m_dimarray[DIM];        //dimensions in pixels
    int m_dimPROD[DIM];        //cumulative product of DIM dimensions
    int m_numvoxels;

    //4) region/selection variables
    int m_orgin[DIM];           //orgin of image in pixels (used with +,-,*,/)
    int m_regionsize[DIM];        //size of image region in pixels (used with +,-,*,/)
    int m_dimPRODregion[DIM];        //cumulative product of DIM dimensions
    int m_numregionvoxels;

    //5) position, scale, projection, and orientation variables
    std::string m_units[DIM];        //unit of measurement for each dimension (eg: mm, or s)
    Vector<double,DIM> m_voxelsize;              //size of a voxel in units defined above
    Vector<double,DIM> m_scalevector;              
    Vector<double,DIM> m_rotorgin;                  //orgin of image in units defined above (used as rotation center)
    Vector<double,DIM> m_worldposition;
    Matrix<double,DIM+1> m_rotationmatrix;     //rotation matrix for image orientation (about rotorgin)
    Matrix<double,DIM+1> m_projectionmatrix;

    //6) secondary matricies (calculated from 4 & 5 above when m_matrixrecalculateflag=true)
    bool m_matrixrecalculateflag;
    Matrix<double,DIM+1> m_voxelsizematrix;        //scale matrix based on voxelsize
    Matrix<double,DIM+1> m_rotateorginmatrix;
    Matrix<double,DIM+1> m_orientationmatrix;
    Matrix<double,DIM+1> m_iorientationmatrix;
    Matrix<double,DIM+1> m_worldmatrix; //translate to world coordinates
    Matrix<double,DIM+1> m_finalmatrix;
    Matrix<double,DIM+1> m_ifinalmatrix;

    //8) loading and viewing variables
    bool m_attemptabort;       //var to read to detect abort
	int m_progress;            //var to update progress to
    void (*startfunction)();
    void (*updatefunction)(char* text,int i);
    void (*endfunction)();
    std::string m_filetype;
    std::vector<ViewInfo<VoxelType,DIM>*> m_viewlist;
};

//************************
//watershednode Class
//************************
template <class VoxelType,int DIM>
class watershednode{
    public:
    int coord[DIM];
    int index;
    VoxelType value,label;
    watershednode(VoxelType new_value,int new_index,int* new_coord,VoxelType new_label){
        value = new_value;
        index = new_index;
        label = new_label;
        for(int i=0;i<DIM;i++) coord[i] = new_coord[i];
    }
};

//************************
//ImageIndexer Class
//************************
// Used for indexing multiple dimensions with operator[]
template<class VoxelType,int DIM,int CDIM>
class ImageIndexer {
public:
    typedef ImageIndexer<VoxelType,DIM,CDIM-1> NEXTELEMENTTYPE;
    ImageIndexer(unsigned int i,Image<VoxelType,DIM> *im){ imageptr = im; location = i; }
    typename NEXTELEMENTTYPE operator[](const unsigned int i){ return NEXTELEMENTTYPE(location + i*imageptr->m_dimPROD[DIM-CDIM],imageptr); }
    VoxelType& operator=(VoxelType& t){ return imageptr->m_data[location]=t;}
    operator VoxelType&(){ return imageptr->m_data[location]; }
private:
    Image<VoxelType,DIM>* imageptr;
    int location;
};
template<class VoxelType,int DIM>
class ImageIndexer<VoxelType,DIM,1> {
public:
    typedef VoxelType& NEXTELEMENTTYPE;
    ImageIndexer(unsigned int i,Image<VoxelType,DIM> *im){ imageptr = im; location = i; }
    NEXTELEMENTTYPE operator[](unsigned int i){ return imageptr->m_data[location + i*imageptr->m_dimPROD[DIM-1]]; }
private:
    Image<VoxelType,DIM>* imageptr;
    int location;
};

}

#include <Plugins.h>

#endif
