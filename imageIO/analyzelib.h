#ifndef ANALYZELIB
#define ANALYZELIB
#define ANALYZE_VERBOSE 0

#define ANALYZE_HEADERSIZE 348

#define DT_NONE 0
#define DT_UNKNOWN 0
#define DT_BINARY 1
#define DT_UNSIGNED_CHAR 2
#define DT_SIGNED_SHORT 4
#define DT_SIGNED_INT 8
#define DT_FLOAT 16
#define DT_COMPLEX 32
#define DT_DOUBLE 64
#define DT_RGB 128
#define DT_ALL 255

#include <time.h>
#include <sys/stat.h>
#include <float.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#define isnan(x) ((x) != (x))

/*!
//Holds analyze header info plus a bit more
*/
class analyzeheaderinfo {
    public:
    analyzeheaderinfo(){
        sizeofheader = ANALYZE_HEADERSIZE;
        strcpy(data_type,"");
        strcpy(db_name,"");
        extents = 16384;
        session_error = 0;
        regular = 'r';
        hkey_un0 = ' '; //is this right default value?
        memset(dim,0,sizeof(dim));
        dim[0] = 4;
        strcpy(vox_units,"mm");
        strcpy(cal_units,"");
        unused1 = 0;
        _datatype = 0; //is this the right default??
        bitpix = 1; //1,8,16,32,64
        dim_un0 = 0;
        memset(pixdim,0,sizeof(pixdim));
        for(int i=1;i<dim[0];i++) pixdim[i] = 1;
        vox_offset = 0;
        funused1=funused2=funused3=0;
        cal_max=cal_min=0;
        compressed=0;
        verified=0;
        glmax=glmin=0;
        strcpy(descrip,"");
        strcpy(aux_file,"");
        orient = 0;
        memset(originator,0,sizeof(originator));
        strcpy(generated,"");
        strcpy(scannum,"");
        strcpy(patient_id,"");
        strcpy(exp_date,"");
        strcpy(exp_time,"");
        strcpy(un0,"");
        views = 0;
        vols_added = 0;
        start_field = 0;
        field_skip = 0;
        omax=omin = 0;
        smax=smin = 0;
        swapendianflag = 0;
    }
    int sizeofheader;
    char data_type[10];
    char db_name[18];
    int extents;
    short session_error;
    char regular;
    char hkey_un0;
    short dim[8];
    char vox_units[4];
    char cal_units[8];
    short unused1;
    short _datatype;
    short bitpix;
    short dim_un0;
    float pixdim[8];
    float vox_offset;
    float funused1,funused2,funused3;
    float cal_max,cal_min;
    int compressed;
    int verified;
    int glmax,glmin;
    char descrip[80];
    char aux_file[24];
    char orient;
    short originator[5];
    char generated[10];
    char scannum[10];
    char patient_id[10];
    char exp_date[10];
    char exp_time[10];
    char un0[3];
    int views;
    int vols_added;
    int start_field;
    int field_skip;
    int omax,omin;
    int smax,smin;
    
    //extra vars
    int swapendianflag;
};

void short_endian_swap(short* x){
    unsigned short t = *((unsigned short*)x);
    t = (t>>8) | (t<<8);
    *x = *((short*)&t);
}

void int_endian_swap(int* x){
    unsigned int t = *((unsigned int*)x);
    t = (t>>24) | ((t<<8) & 0x00FF0000) | ((t>>8) & 0x0000FF00) | (t<<24);
    *x = *((int*)&t);
}

void float_endian_swap(float* x){
    unsigned int t = *((unsigned int*)x);
    t = (t>>24) | ((t<<8) & 0x00FF0000) | ((t>>8) & 0x0000FF00) | (t<<24);
    *x = *((float*)&t);
}

void longlong_endian_swap(long long* x){
    unsigned long long t = *((unsigned long long*)x);
    t = (t>>56) | 
        ((t<<40) & 0x00FF000000000000) |
        ((t<<24) & 0x0000FF0000000000) |
        ((t<<8)  & 0x000000FF00000000) |
        ((t>>8)  & 0x00000000FF000000) |
        ((t>>24) & 0x0000000000FF0000) |
        ((t>>40) & 0x000000000000FF00) |
        (t<<56);
    *x = *((long long*)&t);
}

void double_endian_swap(double* x){
    unsigned long long t = *((unsigned long long*)x);
    t = (t>>56) | 
        ((t<<40) & 0x00FF000000000000) |
        ((t<<24) & 0x0000FF0000000000) |
        ((t<<8)  & 0x000000FF00000000) |
        ((t>>8)  & 0x00000000FF000000) |
        ((t>>24) & 0x0000000000FF0000) |
        ((t>>40) & 0x000000000000FF00) |
        (t<<56);
    *x = *((double*)&t);
}

void getimgfilename(const char* headerfilename,char* imgfilename){
    int len = strlen(headerfilename)-4;
    strncpy(imgfilename,headerfilename,len);
    imgfilename[len++]='.';
    imgfilename[len++]='i';
    imgfilename[len++]='m';
    imgfilename[len++]='g';
    imgfilename[len]=NULL;
}

bool isanalyzefile(const char* filename){
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
	return (strcmp(extension,"hdr")==0);
}

int printanalyzeheader(analyzeheaderinfo* h){
	printf("\nAnalyze Header Vars:\n");
    printf("sizeofheader: %d\n",h->sizeofheader);
    printf("data_type: %s\n",h->data_type);
    printf("db_name: %s\n",h->db_name);
    printf("extents: %d\n",h->extents);
    printf("session_error: %d\n",h->session_error);
    printf("regular: %c\n",h->regular);
    printf("hkey_un0: %c\n",h->hkey_un0);
    for(int i=0;i<8;i++) printf("dim[%d]: %d\n",i,(int)h->dim[i]);
    printf("vox_units: %s\n",h->vox_units);
    printf("cal_units: %s\n",h->cal_units);
    printf("unused1: %d\n",h->unused1);
    printf("_datatype: %d\n",h->_datatype);
    printf("bitpix: %d\n",h->bitpix);
    printf("dim_un0: %d\n",h->dim_un0);
    for(int i=0;i<8;i++) printf("pixdim[%d]: %f\n",i,h->pixdim[i]);
    printf("vox_offset: %f\n",h->vox_offset);
    printf("funused1: %f\n",h->funused1);
    printf("funused2: %f\n",h->funused2);
    printf("funused3: %f\n",h->funused3);
    printf("cal_max: %f\n",h->cal_max);
    printf("cal_min: %f\n",h->cal_min);
    printf("compressed: %d\n",h->compressed);
    printf("verified: %d\n",h->verified);
    printf("glmax: %d\n",h->glmax);
    printf("glmin: %d\n",h->glmin);
    printf("descrip: %s\n",h->descrip);
    printf("aux_file: %s\n",h->aux_file);
    printf("orient: %c\n",h->orient);
    for(int i=0;i<5;i++) printf("originator[%d]: %d\n",i,h->originator[i]);
    printf("generated: %s\n",h->generated);
    printf("generated: %s\n",h->generated);
    printf("scannum: %s\n",h->scannum);
    printf("patient_id: %s\n",h->patient_id);
    printf("exp_date: %s\n",h->exp_date);
    printf("exp_time: %s\n",h->exp_time);
    printf("un0: %s\n",h->un0);
    printf("views: %d\n",h->views);
    printf("vols_added: %d\n",h->vols_added);
    printf("start_field: %d\n",h->start_field);
    printf("field_skip: %d\n",h->field_skip);
    printf("omax: %d\n",h->omax);
    printf("omin: %d\n",h->omin);
    printf("smax: %d\n",h->smax);
    printf("smin: %d\n",h->smin);
    return 1;
}

int readanalyzeheader(const char* fname,analyzeheaderinfo* h){
	FILE* f = fopen(fname,"rb");
	if (!f){
		if (ANALYZE_VERBOSE) printf("could not open file.\n");
		return 0;
	}
    int swapendianflag=h->swapendianflag=0;
	fread( (char*)&h->sizeofheader, sizeof(int),1,f );
    if (h->sizeofheader>ANALYZE_HEADERSIZE){
        swapendianflag = h->swapendianflag = 1;
        int_endian_swap(&h->sizeofheader);
    }
	fread( (char*)&h->data_type, sizeof(char)*10,1,f );
	fread( (char*)&h->db_name, sizeof(char)*18,1,f );
	fread( (char*)&h->extents, sizeof(int),1,f );
    if (swapendianflag) int_endian_swap(&h->extents);
	fread( (char*)&h->session_error, sizeof(short),1,f );
    if (swapendianflag) short_endian_swap(&h->session_error);
	fread( (char*)&h->regular, sizeof(char),1,f );
	fread( (char*)&h->hkey_un0, sizeof(char),1,f );
	for(int i=0;i<8;i++){
        fread( (char*)&h->dim[i], sizeof(short),1,f );
        if (swapendianflag) short_endian_swap(&h->dim[i]);
    }
	fread( (char*)&h->vox_units, sizeof(char)*4,1,f );
	fread( (char*)&h->cal_units, sizeof(char)*8,1,f );
	fread( (char*)&h->unused1, sizeof(short),1,f );
    if (swapendianflag) short_endian_swap(&h->unused1);
	fread( (char*)&h->_datatype, sizeof(short),1,f );
    if (swapendianflag) short_endian_swap(&h->_datatype);
	fread( (char*)&h->bitpix, sizeof(short),1,f );
    if (swapendianflag) short_endian_swap(&h->bitpix);
	fread( (char*)&h->dim_un0, sizeof(short),1,f );
    if (swapendianflag) short_endian_swap(&h->dim_un0);
	for(int i=0;i<8;i++){
        fread( (char*)&h->pixdim[i], sizeof(float),1,f );
        if (swapendianflag) float_endian_swap(&h->pixdim[i]);
    }
	fread( (char*)&h->vox_offset, sizeof(float),1,f );
    if (swapendianflag) float_endian_swap(&h->vox_offset);
	fread( (char*)&h->funused1, sizeof(float),1,f );
    if (swapendianflag) float_endian_swap(&h->funused1);
	fread( (char*)&h->funused2, sizeof(float),1,f );
    if (swapendianflag) float_endian_swap(&h->funused2);
	fread( (char*)&h->funused3, sizeof(float),1,f );
    if (swapendianflag) float_endian_swap(&h->funused3);
	fread( (char*)&h->cal_max, sizeof(float),1,f );
    if (swapendianflag) float_endian_swap(&h->cal_max);
	fread( (char*)&h->cal_min, sizeof(float),1,f );
    if (swapendianflag) float_endian_swap(&h->cal_min);
	fread( (char*)&h->compressed, sizeof(int),1,f );
    if (swapendianflag) int_endian_swap(&h->compressed);
	fread( (char*)&h->verified, sizeof(int),1,f );
    if (swapendianflag) int_endian_swap(&h->verified);
	fread( (char*)&h->glmax, sizeof(int),1,f );
    if (swapendianflag) int_endian_swap(&h->glmax);
	fread( (char*)&h->glmin, sizeof(int),1,f );
    if (swapendianflag) int_endian_swap(&h->glmin);
	fread( (char*)&h->descrip, sizeof(char)*80,1,f );
	fread( (char*)&h->aux_file, sizeof(char)*24,1,f );
	fread( (char*)&h->orient, sizeof(char),1,f );
	for(int i=0;i<5;i++){
        fread( (char*)&h->originator[i], sizeof(short),1,f );
        if (swapendianflag) short_endian_swap(&h->originator[i]);
    }
	fread( (char*)&h->generated, sizeof(char)*10,1,f );
	fread( (char*)&h->scannum, sizeof(char)*10,1,f );
	fread( (char*)&h->patient_id, sizeof(char)*10,1,f );
	fread( (char*)&h->exp_date, sizeof(char)*10,1,f );
	fread( (char*)&h->exp_time, sizeof(char)*10,1,f );
	fread( (char*)&h->un0, sizeof(char)*3,1,f );
    
	fread( (char*)&h->views, sizeof(int),1,f );
    if (swapendianflag) int_endian_swap(&h->views);
	fread( (char*)&h->vols_added, sizeof(int),1,f );
    if (swapendianflag) int_endian_swap(&h->vols_added);
	fread( (char*)&h->start_field, sizeof(int),1,f );
    if (swapendianflag) int_endian_swap(&h->start_field);
	fread( (char*)&h->field_skip, sizeof(int),1,f );
    if (swapendianflag) int_endian_swap(&h->field_skip);
	fread( (char*)&h->omax, sizeof(int),1,f );
    if (swapendianflag) int_endian_swap(&h->omax);
	fread( (char*)&h->omin, sizeof(int),1,f );
    if (swapendianflag) int_endian_swap(&h->omin);
	fread( (char*)&h->smax, sizeof(int),1,f );
    if (swapendianflag) int_endian_swap(&h->smax);
	fread( (char*)&h->smin, sizeof(int),1,f );
    if (swapendianflag) int_endian_swap(&h->smin);

    fclose(f);
	return 1;
}

bool openanalyzefile(const char* filename,void* dest,analyzeheaderinfo* h){
    if (!readanalyzeheader(filename,h)) return false;
    if (ANALYZE_VERBOSE) printanalyzeheader(h);
    char imgfilename[1000]; 
    getimgfilename(filename,imgfilename);
    FILE* fp = fopen(imgfilename,"rb");
	if (fp){
        int SIZE = h->dim[1]*h->dim[2]*h->dim[3]*h->dim[4];
        switch(h->_datatype){
            case DT_UNSIGNED_CHAR:{
                fread( (char*)dest, sizeof(unsigned char), SIZE, fp);
                break;
            }
            case DT_SIGNED_SHORT:{
                fread( (char*)dest, sizeof(short), SIZE, fp);
                short* data = (short*)dest;
                if (h->swapendianflag){
                    for(int i=0;i<SIZE;i++) short_endian_swap(&data[i]);
                }
                break;
            }
            case DT_SIGNED_INT:{
                fread( (char*)dest, sizeof(int), SIZE, fp);
                int* data = (int*)dest;
                if (h->swapendianflag){
                    for(int i=0;i<SIZE;i++) int_endian_swap(&data[i]);
                }
                break;
            }
            case DT_FLOAT:{
                fread( (char*)dest, sizeof(float), SIZE, fp);
                float* data = (float*)dest;
                if (h->swapendianflag){
                    for(int i=0;i<SIZE;i++) float_endian_swap(&data[i]);
                }
                break;
            }
            case DT_COMPLEX:{
                fread( (char*)dest, sizeof(float), SIZE*2, fp);
                float* data = (float*)dest;
                if (h->swapendianflag){
                    for(int i=0;i<SIZE*2;i++) float_endian_swap(&data[i]);
                }
                break;
            }
            case DT_DOUBLE:{
                fread( (char*)dest, sizeof(double), SIZE, fp);
                double* data = (double*)dest;
                if (h->swapendianflag){
                    for(int i=0;i<SIZE;i++) double_endian_swap(&data[i]);
                }
                break;
            }
            case DT_RGB:{
                fread( (char*)dest, sizeof(unsigned char), SIZE*3, fp);
                break;
            }
            case DT_BINARY: //...not yet implemented
            default: {
                //DT_ALL, DT_UNKNOWN, DT_NONE cases
                printf("Invalid DataType: %d\n",h->_datatype);
                fclose(fp); return false;
            }
        }
        
        fclose(fp);
    } else return false;
	return true;
}

int writeanalyzeslice(const char* fname,analyzeheaderinfo* h,void* data){
    if (ANALYZE_VERBOSE) printanalyzeheader(h);
	FILE* f;
	f = fopen(fname,"wb");
	if (!f){
		printf("could not open file for saving.\n");
		return 0;
	}
	fwrite( (char*)&h->sizeofheader, sizeof(int),1,f );
	fwrite( (char*)&h->data_type, sizeof(char)*10,1,f );
	fwrite( (char*)&h->db_name, sizeof(char)*18,1,f );
	fwrite( (char*)&h->extents, sizeof(int),1,f );
	fwrite( (char*)&h->session_error, sizeof(short),1,f );
	fwrite( (char*)&h->regular, sizeof(char),1,f );
	fwrite( (char*)&h->hkey_un0, sizeof(char),1,f );
	for(int i=0;i<8;i++) fwrite( (char*)&h->dim[i], sizeof(short),1,f );
	fwrite( (char*)&h->vox_units, sizeof(char)*4,1,f );
	fwrite( (char*)&h->cal_units, sizeof(char)*8,1,f );
	fwrite( (char*)&h->unused1, sizeof(short),1,f );
	fwrite( (char*)&h->_datatype, sizeof(short),1,f );
	fwrite( (char*)&h->bitpix, sizeof(short),1,f );
	fwrite( (char*)&h->dim_un0, sizeof(short),1,f );
	for(int i=0;i<8;i++) fwrite( (char*)&h->pixdim[i], sizeof(float),1,f );
	fwrite( (char*)&h->vox_offset, sizeof(float),1,f );
	fwrite( (char*)&h->funused1, sizeof(float),1,f );
	fwrite( (char*)&h->funused2, sizeof(float),1,f );
	fwrite( (char*)&h->funused3, sizeof(float),1,f );
	fwrite( (char*)&h->cal_max, sizeof(float),1,f );
	fwrite( (char*)&h->cal_min, sizeof(float),1,f );
	fwrite( (char*)&h->compressed, sizeof(int),1,f );
	fwrite( (char*)&h->verified, sizeof(int),1,f );
	fwrite( (char*)&h->glmax, sizeof(int),1,f );
	fwrite( (char*)&h->glmin, sizeof(int),1,f );
	fwrite( (char*)&h->descrip, sizeof(char)*80,1,f );
	fwrite( (char*)&h->aux_file, sizeof(char)*24,1,f );
	fwrite( (char*)&h->orient, sizeof(char),1,f );
	for(int i=0;i<5;i++) fwrite( (char*)&h->originator[i], sizeof(short),1,f );
	fwrite( (char*)&h->generated, sizeof(char)*10,1,f );
	fwrite( (char*)&h->scannum, sizeof(char)*10,1,f );
	fwrite( (char*)&h->patient_id, sizeof(char)*10,1,f );
	fwrite( (char*)&h->exp_date, sizeof(char)*10,1,f );
	fwrite( (char*)&h->exp_time, sizeof(char)*10,1,f );
	fwrite( (char*)&h->un0, sizeof(char)*3,1,f );
	fwrite( (char*)&h->views, sizeof(int),1,f );
	fwrite( (char*)&h->vols_added, sizeof(int),1,f );
	fwrite( (char*)&h->start_field, sizeof(int),1,f );
	fwrite( (char*)&h->field_skip, sizeof(int),1,f );
	fwrite( (char*)&h->omax, sizeof(int),1,f );
	fwrite( (char*)&h->omin, sizeof(int),1,f );
	fwrite( (char*)&h->smax, sizeof(int),1,f );
	fwrite( (char*)&h->smin, sizeof(int),1,f );
    fclose(f);
    
    char imgfilename[1000];
    getimgfilename(fname,imgfilename);
	FILE* fp = fopen(imgfilename,"wb");
	if (!fp){
		printf("could not open file for saving.\n");
		return 0;
	}

    int SIZE = h->dim[1]*h->dim[2]*h->dim[3]*h->dim[4];
    switch(h->_datatype){
        case DT_UNSIGNED_CHAR:{
	        fwrite( (char*)data, sizeof(unsigned char), SIZE, fp);
            break;
        }
        case DT_SIGNED_SHORT:{
	        fwrite( (char*)data, sizeof(short), SIZE, fp);
            break;
        }
        case DT_SIGNED_INT:{
	        fwrite( (char*)data, sizeof(int), SIZE, fp);
            break;
        }
        case DT_FLOAT:{
	        fwrite( (char*)data, sizeof(float), SIZE, fp);
            break;
        }
        case DT_COMPLEX:{
	        fwrite( (char*)data, sizeof(float), SIZE*2, fp);
            break;
        }
        case DT_DOUBLE:{
	        fwrite( (char*)data, sizeof(double), SIZE, fp);
            break;
        }
        case DT_RGB:{
	        fwrite( (char*)data, sizeof(unsigned char), SIZE*3, fp);
            break;
        }
        case DT_BINARY: //...not yet implemented
        default: {
            //DT_ALL, DT_UNKNOWN, DT_NONE cases
            printf("Invalid DataType: %d\n",h->_datatype);
            fclose(fp); return false;
        }
    }
    fclose(fp);
    
	return 1;
}

#endif