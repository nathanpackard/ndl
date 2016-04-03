#define DEFAULTGRIDSIZE 256
#include <image.h>
#include <time.h>

using namespace ndl;

//globals
//~ Image<unsigned char,2> maskimage; //global variable
Image<unsigned char,3> maskimage; //global variable

//functions
bool volumemask(int index){
    if (maskimage.isempty()) return true;
    //~ return maskimage.m_data[index % maskimage.m_numvoxels];
    return maskimage.m_data[index];
}

void makemask(Image<float,3>& volumeimage){
    maskimage.setupdimensions(volumeimage.m_dimarray,1);
    maskimage.setupdata();
    maskimage.setvalue(0);
    bool dims[3];
    dims[0] = true;
    dims[1] = true;
    dims[2] = false;
    maskimage.DrawEllipse(maskimage.m_orgin,maskimage.m_regionsize,dims,1,true);
}

void makefilter(Image<float,1>& myfilter,int filter,int detwidth){
    if (filter!=0 && filter!=6){
        int npnts = nextpowof2(detwidth)*2; // *2 for padding
        myfilter.setupdimensions(&npnts,1);
        myfilter.setupdata();
        myfilter.MakeRamp(false);
        if (filter==2 || filter==3) myfilter.SincWindow(false);//Shepp Logan / New Shepp Logan
        if (filter==4) 1;//Hamming
        if (filter==5) 1;//Hamming Cutoff
        if (filter==7) myfilter.HanningWindow(false);//Hann (hanning)
    }
}

int getdist(int i,int j,int nelements){
    int themax = (std::max)(i,j);
    int themin = (std::min)(i,j);
    return (std::min)(themax-themin,themin - (themax-nelements));
}

void makesequence(int* output, int nelements){
    int* tarray = new int[nelements];

    int ninetydeg = nelements/4;
    //initialize array
    int count=0;
    output[0]=count++;
    tarray[0]=1;
    for(int i=1;i<nelements;i++) tarray[i]=0;

    while(true){
        //find number with max distance from a neighbor
        int maxdist = 0;
        int maxdistindex = 0;
        for(int i=0;i<nelements;i++){
            if (tarray[i]!=0) continue;
            
            //calculate the distance to the closest neighbor
            int mindist = nelements;
            int mindistindex = 0;
            for(int j=0;j<nelements;j++){
                if (tarray[j]==0 || j==i) continue;
                int dist = getdist(i,j,nelements);
                if (dist<mindist){
                    mindist = dist;
                    mindistindex = j;
                }
            }
            
            if (mindist>maxdist){
                maxdistindex=i;
                maxdist=mindist;
            }
        }
        //if nothing found, we're done
        if (maxdist==0) break;
            
        //otherwise save the projection
        output[count++] = maxdistindex;
        tarray[maxdistindex] = 1;
        
        //now find 90 deg offset image
        int newindex = maxdistindex + ninetydeg;
        if (newindex>=nelements) newindex-=nelements;
        int savedindex = newindex;
        while(tarray[newindex]!=0){
            newindex++;
            if (newindex>=nelements) newindex-=nelements;
            if (newindex==savedindex) break;
        }
        if (tarray[newindex]==0){
            output[count++] = newindex;
            tarray[newindex] = 1;
        }
    }
    for(int i=0;i<nelements;i++) printf("%d) %d, %d\n",i,tarray[i],output[i]);
    delete [] tarray;
}

//create a new volume image
void newvolume(Image<float,3>& volumeimage,bctfolderinfo& bctfolder,char* scanfolder,int& volx,int& voly,int& volz,float& gridsizex_mm,float& gridsizey_mm,float& gridsizez_mm,float& xcenter_mm,float& ycenter_mm,float& ztop_mm){
	//calculate the grid size in mm
    int xsize = bctfolder.detx1minusx0 + 1;
    int ysize = bctfolder.dety1-bctfolder.dety0 + 1;
    float themag = bctfolder.mag[0];
	if (gridsizex_mm==-1) gridsizex_mm = xsize*bctfolder.xsizemm/themag;
	if (gridsizey_mm==-1) gridsizey_mm = xsize*bctfolder.xsizemm/themag;
	if (gridsizez_mm==-1) gridsizez_mm = ysize*bctfolder.ysizemm/themag;
    if (volx==-1) volx = DEFAULTGRIDSIZE;
    if (voly==-1) voly = volx;
    if (volz==-1) volz = 0.99 + volx*gridsizez_mm/gridsizex_mm;
    if (xcenter_mm==-1) xcenter_mm = gridsizex_mm*0.5;
    if (ycenter_mm==-1) ycenter_mm = gridsizey_mm*0.5;
    if (ztop_mm==-1) ztop_mm = -bctfolder.ysizemm*bctfolder.centerrayy[0]/bctfolder.mag[0];
    printf("vol: %d,%d,%d\n",volx,voly,volz);
    printf("gridsize: %f,%f,%f\n",gridsizex_mm,gridsizey_mm,gridsizez_mm);
    
    //setup volume dimensions
    int darray[3];
    darray[0]=volx;
    darray[1]=voly;
    darray[2]=volz;
    volumeimage.setupdimensions(darray,1);
    volumeimage.setupdata();
    makemask(volumeimage);
    
    //set voxel size
    Vector<double,3> voxelsize;
    volumeimage.m_voxelsize[0]=gridsizex_mm/volx;
    volumeimage.m_voxelsize[1]=gridsizey_mm/voly;
    volumeimage.m_voxelsize[2]=gridsizez_mm/volz;
    volumeimage.resetvoxelsize();
    
    //set rotation orgin
    Vector<double,3> rotorgin;
    rotorgin[0] = xcenter_mm;
    rotorgin[1] = ycenter_mm;
    rotorgin[2] = 0;
    volumeimage.setrotateorgin(rotorgin);
     
    //set world position
    Vector<double,3> worldposition;
    worldposition[0]=bctfolder.SIC[0];
    worldposition[1]=0;
    worldposition[2]=ztop_mm;
    volumeimage.setworldposition(worldposition);
    
    //set projection
    volumeimage.setprojection(bctfolder.SID[0],0);
    
    //volumeimage.printinfo("volume image");
}

void loadvolume(Image<float,3>& volumeimage,bctfolderinfo& bctfolder,char* scanfolder,int seriesid){
    if (seriesid==-1) seriesid = bctfolder.lastseriesid;
	char filename[1000];
    sprintf(filename,"%s\\CTi\\CT%04d_%02d.0001",scanfolder,bctfolder.scanid,seriesid);
    volumeimage.loadfile(filename,"",true);

    //set rotation orgin
    Vector<double,3> rotorgin;
    rotorgin[0] = volumeimage.m_rotorgin[0];
    rotorgin[1] = volumeimage.m_rotorgin[1];
    rotorgin[2] = 0;
    volumeimage.setrotateorgin(rotorgin);
     
    //set world position
    Vector<double,3> worldposition;
    worldposition[0]=bctfolder.SIC[0];
    worldposition[1]=0;
    worldposition[2]=-bctfolder.ysizemm*bctfolder.centerrayy[0]/bctfolder.mag[0];
    volumeimage.setworldposition(worldposition);
    
    //set projection
    volumeimage.setprojection(bctfolder.SID[0],0);
    
    //Convert to attenuation coefficients
    //data_ATT = (((data_HU/1000.0) + 1)*0.2195)*m_voxelsize
    volumeimage/=1000;
    volumeimage+=1;
    volumeimage*=0.2195*volumeimage.m_voxelsize[0];
    
    //volumeimage.printinfo("volume image");
}

void savevolume(Image<float,3>& volumeimage,bctfolderinfo& bctfolder,char* scanfolder,int nslices,char* notes,double computationtime,int filter_flag,bool huconvert=true,bool huflag=true){
    char savefilename[1000];
    char tempfilename[1000];
    
    //convert to HU if needed
    if (huconvert){
        float att_water = 0.2109; //HARD CODED FOR NOW, NEED TO CHANGE LATER
        volumeimage-=att_water;
        volumeimage/=(att_water/1000.0);
    }
    
    //update for most current data
    loadbctfolderdata(scanfolder,&bctfolder); 
    
    //Populate Header
    bctheaderinfo t;
    t.nxct = volumeimage.m_dimarray[0];
    t.nyct = volumeimage.m_dimarray[1];
    t.scanid = bctfolder.scanid;
    //t.islices is updated while saving
    t.filter_flag = filter_flag; //***************
    t.series = bctfolder.lastseriesid+1;
    t.nrays = bctfolder.detx;
    t.nviews = (bctfolder.endframe-bctfolder.startframe+1);
    //t.zzz is updated while saving
    t.ctpixel_mm = volumeimage.m_voxelsize[0];
    t.slice_thickness = volumeimage.m_voxelsize[2];
    t.spacebetweenslices = volumeimage.m_voxelsize[2];
    t.delw_mm = bctfolder.xsizemm;
    t.delz_mm = bctfolder.ysizemm;
    t.reconFOV = volumeimage.m_voxelsize[0]*volumeimage.m_dimarray[0];
    t.xim = volumeimage.m_dimarray[2];
    t.min5 = 0;
    t.max5 = 0;
    t.mean = 0;
    t.sigma = 0;
    if (huflag) t.HU_flag = 1; else t.HU_flag = 2;
    t.zzz1 = volumeimage.m_worldposition[2];
    t.zzz2 = (volumeimage.m_dimarray[2]-1)*volumeimage.m_voxelsize[2]+t.zzz1;
    
    //save the bct volume
    sprintf(savefilename,"%s/CTi/CT%04d_%02d.0001",scanfolder,bctfolder.scanid,bctfolder.lastseriesid+1);
    volumeimage.savefile(savefilename,"",true,(void*)&t);
    updateinfandhis(scanfolder,bctfolder.lastseriesid+1,nslices,huflag,notes);
}

void newprojectionimage(Image<float,3>& projectionimage,bctfolderinfo& bctfolder,char* scanfolder,int frame){
    //setup volume dimensions
    int darray[3];
    darray[0]=1;
    darray[1]=bctfolder.detx;
    darray[2]=bctfolder.dety;
    projectionimage.setupdimensions(darray,1);
    projectionimage.setupdata();
    projectionimage.setvalue(0);

    //set voxel size
    Vector<double,3> voxelsize;
    projectionimage.m_voxelsize[0]=1;
    projectionimage.m_voxelsize[1]=bctfolder.xsizemm;
    projectionimage.m_voxelsize[2]=bctfolder.ysizemm;
    projectionimage.resetvoxelsize();
    
    //set rotation orgin
    Vector<double,3> rotorgin;
    rotorgin[0]=0;
    //~ rotorgin[1]=bctfolder.xsizemm*(bctfolder.centerrayx[0]+0.5);
    //~ rotorgin[2]=bctfolder.ysizemm*(bctfolder.centerrayy[0]+0.5);
    rotorgin[1]=bctfolder.xsizemm*(bctfolder.centerrayx[0]);
    rotorgin[2]=bctfolder.ysizemm*(bctfolder.centerrayy[0]);
    projectionimage.setrotateorgin(rotorgin);

    //rotate
    //~ projectionimage.setrotate(-M_PI*bctfolder.tiltangle[0]/180,1,2);
    //~ projectionimage.setrotate(M_PI*bctfolder.tiltangle[0]/180,1,2);

    //set world position
    Vector<double,3> worldposition;
    worldposition[0]=bctfolder.SID[0];
    worldposition[1]=0;
    worldposition[2]=0;
    projectionimage.setworldposition(worldposition);

    //set selection
    int xsize = bctfolder.detx1minusx0 + 1;
    int ysize = bctfolder.dety1-bctfolder.dety0 + 1;
	int x1 = bctfolder.detx/2.0 - xsize/2.0;
	int x2 = bctfolder.detx/2.0 + ysize/2.0;
    int y1 = bctfolder.dety0;
    int y2 = bctfolder.dety1;
    projectionimage.select(0,x1,y1,1,xsize,ysize);
}

void logprojection(Image<float,3>& projectionimage,bctfolderinfo& bctfolder,char* scanfolder,float* weightdata=0){
    //calculate I0
    projectionimage.select(0,bctfolder.i0x1,bctfolder.ioy1,1,1+bctfolder.i0x2 - bctfolder.i0x1,1+bctfolder.ioy2 - bctfolder.ioy1);
    float I0 = projectionimage.getmean();
    projectionimage.select();
    
    //normalize and log the image
    float* data = projectionimage.m_data;
    NDL_FOREACHPL(projectionimage){
        int index;
        float coord[2];
        NDL_GETCOORD(index,coord);
        if (data[index]>0.0 && I0>0.0){
            float value = I0/data[index];
            //value*=value;
            if (weightdata) weightdata[index]=1/value;
            data[index] = log(value);
        } else data[index]=0;
    } NDL_ENDFOREACH
}

void flatfieldprojection(Image<float,3>& projectionimage,bctfolderinfo& bctfolder,char* scanfolder){
    //*************************
    //Load the image
    //*************************
    //setup the projection image
	char filename[1000];
    char fileparameters[500];
    
    //*************************
    //Flat Field the image
    //*************************
    //load offset image
    sprintf(filename,"%s\\RAW\\offset%04d.raw",scanfolder,bctfolder.scanid);
    sprintf(fileparameters,"-filetype:raw -x:1 -y:%d -z:%d -xsize:1 -ysize:%f -zsize:%f -numcolors:1 -datatype:float",bctfolder.detx,bctfolder.dety,bctfolder.xsizemm,bctfolder.ysizemm);
    Image<float,3> Offsetimage(filename,fileparameters);
    
    //~ //load Aimage
    //~ sprintf(filename,"%s\\CAL\\Aimage%04d.raw",scanfolder,bctfolder.calid);
    //~ sprintf(fileparameters,"-filetype:raw -x:1 -y:%d -z:%d -xsize:1 -ysize:%f -zsize:%f -numcolors:1 -datatype:float",bctfolder.detx,bctfolder.dety,bctfolder.xsizemm,bctfolder.ysizemm);
    //~ Image<float,3> Aimage(filename,fileparameters);

    //load Bimage
    sprintf(filename,"%s\\CAL\\Bimage%04d.raw",scanfolder,bctfolder.calid);
    sprintf(fileparameters,"-filetype:raw -x:1 -y:%d -z:%d -xsize:1 -ysize:%f -zsize:%f -numcolors:1 -datatype:float",bctfolder.detx,bctfolder.dety,bctfolder.xsizemm,bctfolder.ysizemm);
    Image<float,3> Bimage(filename,fileparameters);

    //load Rimage
    sprintf(filename,"%s\\CAL\\Rimage%04d.raw",scanfolder,bctfolder.calid);
    sprintf(fileparameters,"-filetype:raw -x:1 -y:%d -z:%d -xsize:1 -ysize:%f -zsize:%f -numcolors:1 -datatype:float",bctfolder.detx,bctfolder.dety,bctfolder.xsizemm,bctfolder.ysizemm);
    Image<float,3> Rimage(filename,fileparameters);

    //calculate flat fielded projection image
    projectionimage -= Offsetimage;
    projectionimage /= Bimage;
    
    //*************************
    //Dead Pixel Correction
    //*************************
    //mark dead pixels from file onto Rimage
    for(int i=0;i<bctfolder.numdeadpixels;i++){
        Rimage[0][bctfolder.deadpixelsx[i]][bctfolder.deadpixelsy[i]] = 0;
    }
    //fill in dead pixels based on Rimage
    projectionimage.fillholes(Rimage,(float)0,(float)0.009);
    
    //do a cone beam weighting on the projection image
    float SID = bctfolder.SID[0];
    float SID2 = SID*SID;
    float* data = projectionimage.m_data;
    NDL_FOREACHPL(projectionimage){
        int index;
        float coord[2];
        NDL_GETCOORD(index,coord);
        float dist1 = coord[0]-bctfolder.centerrayx[0];
        float dist2 = coord[1]-bctfolder.centerrayy[0];
        data[index] /= SID / sqrt( SID2 + dist1*dist1 + dist2*dist2 );        
    } NDL_ENDFOREACH
    
}

//Load a projection image
//Flat Field the image
//then normalize to mu and weight the image, 
//then take the log (uses: Ix=I0*e^-ux), 
//then apply the filter
void loadprojectionimage(Image<float,3>& projectionimage,bctfolderinfo& bctfolder,char* scanfolder,int frame,Image<float,1>* myfilter,float denoise,Image<float,3>* weightimage=0){
    //*************************
    //load and flat field the projection image
    //*************************
	char filename[1000];
    char fileparameters[500];
    sprintf(filename,"%s\\RAW\\scan%04d_%04d.raw",scanfolder,bctfolder.scanid,frame);
    printf("%s\n",filename);
    sprintf(fileparameters,"-filetype:raw -x:1 -y:%d -z:%d -xsize:1 -ysize:%f -zsize:%f -numcolors:1 -datatype:short",bctfolder.detx,bctfolder.dety,bctfolder.xsizemm,bctfolder.ysizemm);
    Image<short,3> rawimage(filename,fileparameters);
    projectionimage = rawimage;
    flatfieldprojection(projectionimage,bctfolder,scanfolder);
    
    //******************************
    //setup projection image geometry
    //******************************
    //set rotation orgin
    Vector<double,3> rotorgin;
    rotorgin[0]=0;
    // rotorgin[1]=bctfolder.xsizemm*(bctfolder.centerrayx[0]+0.5);
    // rotorgin[2]=bctfolder.ysizemm*(bctfolder.centerrayy[0]+0.5);
    rotorgin[1]=bctfolder.xsizemm*(bctfolder.centerrayx[0]);
    rotorgin[2]=bctfolder.ysizemm*(bctfolder.centerrayy[0]);
    projectionimage.setrotateorgin(rotorgin);
    //rotate
    // projectionimage.setrotate(-M_PI*bctfolder.tiltangle[0]/180,1,2);
    // projectionimage.setrotate(M_PI*bctfolder.tiltangle[0]/180,1,2);
    //set world position
    Vector<double,3> worldposition;
    worldposition[0]=bctfolder.SID[0];
    worldposition[1]=0;
    worldposition[2]=0;
    projectionimage.setworldposition(worldposition);
    
    //******************************
    //log the image 
    //and create weight image if need be
    //******************************
    if (weightimage){
        *weightimage = projectionimage;
        logprojection(projectionimage,bctfolder,scanfolder,weightimage->m_data);
    } else logprojection(projectionimage,bctfolder,scanfolder);

    //******************************
    //filter the projection image if need be
    //******************************
    int count=0;
    if (myfilter && !myfilter->isempty()){
        Image<float,3> projreal;
        Image<float,3> projimag;
        projectionimage.fft_padded(projreal,projimag,1);
        NDL_FOREACHPL(projreal){
            int index;
            int coord[3];
            NDL_GETCOORD(index,coord);
            projreal(index)*= (*myfilter)(coord[1]);
            projimag(index)*= (*myfilter)(coord[1]);
        } NDL_ENDFOREACH
        projectionimage.ifft(projreal,projimag,1);
    }
    
    //*************************
    //denoise the projection image if need be
    //*************************
    if (denoise){
        projectionimage.minmax99();
        Image<float,3> originalimage(projectionimage);
        originalimage.addview();
        projectionimage.addview();
        
        if (denoise>1){
            //denoise>1 is taken to be a flag to do adaptive denoising
            denoise-=1;
            projectionimage.TVdenoise(10,1,denoise);
        } else projectionimage.TVdenoise(10,1,denoise);

        //deblurring
        //float stdev[3];
        //stdev[0]=0; stdev[1]=0.3; stdev[2]=0.3;
        //projectionimage.TVdenoise(2,1,1,0,stdev);
        
        //~ projectionimage.minmax99();
        //~ projectionimage.viewloop();
        //~ exit(0);

        projectionimage.removeviews();
    }
    
    //*************************
    //set selection
    //*************************
    int xsize = bctfolder.detx1minusx0 + 1;
    int ysize = bctfolder.dety1-bctfolder.dety0 + 1;
	int x1 = bctfolder.detx/2.0 - xsize/2.0;
	int x2 = bctfolder.detx/2.0 + ysize/2.0;
    int y1 = bctfolder.dety0;
    int y2 = bctfolder.dety1;
    projectionimage.select(0,x1,y1,1,xsize,ysize);
}

void backproject(Image<float,3>& projectionimage,Image<float,3>& volumeimage,bctfolderinfo& bctfolder,int frame,int type=0){
    printf("back projecting %d/%d\n",frame-bctfolder.startframe+1,bctfolder.endframe-bctfolder.startframe+1);

    //update volume geometry
    double angle = bctfolder.theta[frame];
    while (angle<0) angle+=360;
    volumeimage.setrotate(M_PI*angle/180);
    
    //interpolation types: NDL_NN, NDL_LINEAR, NDL_CUBIC
    float clampmin = -1000000;
    float clampmax = 10000000;
    
    //standard summing
    if (type==0) volumeimage._backtransform<NDL_LINEAR,addvalue<float>,volumemask>(projectionimage,clampmin,clampmax);
    
    //multiplying
    if (type==1) volumeimage._backtransform<NDL_LINEAR,multvalue<float>,volumemask>(projectionimage,clampmin,clampmax);

    //min
    if (type==2) volumeimage._backtransform<NDL_LINEAR,minvalue<float>,volumemask>(projectionimage,clampmin,clampmax);

    //max
    if (type==3) volumeimage._backtransform<NDL_LINEAR,maxvalue<float>,volumemask>(projectionimage,clampmin,clampmax);
    
    //geometric average
    if (type==4) volumeimage._backtransform<NDL_LINEAR,normvalue<float>,volumemask>(projectionimage,clampmin,clampmax);
        
    //arethmetic average
    if (type==5) volumeimage._backtransform<NDL_LINEAR,avevalue<float>,volumemask>(projectionimage,clampmin,clampmax);
}

void forwardproject(Image<float,3>& projectionimage,Image<float,3>& volumeimage,bctfolderinfo& bctfolder,int frame,Image<float,3>* raylengthimage=0){
    printf("forward projecting %d/%d\n",frame-bctfolder.startframe+1,bctfolder.endframe-bctfolder.startframe+1);

    //update volume geometry
    double angle = bctfolder.theta[frame];
    //~ double angle = bctfolder.theta[frame];
    while (angle<0) angle+=360;
    volumeimage.setrotate(M_PI*angle/180);
    
    //do the transformation
    int interptype = NDL_LINEAR; //NDL_NN, NDL_LINEAR, NDL_CUBIC
    volumeimage.project<volumemask>(projectionimage,-1000000,10000000,interptype,raylengthimage);
}

void FDKRecon(bctfolderinfo& bctfolder,char* scanfolder,int volx,int voly,int volz,float gridsizex_mm,float gridsizey_mm,float gridsizez_mm,float xcenter_mm, float ycenter_mm, float ztop_mm,int skipfactor,int breakoutafterframe,int filter,char* notes,float denoise){
    //precompute CT reconstruction filter if need be
    Image<float,1> myfilter;
    makefilter(myfilter,filter,bctfolder.detx);
    
    //make an empty volume
    Image<float,3> volumeimage;
    newvolume(volumeimage,bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm);

    //loop through projections and backproject
    Image<float,3> projectionimage;
    int total = (bctfolder.endframe-bctfolder.startframe+1);
    clock_t start = clock();
        
    int* seqarray = new int[total];
    makesequence(seqarray,total);
    if (breakoutafterframe>0) total = (std::min)(total,breakoutafterframe);
    for(int c=skipfactor-1;c<total;c+=skipfactor){
        int frame = seqarray[c] + bctfolder.startframe;
        loadprojectionimage(projectionimage,bctfolder,scanfolder,frame,&myfilter,denoise);
        backproject(projectionimage,volumeimage,bctfolder,frame);
        if (c % 10 == 0 || c<5) volumeimage.minmax99();
        if (c==0) volumeimage.addview();
        volumeimage.refreshviews();
        double ellapsed = double(clock()-start)/double(CLOCKS_PER_SEC);
        printf ("==========================\n%d: Ellapsed time: %d sec, %d sec remaining\n",c, (int)ellapsed, (int)(ellapsed*total/(c+1) - ellapsed) );
    }
    delete [] seqarray;
    
    //save the result
    double ellapsed = double(clock()-start)/double(CLOCKS_PER_SEC);
    savevolume(volumeimage,bctfolder,scanfolder,volz,notes,ellapsed,filter);

    //view the result
    volumeimage.minmax99();
    volumeimage.refreshviews();
    //~ volumeimage.viewloop();
}

void MedianFDKRecon(bctfolderinfo& bctfolder,char* scanfolder,int volx,int voly,int volz,float gridsizex_mm,float gridsizey_mm,float gridsizez_mm,float xcenter_mm, float ycenter_mm, float ztop_mm,int skipfactor,int breakoutafterframe,int filter,char* notes,float denoise){
    //precompute CT reconstruction filter if need be
    Image<float,1> myfilter;
    makefilter(myfilter,filter,bctfolder.detx);
    
    //make an empty volume
    Image<float,3> volumeimage1;
    Image<float,3> volumeimage2;
    Image<float,3> volumeimage3;
    Image<float,3> volumeimage4;
    newvolume(volumeimage1,bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm);
    newvolume(volumeimage2,bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm);
    newvolume(volumeimage3,bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm);
    newvolume(volumeimage4,bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm);
    volumeimage1.addview();
    volumeimage2.addview();
    volumeimage3.addview();
    volumeimage4.addview();

    //loop through projections and backproject
    Image<float,3> projectionimage;
    int total = (bctfolder.endframe-bctfolder.startframe+1);
    clock_t start = clock();
    int frame;
    int* seqarray = new int[total];
    makesequence(seqarray,total);
    if (breakoutafterframe>0) total = (std::min)(total,breakoutafterframe);
    int stotal = total/4;
    for(int c=0;c<total;c++){
        frame = seqarray[c] + bctfolder.startframe;
        if (c<stotal){
            loadprojectionimage(projectionimage,bctfolder,scanfolder,frame,&myfilter,denoise);
            backproject(projectionimage,volumeimage1,bctfolder,frame);
        } else
        if (c<stotal*2){
            loadprojectionimage(projectionimage,bctfolder,scanfolder,frame,&myfilter,denoise);
            backproject(projectionimage,volumeimage2,bctfolder,frame);
        } else
        if (c<stotal*3){
            loadprojectionimage(projectionimage,bctfolder,scanfolder,frame,&myfilter,denoise);
            backproject(projectionimage,volumeimage3,bctfolder,frame);
        } else {
            loadprojectionimage(projectionimage,bctfolder,scanfolder,frame,&myfilter,denoise);
            backproject(projectionimage,volumeimage4,bctfolder,frame);
        }

        if (c % 10 == 0 || c<5){
            volumeimage1.minmax99();
            volumeimage2.m_min = volumeimage1.m_min;
            volumeimage2.m_max = volumeimage1.m_max;
            volumeimage3.m_min = volumeimage1.m_min;
            volumeimage3.m_max = volumeimage1.m_max;
            volumeimage4.m_min = volumeimage1.m_min;
            volumeimage4.m_max = volumeimage1.m_max;
        }
        volumeimage1.refreshviews();
        volumeimage2.refreshviews();
        volumeimage3.refreshviews();
        volumeimage4.refreshviews();
        double ellapsed = double(clock()-start)/double(CLOCKS_PER_SEC);
        printf ("==========================\n%d: Ellapsed time: %d sec, %d sec remaining\n",c, (int)ellapsed, (int)(ellapsed*total/(c+1) - ellapsed) );
    }
    delete [] seqarray;
    
    //do calculations over 4 volumes
    NDL_FOREACHPL(volumeimage1){
        int index;
        NDL_GETINDEX(index);
        float v1 = volumeimage1.m_data[index];
        float v2 = volumeimage2.m_data[index];
        float v3 = volumeimage3.m_data[index];
        float v4 = volumeimage4.m_data[index];
        
        //mean
        volumeimage1.m_data[index] = (v1+v2+v3+v4)/4;

        //geometric mean
        volumeimage2.m_data[index] = sqrt(sqrt((std::max)((float)0,v1*v2*v3*v4)));
        //median
        //volumeimage2.m_data[index] = ((std::min)((std::max)(v1,v2),(std::max)(v3,v4)) + max(min(v1,v2),(std::min)(v3,v4)))/2;

        //max
        volumeimage3.m_data[index] = (std::max)((std::max)(v1,v2),(std::max)(v3,v4));

        //min
        volumeimage4.m_data[index] = (std::min)((std::min)(v1,v2),(std::min)(v3,v4));
    } NDL_ENDFOREACH
    
    //save the result
    double ellapsed = double(clock()-start)/double(CLOCKS_PER_SEC);
    char finalnotes[1000];
    sprintf(finalnotes,"%s MEAN",notes);
    savevolume(volumeimage1,bctfolder,scanfolder,volz,finalnotes,ellapsed,filter);
    sprintf(finalnotes,"%s GEOMETRIC_MEAN",notes);
    savevolume(volumeimage2,bctfolder,scanfolder,volz,finalnotes,ellapsed,filter);
    sprintf(finalnotes,"%s MAX",notes);
    savevolume(volumeimage3,bctfolder,scanfolder,volz,finalnotes,ellapsed,filter);
    sprintf(finalnotes,"%s MIN",notes);
    savevolume(volumeimage4,bctfolder,scanfolder,volz,finalnotes,ellapsed,filter);

    //view the result
    volumeimage1.minmax99();
    volumeimage2.m_min = volumeimage1.m_min;
    volumeimage2.m_max = volumeimage1.m_max;
    volumeimage3.m_min = volumeimage1.m_min;
    volumeimage3.m_max = volumeimage1.m_max;
    volumeimage4.m_min = volumeimage1.m_min;
    volumeimage4.m_max = volumeimage1.m_max;    
    volumeimage1.refreshviews();
    volumeimage2.refreshviews();
    volumeimage3.refreshviews();
    volumeimage4.refreshviews();
    volumeimage1.viewloop();
}

void WeightedFDKRecon(bctfolderinfo& bctfolder,char* scanfolder,int volx,int voly,int volz,float gridsizex_mm,float gridsizey_mm,float gridsizez_mm,float xcenter_mm, float ycenter_mm, float ztop_mm,int skipfactor,int breakoutafterframe,int filter,char* notes,float denoise){
    //precompute CT reconstruction filter if need be
    Image<float,1> myfilter;
    makefilter(myfilter,filter,bctfolder.detx);
    
    //make an empty volume
    Image<float,3> volumeimage;
    Image<float,3> weightvolume;
    newvolume(volumeimage,bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm);
    newvolume(weightvolume,bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm);
    volumeimage.addview();
    weightvolume.addview();
    
    //loop through projections and backproject
    Image<float,3> projectionimage;
    Image<float,3> weightimage;
    int total = (bctfolder.endframe-bctfolder.startframe+1);
    clock_t start = clock();
        
    int* seqarray = new int[total];
    makesequence(seqarray,total);
    if (breakoutafterframe>0) total = (std::min)(total,breakoutafterframe);
    for(int c=0;c<total;c++){
        int frame = seqarray[c] + bctfolder.startframe;
        loadprojectionimage(projectionimage,bctfolder,scanfolder,frame,&myfilter,denoise,&weightimage);
        
        backproject(projectionimage,volumeimage,bctfolder,frame);
        backproject(weightimage,weightvolume,bctfolder,frame);
        
        if (c % 10 == 0 || c<5){
            volumeimage.minmax99();
            weightvolume.minmax99();
        }
        volumeimage.refreshviews();
        weightvolume.refreshviews();
        double ellapsed = double(clock()-start)/double(CLOCKS_PER_SEC);
        printf ("==========================\n%d: Ellapsed time: %d sec, %d sec remaining\n",c, (int)ellapsed, (int)(ellapsed*total/(c+1) - ellapsed) );
    }
    delete [] seqarray;
    volumeimage /= weightvolume;
    
    //save the result
    double ellapsed = double(clock()-start)/double(CLOCKS_PER_SEC);
    savevolume(volumeimage,bctfolder,scanfolder,volz,notes,ellapsed,filter);

    //view the result
    volumeimage.minmax99();
    volumeimage.refreshviews();
    volumeimage.viewloop();
}

void ARTRecon(bctfolderinfo& bctfolder,char* scanfolder,int volx,int voly,int volz,float gridsizex_mm,float gridsizey_mm,float gridsizez_mm,float xcenter_mm, float ycenter_mm, float ztop_mm,int numiterations,int numsubsets,int skipfactor,int breakoutafterframe,char* notes,float denoise,bool tvstep=false){
    Image<float,3> volumeimage;
    Image<float,3> projectionimage;
    Image<float,3> tprojectionimage;
    Image<float,3> projectionlengthimage;

    clock_t start = clock();
    newvolume(volumeimage,bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm);
    volumeimage.setvalue(0);
    volumeimage.addview();
    
    int nframes = bctfolder.endframe - bctfolder.startframe + 1;
    int* seqarray = new int[nframes];
    makesequence(seqarray,nframes);
    for(int iter=0;iter<numiterations;iter++){
        if (breakoutafterframe>0) nframes = (std::min)(nframes,breakoutafterframe);
        for(int i=0;i<nframes;i++){
            int frame = seqarray[i] + bctfolder.startframe;
                        
            newprojectionimage(tprojectionimage,bctfolder,scanfolder,frame);
            forwardproject(tprojectionimage,volumeimage,bctfolder,frame,&projectionlengthimage);
            loadprojectionimage(projectionimage,bctfolder,scanfolder,frame,0,denoise);
            projectionimage-=tprojectionimage;
            backproject(projectionimage,volumeimage,bctfolder,frame);

            //update minmax and norm every 10 views
            if (i<10 || i % 10 == 0){
                volumeimage.minmax99();
                printf("****************************\n");
                printf("Frame: %d, Norm: %f\n",i+1,projectionimage.getnorm());
                printf("****************************\n");
            }
            
            //add constraints every 10 angles
            if (i>0 && i % 10==0 && i<nframes-10){
                volumeimage.Positive();
                if (tvstep) volumeimage.TVdenoise(10,1,0.2);
            }
            
            volumeimage.refreshviews();
        }
    }
    
    //save the result
    double ellapsed = double(clock()-start)/double(CLOCKS_PER_SEC);
    savevolume(volumeimage,bctfolder,scanfolder,volz,notes,ellapsed,0);

    //view the result
    volumeimage.minmax99();
    volumeimage.refreshviews();
    //~ volumeimage.viewloop();

    //cleanup
    delete [] seqarray;
}

void ProbRecon(bctfolderinfo& bctfolder,char* scanfolder,int volx,int voly,int volz,float gridsizex_mm,float gridsizey_mm,float gridsizez_mm,float xcenter_mm, float ycenter_mm, float ztop_mm,int skipfactor,int breakoutafterframe,int filter,char* notes,float denoise){
    //precompute CT reconstruction filter if need be
    Image<float,1> myfilter;
    makefilter(myfilter,filter,bctfolder.detx);
    
    //make an empty volume
    Image<float,3> volumeimage;
    Image<float,3> tvolumeimage;
    newvolume(volumeimage,bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm);
    newvolume(tvolumeimage,bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm);
    volumeimage.addview();
    tvolumeimage.addview();

    //loop through projections and backproject
    Image<float,3> projectionimage;
    int total = (bctfolder.endframe-bctfolder.startframe+1);
    clock_t start = clock();
        
    int* seqarray = new int[total];
    makesequence(seqarray,total);
    if (breakoutafterframe>0) total = (std::min)(total,breakoutafterframe);
    int factor=0;
    for(int c=skipfactor-1;c<total;c+=skipfactor){
        int frame = seqarray[c] + bctfolder.startframe;
        loadprojectionimage(projectionimage,bctfolder,scanfolder,frame,&myfilter,denoise);
        backproject(projectionimage,tvolumeimage,bctfolder,frame);
        
        if (c % 32 == 0 && c>0){
            tvolumeimage.ClampMin(0);
            tvolumeimage.TVdenoise(10,1,0.1);
            if (c==32) volumeimage = tvolumeimage;
            else volumeimage += tvolumeimage;
            volumeimage.minmax99();
            tvolumeimage.setvalue(0);
            factor++;
        }
        if (c % 4 == 0) tvolumeimage.minmax99();
        tvolumeimage.refreshviews();
        volumeimage.refreshviews();
        double ellapsed = double(clock()-start)/double(CLOCKS_PER_SEC);
        printf ("==========================\n%d: Ellapsed time: %d sec, %d sec remaining\n",c, (int)ellapsed, (int)(ellapsed*total/(c+1) - ellapsed) );
    }
    delete [] seqarray;
    
    //take root
    //volumeimage.Pow(1.0/(float)factor);
    
    //save the result
    double ellapsed = double(clock()-start)/double(CLOCKS_PER_SEC);
    savevolume(volumeimage,bctfolder,scanfolder,volz,notes,ellapsed,filter);

    //view the result
    volumeimage.minmax99();
    volumeimage.refreshviews();
    volumeimage.viewloop();
}

void OSEMRecon(bctfolderinfo& bctfolder,char* scanfolder,int volx,int voly,int volz,float gridsizex_mm,float gridsizey_mm,float gridsizez_mm,float xcenter_mm, float ycenter_mm, float ztop_mm,int numiterations,int numsubsets,int skipfactor,int breakoutafterframe,char* notes,float denoise){
    Image<float,3> volumeimage;
    Image<float,3> projectionimage;
    Image<float,3> tprojectionimage;
    Image<float,3> projectionlengthimage;

    clock_t start = clock();
    newvolume(volumeimage,bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm);
    volumeimage.setvalue(0);
    volumeimage+=maskimage;
    volumeimage.minmax();
    volumeimage.addview();
    
    int nframes = bctfolder.endframe - bctfolder.startframe + 1;
    int* seqarray = new int[nframes];
    makesequence(seqarray,nframes);
    int subsetsize = nframes/numsubsets;
    int iter=0;
    if (breakoutafterframe>0) nframes = (std::min)(nframes,breakoutafterframe);
    for(int i=0;i<nframes;i++){
        int frame = seqarray[i] + bctfolder.startframe;
        
        loadprojectionimage(projectionimage,bctfolder,scanfolder,frame,0,denoise);
        projectionimage.ClampMin(0);
        newprojectionimage(tprojectionimage,bctfolder,scanfolder,frame);
        forwardproject(tprojectionimage,volumeimage,bctfolder,frame,&projectionlengthimage);
        projectionimage/=tprojectionimage;
        projectionimage.ClampMin(0);
        backproject(projectionimage,volumeimage,bctfolder,frame,1);

        //update minmax and norm every 10 views
        if (i % 10 == 0 || i<5){
            volumeimage.minmax99();
            printf("****************************\n");
            printf("Frame: %d, Norm: %f\n",i+1,projectionimage.getnorm());
            printf("****************************\n");
        }
        volumeimage.refreshviews();
        
        //handle subsets
        if ((i>0) && (i%subsetsize==0)){
            iter++;
            if (iter<numiterations) i-=subsetsize;
            else iter=0;
        }
    }
    
    //save the result
    double ellapsed = double(clock()-start)/double(CLOCKS_PER_SEC);
    savevolume(volumeimage,bctfolder,scanfolder,volz,notes,ellapsed,0);

    //view the result
    volumeimage.minmax99();
    volumeimage.refreshviews();
    //~ volumeimage.viewloop();

    //cleanup
    delete [] seqarray;
}

void forwardprojectsim(bctfolderinfo& bctfolder,char* scanfolder,int seriesid){
    //load a volume image
    Image<float,3> volumeimage;
    loadvolume(volumeimage,bctfolder,scanfolder,seriesid);
    
    //loop through projections and forward project
    Image<float,3> projectionimage;
    int total = (bctfolder.endframe-bctfolder.startframe+1);
    clock_t start = clock();
    for(int c=0;c<total;c++){
        int frame=bctfolder.startframe + c;
        newprojectionimage(projectionimage,bctfolder,scanfolder,frame);
        forwardproject(projectionimage,volumeimage,bctfolder,frame,0);
        
        if (c % 10 == 0) projectionimage.minmax99();
        if (c==0) projectionimage.addview();
        projectionimage.refreshviews();
        
        //save the result
        //~ projectionimage...
        
        double ellapsed = double(clock()-start)/double(CLOCKS_PER_SEC);
        printf ("==========================\n%d: Ellapsed time: %d sec, %d sec remaining\n",c, (int)ellapsed, (int)(ellapsed*total/(c+1) - ellapsed) );
    }
    
    //view the last result
    projectionimage.minmax99();
    projectionimage.refreshviews();
    //~ projectionimage.viewloop();
}

void denoise3D(bctfolderinfo& bctfolder,char* scanfolder,char* notes,int seriesid,float denoise){
    //load a volume image
    Image<float,3> volumeimage;
    loadvolume(volumeimage,bctfolder,scanfolder,seriesid);
    
    //Do Denoising
    volumeimage.minmax99();
    volumeimage.addview();
    volumeimage.TVdenoise(10,1,denoise);
    double ellapsed=0;
    //char* notes = "Denoised_Volume_lambda_0.1";
    
    //save and view
    savevolume(volumeimage,bctfolder,scanfolder,volumeimage.m_dimarray[2],notes,ellapsed,0,false);
    volumeimage.minmax99();
    volumeimage.viewloop();
}


void projectiontester(bctfolderinfo& bctfolder,char* scanfolder,int volx,int voly,int volz,float gridsizex_mm,float gridsizey_mm,float gridsizez_mm,float xcenter_mm, float ycenter_mm, float ztop_mm,int skipfactor,int seriesid,float denoise){
    //TESTING
    //bctfolder.dety0
    //bctfolder.detx1minusx0
    //~ bctfolder.dety1=100;
    
    //load a volume image
    Image<float,3> volumeimage;
    
    //~ loadvolume(volumeimage,bctfolder,scanfolder,seriesid);
    newvolume(volumeimage,bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm);
    //loop through projections and forward project
    
    Image<float,1> myfilter;
    Image<float,3> original_projectionimage;
    Image<float,3> new_projectionimage;
    Image<float,3> diff_projectionimage;
    Image<float,3> projectionlengthimage;

    int total = (bctfolder.endframe-bctfolder.startframe+1);
    clock_t start = clock();
    for(int c=0;c<total;c+=skipfactor){
        int frame=bctfolder.startframe + c;
                
        loadprojectionimage(original_projectionimage,bctfolder,scanfolder,frame,&myfilter,denoise);
        //~ original_projectionimage.setvalue(0);
        original_projectionimage[0][600][50]=10;
        original_projectionimage[0][600][48]=10;
        original_projectionimage[0][600][52]=10;
        original_projectionimage[0][602][50]=10;
        original_projectionimage[0][598][50]=10;
        
        //~ original_projectionimage[0][500][50]=10;
        //~ original_projectionimage[0][500][48]=10;
        //~ original_projectionimage[0][500][52]=10;
        //~ original_projectionimage[0][502][50]=10;
        //~ original_projectionimage[0][498][50]=10;
        
        //~ original_projectionimage[0][550][80]=10;
        //~ original_projectionimage[0][550][78]=10;
        //~ original_projectionimage[0][550][82]=10;
        //~ original_projectionimage[0][552][80]=10;
        //~ original_projectionimage[0][448][80]=10;
        
        backproject(original_projectionimage,volumeimage,bctfolder,frame);
        newprojectionimage(new_projectionimage,bctfolder,scanfolder,frame);
        forwardproject(new_projectionimage,volumeimage,bctfolder,frame,&projectionlengthimage);
        
        
        diff_projectionimage = original_projectionimage;

        diff_projectionimage*=new_projectionimage;
        diff_projectionimage.Sqrt();
        
        //~ diff_projectionimage-=new_projectionimage;
        
        
        //~ original_projectionimage.savefile("origimage.raw");
        //~ new_projectionimage.savefile("projimage.raw");
        //~ diff_projectionimage.savefile("diffimage.raw");
        
        projectionlengthimage.minmax();
        original_projectionimage.minmax99();
        new_projectionimage.minmax99();
        diff_projectionimage.minmax99();
        volumeimage.minmax99();
        
        printf("========================================\n");
        printf("original_projectionimage: %f\n",original_projectionimage.getnorm());
        printf("new_projectionimage: %f\n",new_projectionimage.getnorm());
        printf("diff_projectionimage: %f\n",diff_projectionimage.getnorm());
        printf("========================================\n");
        
        //~ if (c==0){
            new_projectionimage.addview();
            original_projectionimage.addview();
            diff_projectionimage.addview();
            projectionlengthimage.addview();
        //~ } else {
            //~ new_projectionimage.refreshviews();
            //~ original_projectionimage.refreshviews();
            //~ diff_projectionimage.refreshviews();
            //~ volumeimage.refreshviews();
        //~ }
        
        volumeimage.setvalue(0);
    }
    
    //view the last result
    new_projectionimage.minmax99();
    new_projectionimage.refreshviews();
    //~ new_projectionimage.viewloop();
}

int main(int argc,char* argv[]){
   	if (argc<=1){
        printf("\n************************\n");
		printf("Recon Usage: %s [-algorithm:<algorithm> -numiterations:<numiterations> -numsubsets:<numsubsets> -skipfactor:<skipfactor> -breakoutafterframe:<breakoutafterframe> -reconwidth:<reconwidth> -reconheight:<reconheight> -recondepth:<recondepth> -seriesid:<seriesid> -notes:<notes>] \"<scanfolder>\"\n\n",argv[0]);
        printf("************************\n");
        printf("--------------------------\n");
        printf("algorithm = [0.x,1.x,2,3,4,5,6] (where x=Recon Filter Type)\n");
        printf("--------------------------\n");
        printf("< 0.x > -- 3-Median Filtered Backprojection (experimental)\n");
        printf("< 1.x > -- Filtered Backprojection\n");
        printf("  < 2 > -- Expectation Maximization (EM) \n");
        printf("  < 3 > -- Algebraic Reconstruction Technique (ART)\n");
        printf("  < 4 > -- Total Variation Reconstruction (TV)\n");
        printf("  < 5 > -- Probabilistic (experimental)\n");
        printf("  < 6 > -- Forward Projection Simulation\n");
        printf("  < 7 > -- Forward/Back Projection Testing\n");
        printf("--------------------------\n");
		printf("Recon Filter Type = [0,1,2,3,4,5,6,7]\n");
		printf("--------------------------\n");
		printf(" < 0 > -- No Filter\n");
		printf(" < 1 > -- RAMP\n");
		printf(" < 2 > -- Shepp Logan\n");
		printf(" < 3 > -- New Shepp Logan\n");
		printf(" < 4 > -- Hamming\n");
		printf(" < 5 > -- Hamming Cutoff\n");
		printf(" < 6 > -- No Filter (same as 0)\n");
		printf(" < 7 > -- Hann (aka hanning)\n\n");
		printf("numiterations = The number of iterations to use during reconstruction\n");
		printf("numsubsets = The number of subsets to use during reconstruction\n");
		printf("skipfactor = skipfactor=1 means all images, skipfactor=2 means every other image, etc.\n");
		printf("breakoutafterframe = stop early after this many frames\n");
		printf("seriesid = The seriesid to use for new reconstructions\n");
		printf("notes = Any notes to be put in the .his file (NO SPACES ALLOWED)\n");
		printf("reconwidth \\\nreconheight = The voxel dimensions of the reconstruction\nrecondepth /\n");
		printf("voxelwidth \\\nvoxelheight = The dimensions of a voxel in mm\nvoxeldepth /\n");
		printf("denoise = specify the amount of TV denoising to apply to projection data\n");
        exit(0);
    }
        
    //setup recon defaults
    char* scanfolder = argv[argc-1];
    int algorithm=1;
    int filter=3;
    int volx=-1;
    int voly=-1;
    int volz=-1;
    float gridsizex_mm=-1;
    float gridsizey_mm=-1;
    float gridsizez_mm=-1;
    float xcenter_mm=-1;
    float ycenter_mm=-1;
    float ztop_mm=-1;
    int numiterations=1;
    int numsubsets=1;
    int skipfactor=1;
    int seriesid=-1;
    int breakoutafterframe=0;
    char notes[1000];
    float denoise=0;
    strcpy(notes,"");
    
    //load optional parameters
    std::map<std::string,std::string> flaglist;
    std::string optionalparams = getparamstring(argv,1,argc-2);
    getoptionalparams(optionalparams,flaglist);
    
    //handle parameters
    if (flaglist.find("algorithm") != flaglist.end()){
        float talgorithm = atof(flaglist["algorithm"].c_str());
        algorithm = (int)talgorithm;
        filter = (int)(10.0*(talgorithm - (float)algorithm)+0.5); //add 0.5 to handle rounding properly
    }
    if (flaglist.find("reconwidth") != flaglist.end()) volx=atoi(flaglist["reconwidth"].c_str());
    if (flaglist.find("reconheight") != flaglist.end()) voly=atoi(flaglist["reconheight"].c_str());
    if (flaglist.find("recondepth") != flaglist.end()) volz=atoi(flaglist["recondepth"].c_str());
    if (flaglist.find("voxelwidth") != flaglist.end()) gridsizex_mm=atof(flaglist["voxelwidth"].c_str());
    if (flaglist.find("voxelheight") != flaglist.end()) gridsizey_mm=atof(flaglist["voxelheight"].c_str());
    if (flaglist.find("voxeldepth") != flaglist.end()) gridsizez_mm=atof(flaglist["voxeldepth"].c_str());
    if (flaglist.find("numiterations") != flaglist.end()) numiterations=atoi(flaglist["numiterations"].c_str());
    if (flaglist.find("numsubsets") != flaglist.end()) numsubsets=atoi(flaglist["numsubsets"].c_str());
    if (flaglist.find("skipfactor") != flaglist.end()) skipfactor=atoi(flaglist["skipfactor"].c_str());
    if (flaglist.find("breakoutafterframe") != flaglist.end()) breakoutafterframe=atoi(flaglist["breakoutafterframe"].c_str());
    if (flaglist.find("seriesid") != flaglist.end()) seriesid=atoi(flaglist["seriesid"].c_str());
    if (flaglist.find("notes") != flaglist.end()) strcpy(notes,flaglist["notes"].c_str());
    if (flaglist.find("denoise") != flaglist.end()) denoise=atof(flaglist["denoise"].c_str());
    
    //read data from folder
    bctfolderinfo bctfolder;
    loadbctfolderdata(scanfolder,&bctfolder);
    
    // run the reconstruction
    switch(algorithm){
        case 0: {
            //startgui(_3MedianFDK_Recon);
            break;
        }
        case 1: {
            printf("starting FDK (filter: %d)\n",filter);
            FDKRecon(bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm,skipfactor,breakoutafterframe,filter,notes,denoise);
            break;
        }
        case 2: {
            printf("starting OSEM\n");
            OSEMRecon(bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm,numiterations,numsubsets,skipfactor,breakoutafterframe,notes,denoise);
            break;
        }
        case 3: {
            printf("starting ART\n");
            ARTRecon(bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm,numiterations,numsubsets,skipfactor,breakoutafterframe,notes,denoise);
            break;
        }
        case 4: {
            printf("starting TV\n");
            ARTRecon(bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm,numiterations,numsubsets,skipfactor,breakoutafterframe,notes,denoise,true);
            break;
        }
        case 5: {
            printf("starting Probabilistic\n");
            ProbRecon(bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm,skipfactor,breakoutafterframe,filter,notes,denoise);
            break;
        }
        case 6: {
            printf("starting forwardproject simulation\n");
            forwardprojectsim(bctfolder,scanfolder,seriesid);
            break;
        }
        case 7: {
            printf("starting projection tester\n");
            projectiontester(bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm,skipfactor,seriesid,denoise);
            break;
        }
        case 8: {
            printf("starting 3d denoise\n");
            denoise3D(bctfolder,scanfolder,notes,seriesid,denoise);
            break;
        }
        case 9: {
            printf("starting weighted FDK (filter: %d)\n",filter);
            WeightedFDKRecon(bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm,skipfactor,breakoutafterframe,filter,notes,denoise);
            break;
        }
        case 10: {
            printf("starting median FDK (filter: %d)\n",filter);
            MedianFDKRecon(bctfolder,scanfolder,volx,voly,volz,gridsizex_mm,gridsizey_mm,gridsizez_mm,xcenter_mm,ycenter_mm,ztop_mm,skipfactor,breakoutafterframe,filter,notes,denoise);
            break;
        }
        default: {
            printf("unknown algorithm: %d\n",algorithm);
        }
    }
        
    //done
    printf("\nGoodbye\n");
    return 1;
}

