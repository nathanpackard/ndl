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

/*!
DICOM Image Format Plugin.
To use this plugin, you must type
#define NDL_USE_DCMTK
before you include ndl.h in your
project. Also, you must
have dcmtkheader.h in your include path
in your include path.
The DCMTK library is also 
requried when using this plugin, be sure
to link your project to the following .lib files:
- ofstd.lib 
- dcmdata.lib
- dcmimgle.lib 
- dcmimage.lib 

And in windows:
- netapi32.lib 
- wsock32.lib

Compile in windows with something like this: 
"cl /Ox /EHsc /D_WIN32_WINNT=0x0400 /MD <list of source files> /link <list of libraries> /NODEFAULTLIB:LIBCMT"
*/

# include <dcmtkheader.h>
#define ITOSTR(value) itoa((value),tmpstr,10)

namespace ndl {

template<class VoxelType,int DIM>
class DicomPlugin {
public:
    static std::string imagetypename(){ return "dicom"; }
    static std::string imageopenextension(){ return ""; }
    static std::string imagesaveextension(){ return "dcm"; }
    static bool isfilecompatible(std::string filename,std::string flags,bool sequenceflag=false,bool exactflag=false){
        std::filebuf fb; //disable error messages
        std::ostream newCerr(&fb);
        ofConsole.setCerr(&newCerr);
        DcmFileFormat defaultheader;
        DcmFileFormat* info = &defaultheader;
        OFCondition status = info->loadFile(filename.c_str());
        if ( status.bad() ) return false;
        DcmDataset *dataset = info->getDataset();

        //check if valid dimensions
        long numframes;
        dataset->findAndGetLongInt(DCM_NumberOfFrames, numframes);
        int mindim = 1 + (int)(numframes>1) + (int)(sequenceflag);
        if (mindim>=DIM) return false;

        //check if valid type
        Uint16 pixelrep; //0 is unsigned 1 is signed
        Uint16 bitsallocated; //0 is unsigned 1 is signed
        dataset->findAndGetUint16(DCM_PixelRepresentation, pixelrep);
        dataset->findAndGetUint16(DCM_BitsAllocated, bitsallocated);
        bool difftype = true;
        if (bitsallocated==8){
            if (pixelrep){
                if (getdatatype<VoxelType>()==G_CHAR) difftype=false;
            } else {
                if (getdatatype<VoxelType>()==G_UNSIGNED_CHAR) difftype=false;
            }
        } else
        if (bitsallocated==16){
            if (pixelrep){
                if (getdatatype<VoxelType>()==G_SHORT) difftype=false;
            } else {
                if (getdatatype<VoxelType>()==G_UNSIGNED_SHORT) difftype=false;
            }
        } else
        if (bitsallocated==32){
            if (pixelrep){
                if (getdatatype<VoxelType>()==G_INT) difftype=false;
            } else {
                if (getdatatype<VoxelType>()==G_UNSIGNED_INT) difftype=false;
            }
        }
        if (exactflag && difftype) return false;
        return true;
    }
    
    static bool loadfile(Image<VoxelType,DIM>& im,std::vector<std::string>& sequence,bool exactmatchonly,std::string flags,void* headerpntr){
        int numinsequence = sequence.size();
        //read the file information and
        //check if valid dimensions and type
        DcmFileFormat* info;
        DcmFileFormat defaultheader;
        std::filebuf fb; //disable error messages
        std::ostream newCerr(&fb);
        ofConsole.setCerr(&newCerr);
        if (headerpntr) info = static_cast<DcmFileFormat*>(headerpntr);
        else info = &defaultheader;
        OFCondition status = info->loadFile(sequence[0].c_str());
        if ( status.bad() ){
            std::cout << sequence[0] << "\n" << "Error: cannot read DICOM file (" << status.text() << ")" << std::endl;
            return false;
        }
        DcmDataset *dataset = info->getDataset();
        DicomImage *image = new DicomImage(static_cast<DcmObject*>(info), dataset->getOriginalXfer());
        if (image==0 || image->getStatus() != EIS_Normal){
            std::cout << "Error: cannot check DICOM image (" << DicomImage::getString(image->getStatus()) << ")" << std::endl;
            delete image;
            return false;
        }
        Uint16 ncolors;
        Uint16 pixelrep; //0 is unsigned 1 is signed
        Uint16 bitsallocated; //0 is unsigned 1 is signed
        long numframes;
        dataset->findAndGetUint16(DCM_SamplesPerPixel, ncolors);
        dataset->findAndGetUint16(DCM_PixelRepresentation, pixelrep);
        dataset->findAndGetUint16(DCM_BitsAllocated, bitsallocated);
        dataset->findAndGetLongInt(DCM_NumberOfFrames, numframes);
        OFCondition cond;
        DcmElement *elem = 0;
        double x,y,z; //get voxel size
        cond = dataset->findAndGetFloat64(DCM_SpacingBetweenSlices,z); //get z spacing
        if(!cond.good()) cond = dataset->findAndGetFloat64(DCM_SliceThickness,z);
        if(!cond.good()) z=1;
        cond = dataset->findAndGetElement(DCM_PixelSpacing, elem); //get x and y spacing
        if(cond.good()) {
            elem->getFloat64(x, 0);
            elem->getFloat64(y, 1);
        } else {
            x=1;
            y=1;
        }
        double slope,intercept; // get slope/intercept
        unsigned long numpixelsread;
        OFCondition tstatus;
        tstatus = dataset->findAndGetFloat64(DCM_RescaleSlope,slope);
        if (tstatus.bad()) slope=1;
        tstatus = dataset->findAndGetFloat64(DCM_RescaleIntercept,intercept);
        if (tstatus.bad()) intercept=0;
        //printf("x:%f, y:%f, z:%f\n",x,y,z);
        //printf("ncolors:%d\n",ncolors);
        //printf("image->getWidth():%d\n",image->getWidth());
        //printf("image->getHeight():%d\n",image->getHeight());
        //printf("image->getFrameCount():%d\n",image->getFrameCount());
        bool difftype = true;
        if (bitsallocated==8){
            if (pixelrep){
                if (getdatatype<VoxelType>()==G_CHAR) difftype=false;
            } else {
                if (getdatatype<VoxelType>()==G_UNSIGNED_CHAR) difftype=false;
            }
        } else
        if (bitsallocated==16){
            if (pixelrep){
                if (getdatatype<VoxelType>()==G_SHORT) difftype=false;
            } else {
                if (getdatatype<VoxelType>()==G_UNSIGNED_SHORT) difftype=false;
            }
        } else
        if (bitsallocated==32){
            if (pixelrep){
                if (getdatatype<VoxelType>()==G_INT) difftype=false;
            } else {
                if (getdatatype<VoxelType>()==G_UNSIGNED_INT) difftype=false;
            }
        }
        if (exactmatchonly && difftype) return false;
        int mindim = 1 + (int)(image->getFrameCount()>1) + (int)(numinsequence>1);
        if (mindim>=DIM) return false;
        
        //setup the image
        im.m_dimarray[0]=image->getWidth();
        im.m_dimarray[1]=image->getHeight();
        im.m_voxelsize[0]=x;
        im.m_voxelsize[1]=y;
        im.m_units[0]="mm";
        im.m_units[1]="mm";
        int c=1;
        int imagesizetimescolor = im.m_dimarray[0]*im.m_dimarray[1]*ncolors;
        if (image->getFrameCount()>1){
            c++;
            im.m_dimarray[c]=image->getFrameCount();
            im.m_voxelsize[c]=z;
            im.m_units[c]="mm";
            imagesizetimescolor*=image->getFrameCount();
        }
        if (numinsequence>1){
            c++;
            im.m_dimarray[c]=numinsequence;
            if (image->getFrameCount()==1){
                im.m_voxelsize[c]=z;
                im.m_units[c]="mm";
            } else {
                im.m_voxelsize[c]=1;
                im.m_units[c]="mm";
            }
        }
        delete image;
        im.setupdimensions(im.m_dimarray,ncolors);
        im.setupdata();
        
        //load the image data into the image
        VoxelType* datapntr = new VoxelType[imagesizetimescolor];
        char text[1000];
        for (unsigned int i = 0;i < numinsequence;i++){
            sprintf(text,"loading %s: %s",imagetypename().c_str(),printfilename(sequence[i]).c_str());
            im.updateprogress(text,100*i/numinsequence);
            int offset = i*imagesizetimescolor;
            OFCondition status = info->loadFile(sequence[i].c_str());
            if ( status.bad() ){
                im.updateprogress(text,100);
                std::cout << sequence[0] << "\n" << "Error: cannot read DICOM file (" << status.text() << ")" << std::endl;
                delete [] datapntr;
                return false;
            }
            DcmDataset *dataset = info->getDataset();
            DicomImage *image = new DicomImage(static_cast<DcmObject*>(info), dataset->getOriginalXfer());
            if (image==0 || image->getStatus() != EIS_Normal){
                im.updateprogress(text,100);
                std::cout << "Error: cannot check DICOM image (" << DicomImage::getString(image->getStatus()) << ")" << std::endl;
                delete [] datapntr;
                delete image;
                return false;
            }
            
            if (bitsallocated==8){
                const Uint8* tpixelData;
                dataset->findAndGetUint8Array(DCM_PixelData,tpixelData,&numpixelsread);
                if (numpixelsread>imagesizetimescolor) numpixelsread=imagesizetimescolor;
                for (int t=0;t<numpixelsread;t++) im.m_data[offset+t]=tpixelData[t]*slope + intercept;
            } else
            if (bitsallocated==16){
                const Uint16* tpixelData;
                dataset->findAndGetUint16Array(DCM_PixelData,tpixelData,&numpixelsread,true);
                if (numpixelsread>imagesizetimescolor) numpixelsread=imagesizetimescolor;
                for (int t=0;t<numpixelsread;t++) im.m_data[offset+t]=tpixelData[t]*slope + intercept;
            } else
            if (bitsallocated==32){
                const Uint32* tpixelData;
                dataset->findAndGetUint32Array(DCM_PixelData,tpixelData,&numpixelsread);
                if (numpixelsread>imagesizetimescolor) numpixelsread=imagesizetimescolor;
                for (int t=0;t<numpixelsread;t++) im.m_data[offset+t]=tpixelData[t]*slope + intercept;
            }
            delete image;
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
        
        //setup header
        int difftype = true;
        DcmFileFormat* info;
        DcmFileFormat defaultheader;
        if (headerpntr) info = static_cast<DcmFileFormat*>(headerpntr);
        else info = &defaultheader;
        DcmDataset *dataset = info->getDataset();
        char tmpstr[100];
        char tempstr[500];
        if (im.m_numcolors==3){
            dataset->putAndInsertString(DCM_SamplesPerPixel,ITOSTR(im.m_numcolors));
            dataset->putAndInsertString(DCM_PhotometricInterpretation,"RGB");
        } else {
            dataset->putAndInsertString(DCM_SamplesPerPixel,ITOSTR(im.m_numcolors));
            dataset->putAndInsertString(DCM_PhotometricInterpretation,"MONOCHROME2");
        }
        
        dataset->putAndInsertString(DCM_Columns,ITOSTR(im.m_dimarray[0]));
        dataset->putAndInsertString(DCM_Rows,ITOSTR(im.m_dimarray[1]));
        if (DIM>=3){
            sprintf(tempstr,"%f",im.m_voxelsize[2]);
            dataset->putAndInsertString(DCM_SpacingBetweenSlices,tempstr);
            sprintf(tempstr,"%f",im.m_voxelsize[2]);
            dataset->putAndInsertString(DCM_SliceThickness,tempstr);
        } else {
            //dataset->putAndInsertString(DCM_SpacingBetweenSlices,"1");
            //dataset->putAndInsertString(DCM_SliceThickness,"1");
        }
        
        //iterate over sequence
        int mindim = getlastimagedimensionindex(im);
        int imagesize = im.m_numvoxels/im.m_numcolors;
        if (numinsequence==1){
            if (DIM>=3){
                float slicelocation = im.m_dimarray[2]*im.m_voxelsize[2]/2.0;
                dataset->putAndInsertString(DCM_NumberOfFrames,ITOSTR(im.m_dimarray[2]));
                sprintf(tempstr,"%f\\%f\\%f",((float)im.m_dimarray[0]/2.0)*im.m_voxelsize[0],/*-1.0**/slicelocation,((float)im.m_dimarray[1]/2.0)*im.m_voxelsize[1]);
                dataset->putAndInsertString(DCM_ImagePositionPatient,tempstr);
                sprintf(tempstr,"%f",slicelocation);
                dataset->putAndInsertString(DCM_SliceLocation,tempstr);
            }// else ...
        } else imagesize/=im.m_dimarray[mindim];
        int imagesizetimescolor = imagesize*im.m_numcolors;
        char text[1000];
        for (unsigned int i = 0;i < numinsequence;i++){
            sprintf(text,"saving %s: %s",imagetypename().c_str(),printfilename(sequence[i]).c_str());
            im.updateprogress(text,100*i/numinsequence);
            
            //update header stuff for this image
            //slice specific data
            int offset = i*imagesizetimescolor;
            if (numinsequence>1){
                if (DIM>=3){
                    float slicelocation = (i+0.5)*im.m_voxelsize[2]; //subtract half a pixel so that we measure from the center of the pixel
                    dataset->putAndInsertString(DCM_NumberOfFrames,ITOSTR(1));
                    sprintf(tempstr,"%f\\%f\\%f",((float)im.m_dimarray[0]/2.0)*im.m_voxelsize[0],/*-1.0**/slicelocation,((float)im.m_dimarray[1]/2.0)*im.m_voxelsize[1]);
                    dataset->putAndInsertString(DCM_ImagePositionPatient,tempstr);
                    sprintf(tempstr,"%f",slicelocation);
                    dataset->putAndInsertString(DCM_SliceLocation,tempstr);
                }// else ...
            }
            
            //pixel data
            switch (getdatatype<VoxelType>()){
                case G_CHAR:
                    dataset->putAndInsertString(DCM_BitsAllocated,ITOSTR(8));
                    dataset->putAndInsertString(DCM_BitsStored,ITOSTR(8));
                    dataset->putAndInsertString(DCM_HighBit,ITOSTR(7));
                    dataset->putAndInsertString(DCM_PixelRepresentation,ITOSTR(1)); //0 is unsigned 1 is signed
                    dataset->putAndInsertUint8Array(DCM_PixelData,(Uint8*)&im.m_data[offset],im.m_dimarray[0]*im.m_dimarray[1]*im.m_dimarray[2]);
                    break;
                case G_UNSIGNED_CHAR:
                    dataset->putAndInsertString(DCM_BitsAllocated,ITOSTR(8));
                    dataset->putAndInsertString(DCM_BitsStored,ITOSTR(8));
                    dataset->putAndInsertString(DCM_HighBit,ITOSTR(7));
                    dataset->putAndInsertString(DCM_PixelRepresentation,ITOSTR(0)); //0 is unsigned 1 is signed
                    dataset->putAndInsertUint8Array(DCM_PixelData,(Uint8*)&im.m_data[offset],im.m_dimarray[0]*im.m_dimarray[1]*im.m_dimarray[2]);
                    break;
                case G_SHORT:
                    dataset->putAndInsertString(DCM_BitsAllocated,ITOSTR(16));
                    dataset->putAndInsertString(DCM_BitsStored,ITOSTR(16));
                    dataset->putAndInsertString(DCM_HighBit,ITOSTR(15));
                    dataset->putAndInsertString(DCM_PixelRepresentation,ITOSTR(1)); //0 is unsigned 1 is signed
                    dataset->putAndInsertUint16Array(DCM_PixelData,(Uint16*)&im.m_data[offset],im.m_dimarray[0]*im.m_dimarray[1]*im.m_dimarray[2]);
                    break;
                case G_UNSIGNED_SHORT:
                    dataset->putAndInsertString(DCM_BitsAllocated,ITOSTR(16));
                    dataset->putAndInsertString(DCM_BitsStored,ITOSTR(16));
                    dataset->putAndInsertString(DCM_HighBit,ITOSTR(15));
                    dataset->putAndInsertString(DCM_PixelRepresentation,ITOSTR(0)); //0 is unsigned 1 is signed
                    dataset->putAndInsertUint16Array(DCM_PixelData,(Uint16*)&im.m_data[offset],im.m_dimarray[0]*im.m_dimarray[1]*im.m_dimarray[2]);
                    break;
                case G_INT:{
                    int* tdata = (int*)&im.m_data[offset];
                    Sint16* buffer = new Sint16[imagesizetimescolor];
                    for(int i=0;i<imagesizetimescolor;i++) buffer[i] = tdata[i];
                    dataset->putAndInsertString(DCM_BitsAllocated,ITOSTR(16));
                    dataset->putAndInsertString(DCM_BitsStored,ITOSTR(16));
                    dataset->putAndInsertString(DCM_HighBit,ITOSTR(15));
                    dataset->putAndInsertString(DCM_PixelRepresentation,ITOSTR(1)); //0 is unsigned 1 is signed
                    dataset->putAndInsertUint16Array(DCM_PixelData,(Uint16*)buffer,imagesizetimescolor);
                    delete [] buffer;
                } break;
                case G_LONGLONG:{
                    long long* tdata = (long long*)&im.m_data[offset];
                    Sint16* buffer = new Sint16[imagesizetimescolor];
                    for(int i=0;i<imagesizetimescolor;i++) buffer[i] = tdata[i];
                    dataset->putAndInsertString(DCM_BitsAllocated,ITOSTR(16));
                    dataset->putAndInsertString(DCM_BitsStored,ITOSTR(16));
                    dataset->putAndInsertString(DCM_HighBit,ITOSTR(15));
                    dataset->putAndInsertString(DCM_PixelRepresentation,ITOSTR(1)); //0 is unsigned 1 is signed
                    dataset->putAndInsertUint16Array(DCM_PixelData,(Uint16*)buffer,imagesizetimescolor);
                    delete [] buffer;
                } break;
                case G_UNSIGNED_INT:{
                    unsigned int* tdata = (unsigned int*)&im.m_data[offset];
                    Uint16* buffer = new Uint16[imagesizetimescolor];
                    for(int i=0;i<imagesizetimescolor;i++) buffer[i] = tdata[i];
                    dataset->putAndInsertString(DCM_BitsAllocated,ITOSTR(16));
                    dataset->putAndInsertString(DCM_BitsStored,ITOSTR(16));
                    dataset->putAndInsertString(DCM_HighBit,ITOSTR(15));
                    dataset->putAndInsertString(DCM_PixelRepresentation,ITOSTR(0)); //0 is unsigned 1 is signed
                    dataset->putAndInsertUint16Array(DCM_PixelData,(Uint16*)buffer,imagesizetimescolor);
                    delete [] buffer;
                } break;
                case G_UNSIGNED_LONGLONG:{
                    unsigned long long* tdata = (unsigned long long*)&im.m_data[offset];
                    Uint16* buffer = new Uint16[imagesizetimescolor];
                    for(int i=0;i<imagesizetimescolor;i++) buffer[i] = tdata[i];
                    dataset->putAndInsertString(DCM_BitsAllocated,ITOSTR(16));
                    dataset->putAndInsertString(DCM_BitsStored,ITOSTR(16));
                    dataset->putAndInsertString(DCM_HighBit,ITOSTR(15));
                    dataset->putAndInsertString(DCM_PixelRepresentation,ITOSTR(0)); //0 is unsigned 1 is signed
                    dataset->putAndInsertUint16Array(DCM_PixelData,(Uint16*)buffer,imagesizetimescolor);
                    delete [] buffer;
                } break;
                case G_FLOAT:{
                    float* tdata = (float*)&im.m_data[offset];
                    Sint16* buffer = new Sint16[imagesizetimescolor];
                    for(int i=0;i<imagesizetimescolor;i++) buffer[i] = tdata[i] + 0.5; //for rounding
                    dataset->putAndInsertString(DCM_BitsAllocated,ITOSTR(16));
                    dataset->putAndInsertString(DCM_BitsStored,ITOSTR(16));
                    dataset->putAndInsertString(DCM_HighBit,ITOSTR(15));
                    dataset->putAndInsertString(DCM_PixelRepresentation,ITOSTR(1)); //0 is unsigned 1 is signed
                    dataset->putAndInsertUint16Array(DCM_PixelData,(Uint16*)buffer,imagesizetimescolor);
                    delete [] buffer;
                } break;
                case G_DOUBLE:{
                    double* tdata = (double*)&im.m_data[offset];
                    Sint16* buffer = new Sint16[imagesizetimescolor];
                    for(int i=0;i<imagesizetimescolor;i++) buffer[i] = tdata[i] + 0.5; //for rounding
                    dataset->putAndInsertString(DCM_BitsAllocated,ITOSTR(16));
                    dataset->putAndInsertString(DCM_BitsStored,ITOSTR(16));
                    dataset->putAndInsertString(DCM_HighBit,ITOSTR(15));
                    dataset->putAndInsertString(DCM_PixelRepresentation,ITOSTR(1)); //0 is unsigned 1 is signed
                    dataset->putAndInsertUint16Array(DCM_PixelData,(Uint16*)buffer,imagesizetimescolor);
                    delete [] buffer;
                } break;
            }
            im.updateprogress(text,100);
            
            //save
            OFCondition status = info->saveFile(sequence[i].c_str(),EXS_LittleEndianExplicit);
            if ( status.bad() ){
                std::cout << sequence[i] << "\n" << "Error: cannot write DICOM file (" << status.text() << ")" << std::endl;
                return false;
            }
        }
        printf("\n");
        return true;
    }

    static bool checkfilesequence(std::string filename,std::vector<std::string>& sequence,std::string flags){
        //Get series id for filename
        std::string studyid;
        long seriesnumber = 0;
        long acquisitionnumber = 0;
        long imagennumber = 0;
        std::filebuf fb; //disable error messages
        std::ostream newCerr(&fb);
        ofConsole.setCerr(&newCerr);
        OFCondition tstatus;
        DcmFileFormat fileformat;
        OFCondition status = fileformat.loadFile(filename.c_str());
        if (status.bad()){
            sequence.clear();
            std::cout << "ERROR: Can't Open: " << filename << std::endl;
            return false;
        }
        DcmDataset *dataset = fileformat.getDataset();
        const char* pstudyid;
        tstatus = dataset->findAndGetString(DCM_StudyID, pstudyid);
        if (tstatus.bad()) studyid = ""; else studyid = "";//pstudyid;
        tstatus = dataset->findAndGetLongInt(DCM_SeriesNumber, seriesnumber);
        if (tstatus.bad()) seriesnumber = 0;
        tstatus = dataset->findAndGetLongInt(DCM_AcquisitionNumber, acquisitionnumber);
        if (tstatus.bad()) acquisitionnumber = 0;
        tstatus = dataset->findAndGetLongInt(DCM_InstanceNumber, imagennumber);
        if (tstatus.bad()){
            checkfilesequence_default(filename,sequence,flags);
            return true; //if no image number info, return false to just use filenames
        }
                        
        //save images of same DCM_StudyID,DCM_SeriesNumber, and DCM_AcquisitionNumber
        std::vector< std::pair<std::string,int> > labelsizelist;
        for(int i=sequence.size()-1;i>=0;i--){
            std::string current_studyid;
            long current_seriesnumber = 0;
            long current_acquisitionnumber = 0;
            long current_imagennumber = 0;
            DcmFileFormat fileformat;
            OFCondition status = fileformat.loadFile(sequence[i].c_str());
            bool keepit = true;
            if (!status.bad()){
                DcmDataset *dataset = fileformat.getDataset();
                const char* pstudyid;
                tstatus = dataset->findAndGetString(DCM_StudyID, pstudyid);
                if (tstatus.bad()) current_studyid = ""; else current_studyid = "";//pstudyid;
                tstatus = dataset->findAndGetLongInt(DCM_SeriesNumber, current_seriesnumber);
                if (tstatus.bad()) current_seriesnumber = 0;
                tstatus = dataset->findAndGetLongInt(DCM_AcquisitionNumber, current_acquisitionnumber);
                if (tstatus.bad()) current_acquisitionnumber = 0;
                tstatus = dataset->findAndGetLongInt(DCM_InstanceNumber, current_imagennumber);
                if (tstatus.bad()) current_imagennumber = 0;
                if (current_studyid!=studyid || current_acquisitionnumber!=acquisitionnumber){
                    keepit = false;
                }
            } else keepit = false;
            if (keepit) labelsizelist.push_back( std::pair<std::string,int>(sequence[i],current_imagennumber) );
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