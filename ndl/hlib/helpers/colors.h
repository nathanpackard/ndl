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

#ifndef COLORS_H
#define COLORS_H

float COLORS_CLAMP(float x, float l, float h){
    return (x > h) ? h : ((x < l) ? l : x);
}
//========================================================
//transfercolors
//========================================================
unsigned char redcolortable[768];
unsigned char greencolortable[768];
unsigned char bluecolortable[768];
unsigned char markupcolortable[768];
unsigned char hotcolortable[768];
unsigned char coldhotcolortable[768];

//setup color swatch with diverse colors for use in graphs/etc
const unsigned char color_darkred[3] = { 100, 0, 0 };
const unsigned char color_yellow[3] = { 255, 255, 0 };
const unsigned char randomswatch[48] = { 255, 0, 0, 
                                            0, 255, 0, 
                                            0, 0, 255,
                                            0, 255, 255, 
                                            255, 255, 0, 
                                            255, 0, 255, 
                                            71, 37, 0, 
                                            37, 54, 0, 
                                            153, 0, 0, 
                                            51, 51, 51, 
                                            102, 51, 17, 
                                            51, 85, 0, 
                                            221, 17, 17, 
                                            102, 85, 85, 
                                            170, 51, 34, 
                                            68, 136, 16 };

void setlinearcolormap(unsigned char* data,int start,int end,float rscale=1,float rbias=0,float gscale=1,float gbias=0,float bscale=1,float bbias=0){
    for (int i=start;i<=end;i++){
        float r = i*rscale+rbias;
        float g = i*gscale+gbias;
        float b = i*bscale+bbias;
        data[i*3]=COLORS_CLAMP(r,0,255);
        data[i*3+1]=COLORS_CLAMP(g,0,255);
        data[i*3+2]=COLORS_CLAMP(b,0,255);
    }
}

void setupcolormaps(){
    //setup color maps
    setlinearcolormap(redcolortable,0,255,1,0,0,0,0,0);
    setlinearcolormap(greencolortable,0,255,0,0,1,0,0,0);
    setlinearcolormap(bluecolortable,0,255,0,0,0,0,1,0);

    //markup - gray, except last five are colors
    setlinearcolormap(markupcolortable,0,250);
    markupcolortable[251*3]=255; //red
    markupcolortable[251*3+1]=0;
    markupcolortable[251*3+2]=0;
    markupcolortable[252*3]=0; //green
    markupcolortable[252*3+1]=255;
    markupcolortable[252*3+2]=0;
    markupcolortable[253*3]=0; //blue
    markupcolortable[253*3+1]=0;
    markupcolortable[253*3+2]=255;
    markupcolortable[254*3]=255; //yellow
    markupcolortable[254*3+1]=255;
    markupcolortable[254*3+2]=0;
    markupcolortable[255*3]=255; //magenta
    markupcolortable[255*3+1]=0;
    markupcolortable[255*3+2]=255;
        
    //hot
    float rbias=0.0;
    float rscale=8.0/3.0;
    float gscale=8.0/3.0;
    float bscale=8.0/2.0;
    float gbias=-1*((255-rbias)/rscale)*gscale;
    float bbias=-1*((255-gbias)/gscale)*bscale;    
    setlinearcolormap(hotcolortable,0,255,rscale,rbias,gscale,gbias,bscale,bbias);
    
    //cold to hot
    rscale = 0; rbias = 0;
    gscale = 4.25; gbias = -256;
    bscale = 4.25; bbias = 0;
    setlinearcolormap(coldhotcolortable,1,120,rscale,rbias,gscale,gbias,bscale,bbias);
    rscale = 4.25; rbias = -512;
    gscale = -4.25; gbias = 1024;
    bscale = -4.25; bbias = 768;
    setlinearcolormap(coldhotcolortable,120,240,rscale,rbias,gscale,gbias,bscale,bbias);
    rscale = -4.25; rbias = 1280;
    gscale = 0; gbias = 0;
    bscale = 0; bbias = 0;
    setlinearcolormap(coldhotcolortable,240,255,rscale,rbias,gscale,gbias,bscale,bbias);
}

template<class T1,class T2>
void transfercolors(T1* inputdata,T2* outputdata,int npixels,int ninputcolors,int noutputcolors,bool splitinputcolorflag=false,bool splitoutputcolorflag=false,float* aarray=0,float* barray=0,T2* LUT=0,T2 clampmin=0,T2 clampmax=0,int inputcolorstartindex=0,int inputcolorendindex=-1,int outputcolorstartindex=0,int outputcolorendindex=-1){
    int inputindex,outputindex,dinput,doutput,dinput2,doutput2;
    float value = 0;
    float value2 = 0;
    float value3 = 0;
    bool doscalebias=false;
    bool doclamp=false;
    if (aarray || barray) doscalebias=true;
    if (clampmin<clampmax) doclamp=true;
    if (inputcolorendindex==-1) inputcolorendindex+=ninputcolors;
    if (outputcolorendindex==-1) outputcolorendindex+=noutputcolors;
    int outputrange = outputcolorendindex - outputcolorstartindex + 1;
    int inputrange = inputcolorendindex - inputcolorstartindex + 1;
    if (splitinputcolorflag){ inputindex = npixels*inputcolorstartindex; dinput=1; dinput2 = npixels; } else { inputindex=inputcolorstartindex; dinput=ninputcolors; dinput2 = 1;}
    if (splitoutputcolorflag){ outputindex = npixels*outputcolorstartindex; doutput = 1; doutput2 = npixels; } else { outputindex=outputcolorstartindex; doutput = noutputcolors;  doutput2 = 1;}

    //HANDLE A BUNCH OF SPECIAL CASES (OPTIMIZED FOR SPEED), OTHERWISE HANDLE THE GENERAL CASE
    if (outputrange==1){
        for(int c=0;c<npixels;c++){
            if (doscalebias) value = aarray[outputcolorstartindex]*inputdata[inputindex]+barray[outputcolorstartindex]; else value = inputdata[inputindex];
            if (doclamp) value = COLORS_CLAMP(value,clampmin,clampmax);
            if (LUT) outputdata[outputindex] = LUT[(unsigned int)value+(unsigned int)value+(unsigned int)value]; else outputdata[outputindex] = value;
            inputindex+=dinput;
            outputindex+=doutput;
        }
    } else
    if (outputrange==2 && inputrange==1){
        for(int c=0;c<npixels;c++){
            if (doscalebias) value = aarray[0+outputcolorstartindex]*inputdata[inputindex]+barray[0+outputcolorstartindex]; else value = inputdata[inputindex];
            if (doclamp) value = COLORS_CLAMP(value,clampmin,clampmax);
            if (LUT){
                unsigned int lutindex = (unsigned int)value+(unsigned int)value+(unsigned int)value;
                outputdata[outputindex] = LUT[lutindex]; 
                outputdata[outputindex+doutput2] = LUT[lutindex+1]; 
            } else {
                outputdata[outputindex] = value;
                outputdata[outputindex+doutput2] = value;
            }
            inputindex+=dinput;
            outputindex+=doutput;
        }
    } else
    if (outputrange==2 && inputrange>=2){
        for(int c=0;c<npixels;c++){
            if (doscalebias){
                value = aarray[outputcolorstartindex]*inputdata[inputindex]+barray[outputcolorstartindex]; 
                value2 = aarray[1+outputcolorstartindex]*inputdata[inputindex+dinput2]+barray[1+outputcolorstartindex]; 
            } else {
                value = inputdata[inputindex];
                value2 = inputdata[inputindex+dinput2];
            }
            if (doclamp){
                value = COLORS_CLAMP(value,clampmin,clampmax);
                value2 = COLORS_CLAMP(value2,clampmin,clampmax);
            }
            if (LUT){
                outputdata[outputindex] = LUT[(unsigned int)value+(unsigned int)value+(unsigned int)value]; 
                outputdata[outputindex+doutput2] = LUT[(unsigned int)value2+(unsigned int)value2+(unsigned int)value2+1]; 
            } else {
                outputdata[outputindex] = value;
                outputdata[outputindex+doutput2] = value2;
            }
            inputindex+=dinput;
            outputindex+=doutput;
        }
    } else
    if (outputrange==3 && inputrange==1){
        for(int c=0;c<npixels;c++){
            if (doscalebias) value = aarray[outputcolorstartindex]*inputdata[inputindex]+barray[outputcolorstartindex]; else value = inputdata[inputindex];
            if (doclamp) value = COLORS_CLAMP(value,clampmin,clampmax);
            if (LUT){
                unsigned int lutindex = (unsigned int)value+(unsigned int)value+(unsigned int)value;
                outputdata[outputindex] = LUT[lutindex]; 
                outputdata[outputindex+doutput2] = LUT[lutindex+1]; 
                outputdata[outputindex+doutput2+doutput2] = LUT[lutindex+2]; 
            } else {
                outputdata[outputindex] = value;
                outputdata[outputindex+doutput2] = value;
                outputdata[outputindex+doutput2+doutput2] = value;
            }
            inputindex+=dinput;
            outputindex+=doutput;
        }
    } else
    if (outputrange==3 && inputrange==2){
        for(int c=0;c<npixels;c++){
            if (doscalebias){
                value = aarray[outputcolorstartindex]*inputdata[inputindex]+barray[outputcolorstartindex]; 
                value2 = aarray[1+outputcolorstartindex]*inputdata[inputindex+dinput2]+barray[1+outputcolorstartindex]; 
            } else {
                value = inputdata[inputindex];
                value2 = inputdata[inputindex+dinput2];
            }
            if (doclamp){
                value = COLORS_CLAMP(value,clampmin,clampmax);
                value2 = COLORS_CLAMP(value2,clampmin,clampmax);
            }
            if (LUT){
                outputdata[outputindex] = LUT[(unsigned int)value+(unsigned int)value+(unsigned int)value]; 
                outputdata[outputindex+doutput2] = LUT[(unsigned int)value2+(unsigned int)value2+(unsigned int)value2+1]; 
                outputdata[outputindex+doutput2+doutput2] = LUT[(unsigned int)value2+(unsigned int)value2+(unsigned int)value2+2]; 
            } else {
                outputdata[outputindex] = value;
                outputdata[outputindex+doutput2] = value2;
                outputdata[outputindex+doutput2+doutput2] = value2;
            }
            inputindex+=dinput;
            outputindex+=doutput;
        }
    } else
    if (outputrange==3 && inputrange>=3){
        for(int c=0;c<npixels;c++){
            if (doscalebias){
                value = aarray[outputcolorstartindex]*inputdata[inputindex]+barray[outputcolorstartindex]; 
                value2 = aarray[1+outputcolorstartindex]*inputdata[inputindex+dinput2]+barray[1+outputcolorstartindex]; 
                value3 = aarray[2+outputcolorstartindex]*inputdata[inputindex+dinput2+dinput2]+barray[2+outputcolorstartindex]; 
            } else {
                value = inputdata[inputindex];
                value2 = inputdata[inputindex+dinput2];
                value3 = inputdata[inputindex+dinput2+dinput2];
            }
            if (doclamp){
                value = COLORS_CLAMP(value,clampmin,clampmax);
                value2 = COLORS_CLAMP(value2,clampmin,clampmax);
                value3 = COLORS_CLAMP(value3,clampmin,clampmax);
            }
            if (LUT){
                outputdata[outputindex] = LUT[(unsigned int)value+(unsigned int)value+(unsigned int)value]; 
                outputdata[outputindex+doutput2] = LUT[(unsigned int)value2+(unsigned int)value2+(unsigned int)value2+1]; 
                outputdata[outputindex+doutput2+doutput2] = LUT[(unsigned int)value3+(unsigned int)value3+(unsigned int)value3+2]; 
            } else {
                outputdata[outputindex] = value;
                outputdata[outputindex+doutput2] = value2;
                outputdata[outputindex+doutput2+doutput2] = value3;
            }
            inputindex+=dinput;
            outputindex+=doutput;
        }
    } else 
    //GENERAL CASE
    for(int c=0;c<npixels;c++){
        int idelta = 0;
        int odelta = 0;
        for(unsigned int i=0;i<outputrange;i++){
            if (i<inputrange){
                if (doscalebias) value = aarray[i+outputcolorstartindex]*inputdata[inputindex+idelta]+barray[i+outputcolorstartindex]; else value = inputdata[inputindex+idelta];
                if (doclamp) value = COLORS_CLAMP(value,clampmin,clampmax);
                idelta+=dinput2;
            }
            if (LUT) outputdata[outputindex+odelta] = LUT[(unsigned int)value+(unsigned int)value+(unsigned int)value+i];
            else outputdata[outputindex+odelta] = value;
            odelta+=doutput2;
        }
        inputindex+=dinput;
        outputindex+=doutput;
    }
}

#endif