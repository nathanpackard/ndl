/* prepostdemo.c */
#include <windows.h>
#include<iostream>

class runfirst{
  public:
    runfirst(){ MessageBox(NULL, TEXT("RUNFIRST!"), TEXT("Hello"), 0); }
} IPC_RUNFIRST;

int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,PSTR szCmdLine,int iCmdShow){
   MessageBox(NULL, TEXT("Hello, Main Program!"), TEXT("Hello"), 0);
   return 0;
}
