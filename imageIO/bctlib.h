#ifndef BCTLIB
#define BCTLIB
#define VERBOSE 0

#define BCTHEADERSIZE 80
#include <time.h>
#include <sys/stat.h>
#include <float.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#define isnan(x) ((x) != (x))

#ifndef BCTLIB_CLAMP
#define BCTLIB_CLAMP(x, l, h) (((x) > (h)) ? (h) : (((x) < (l)) ? (l) : (x))) 
#endif

/*!
//Holds breast CT header info plus a bit more
*/
class bctheaderinfo {
    public:
    bctheaderinfo(){        
        //set defaults
        nxct=nyct=scanid=islices=filter_flag=series=nrays=nviews=zzz=ctpixel_mm=slice_thickness=delw_mm=delz_mm=reconFOV=xim=min5=max5=mean=sigma=HU_flag=spacebetweenslices=zzz1=zzz2=0;
        RLvector[0]=RLvector[1]=RLvector[2]=0;
        APvector[0]=APvector[1]=APvector[2]=0;
        ISvectordirection=1;
        day=month=year=hour=min=sec=0;
        hassegmentation=hasregistration=0;
    }
	float nxct,nyct,scanid,islices,filter_flag,series,nrays,nviews,zzz,ctpixel_mm,slice_thickness,delw_mm,delz_mm,reconFOV,xim,min5,max5,mean,sigma,HU_flag;
	float spacebetweenslices,zzz1,zzz2;
    double RLvector[3]; //left-right vector
    double APvector[3]; //antirior-postieror vector
    int ISvectordirection; //-1 or 1
    int day,month,year,hour,min,sec; //info from time stamp
    int hassegmentation, hasregistration; //check for other available files
    int segmentationvalid; //0 or 1
    int lastchestwallslice; //-1 means not visible
    int firstgoodslice; //-1 means not visible
    int lastgoodslice; //-1 means not visible
};

class bctfolderinfo {
    public:
    bctfolderinfo(){
        //zero out any previous data
        memset(this,0,sizeof(*this));
    }
    int scanid;
    char scannertag[100];
    
    //parametr file info
    float kvp;
    float ma;
    int nimages;
    float scan_time;
    int openshutterhour;
    int openshuttermin;
    int openshuttersec;
    int closeshutterhour;
    int closeshuttermin;
    int closeshuttersec;
    float diameter;
    int scanmonth;
    int scanday;
    int scanyear;
    int detmode;
    char detgain[10];
    char cupsize[10];
    int calid;
    float toppos;
    float bottompos;
    int bbdatascanid1;
    int bbdatascanid2;
    float offsetangle;
    bool irregulartrajectory; //calculated after loading above data
    float ffkvp;
    char ffmarange[500];
    int nffimages;
    float fffps;
    char ffdetmodestring[500];
    char ffscannername[500];
    char fffiltration[500];
    int ffscanmonth,ffscanday,ffscanyear;
    float ffdelay;
    char ffvtrajectory[500];
    int detx,dety;
    float xsizemm,ysizemm;
    int deadleft,deadright,deadtop,deadbottom;
    int numbits;
    float Rsquaredlinearitytolerance;
    int interlacepattern,Interlaceswaprow,isIrregularTrajectory,DynamicFlag;

    //ctscan file info
    //int scanid;
    char objectdescription[500];
    //int scanmonth;
    //int scanday;
    //int scanyear;
    //float kvp;
    //float ma;
    float degrees;
    //int nimages;
    float fps;
    char detmodestring[500];
    char scannername[500];
    char filtration[50];
    char vtrajectory[50];
    //int calid;
    char scanneroperator[500];
    int detbinningx; //calculated after loading above data
    int detbinningy; //calculated after loading above data

    //cal file info (has two calibration sets for irregular trajectories)
    float centerrayx[2];
    float centerrayy[2];
    float tiltangle[2];
    float SID[2];
    float SIC[2];
    float mag[2]; //calculated after loading above data
    
    //frame file info
    int startframe;
    int endframe;
    
    //patient file info
    int patient_id;
    char lastname[100];
    char firstname[100];
    int DOB_month;
    int DOB_day;
    int DOB_year;
    char MRN[100];
    //int scanid;
    char bodypart[100];
    char metric[100];
    char quality[100];
    char scandescription[500];
    char digitalfilm[500];
    char labresult[500];
    char gender;
    bool contrast;
    int contrastdelay;
    int ispatient;
    char race[500];
    char tabletop[500];
    char foreignobject[500];
    int issuspicious;
    float height_in;
    float weight_lbs;
    
    //recon plan file info
    int vid;
    int dety0;
    int dety1;
    int detx1minusx0;
    int numslices;
    float slicethickness_mm;
    int i0x1;
    int ioy1;
    int i0x2;
    int ioy2;
    
    //cylinder file info
    float ro;
    float xo;
    float yo;
    float ho; //*****
    
    //roi file info
    float roi_ro;
    float roi_xo;
    float roi_yo;
    
    //dicom file info
    char uidSite[500];
    char uidScanner[500];
    char institutionname[500];
    
    //angle file info (max number of projection images = 5000 for now)
    float theta[5000];
    float vpos[5000];
    
    //dead pixel info (max number of dead pixels = 10000 for now)
    int deadpixelsx[5000];
    int deadpixelsy[5000];
    int numdeadpixels;
    
    //.inf file info
    int lastseriesid;
    int lastseriesnumslices;
    
    //.his file info and header info for each series
    int seriesid[100];
    int seriesnumslices[100];
    //huflag
    //recondate 
    //time
    //recon computer name
    bctheaderinfo seriesheader[100];
    int numseries;
};

int getbctfolderfromctfile(char* filename,char* folder){
    strcpy(folder,filename);
    int c=0;
    for(int i=strlen(folder);i>=0;i--){
        if (folder[i]=='\\' || folder[i]=='/') c++;
        if (c==2){
            folder[i]=NULL;
            return 1;
        }
    }
    return 0;
}

char* trim(char *str){
    int last = strlen(str)-1;
    for(int i=last;i>=0;i--){
        if (str[i] == ' ' || str[i] == '\t' || str[i] == '\r' || str[i] == '\n') str[i]=NULL;
        else break;
    }
    last = strlen(str)-1;    
    int count=0;
    for(int i=0;i<=last;i++){
        if (str[i] == ' ' || str[i] == '\t' || str[i] == '\r' || str[i] == '\n') count++;
        else break;
    }
    return &str[count];
} 

char* trimbeforecolon(char *str){
    int last = strlen(str)-1;    
    int i;
    for(i=0;i<=last;i++){
        if (str[i] == ':') break;
    }
    for(++i;i<=last;i++){
        if (str[i] != ' ') break;
    }
    if (i<=last) return trim(&str[i]);
    return NULL;
} 

bool extractfoldername(const char* filename,char* foldername){
    //extract the foldername
    int index = -1;
    int last = strlen(filename)-1;
    for(int i=last;i>=0;i--){
        if (filename[i]=='/' || filename[i]=='\\'){
            index = i;
            break;
        }
    }
    if (index!=-1 && index<last){
        strncpy(foldername,filename,index);
        foldername[index]=NULL;
    } else return 0;
    return 1;
}

bool extracttoplevelfoldername(const char* folder,char* foldername){
    //extract the foldername
    int index = -1;
    int last = strlen(folder)-1;
    for(int i=last;i>=0;i--){
        if (folder[i]=='/' || folder[i]=='\\'){
            index = i;
            break;
        }
    }
    if (index!=-1 && index<last){
        strncpy(foldername,&folder[index+1],last - index);
        foldername[last-index]=NULL;
    } else return 0;
    return 1;
}


int isbctFolder(char* folder,bctfolderinfo* f){
    char foldername[1000];
    if (!extracttoplevelfoldername(folder,foldername)) return 0;
    int folderstrlen = strlen(foldername);
    if (strncmp(foldername,"CT",2)!=0) return 0; //must begin with 'CT'
    if (folderstrlen<6) return 0; //must be at least 6 characters long
    
    //get the scanid (must be at least 4 digits at end of folder name
    int numdigitsonright=0;
    for(int c = folderstrlen-1;c>=0;c--){
        if (foldername[c]!='0'&& foldername[c]!='1'&& foldername[c]!='2'&& foldername[c]!='3'&& foldername[c]!='4'&& foldername[c]!='5'&& foldername[c]!='6'&& foldername[c]!='7'&& foldername[c]!='8'&& foldername[c]!='9') break;
        numdigitsonright++;
    }
    if (numdigitsonright<4) return 0;
    f->scanid = atoi(&foldername[folderstrlen-numdigitsonright]);
    
    //get the scanner tag (everything between CT and the scanid
    int taglength = folderstrlen-numdigitsonright-2;
    strncpy(f->scannertag,&foldername[2],taglength);
    f->scannertag[taglength]=NULL;
    
    if (strcmp(f->scannertag,"C")==0 || strcmp(f->scannertag,"M")==0) return 0; //we don't support mouse and computer yet...

    return 1;
}

bool isbctfile(const char* filename){
    char extension[1000];
    
    //extract the extension
    int index = -1;
    int last = strlen(filename)-1;
    for(int i=last;i>=0;i--){
        if (filename[i]=='.'){
            index = i;
            break;
        }
    }
    if (index!=-1 && index<last){
        strncpy(extension,&filename[index+1],last - index);
        extension[last-index]=NULL;
    } else return 0;
    
    //~ printf("Extension: %s\n",extension);
    //~ printf("atoi_Extension: %d\n",atoi(extension));
    //~ printf("strlen_Extension: %d\n",strlen(extension));
    
	return (atoi(extension)>0 && strlen(extension)>=4);
}

int readParmtrXXX(char* filename,bctfolderinfo* f){
    char buffer[500];
    FILE* fp = fopen(filename,"r");
    if (fp==NULL){
        if (VERBOSE) printf("ERROR: Couldn't open file '%s' (%s) \n",filename,strerror( errno ));
        f->scanmonth=1;
        f->scanday=1;
        f->scanyear=1900;

        return 0;
    } else {
        int linenum=1;
        while (fgets(buffer,500,fp)){
            if (linenum==1){
                for(unsigned int i=0;i<strlen(buffer);i++) if (buffer[i]==','){ buffer[i]=' '; break;}
                sscanf(buffer,"%s",f->lastname);
                if (strlen(f->lastname)<strlen(buffer)) strcpy(f->firstname,trim(&buffer[strlen(f->lastname)+1]));
            }
            if (linenum==2) f->kvp = (float)atof(buffer);
            if (linenum==3) f->ma = (float)atof(buffer);
            if (linenum==4) f->nimages = atoi(buffer);
            if (linenum==5) f->diameter = (float)atof(buffer);
            if (linenum==6) sscanf(buffer,"%d:%d:%d",&f->openshutterhour,&f->openshuttermin,&f->openshuttersec);
            if (linenum==7) sscanf(buffer,"%d:%d:%d",&f->closeshutterhour,&f->closeshuttermin,&f->closeshuttersec); 
            if (linenum==8) f->scan_time = (float)atof(buffer);
            if (linenum==9){
                sscanf(buffer,"%d/%d/%d",&f->scanmonth,&f->scanday,&f->scanyear); 
                if (f->scanyear<100) f->scanyear+=2000;
            }
            if (linenum==10) f->detmode = atoi(buffer); 
            if (linenum==11) sscanf(buffer,"%s",f->detgain); 
            if (linenum==12) sscanf(buffer,"%s",f->cupsize); 
            if (linenum==13) f->calid = atoi(buffer); 
            if (linenum==14) f->toppos = (float)atof(buffer);
            if (linenum==15) f->bottompos = (float)atof(buffer);
            if (linenum==16) f->bbdatascanid1 = atoi(buffer);
            if (linenum==17) f->bbdatascanid2 = atoi(buffer);
            if (linenum==18) f->offsetangle = (float)atof(buffer);
            linenum++;
        }
        if (f->detmode>=100) f->irregulartrajectory=1; else f->irregulartrajectory=0;
    }
    fclose(fp);
    return 1;
}

int readCalFile(char* filename,int index,bctfolderinfo* f){
    char buffer[500];
	FILE* fp = fopen(filename,"r");
	if(fp==NULL){
		if (VERBOSE) printf("ERROR: cal file not found, '%s'\n",filename);
		return 0;
	} else {
		int linenum=1;
		while (fgets(buffer,500,fp)){
			if (linenum==1) sscanf(buffer,"%f",&f->centerrayx[index]);
			if (linenum==2) sscanf(buffer,"%f",&f->centerrayy[index]);
			if (linenum==3) sscanf(buffer,"%f",&f->tiltangle[index]);
			if (linenum==4) sscanf(buffer,"%f",&f->SIC[index]);
			if (linenum==5) sscanf(buffer,"%f",&f->SID[index]);
			if (linenum>5) break;
			linenum++;
		}
        f->mag[index] = f->SID[index]/f->SIC[index];
	}
    fclose(fp);
    return 1;
}

int readCalInfoFile(char* filename,bctfolderinfo* f){
    char tbuffer[500];
    char buffer[500];
    FILE* fp = fopen(filename,"r");
    if (fp==NULL){
        if (VERBOSE) printf("ERROR: Couldn't open file: %s\n",filename);
        return 0;
    } else {
        int linenum=1;
        while (fgets(tbuffer,500,fp)){
            strcpy(buffer,trimbeforecolon(tbuffer));
            //if (linenum==1) sscanf(buffer,"%d",&f->calid);
            if (linenum==2) f->ffkvp = (float)atof(buffer);
            if (linenum==3) sprintf(f->ffmarange,"%s",trim(buffer)); 
            if (linenum==4) f->nffimages = atoi(buffer);
            if (linenum==5) f->fffps = (float)atof(buffer);
            if (linenum==6) sprintf(f->ffdetmodestring,"%s",trim(buffer));
            if (linenum==7) sprintf(f->ffscannername,"%s",trim(buffer));
            if (linenum==8) sprintf(f->fffiltration,"%s",trim(buffer));
            if (linenum==9) sscanf(buffer,"%d/%d/%d",&f->ffscanmonth,&f->ffscanday,&f->ffscanyear); 
            if (linenum==10) f->ffdelay = (float)atof(buffer);
            if (linenum==11) sprintf(f->ffvtrajectory,"%s",trim(buffer));
            linenum++;
        }
    }
    fclose(fp);
    return 1;
}

int readDetInfoFile(char* filename,bctfolderinfo* f){
    char buffer[500];
    FILE* fp = fopen(filename,"r");
    if (fp==NULL){
        if (VERBOSE) printf("ERROR: Couldn't open file: %s\n",filename);
        return 0;
    } else {
        int linenum=1;
        while (fgets(buffer,500,fp)){
            if (linenum==1) f->detx = atoi(buffer);
            if (linenum==2) f->dety = atoi(buffer);
            if (linenum==3) f->xsizemm = (float)atof(buffer);
            if (linenum==4) f->ysizemm = (float)atof(buffer);
            if (linenum==5) f->deadleft = atoi(buffer);
            if (linenum==6) f->deadright = atoi(buffer);
            if (linenum==7) f->deadtop = atoi(buffer);
            if (linenum==8) f->deadbottom = atoi(buffer);
            if (linenum==9) f->numbits = atoi(buffer);
            if (linenum==10) f->Rsquaredlinearitytolerance = (float)atof(buffer);
            if (linenum==11) f->interlacepattern = atof(buffer);
            if (linenum==12) f->Interlaceswaprow = atoi(buffer);
            if (linenum==13) f->isIrregularTrajectory = atoi(buffer);
            if (linenum==14) f->DynamicFlag = atoi(buffer);
            linenum++;
        }
        
        //~ if (f->interlacepattern){
            //~ f->ysizemm*=2; //Note: is this right??
        //~ }
    }
    fclose(fp);
    return 1;
}

int readframefile(char* filename,bctfolderinfo* f){
    char buffer[500];
	FILE* fp = fopen(filename,"r");
	if(fp==NULL){
		if (VERBOSE) printf("ERROR: frame file not found, '%s'\n",filename);
		return 0;
	} else {
		int linenum=1;
		while (fgets(buffer,500,fp)){
			if (linenum==1) sscanf(buffer,"%d %d",&f->startframe,&f->endframe);
			if (linenum>1) break;
			linenum++;
		}
	}
    fclose(fp);
    return 1;
}

int printbctheader(bctheaderinfo* h){
	printf("BreastCT Header Vars:\n");
	printf("nxct: %f\nnyct: %f\nscanid: %f\nislices: %f\nfilter_flag: %f\nseries: %f\nnrays: %f\nnviews: %f\nzzz: %f\nct_pixel_mm: %f\nslice_thickness: %f\ndelw_mm: %f\ndelz_mm: %f\nreconFOV: %f\nxim: %f\nmin5: %f\nmax5: %f\nmean: %f\nsigma: %f\nHU_flag: %f\n",h->nxct,h->nyct,h->scanid,h->islices,h->filter_flag,h->series,h->nrays,h->nviews,h->zzz,h->ctpixel_mm,h->slice_thickness,h->delw_mm,h->delz_mm,h->reconFOV,h->xim,h->min5,h->max5,h->mean,h->sigma,h->HU_flag);
	printf("Space Between slices: %f mm\n\n",h->spacebetweenslices);
    return 1;
}

int printbctfolderinfo(bctfolderinfo* f){
	printf("Select BreastCT Folder Vars:\n");
    printf("kvp: %f\nma: %f\nnimages: %d\n",f->kvp,f->ma,f->nimages);
    printf("scan date: (%02d/%02d/%04d)\n",f->scanmonth,f->scanday,f->scanyear);
    if (strlen(f->scandescription)>0) printf("%s\n",f->scandescription);
    if (strlen(f->objectdescription)>0) printf("%s\n",f->objectdescription);
    if (f->ispatient){
        printf("pid: %d, (MRN: %s)\n",f->patient_id,f->MRN);
    }
    return 1;
}

int readbctheader(const char* fname,bctheaderinfo* h){
	FILE* f;
	char firstfile[1000];
	char lastfile[1000];

	f = fopen(fname,"rb");
	if (!f){
		if (VERBOSE) printf("could not open file.\n");
		return 0;
	}
	fread( (char*)&h->nxct, 4,1,f );
	fread( (char*)&h->nyct, 4,1,f );
	fread( (char*)&h->scanid, 4,1,f );
	fread( (char*)&h->islices, 4,1,f );
	fread( (char*)&h->filter_flag, 4,1,f );
	fread( (char*)&h->series, 4,1,f );
	fread( (char*)&h->nrays, 4,1,f );
	fread( (char*)&h->nviews, 4,1,f );
	fread( (char*)&h->zzz, 4,1,f );
	fread( (char*)&h->ctpixel_mm, 4,1,f );
	fread( (char*)&h->slice_thickness, 4,1,f );
	fread( (char*)&h->delw_mm, 4,1,f );
	fread( (char*)&h->delz_mm, 4,1,f );
	fread( (char*)&h->reconFOV, 4,1,f );
	fread( (char*)&h->xim, 4,1,f );
	fread( (char*)&h->min5, 4,1,f );
	fread( (char*)&h->max5, 4,1,f );
	fread( (char*)&h->mean, 4,1,f );
	fread( (char*)&h->sigma, 4,1,f );
	fread( (char*)&h->HU_flag, 4,1,f );
	fclose(f);

	strcpy(firstfile,fname);
	strcpy(lastfile,fname);
	sprintf(strrchr(firstfile,'.'),".0001");
	sprintf(strrchr(lastfile,'.'),".%04d",(int)h->xim);

    h->spacebetweenslices=0;
	if (h->xim>1){
		f = fopen(firstfile,"rb");
		if (f){
			fseek(f,8*4,SEEK_SET);
			fread( (char*)&h->zzz1, 4,1,f );
			fclose(f);
			f = fopen(lastfile,"rb");
			if (f){
				fseek(f,8*4,SEEK_SET);
				fread( (char*)&h->zzz2, 4,1,f );
				fclose(f);
				
				h->spacebetweenslices = (h->zzz2 - h->zzz1)/(h->xim-1);
			}
		}
	}

    if ((h->HU_flag!=1 && h->HU_flag!=0) || (h->HU_flag==0 && h->mean==0 && h->sigma==0 && h->min5==0 && h->max5==0)){
        //Dead pixel correction (needed for some older images)
        //NOTE: mag_factor in the header later got switched to
        //is_HU (hounsfield units). So if HU_flag!=0 or 1,
        //we know its an old image. if HU_flag=1, we assume 
        //that it is saying that is_HU=1 (it is a houndsfield image).
        //if m_HU_flag==0 and several other tags are 0, we assume
        //its an old image with incomplete header
        
        //set orientation 101.3 deg = correction angle
        h->RLvector[0] = -0.19594614424251769003915530808519;
        h->RLvector[1] = -0.80405385575748230996084469191481;
        // h->RLvector[0] = -.5;
        // h->RLvector[1] = -.5;
        // h->RLvector[0] = -1;
        // h->RLvector[1] = 0;
        // h->RLvector[2] = 0;
        h->APvector[0] = 0;
        h->APvector[1] = 0;
        h->APvector[2] = -1;
        h->ISvectordirection=1;
    } else {
        //set orientation
        h->RLvector[0] = -1;
        h->RLvector[1] = 0;
        h->RLvector[2] = 0;
        h->APvector[0] = 0;
        h->APvector[1] = 0;
        h->APvector[2] = -1;
        h->ISvectordirection=1;
    }

    //get file timestamp of this file
    struct stat buf;
    struct tm *tminfo;
    char timebuf[100];
    stat( fname, &buf );
    tminfo = localtime ( &buf.st_mtime );    
    strftime(timebuf, sizeof(timebuf), "%d", tminfo);
    h->day = atoi(timebuf);
    strftime(timebuf, sizeof(timebuf), "%m", tminfo);
    h->month = atoi(timebuf);
    strftime(timebuf, sizeof(timebuf), "%Y", tminfo);
    h->year = atoi(timebuf);
    strftime(timebuf, sizeof(timebuf), "%H", tminfo);
    h->hour = atoi(timebuf);
    strftime(timebuf, sizeof(timebuf), "%M", tminfo);
    h->min = atoi(timebuf);
    strftime(timebuf, sizeof(timebuf), "%S", tminfo);
    h->sec = atoi(timebuf);
    
    //----------------------------------
    //determine existance of other files
    //----------------------------------
    char tempfilename[1000];
    char tempfoldername[500];
    FILE* fp;
    getbctfolderfromctfile((char*)fname,tempfoldername);
    
    //check for segmentation files
	sprintf(tempfilename,"%s\\SEG\\SEG%04d_%02d_0001.raw",tempfoldername,(int)h->scanid,(int)h->series);
    fp = fopen(tempfilename,"r");
    if (fp==NULL) h->hassegmentation=0;
    else { h->hassegmentation=1; fclose(fp); }
    
    //check for registration files
	sprintf(tempfilename,"%s\\REG\\REG%04d_%02d_0001.raw",tempfoldername,(int)h->scanid,(int)h->series);
    fp = fopen(tempfilename,"r");
    if (fp==NULL) h->hasregistration=0;
    else { h->hasregistration=1; fclose(fp); }

    //check for info file
	sprintf(tempfilename,"%s\\CTi\\CT%04d_%02d.info",tempfoldername,(int)h->scanid,(int)h->series);
    fp = fopen(tempfilename,"r");
    if (fp==NULL){
        h->segmentationvalid=0;
        h->lastchestwallslice=-1;
        h->firstgoodslice=-1;
        h->lastgoodslice=-1;
    } else {
        char tbuffer[500];
        char buffer[500];
        int linenum=1;
        while (fgets(tbuffer,500,fp)){
            strcpy(buffer,trimbeforecolon(tbuffer));
            if (strlen(buffer)==0){ linenum++; continue; }
            
            if (linenum==1) sscanf(buffer,"%d",&h->segmentationvalid);
            if (linenum==2) sscanf(buffer,"%d",&h->lastchestwallslice);
            if (linenum==3) sscanf(buffer,"%d",&h->firstgoodslice);
            if (linenum==4) sscanf(buffer,"%d",&h->lastgoodslice);
            
            linenum++;
        }        
        
        fclose(fp); 
    }

	return 1;
}

bool writeinfofile(const char* fname,bctheaderinfo* h){
    FILE* fp = fopen(fname,"w");
    if (fp!=NULL){
        char buffer[500];
        sprintf(buffer,"Segmentation Valid: %d\n",h->segmentationvalid); fputs(buffer,fp);
        sprintf(buffer,"Last Chest Wall Slice: %d\n",h->lastchestwallslice); fputs(buffer,fp);
        sprintf(buffer,"First Good Slice (after Chest Wall): %d\n",h->firstgoodslice); fputs(buffer,fp);
        sprintf(buffer,"Last Good Slice (before Nipple): %d\n",h->lastgoodslice); fputs(buffer,fp);
        fclose(fp); 
    } else return false;
    return true;
}

int readPatientInfoXXX(char* filename,bctfolderinfo* f) {
    f->patient_id=0;
    f->gender='-';
    f->DOB_month=1;
    f->DOB_day=1;
    f->DOB_year=1900;
    strcpy(f->digitalfilm,"No");
    strcpy(f->labresult,"No");
    f->contrast=0;
    f->contrastdelay=0;
    f->issuspicious=0;
    f->height_in=0;
    f->weight_lbs=0;
    strcpy(f->race,"Unspecified");
    strcpy(f->tabletop,"Unspecified");
    strcpy(f->foreignobject,"none");
    strcpy(f->lastname,"");
    strcpy(f->firstname,"");
    strcpy(f->bodypart,"");
    strcpy(f->metric,"");
    strcpy(f->quality,"");
    strcpy(f->scandescription,"");
    strcpy(f->MRN,"000000");

    char tbuffer[500];
    char buffer[500];
    FILE* fp = fopen(filename,"r");
    if (fp==NULL){
        if (VERBOSE) printf("ERROR: Couldn't open file: %s\n",filename);
        f->ispatient=0;
        return 0;
    } else {
        f->ispatient=1;
        f->gender='F';
        int linenum=1;
        while (fgets(tbuffer,500,fp)){
            strcpy(buffer,trimbeforecolon(tbuffer));
            if (strlen(buffer)==0){ linenum++; continue; }
            
            if (linenum==1) sscanf(buffer,"%d",&f->patient_id);
            if (linenum==2){
                for(unsigned int i=0;i<strlen(buffer);i++) if (buffer[i]==','){ buffer[i]=' '; break;}
                sscanf(buffer,"%s",f->lastname);
                if (strlen(f->lastname)<strlen(buffer)) strcpy(f->firstname,trim(&buffer[strlen(f->lastname)+1]));
            }
            if (linenum==3) sscanf(buffer,"%d/%d/%d",&f->DOB_month,&f->DOB_day,&f->DOB_year);
            if (linenum==4) sscanf(buffer,"%s",&f->MRN);
            //if (linenum==5) sscanf(buffer,"%d",&f->scanid);
            if (linenum==6) strcpy(f->bodypart,trim(buffer));
            if (linenum==7) strcpy(f->metric,trim(buffer));
            if (linenum==8) strcpy(f->quality,trim(buffer));
            if (linenum==9) strcpy(f->scandescription,trim(buffer));
            if (linenum==10) strcpy(f->digitalfilm,trim(buffer));
            if (linenum==11) strcpy(f->labresult,trim(buffer));
            if (linenum==12) sscanf(buffer,"%c",&f->gender);
            if (linenum==13) sscanf(buffer,"%d",&f->contrast);
            if (linenum==14) sscanf(buffer,"%d",&f->contrastdelay);
            if (linenum==15) strcpy(f->race,trim(buffer));
            if (linenum==16) strcpy(f->tabletop,trim(buffer));
            if (linenum==17) strcpy(f->foreignobject,trim(buffer));
            if (linenum==18) sscanf(buffer,"%d",&f->issuspicious);
            if (linenum==19) sscanf(buffer,"%f",&f->height_in);
            if (linenum==20) sscanf(buffer,"%f",&f->weight_lbs);
            
            linenum++;
        }
    }
    fclose(fp);
    return 1;
}

int writePatientInfoXXX(char* filename,bctfolderinfo* f) {
    char tbuffer[500];
    char buffer[500];
    FILE* fp = fopen(filename,"w");
    if (fp==NULL){
        if (VERBOSE) printf("ERROR: Couldn't open file for writing: %s\n",filename);
        return 0;
    } else {
        sprintf(buffer,"Patient ID: %d\n",f->patient_id); fputs(buffer,fp);
        sprintf(buffer,"Patient Name: %s, %s\n",f->lastname,f->firstname); fputs(buffer,fp);
        sprintf(buffer,"Date of Birth: %02d/%02d/%04d\n",f->DOB_month,f->DOB_day,f->DOB_year); fputs(buffer,fp);
        sprintf(buffer,"Medical Record #: %s\n",f->MRN); fputs(buffer,fp);
        sprintf(buffer,"Scan ID: %04d\n",f->scanid); fputs(buffer,fp);
        sprintf(buffer,"Body Part: %s\n",f->bodypart); fputs(buffer,fp);
        sprintf(buffer,"Metric: %s\n",f->metric); fputs(buffer,fp);
        sprintf(buffer,"Quality: %s\n",f->quality); fputs(buffer,fp);
        sprintf(buffer,"Scan Description: %s\n",f->scandescription); fputs(buffer,fp);
        sprintf(buffer,"Digital Film: %s\n",f->digitalfilm); fputs(buffer,fp);
        sprintf(buffer,"Lab Result: %s\n",f->labresult); fputs(buffer,fp);
        sprintf(buffer,"Gender: %c\n",f->gender); fputs(buffer,fp);
        sprintf(buffer,"Contrast: %d\n",f->contrast); fputs(buffer,fp);
        sprintf(buffer,"ContrastDelay: %d\n",f->contrastdelay); fputs(buffer,fp);
        sprintf(buffer,"Race: %s\n",f->race); fputs(buffer,fp);
        sprintf(buffer,"TableTop: %s\n",f->tabletop); fputs(buffer,fp);
        sprintf(buffer,"ForeignObject: %s\n",f->foreignobject); fputs(buffer,fp);
        sprintf(buffer,"isSuspicious: %d\n",f->issuspicious); fputs(buffer,fp);
        sprintf(buffer,"Height_in: %f\n",f->height_in); fputs(buffer,fp);
        sprintf(buffer,"Weight_lbs: %f\n",f->weight_lbs); fputs(buffer,fp);
        fclose(fp);
        return 1;
    }
}

int readReconPlan(char* filename,bctfolderinfo* f) {
    char buffer[500];
	FILE* fp = fopen(filename,"r");
	if(fp==NULL){
		if (VERBOSE) printf("ERROR: recon_plan file not found, '%s'\n",filename);
		return 0;
	} else {
		int linenum=1;
		while (fgets(buffer,500,fp)){
			if (linenum==1) sscanf(buffer,"%d",&f->vid);
			if (linenum==2) sscanf(buffer,"%d",&f->dety0);
			if (linenum==3) sscanf(buffer,"%d",&f->dety1);
			if (linenum==4) sscanf(buffer,"%d",&f->detx1minusx0);
			if (linenum==5) sscanf(buffer,"%d",&f->numslices);
			if (linenum==6) sscanf(buffer,"%f",&f->slicethickness_mm);
			if (linenum==7) sscanf(buffer,"%d",&f->i0x1);
			if (linenum==8) sscanf(buffer,"%d",&f->ioy1);
			if (linenum==9) sscanf(buffer,"%d",&f->i0x2);
			if (linenum==10) sscanf(buffer,"%d",&f->ioy2);
			if (linenum>10) break;
			linenum++;
		}
	}
    fclose(fp);
    
    if (f->interlacepattern){ //NOTE: CHECK THIS!!
        f->ioy1/=2;
        f->ioy2/=2;
        f->dety0/=2;
        f->dety1/=2;
    }
    
    return 1;
}

int READ_cylinder(char* filename,bctfolderinfo* f){
    char buffer[500];
	FILE* fp = fopen(filename,"r");
	if(fp==NULL){
		if (VERBOSE) printf("ERROR: cylinder file not found, '%s'\n",filename);
		return 0;
	} else {
		int linenum=1;
		while (fgets(buffer,500,fp)){
			if (linenum==1) sscanf(buffer,"%f",&f->ro);
			if (linenum==2) sscanf(buffer,"%f",&f->xo);
			if (linenum==3) sscanf(buffer,"%f",&f->yo);
			if (linenum==4) sscanf(buffer,"%f",&f->ho);
            //*********************
			if (linenum>4) break;
			linenum++;
		}
	}
	fclose(fp);
    return 1;
}

int READ_roi(char* filename,bctfolderinfo* f){
    char buffer[500];
	FILE* fp = fopen(filename,"r");
	if(fp==NULL){
		if (VERBOSE) printf("ERROR: roi file not found, '%s'\n",filename);
		return 0;
	} else {
		int linenum=1;
		while (fgets(buffer,500,fp)){
			if (linenum==1) sscanf(buffer,"%f",&f->roi_ro);
			if (linenum==2) sscanf(buffer,"%f",&f->roi_xo);
			if (linenum==3) sscanf(buffer,"%f",&f->roi_yo);
			if (linenum>3) break;
			linenum++;
		}
	}
	fclose(fp);
    return 1;
}

int READ_dicom(char* filename,bctfolderinfo* f){
    char buffer[500];
    char tbuffer[500];
	FILE* fp = fopen(filename,"r");
	if(fp==NULL){
		if (VERBOSE) printf("ERROR: dicominfo file not found '%s'\nsetting default values\n",filename);
        strcpy(f->uidSite,"1.2.826.0.1.3680043.2.1197"); //ucdmc uid
        strcpy(f->uidScanner,"1.2.826.0.1.3680043.2.1197.0"); //original albion
        strcpy(f->institutionname,"UCDAVIS MEDICAL CENTER");
		return 0;
	} else {
        int linenum=1;
        while (fgets(tbuffer,500,fp)){
            strcpy(buffer,trimbeforecolon(tbuffer));
            if (linenum==1) strcpy(f->uidSite,buffer);
            if (linenum==2) strcpy(f->uidScanner,buffer);
            if (linenum==3) strcpy(f->institutionname,buffer);
            if (linenum>3) break;
            linenum++;
        }
	}
    fclose(fp);
    return 1;
}

int readctscanfile(char* filename,bctfolderinfo* f){
    char tbuffer[500]; 
    char buffer[500];
    FILE* fp = fopen(filename,"r");
    if (fp==NULL){
        if (VERBOSE) printf("ERROR: Couldn't open file: %s\n",filename);
        return 0;
    } else {
        int linenum=1;
        while (fgets(tbuffer,500,fp)){
            if(tbuffer[0]=='*'){
                fclose(fp);
                return 0;
            }
            strcpy(buffer,trimbeforecolon(tbuffer));
            //if (linenum==1) sscanf(buffer,"%d",&f->scanid);
            if (linenum==2) strcpy(f->objectdescription,trim(buffer));
            if (linenum==3) sscanf(buffer,"%d/%d/%d",&f->scanmonth,&f->scanday,&f->scanyear); 
            if (linenum==4) f->kvp = (float)atof(buffer);
            if (linenum==5) f->ma = (float)atof(buffer);
            if (linenum==6) f->degrees = (float)atof(buffer);
            if (linenum==7) f->nimages = atoi(buffer);
            if (linenum==8) f->fps = (float)atof(buffer);
            if (linenum==9){
                strcpy(f->detmodestring,trim(buffer));
                sscanf(buffer,"%dx%d",&f->detbinningx,&f->detbinningy);
            }
            if (linenum==10) sscanf(buffer,"%s",&f->scannername);
            if (linenum==11) sscanf(buffer,"%s",&f->filtration);
            if (linenum==12) sscanf(buffer,"%s",&f->vtrajectory);
            if (linenum==13) f->calid = atoi(buffer); 
            if (linenum==14) sscanf(buffer,"%s",&f->scanneroperator);
            linenum++;
        }
    }
    fclose(fp);
    return 1;
}

int READ_anglefile(char* filename,bctfolderinfo* f){
    char buffer[500];
	FILE* fp = fopen(filename,"r");
	if(fp==NULL){
		if (VERBOSE) printf("ERROR: angle file not found, '%s'\n",filename);
		return 0;
	} else {
		int a;
		float b,c,d;
		while (fgets(buffer,500,fp)){
			sscanf(buffer,"%d %f %f %f",&a,&b,&c,&d);
			if (a>=0 && a<=f->nimages){
				f->theta[a]=c;
				f->vpos[a]=d;
			} else {
				if (VERBOSE) printf("ERROR: bad angle file, '%s'\n",filename);
                fclose(fp);
				return 0;
			}
		}
	}
    fclose(fp);
    return 1;
}
	
int READ_inffile(char* filename,bctfolderinfo* f){
    char buffer[500];
	FILE* fp = fopen(filename,"r");
	if(fp==NULL){
		if (VERBOSE) printf("NOTE: inf file not found, '%s'\n",filename);
        return 0;
	} else {
		while (fgets(buffer,500,fp)){
			sscanf(buffer,"%d %d",&f->lastseriesid,&f->lastseriesnumslices);
		}
	}
	fclose(fp);
    return 1;
}

int READ_hisfileandheaders(char* filename,char* folder,bctfolderinfo* f){
    f->numseries=0;
    char buffer[500];
    char fname[500];
	FILE* fp = fopen(filename,"r");
	if(fp==NULL){
		if (VERBOSE) printf("NOTE: his file not found, '%s'\n",filename);
        return 0;
	} else {
		while (fgets(buffer,500,fp)){
			sscanf(buffer,"%d %d",&f->seriesid[f->numseries],&f->seriesnumslices[f->numseries]);
          	sprintf(fname,"%s/CTi/CT%04d_%02d.0001",folder,f->scanid,f->seriesid[f->numseries]);
            readbctheader(fname,&f->seriesheader[f->numseries]);
            f->numseries++;
		}
	}
	fclose(fp);
    return 1;
}

bool updateinfandhis(char* folder,int newseriesid,int newseriesnumslices,int huflag,char* notes){
    FILE* fp;
    bctfolderinfo f;

    //check to see if the folder is a bctfolder
    //this will also define scanid and scannertag
    if (!isbctFolder(folder,&f)) return false; //ERROR NOT BCT FOLDER
    
    //calculate time and computer name for .his file
    char now_str[100];
    char computername[1000];
    char filename[1000];
	char *cname;
	time_t rawtime;
	struct tm* timeinfo;
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	sprintf ( now_str, "%04d-%02d-%02d %02d:%02d:%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec );
	if (( cname = getenv( "COMPUTERNAME" )) != NULL ) {
		strncpy( computername, cname, 99 );
		//printf("\n computer name is %s\n",computername);
	} else strncpy( computername, "NULL",4 ); //didn't catch computer name
    
    //write .inf file
    sprintf(filename,"%s/CTi/CT%04d.inf",folder,f.scanid);
    fp = fopen(filename,"w");
    fprintf(fp,"%d %d\n",newseriesid,newseriesnumslices);
    fclose(fp);
   
    //write .his file and series headers
    sprintf(filename,"%s/CTi/CT%04d.his",folder,f.scanid);
    fp = fopen(filename,"a");
    fprintf(fp,"%5d  %5d  %1d %s  %s  %s\n",newseriesid,newseriesnumslices,huflag, now_str,computername,notes);
    fclose(fp);

    return false;
}

int readFinalDeadPixelsFile(char* filename,bctfolderinfo* f){
    char buffer[500];
	FILE* fp = fopen(filename,"r");
	if(fp==NULL){
		if (VERBOSE) printf("NOTE: inf file not found, '%s'\n",filename);
        return 0;
	} else {
		while (fgets(buffer,500,fp)){
			sscanf(buffer,"%d %d",&f->deadpixelsx[f->numdeadpixels],&f->deadpixelsy[f->numdeadpixels]);
            f->numdeadpixels++;
            if (f->numdeadpixels>=5000) break;
		}
	}
	fclose(fp);
    return 1;
}

int loadbctfolderdata(char* folder,bctfolderinfo* f){
    char filename[1000];
        
    //check to see if the folder is a bctfolder
    //this will also define scanid and scannertag
    if (!isbctFolder(folder,f)){
        //ERROR NOT BCT FOLDER
        return 0;
    }
    
    //read paramtr file
    sprintf(filename,"%s/XXX/parmtr%04d.xxx",folder,f->scanid);
    readParmtrXXX(filename,f);
    if (VERBOSE) printf("PARAMTR FILE:\n%s %s\n%f\n%f\n%d\n%f\n%d:%d:%d\n%02d:%02d:%02d\n%f\n%02d/%02d/%02d\n%d\n%s\n%s\n%d\n%f\n%f\n%d\n%d\n%f\n",f->lastname,f->firstname,f->kvp,f->ma,f->nimages,f->scan_time,f->openshutterhour,f->openshuttermin,f->openshuttersec,f->closeshutterhour,f->closeshuttermin,f->closeshuttersec,f->diameter,f->scanmonth,f->scanday,f->scanyear,f->detmode,f->detgain,f->cupsize,f->calid,f->toppos,f->bottompos,f->bbdatascanid1,f->bbdatascanid2,f->offsetangle);
    
    //read ctscan file
    sprintf(filename,"%s/XXX/ctscan%04d.xxx",folder,f->scanid);
    if (VERBOSE) printf("CTSCAN FILE:\n");
    readctscanfile(filename,f);
    if (VERBOSE) printf("\n");
    
    //read cal file (first check to see if there are 2 cals for irregular trajectories)
    if (!f->irregulartrajectory){
        sprintf(filename,"%s/CAL/%scalfactors%04d.cal",folder,f->scannertag,f->calid);
        readCalFile(filename,0,f);
        if (VERBOSE) printf("CAL FILE:\n%f\n%f\n%f\n%f\n%f\n%f\n\n",f->centerrayx[0],f->centerrayy[0],f->tiltangle[0],f->SID[0],f->SIC[0],f->mag[0]);
    } else {
        sprintf(filename,"%s/CAL/%scalfactors%04d_1.cal",folder,f->scannertag,f->calid);
        readCalFile(filename,0,f);
        sprintf(filename,"%s/CAL/%scalfactors%04d_2.cal",folder,f->scannertag,f->calid);
        readCalFile(filename,1,f);
        if (VERBOSE) printf("CAL FILE1:\n%f\n%f\n%f\n%f\n%f\n%f\n\n",f->centerrayx[0],f->centerrayy[0],f->tiltangle[0],f->SID[0],f->SIC[0],f->mag[0]);
        if (VERBOSE) printf("CAL FILE2:\n%f\n%f\n%f\n%f\n%f\n%f\n\n",f->centerrayx[1],f->centerrayy[1],f->tiltangle[1],f->SID[1],f->SIC[1],f->mag[1]);
    }

    //read ctcal
    sprintf(filename,"%s/CAL/ctcal%04d.cal",folder,f->calid);
    readCalInfoFile(filename,f);
    if (VERBOSE) printf("CAL INFO FILE: ... \n");

    //read finaldeadpixels (skip deadpixels, deadrows,and deadcols)
    sprintf(filename,"%s/CAL/%sdeadpixels%04d.cal",folder,f->scannertag,f->calid);
    readFinalDeadPixelsFile(filename,f);
    if (VERBOSE) printf("Final Dead Pixels: ... \n");
        
    //read detinfo
    sprintf(filename,"%s/CAL/detinfo%04d.cal",folder,f->calid);
    readDetInfoFile(filename,f);
    if (VERBOSE) printf("DETECTOR INFO FILE: ... \n");    

    //read .ini files???? (annotations)

    //read frame file
    sprintf(filename,"%s/XXX/frames%04d.xxx",folder,f->scanid);
    if (VERBOSE) printf("FRAME FILE:\n");
    readframefile(filename,f);
    if (VERBOSE) printf("%d %d\n\n",f->startframe,f->endframe);
    
    //read patient file
    sprintf(filename,"%s/xxx/patientinfo%04d.xxx",folder,f->scanid);
    if (VERBOSE) printf("PATIENTINFO:\n");
    readPatientInfoXXX(filename,f);
    if (VERBOSE) printf("patient_id: %d\nlastname: %s\nfirstname: %s\nDOB: %02d/%02d/%02d\nMRN: %s\nscanid: %d\nbody part: %s\nmetric: %s\nquality: %s\nscan description: %s\ndigital film: %s\nlab result: %s\ngender: %c\ncontrast: %d\ncontrastdelay: %d\n\n",f->patient_id,f->lastname,f->firstname,f->DOB_month,f->DOB_day,f->DOB_year,f->MRN,f->scanid,f->bodypart,f->metric,f->quality,f->scandescription,f->digitalfilm,f->labresult,f->gender,f->contrast,f->contrastdelay);

    //read recon plan file
    sprintf(filename,"%s/XXX/recon_plan%04d.xxx",folder,f->scanid);
    if (VERBOSE) printf("RECON PLAN:\n");
    readReconPlan(filename,f);
    if (VERBOSE) printf("\n");
    
    //read cylinder
    sprintf(filename,"%s/XXX/cylinder%04d.xxx",folder,f->scanid);
    if (VERBOSE) printf("CYLINDER:\n");
    READ_cylinder(filename,f);
    if (VERBOSE) printf("\n");
    
    //read angle file
    sprintf(filename,"%s/XXX/angles%04d.xxx",folder,f->scanid);
    if (VERBOSE) printf("ANGLE FILE:\n");
    READ_anglefile(filename,f);
    if (VERBOSE) printf("\n");

    //read .inf file
    sprintf(filename,"%s/CTi/CT%04d.inf",folder,f->scanid);
    if (VERBOSE) printf("INF FILE:\n");
    READ_inffile(filename,f);
    if (VERBOSE) printf("\n");
   
    //read .his file and series headers
    sprintf(filename,"%s/CTi/CT%04d.his",folder,f->scanid);
    if (VERBOSE) printf("SERIES FROM .HIS FILE:\n");
    READ_hisfileandheaders(filename,folder,f);
    //********************
    if (VERBOSE) printf("\n");
    
    //read roi file
    sprintf(filename,"%s/XXX/ROI%04d.xxx",folder,f->scanid);
    if (VERBOSE) printf("ROI:\n");
    READ_roi(filename,f);
    if (VERBOSE) printf("\n");

    //read dicominfo file
    sprintf(filename,"%s/XXX/dicominfo%04d.xxx",folder,f->scanid);
    if (VERBOSE) printf("DICOMINFO FILE:\n");
    READ_dicom(filename,f);
    if (VERBOSE) printf("\n");
    
    return 1;
}

bool openbctfile(const char* filename,float* dest,bctheaderinfo* h){
	// obtain file size:
	FILE* fp = fopen(filename,"rb");
	if (fp==0) return false;
	fseek (fp , 0 , SEEK_END);
	long lSize = ftell(fp);
    if (lSize>BCTHEADERSIZE){
        fseek(fp,BCTHEADERSIZE,SEEK_SET);
        fread( (char*)dest, 1, lSize - BCTHEADERSIZE, fp);
        long s1 = (lSize - BCTHEADERSIZE)/sizeof(float);
        long s2 = h->nxct*h->nyct*sizeof(float);
        int size = s1;
        if (s2<size) size = s2;
        for(int i=0;i<size;i++){
            dest[i]=BCTLIB_CLAMP(dest[i],-1000,5000);
        }
    }
    fclose(fp);

	return true;
}

bool openbctfilesequence(const char* filename,float* dest){
	bctheaderinfo info;
	readbctheader(filename,&info);
	int imagesize = (int)(info.nxct*info.nxct);
	//load each file into the volume
	for (int i=1;i<=(int)info.xim;i++){
		char newfilename[1000];
		strcpy(newfilename,filename);
		int fnamesize=strlen(newfilename);
		for(int c=fnamesize-1;c>=0;c--){
			if (newfilename[c]=='.'){
				newfilename[c]='\0';
				break;
			}
		}
		sprintf(newfilename,"%s.%04d",newfilename,i);
		if (VERBOSE) printf("loading image: %s\r",newfilename);
		if (!openbctfile(newfilename,&dest[(i-1)*imagesize],&info)) return false;
	}
	return true;
}

int writebctslice(const char* fname,bctheaderinfo* h,float* data){
    if (VERBOSE) printbctheader(h);
	FILE* f;
	f = fopen(fname,"wb");
	if (!f){
		printf("could not open file for saving.\n");
		return 0;
	}
	fwrite( (char*)&h->nxct, 4,1,f );
	fwrite( (char*)&h->nyct, 4,1,f );
	fwrite( (char*)&h->scanid, 4,1,f );
	fwrite( (char*)&h->islices, 4,1,f );
	fwrite( (char*)&h->filter_flag, 4,1,f );
	fwrite( (char*)&h->series, 4,1,f );
	fwrite( (char*)&h->nrays, 4,1,f );
	fwrite( (char*)&h->nviews, 4,1,f );
	fwrite( (char*)&h->zzz, 4,1,f );
	fwrite( (char*)&h->ctpixel_mm, 4,1,f );
	fwrite( (char*)&h->slice_thickness, 4,1,f );
	fwrite( (char*)&h->delw_mm, 4,1,f );
	fwrite( (char*)&h->delz_mm, 4,1,f );
	fwrite( (char*)&h->reconFOV, 4,1,f );
	fwrite( (char*)&h->xim, 4,1,f );
	fwrite( (char*)&h->min5, 4,1,f );
	fwrite( (char*)&h->max5, 4,1,f );
	fwrite( (char*)&h->mean, 4,1,f );
	fwrite( (char*)&h->sigma, 4,1,f );
	fwrite( (char*)&h->HU_flag, 4,1,f );
    fwrite( (char*)data, 4,h->nxct*h->nyct,f );
	fclose(f);
	return 1;
}

int readbctslice(const char* fname,bctheaderinfo* h,float* data){
    if (!readbctheader(fname,h)) return 0;
    //cout << "(" << h->nxct << "," << h->nyct <<")";
    if (!openbctfile(fname,data,h)) return 0;
    if (VERBOSE) printbctheader(h);
    return 1;
}

#endif