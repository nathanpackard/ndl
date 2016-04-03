#ifndef IPC
#define IPC

#ifdef _WIN32
#include <stdio.h>
#include <cstring>
#include <Windows.h>
#include <Winternl.h>
#include <Winuser.h>
#undef max
#undef min

//from the CRT (actual function are sect_attribs.h and crt0dat.c
//This code was rewritten and paraphrased to prevent problems with 
//copyright and licensing
#pragma section(".CRTMP$XCA", long, read)
#pragma section(".CRTMP$XCZ", long, read)
#pragma section(".CRTMP$XIA", long, read)
#pragma section(".CRTMP$XIZ", long, read)
#pragma section(".CRTMA$XCA", long, read)
#pragma section(".CRTMA$XCZ", long, read)
#pragma section(".CRTMA$XIA", long, read)
#pragma section(".CRTMA$XIZ", long, read)
#pragma section(".CRTVT$XCA", long, read)
#pragma section(".CRTVT$XCZ", long, read)
#pragma section(".CRT$XCA", long, read)
#pragma section(".CRT$XCAA", long, read)
#pragma section(".CRT$XCC", long, read)
#pragma section(".CRT$XCZ", long, read)
#pragma section(".CRT$XDA", long, read)
#pragma section(".CRT$XDC", long, read)
#pragma section(".CRT$XDZ", long, read)
#pragma section(".CRT$XIA", long, read)
#pragma section(".CRT$XIAA", long, read)
#pragma section(".CRT$XIC", long, read)
#pragma section(".CRT$XID", long, read)
#pragma section(".CRT$XIY", long, read)
#pragma section(".CRT$XIZ", long, read)
#pragma section(".CRT$XLA", long, read)
#pragma section(".CRT$XLC", long, read)
#pragma section(".CRT$XLD", long, read)
#pragma section(".CRT$XLZ", long, read)
#pragma section(".CRT$XPA", long, read)
#pragma section(".CRT$XPX", long, read)
#pragma section(".CRT$XPXA", long, read)
#pragma section(".CRT$XPZ", long, read)
#pragma section(".CRT$XTA", long, read)
#pragma section(".CRT$XTB", long, read)
#pragma section(".CRT$XTX", long, read)
#pragma section(".CRT$XTZ", long, read)
#pragma section(".rdata$T", long, read)
#pragma section(".rtc$IAA", long, read)
#pragma section(".rtc$IZZ", long, read)
#pragma section(".rtc$TAA", long, read)
#pragma section(".rtc$TZZ", long, read)

typedef void (*_PVFV)(void);
void _initterm(_PVFV *begin, _PVFV *end){
   _PVFV *cur;
   for (cur = begin; cur < end; cur++){
      if (*cur) (**cur)();
   }
}

#pragma data_seg(".CRT$XIA")
_PVFV __xi_a[1] = {NULL};
#pragma data_seg(".CRT$XIZ")
_PVFV __xi_z[1] = {NULL};
#pragma data_seg(".CRT$XCA")
_PVFV __xc_a[1] = {NULL};
#pragma data_seg(".CRT$XCZ")
_PVFV __xc_z[1] = {NULL};
#pragma data_seg(".CRT$XPA")
_PVFV __xp_a[1] = {NULL};
#pragma data_seg(".CRT$XPZ")
_PVFV __xp_z[1] = {NULL};
#pragma data_seg(".CRT$XTA")
_PVFV __xt_a[1] = {NULL};
#pragma data_seg(".CRT$XTZ")
_PVFV __xt_z[1] = {NULL};
#pragma data_seg()

//callback function used in activateprocess
BOOL CALLBACK ActivateEnum(HWND hwnd, LPARAM lParam){
    DWORD ThisProcessID = 0;
    DWORD* ProcessID = (DWORD*)lParam;
    GetWindowThreadProcessId(hwnd, &ThisProcessID);
    if(ThisProcessID == *ProcessID){
        SetForegroundWindow(hwnd);
    }
    return true;
};

int ipc_redirectpoint(){
    _initterm(__xi_a, __xi_z);    /* C Initialization */
    _initterm(__xc_a, __xc_z);    /* C++ Initialization */
    
    //parse out the message
    char* str = new char[strlen(GetCommandLine())+1];
    strcpy(str,GetCommandLine());
    char delims[] = ",";
    char *result = NULL;
    
    //check to see if we are a launched process (should always be true)
    result = strtok( str, delims );
    if (strcmp(result,"IPC_LAUNCHPROCESS")!=0){
        printf("ERROR, NOT IPC PROCESS\n");
        exit(0);
    }
    
    //get the function pointer
    result = strtok( NULL, delims );
    long long addr;
    if(EOF == sscanf(result, "%lld", &addr)){
        printf("Error getting function pointer!\n");
        exit(0);
    }
    //printf("function addr: %lld\n",addr);
    
    //get the message
    result = strtok( NULL, delims );
    //printf("message: %s\n",result);
    
    //start the function
    int (*startfunction)(char*);
    
    startfunction = static_cast<int (*)(char* )>((void*)addr);
    startfunction(result);
   
    //cleanup
    delete [] str;
    
    _initterm(__xp_a, __xp_z);    /* Pre-termination (C++?) */
    _initterm(__xt_a, __xt_z);    /* Termination */    
    
    exit(0);
}

//*************************
// Actual Functions
//*************************
int waitforprocess(int pid){
    HANDLE myHandle = OpenProcess(SYNCHRONIZE,TRUE,pid);
    int exitcode = WaitForSingleObject(myHandle, INFINITE);
    CloseHandle(myHandle);
    return exitcode;
}

int isprocessrunning(int pid){
    HANDLE myHandle = OpenProcess(SYNCHRONIZE,TRUE,pid);
    int exitcode = WaitForSingleObject(myHandle, 0);
    CloseHandle(myHandle);
    return (exitcode==WAIT_TIMEOUT);
}

int activateprocess(int pid){
    DWORD ProcessID = pid;
    int retval = EnumWindows(ActivateEnum, (LPARAM)((void*)&ProcessID));
    return retval;
}

int isactiveprocess(int pid){
    HWND hwnd = GetForegroundWindow();
    DWORD ThisProcessID = 0;
    GetWindowThreadProcessId(hwnd, &ThisProcessID);
    return (ThisProcessID==pid);
}

int launchprocess(int (*startfunction)(char* message),char* message=0){
    //CreateProcess API initialization
    STARTUPINFO siStartupInfo;
    PROCESS_INFORMATION piProcessInfo;
    memset(&siStartupInfo, 0, sizeof(siStartupInfo));
    memset(&piProcessInfo, 0, sizeof(piProcessInfo));
    siStartupInfo.cb = sizeof(siStartupInfo); 
    
    //create a suspended process of the current process
    char textbuffer[5000]; //buffer to hold filename and path of current process
    char commandlinebuffer[5000]; 
    sprintf(commandlinebuffer,"IPC_LAUNCHPROCESS,%lld,%s",(DWORD64)startfunction,message);
    GetModuleFileName(0,textbuffer,sizeof(textbuffer));
    CreateProcess(textbuffer,commandlinebuffer,0,0, false, CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED, 0, 0, &siStartupInfo, &piProcessInfo);

    //change the entry point
    CONTEXT tContext;
    tContext.ContextFlags = CONTEXT_FULL;
    GetThreadContext(piProcessInfo.hThread,&tContext);
    
    #ifdef _WIN64
    tContext.Rcx=(DWORD64)ipc_redirectpoint;
    //printf("IPC: %lld\n",(long long)startfunction);
    #else
    tContext.Eax=(DWORD32)ipc_redirectpoint;
    //printf("IPC: %d\n",(int)startfunction);
    #endif
    
    SetThreadContext(piProcessInfo.hThread,&tContext);
    
    //resume the thread
    ResumeThread(piProcessInfo.hThread);
        
    //close handles (dwProcessId, dwThreadId)
    CloseHandle(piProcessInfo.hProcess);
    CloseHandle(piProcessInfo.hThread);
    
    return piProcessInfo.dwProcessId;
}

#else

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

char IPC_MESSAGE[5000];
#define IPC_INITCODE {  }
#define IPC_CLEANUPCODE {  }

int waitforprocess(int pid){
    int exitcode;
    waitpid(pID,&exitcode);
    return exitcode;
}

int isprocessrunning(int pid){
    //int exitcode;
    //NEEDS TO BE IMPLEMENTED!
    //return (exitcode==WAIT_TIMEOUT);
}

int activateprocess(int pid){
    //NEEDS TO BE IMPLEMENTED!!
    return 1;
}

int isactiveprocess(int pid){
    //NEEDS TO BE IMPLEMENTED
    return 1;
}

int launchprocess(int (*startfunction)(char* message),char* message=0){
   sprintf(IPC_MESSAGE,message);
   pid_t pID = fork();
   if (pID == 0){
      // Code only executed by child process
       exit(startfunction());
    } else if (pID>0) {
      // Code only executed by parent process
      return pID;
    }
    printf("Failed to fork\n")s;
    exit(1);
}

#endif
#endif