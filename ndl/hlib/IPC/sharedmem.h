#ifndef SHAREMEM
#define SHAREMEM

#include "shared_memory_queue.h"
#include <map>
#include <string>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

//***********************
// VARIABLES
//***********************
std::map<void*, XSharedMemoryQueue*> sharedmem_queuearray;
std::map<void*, std::string> sharedmem_namearray;
unsigned long long sharedmem_nextname = 0;

//***********************
// Master Process Functions
//***********************
void getuniquename(char* str){
    #ifdef _WIN32
    int pid = GetCurrentProcessId();
    #else
    int pid = getpid();
    #endif
    
    std::stringstream out;
    out << pid;
    out << "_";
    out << sharedmem_nextname++;
    strcpy(str,out.str().c_str());
}

char* get_sharedmemoryname(void* data){
    if (sharedmem_namearray.find(data) != sharedmem_namearray.end()) return (char*)sharedmem_namearray[data].c_str();
    else return 0;
}

void* allocate_sharedmemory(size_t size){
    //create name for shared memory
    char name[100];
    XSharedMemoryQueue* pShareMemBlock;
    int numtries=1000;
    void* data = 0;
    while(numtries--){
        pShareMemBlock= new XSharedMemoryQueue();
        getuniquename(name);
        //create the shared memory with that name
        bool ret = pShareMemBlock->Initialize(name, size, 1, 1);
        if (!ret){
            delete pShareMemBlock; continue; 
        }
        data = pShareMemBlock->CreateSharedMemoryQueue();
        if (!data){
            delete pShareMemBlock; continue; 
        }
        break;
    }
    if (data==0) return 0;
    
    //save stuff needed for cleanup later
    sharedmem_queuearray[data] = pShareMemBlock;
    sharedmem_namearray[data] = name;
    
    //return new data
    return data;
}

void delete_sharedmemory(void* data){
	if (sharedmem_queuearray.find(data) != sharedmem_queuearray.end()){
        XSharedMemoryQueue* thequeue = sharedmem_queuearray[data];
        thequeue->DeleteSharedMemoryQueue();
        sharedmem_queuearray.erase(data);
        sharedmem_namearray.erase(data);
        delete thequeue;
    }
}

//***********************
// Slave Process Functions
//***********************
void* open_sharedmemory(char* name,size_t size){
    //open the shared memory with that name
    XSharedMemoryQueue* pShareMemBlock = new XSharedMemoryQueue();
    bool ret = pShareMemBlock->Initialize(name, size, 1, 1);
    if (!ret){ delete pShareMemBlock; return 0; }
    void* data = pShareMemBlock->OpenSharedMemoryQueue();
    if (!data){ delete pShareMemBlock; return 0; }
    return data;
}

void close_sharedmemory(void* data){
	if (sharedmem_queuearray.find(data) != sharedmem_queuearray.end()){
        XSharedMemoryQueue* thequeue = sharedmem_queuearray[data];
        thequeue->CloseSharedMemoryQueue();
        sharedmem_queuearray.erase(data);
        sharedmem_namearray.erase(data);
        delete thequeue;
    }
}

#endif