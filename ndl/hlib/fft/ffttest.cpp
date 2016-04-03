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

#define SIZE 16
#include "fft.h"
#include <stdio.h>
#include <time.h>
#include <algorithm>

#define MYTYPE float

void testcomplex(){
    printf("\nTESTCOMPLEX");
    FFTOBJ<MYTYPE> fftobj(SIZE);
    MYTYPE* input = fftobj.fft_input;
    MYTYPE* output = fftobj.fft_output;
    for(int i=0;i<SIZE;i++){
        input[i*2] = std::min(i,10);
        input[i*2+1] = 0;
    }
    printf("\n========================\nORIGINAL\n========================\n");
    for(int i=0;i<SIZE;i++) printf("%f + %fi\n",input[i*2],input[i*2+1]);
    clock_t start = clock();
    for(int i=0;i<768;i++){
        fftobj.fft(SIZE);
    }
    printf("\n========================\nFREQ\n========================\n");
    for(int i=0;i<SIZE;i++) printf("%f + %fi\n",output[i*2],output[i*2+1]);
    fftobj.swapinputoutput();
    fftobj.ifft(SIZE);
    double ellapsed = double(clock()-start)/double(CLOCKS_PER_SEC);
    printf("\n========================\nAND BACK\n========================\n");
    for(int i=0;i<SIZE;i++) printf("%f + %fi\n",input[i*2],input[i*2+1]);
    printf ("==========================\n Ellapsed time: %f sec\n", ellapsed );
}

void testreal(){
    printf("\nTESTREAL");
    FFTOBJ<MYTYPE> fftobj(SIZE);
    MYTYPE* input = fftobj.fft_input;
    MYTYPE* output = fftobj.fft_output;
    for(int i=0;i<SIZE;i++) input[i] = std::min(i,10);
    printf("\n========================\nORIGINAL\n========================\n");
    for(int i=0;i<SIZE;i++) printf("%f\n",input[i]);
    clock_t start = clock();
    for(int i=0;i<768;i++){
        fftobj.realfft(SIZE);
    }
    printf("\n========================\nFREQ\n========================\n");
    for(int i=0;i<SIZE;i++) printf("%f + %fi\n",output[i*2],output[i*2+1]);
    fftobj.swapinputoutput();
    fftobj.realifft(SIZE);
    double ellapsed = double(clock()-start)/double(CLOCKS_PER_SEC);
    printf("\n========================\nAND BACK\n========================\n");
    for(int i=0;i<SIZE;i++) printf("%f\n",input[i]);
    printf ("==========================\n Ellapsed time: %f sec\n", ellapsed );
}



int main(){
    testcomplex();
    testreal();
}