/* ////////////////////////////////////////////////////////////

YS Bitmap / YS PNG library
Copyright (c) 2014 Soji Yamakawa.  All rights reserved.
http://www.ysflight.com

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation 
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS 
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT 
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: decodersample.cpp
//////////////////////////////////////////////////////////// */

// Compiled as :
// cl /DWIN32 decodersample.cpp ..\src\yspng.cpp kernel32.lib user32.lib gdi32.lib advapi32.lib opengl32.lib glu32.lib glaux.lib


#ifdef WIN32
#include <windows.h>
#else
#define CALLBACK
#endif


#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glaux.h>

#include <stdio.h>

#include "yspng.h"


static int winWid,winHei;
YsRawPngDecoder png;

extern "C" void CALLBACK DisplayFunc(void)
{
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	if(png.wid>0 && png.hei>0 && png.rgba!=NULL)
	{
		if(png.hei<=winHei)
		{
			glRasterPos2i(1,png.hei-1);
		}
		else
		{
			glRasterPos2i(1,winHei-1);
		}
		glDrawPixels(png.wid,png.hei,GL_RGBA,GL_UNSIGNED_BYTE,png.rgba);
		auxSwapBuffers();
	}
}

extern "C" void CALLBACK ReshapeFunc(GLsizei x,GLsizei y)
{
	winWid=x;
	winHei=y;
	DisplayFunc();
}

int main(int ac,char *av[])
{
	png.Decode(av[1]);
	png.Flip();   // Need to flip upside down because glDrawPixels draws y=0 bottom.

	auxInitDisplayMode(AUX_DOUBLE|AUX_RGB);
	auxInitPosition(0,0,png.wid+2,png.hei+2);
	auxInitWindow(NULL);

	winWid=png.wid;
	winHei=png.hei;


	glViewport(0,0,png.wid,png.hei);

	int viewport[4];
	glGetIntegerv(GL_VIEWPORT,viewport);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0,(double)viewport[2],(double)viewport[3],0.0,-1.0,1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);

	auxReshapeFunc(ReshapeFunc);
	auxMainLoop(DisplayFunc);

	return 0;
}
