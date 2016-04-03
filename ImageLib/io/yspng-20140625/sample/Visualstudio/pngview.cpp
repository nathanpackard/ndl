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

File Name: pngview.cpp
//////////////////////////////////////////////////////////// */

// cl simple.cpp user32.lib kernel32.lib gdi32.lib

#include <windows.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include <direct.h>

#include <yspng.h>

#include "resource.h"

/********************************************************************/
static int OpenWindow(int lupX,int lupY,int sizX,int sizY);
static int CloseWindow(void);
static LRESULT CALLBACK WindowCallBack(HWND wnd,UINT msg,WPARAM w,LPARAM l);
static void MainLoop(void);
static void FileOpen(void);
static void LoadPng(const char fn[]);
/********************************************************************/

static HWND hWnd=NULL;
static HDC bmpDc=NULL;
static HBITMAP hBmp=NULL;
static int bmpWid,bmpHei;
static int QuitFlag=0;
static int msx,msy;

static int OpenWindow(int lupX,int lupY,int sizX,int sizY)
{
	HMENU hMenu;
	WNDCLASS cls;
	const char *className="PNGViewerWindow";
	const char *windowName="YS PNG Viewer";
	HINSTANCE inst;

	inst=GetModuleHandle(NULL);

	hMenu=LoadMenu(inst,MAKEINTRESOURCE(IDR_PNGVIEWMENU));

	cls.style=CS_OWNDC|CS_BYTEALIGNWINDOW;
	cls.lpfnWndProc=WindowCallBack;
	cls.cbClsExtra=0;
	cls.cbWndExtra=0;
	cls.hInstance=inst;
	cls.hIcon=LoadIcon(inst,MAKEINTRESOURCE(IDI_CONCORDEICON));
	cls.hCursor=LoadCursor(inst,IDC_ARROW);
	cls.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH);
	cls.lpszMenuName=NULL;
	cls.lpszClassName=className;
	if(RegisterClass(&cls))
	{
		RECT rc;
		rc.left=lupX;
		rc.top=lupY;
		rc.right=(unsigned long)(lupX+sizX-1);
		rc.bottom=(unsigned long)(lupY+sizY-1);
		AdjustWindowRect(&rc,WS_OVERLAPPEDWINDOW|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,FALSE);
		lupX=rc.left;
		lupY=rc.top;
		sizX=rc.right-rc.left+1;
		sizY=rc.bottom-rc.top+1;

		hWnd=CreateWindow
		   (className,
		    windowName,
		    WS_OVERLAPPEDWINDOW|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,
		    lupX,lupY,sizX,sizY,
		    NULL,hMenu,inst,NULL);
		if(hWnd!=NULL)
		{
			DragAcceptFiles(hWnd,TRUE);
			ShowWindow(hWnd,SW_SHOWNORMAL);
			return 0;
		}
	}
	return 1;
}

static int CloseWindow(void)
{
	DestroyWindow(hWnd);
	return 0;
}

LRESULT CALLBACK WindowCallBack(HWND hWnd,UINT msg,WPARAM w,LPARAM l)
{
	HDC hdc;

	switch(msg)
	{
	case WM_PAINT:
		hdc=GetDC(hWnd);
		if(bmpDc!=NULL && hBmp!=NULL)
		{
			HBITMAP hBmpSave;
			RECT rect;
			int x,y;

			hBmpSave=(HBITMAP)SelectObject(bmpDc,hBmp);
			GetClientRect(hWnd,&rect);

			for(y=0; y<rect.bottom; y+=bmpHei)
			{
				for(x=0; x<rect.right; x+=bmpWid)
				{
					BitBlt(hdc,x,y,bmpWid,bmpHei,bmpDc,0,0,SRCCOPY);
				}
			}

			SelectObject(bmpDc,hBmpSave);
		}
		ReleaseDC(hWnd,hdc);
		return DefWindowProc(hWnd,msg,w,l);
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MOUSEMOVE:
		msx=LOWORD(l);
		msy=HIWORD(l);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		QuitFlag=1;
		break;
	case WM_DROPFILES:
		{
			HDROP hDrop;
			unsigned nFile;
			char fn[256];

			hDrop=(HDROP)w;
			nFile=DragQueryFile(hDrop,0xffffffff,fn,256);
			if(nFile>0 && DragQueryFile(hDrop,0,fn,256)>0)
			{
				LoadPng(fn);
			}
		}
		break;
	case WM_COMMAND:
		if(w==ID_FILE_OPENPNG)
		{
			FileOpen();
		}
		return DefWindowProc(hWnd,msg,w,l);
	default:
		return DefWindowProc(hWnd,msg,w,l);
	}
	return 0L;
}

static void MainLoop(void)
{
	MSG msg;
	while(QuitFlag==0)
	{
		while(PeekMessage(&msg,0,0,0,PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

static int LoadFileDialog(char fn[],HWND hWndPar)
{
	int result;
	OPENFILENAME para;
	char coffee[256];
	char curpath[256];
	HINSTANCE module;

	char *beer="*.png";
	char *burger="(*.png)\0*.png\0\0";

	module=GetModuleHandle(NULL);

	para.lStructSize=sizeof(OPENFILENAME);
	para.hwndOwner=hWndPar;
	para.hInstance=module;
	para.lpstrFilter=burger;
	para.lpstrCustomFilter=NULL;
	para.nFilterIndex=0;

	_getcwd(curpath,256);
	coffee[0]=0;
	para.lpstrFile=coffee;
	para.lpstrInitialDir=curpath;

	para.nMaxFile=sizeof(coffee);
	para.lpstrFileTitle=NULL;
	para.lpstrTitle="Open PNG";
	para.Flags=OFN_HIDEREADONLY;
	para.lpstrDefExt=beer+2;

	result=GetOpenFileName(&para);
	_chdir(curpath);

	if(result!=0)
	{
		strcpy(fn,coffee);
	}
	return result;
}

static void FileOpen(void)
{
	char fn[MAX_PATH];
	if(LoadFileDialog(fn,hWnd))
	{
		LoadPng(fn);
	}
}

static void LoadPng(const char fn[])
{
	YsRawPngDecoder pngDecoder;
	pngDecoder.Initialize();
	if(pngDecoder.Decode(fn)==YSOK)
	{
		BITMAPINFOHEADER bmiHeader;
		void *bitBuf;

		if(bmpDc==NULL)
		{
			bmpDc=CreateCompatibleDC(GetDC(hWnd));
		}

		if(hBmp!=NULL)
		{
			DeleteObject(hBmp);
			hBmp=NULL;
		}

		bmpWid=(pngDecoder.wid+3)&(~3);
		bmpHei=pngDecoder.hei;

		bmiHeader.biSize=sizeof(bmiHeader);
		bmiHeader.biWidth=bmpWid;
		bmiHeader.biHeight=-bmpHei;
		bmiHeader.biPlanes=1;
		bmiHeader.biBitCount=24;
		bmiHeader.biCompression=BI_RGB;
		bmiHeader.biSizeImage=0;
		bmiHeader.biXPelsPerMeter=2048;
		bmiHeader.biYPelsPerMeter=2048;
		bmiHeader.biClrUsed=0;
		bmiHeader.biClrImportant=0;

		hBmp=CreateDIBSection(bmpDc,(BITMAPINFO *)&bmiHeader,DIB_RGB_COLORS,&bitBuf,NULL,0);

		int x,y;
		unsigned char *buf;
		buf=(unsigned char *)bitBuf;
		for(y=0; y<bmpHei; y++)
		{
			for(x=0; x<bmpWid; x++)
			{
				const unsigned char *rgba;
				if(x<pngDecoder.wid)
				{
					rgba=pngDecoder.rgba+(y*pngDecoder.wid+x)*4;
				}
				else
				{
					rgba=pngDecoder.rgba+(y*pngDecoder.wid+pngDecoder.wid-1)*4;
				}
				buf[(y*bmpWid+x)*3  ]=rgba[2];
				buf[(y*bmpWid+x)*3+1]=rgba[1];
				buf[(y*bmpWid+x)*3+2]=rgba[0];
			}
		}

		InvalidateRect(hWnd,NULL,FALSE);
	}
}

int main(int ac,char *av[])
{
	QuitFlag=0;
	if(OpenWindow(100,100,640,480)==0)
	{
		if(ac==2)
		{
			LoadPng(av[1]);
		}
		else
		{
			FileOpen();
		}

		MainLoop();
		CloseWindow();
	}
	return 0L;
}

int PASCAL WinMain(HINSTANCE inst,HINSTANCE dumb,LPSTR param,int show)
{
	int ac;
	char *av[2],prog[MAX_PATH];

	GetModuleFileName(inst,prog,260);
	av[0]=prog;

	if(param==NULL || param[0]==0)
	{
		ac=1;
	}
	else
	{
		ac=2;
		av[1]=param;
	}
	return main(ac,av);
}
