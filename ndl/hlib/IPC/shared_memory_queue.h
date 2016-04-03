/* ==============================================================================================================================
 * This notice must be untouched at all times.
 *
 * Copyright  IntelliWizard Inc. 
 * All rights reserved.
 * LICENSE: LGPL. 
 * Redistributions of source code modifications must send back to the Intelliwizard Project and republish them. 
 * Web: http://www.intelliwizard.com
 * eMail: info@intelliwizard.com
 * We provide technical supports for UML StateWizard users.
 * ==============================================================================================================================*/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#ifndef SharedMemoryQueue_h
#define SharedMemoryQueue_h 1

#ifdef _WIN32
#	include <windows.h>
	typedef HANDLE shared_memory_t;

#else
	typedef int shared_memory_t;

	#include <string.h>
	#include <unistd.h>
	#include <pthread.h>
	#include <signal.h>
	#include <sys/time.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <linux/unistd.h>

#endif

#ifndef BOOL
	typedef int  BOOL;
#endif
#ifndef FALSE
	#define FALSE	0
#endif
#ifndef TRUE
	#define TRUE	1
#endif
#ifndef NULL
	#define NULL    0
#endif

#define SMESTR_APPLICATION_USER "root"  // The Linux system user name who run this application.

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#ifdef USES_MMAP
#include <sys/mman.h>
#else
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
#endif

void SleepInMilliseconds(int msec)
{
#ifdef _WIN32
	Sleep(msec);
#else
	usleep(msec * 1000);	// usleep wait in microseconds, not milliseconds
#endif
}

class XSharedMemoryQueue 
{
  public:
    XSharedMemoryQueue()
      : m_strMapName(0),
        m_iNodeSize(0),
        m_iNodeCount(0),
        m_iQueueCount(0),
        m_pQueueArray(0),
        m_hMapMem(0),
        m_pMappedPointer(0)
    {}
  
    ~XSharedMemoryQueue(){
        if(m_strMapName)
        {
            free(m_strMapName);
            m_strMapName = 0;
        }
    }

    bool Initialize (const char* strMapName, int iNodeSize, int iNodeCount, int iQueueCount){
        if(strMapName==0||iNodeSize<=0||iNodeCount<=0||iQueueCount<=0)
            return false;
        m_strMapName = strdup(strMapName);
        m_iNodeSize = iNodeSize + 2*sizeof(int); // Node, Prevous Node Pointer, Next Node Pointer
        m_iNodeCount = iNodeCount;
        m_iQueueCount = iQueueCount;
        if(m_strMapName==0){
            Release();
            return false;
        }
        return true;
    }

    void Release (){
        m_iNodeSize = 0;
        m_iNodeCount = 0;
        m_iQueueCount = 0;
    }

    // Link all nodes together as a double linked list: queue 0.
    void* CreateSharedMemoryQueue (){
    #ifdef _WIN32
        
        //changed by Nathan Packard
        m_hMapMem = CreateFileMapping(INVALID_HANDLE_VALUE,NULL, PAGE_READWRITE, 0, m_iNodeSize*m_iNodeCount + m_iQueueCount*sizeof(int), m_strMapName);
        //m_hMapMem = CreateFileMapping(INVALID_HANDLE_VALUE,XAccessControl::getHighPrivSA(), PAGE_READWRITE, 0, m_iNodeSize*m_iNodeCount + m_iQueueCount*sizeof(int), m_strMapName);
        
        if (m_hMapMem == NULL||GetLastError()==ERROR_ALREADY_EXISTS) return 0;
        m_pMappedPointer = MapViewOfFile(m_hMapMem, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, m_iNodeSize*m_iNodeCount+m_iQueueCount*sizeof(int));
    #else
    #ifdef USES_MMAP
        m_hMapMem=open(m_strMapName, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
        if(m_hMapMem<0)
        {
            return 0;
        }
        m_pMappedPointer = mmap(0,
            m_iNodeSize*m_iNodeCount+m_iQueueCount*sizeof(int),
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            m_hMapMem,
            0);
    #else
    //	m_hMapMem=open(m_strMapName, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    //	if(m_hMapMem<0)
    //	{
    //		return 0;
    //	}
        key_t key=(key_t)GenerateKeyFromName(m_strMapName, 1);
        m_hMapMem=shmget(key,
            m_iNodeSize*m_iNodeCount+m_iQueueCount*sizeof(int),
            IPC_CREAT | IPC_EXCL | 0600);
        m_pMappedPointer=NULL;
        
        //assign the gid and uid
        if(getuid()==0)
        {
            // int getpwnam_r(const char *name, struct passwd *pwbuf,char *buf, size_t buflen, struct passwd **pwbufp);

            // The getpwnam_r() and getpwuid_r() functions obtain a structure containing the broken-out fields of the record in the password database, 
            // but store the retrieved passwd structure in the space pointed to by pwbuf. This passwd structure contains pointers to 
            // strings, and these strings are stored in the buffer buf of size buflen. A pointer to the result (in case of success) or 
            // NULL (in case no entry was found or an error occurred) is stored in *pwbufp.

            struct passwd pwd ;
            char pwd_str_buf[513]; 
            memset(&pwd_str_buf, 0, sizeof(pwd_str_buf));
            struct passwd *p_passwd=NULL;
            getpwnam_r(SMESTR_APPLICATION_USER, &pwd, pwd_str_buf, sizeof(pwd_str_buf)-1, &p_passwd);
            
            if(p_passwd)
            {
                shmid_ds sd;
                memset(&sd, 0, sizeof(sd));
                shmctl(m_hMapMem, IPC_STAT, &sd);
                sd.shm_perm.uid = p_passwd->pw_uid;
                sd.shm_perm.gid = p_passwd->pw_gid;
                shmctl(m_hMapMem, IPC_SET, &sd);
            }
        }
        
        int ret=(int)shmat(m_hMapMem, NULL, 0);
        // shmat returns -1 when failed
        if(ret!=-1)
            m_pMappedPointer=(void*)ret;
    #	endif
    #endif

        if (m_pMappedPointer == NULL)
            return 0;

        // Link all nodes together as a double linked list: queue 0.
        int i=0;
        for(i=0;i<m_iNodeCount;i++)
        {
            void* pNode = (char*)m_pMappedPointer+i*m_iNodeSize;
            int *pNext = (int*)((char*)pNode+m_iNodeSize-sizeof(int));
            int *pPrev = (int*)((char*)pNext-sizeof(int));

            if(i==m_iNodeCount-1)
                *pNext = 0;
            else
                *pNext = (i+1)*m_iNodeSize;
            if(i==0)
                *pPrev = (m_iNodeCount-1)*m_iNodeSize;
            else
                *pPrev = (i-1)*m_iNodeSize;
        }

        m_pQueueArray = (int*)((char*)m_pMappedPointer+m_iNodeSize*m_iNodeCount);
        for(i=0;i<m_iQueueCount;i++)
            m_pQueueArray[i] = -1;
        m_pQueueArray[0] = 0;

        return m_pMappedPointer;
    }    
    void DeleteSharedMemoryQueue (){
    #ifdef _WIN32
        if (m_pMappedPointer)
        {
            UnmapViewOfFile (m_pMappedPointer);
            m_pMappedPointer = NULL;
        }
        
        if (m_hMapMem)
        {
            CloseHandle (m_hMapMem);
            m_hMapMem = NULL;
        }
    #else
    #ifdef USES_MMAP
        munmap(m_pMappedPointer, m_iNodeSize*m_iNodeCount+m_iQueueCount*sizeof(int));
        close(m_hMapMem);
    #else
        shmid_ds sd;
        memset(&sd, 0, sizeof(sd));
        shmdt(m_pMappedPointer);
        shmctl(m_hMapMem, IPC_RMID, &sd); // Remove resources.
        close(m_hMapMem);
    #endif
        //unlink(m_strMapName);
    #endif
    }
    
    void* OpenSharedMemoryQueue (){
    #ifdef _WIN32
        m_hMapMem= OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, TRUE, m_strMapName);
        if (m_hMapMem == NULL){
            printf("ERROR: %d\n",GetLastError());
            return 0;
        }
        
        m_pMappedPointer = MapViewOfFile(m_hMapMem, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, m_iNodeSize*m_iNodeCount+m_iQueueCount*sizeof(int));
        if (m_pMappedPointer == NULL) return 0;
    #else
    #ifdef USES_MMAP
        // Open exist
        m_hMapMem=open(m_strMapName, O_RDWR);
        if(m_hMapMem<0)
        {
            return 0;
        }
        m_pMappedPointer = mmap(0,
            m_iNodeSize*m_iNodeCount+m_iQueueCount*sizeof(int),
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            m_hMapMem,
            0);
    #else
    //	m_hMapMem=open(m_strMapName, O_RDWR);
    //	if(m_hMapMem<0)
    //	{
    //		return 0;
    //	}
        key_t key=(key_t)GenerateKeyFromName(m_strMapName, 1);
        m_hMapMem=shmget(key,
            m_iNodeSize*m_iNodeCount+m_iQueueCount*sizeof(int),
            0600);
        m_pMappedPointer=NULL;
        int ret=(int)shmat(m_hMapMem, NULL, 0);
        // shmat returns -1 when failed
        if(ret!=-1)
            m_pMappedPointer=(void*)ret;
    #	endif
    #endif

        m_pQueueArray = (int*)((char*)m_pMappedPointer+m_iNodeSize*m_iNodeCount);
        return m_pMappedPointer;
    }
    
    void CloseSharedMemoryQueue (){
    #ifdef _WIN32
        if (m_pMappedPointer)
        {
            UnmapViewOfFile (m_pMappedPointer);
            m_pMappedPointer = NULL;
        }
        
        if (m_hMapMem)
        {
            CloseHandle (m_hMapMem);
            m_hMapMem = NULL;
        }
    #else
    #ifdef USES_MMAP
        munmap(m_pMappedPointer, m_iNodeSize*m_iNodeCount+m_iQueueCount*sizeof(int));
        close(m_hMapMem);
    #else
        shmdt(m_pMappedPointer);
        close(m_hMapMem);
    #endif
    #endif
    }
    

    void* GetAt(int iIndex){
        if(iIndex<0||iIndex>=m_iNodeCount) return 0;
        return (char*)m_pMappedPointer+m_iNodeSize*iIndex;
    }

    
    int GetIndex(void* pNode){
        return ((char*)pNode-(char*)m_pMappedPointer)/m_iNodeSize;
    }


    void* GetQueueHead(int iQueue){
        if(iQueue<0||iQueue>=m_iQueueCount) return 0;
        if(m_pQueueArray[iQueue]==-1) return 0;
        return (char*)m_pMappedPointer+m_pQueueArray[iQueue];
    }

    void* GetQueueTail(int iQueue){
        if(iQueue<0||iQueue>=m_iQueueCount) return 0;
        if(m_pQueueArray[iQueue]==-1) return 0;

        void* pNode = (char*)m_pMappedPointer+m_pQueueArray[iQueue];
        int *pNext = (int*)((char*)pNode+m_iNodeSize-sizeof(int));
        int *pPrev = (int*)((char*)pNext-sizeof(int));
        return (char*)m_pMappedPointer+*pPrev;
    }

    void* GetNext(void* pNode){
        if(pNode==0) return 0;
        int *pNext = (int*)((char*)pNode+m_iNodeSize-sizeof(int));
        int *pPrev = (int*)((char*)pNext-sizeof(int));
        return (char*)m_pMappedPointer+*pNext;
    }

    void* GetPrev(void* pNode){
        if(pNode==0) return 0;
        int *pNext = (int*)((char*)pNode+m_iNodeSize-sizeof(int));
        int *pPrev = (int*)((char*)pNext-sizeof(int));
        return (char*)m_pMappedPointer+*pPrev;
    }


    /**********************************************************************************************
    Remove the given node from the source queue and add this node to the destination queue.
    If the destination queue is empty, add the node as the head of the queue.
    If the destination queue is not empty, append the node to the destination queue. 

    The souce and destination queues are double linked lists. 
    **********************************************************************************************/
    bool MoveToDestQueueTail(void* pNode, int iDestQueue){
        if(iDestQueue<0||iDestQueue>=m_iQueueCount) return false;
        if(pNode==0) return false;

        //Check if the pNode is the first node of a queue
        int i=0;
        for(i=0;i<m_iQueueCount;i++)
        {
            if(m_pQueueArray[i]!=-1&&(char*)m_pMappedPointer+m_pQueueArray[i]==pNode)
                break;
        }

        void* pNewNode = pNode;
        int *pNodeNext = (int*)((char*)pNewNode+m_iNodeSize-sizeof(int));
        int *pNodePrev = (int*)((char*)pNodeNext-sizeof(int));

        if(i==m_iQueueCount)
        {
            // Not the first node of a queue, remove the node from the double linked quque.
            // Update the previous node' next node pointer
            *(int*)(((char*)m_pMappedPointer+*pNodePrev)+m_iNodeSize-sizeof(int))=*pNodeNext; 
            // Update the next node's previous node pointer 
            *(int*)(((char*)m_pMappedPointer+*pNodeNext)+m_iNodeSize-2*sizeof(int))=*pNodePrev; 
        }
        else
        {
            // The first node of a queue
            if(((char*)m_pMappedPointer+*pNodeNext)==pNewNode) // The node'next pointer is the itself.
            {
                //Only one node in the original queue, becomes an empty queue.
                m_pQueueArray[i] = -1;
            }
            else
            {
                //Not only one node in the orginal queue, remove the node from the double linked quque.
                m_pQueueArray[i] = *pNodeNext;
                *(int*)(((char*)m_pMappedPointer+*pNodePrev)+m_iNodeSize-sizeof(int))=*pNodeNext;
                *(int*)(((char*)m_pMappedPointer+*pNodeNext)+m_iNodeSize-2*sizeof(int))=*pNodePrev;
            }
        }

        if(m_pQueueArray[iDestQueue]==-1)
        {
            // Empty queue, become the fist node. 
            // The queue header, the node'next and previous pointer to itself.
            *pNodeNext = (char*)pNewNode-(char*)m_pMappedPointer; // The offset of the node 
            *pNodePrev = *pNodeNext; // The offset of the node
            m_pQueueArray[iDestQueue] = *pNodeNext;
        }
        else
        {
            // Not empty queue, add the new node behind the head of the new queue.
            void* pHeader = (char*)m_pMappedPointer+m_pQueueArray[iDestQueue];
            int *pHeaderNext = (int*)((char*)pHeader+m_iNodeSize-sizeof(int));
            int *pHeaderPrev = (int*)((char*)pHeaderNext-sizeof(int));

            // Append the new node to the queue tail. Become the new tail of the queue.

            // Update the next pointer of the tail node to the new node 
            *(int*)(((char*)m_pMappedPointer+*pHeaderPrev)+m_iNodeSize-sizeof(int)) 
                = (char*)pNewNode-(char*)m_pMappedPointer;
            // Update the previous pointer of the new node to the tail. 
            *pNodePrev = *pHeaderPrev;
            // Update the old header's previous pointer to the new node (new tail).
            *pHeaderPrev = (char*)pNewNode-(char*)m_pMappedPointer;
            // Update the next pointer of the new to the head.
            *pNodeNext = m_pQueueArray[iDestQueue];
        }

        return true;
    }
    
    
    int LocateQueue(void* pNode){
        int i=0;
        for(i=0;i<m_iQueueCount;i++)
        {
            void* pFirstTmp = GetQueueHead(i);
            void* pTmp = pFirstTmp;
            while(pTmp)
            {
                if(pTmp==pNode) return i;
                pTmp = GetNext(pTmp);
                if(pTmp==pFirstTmp) break;
            }
        }

        return -1;
    }



    void DumpQueue (int iQueue, bool bDumpNode){
        int iCount = 0;
        void* pTmp = 0;
        void* pFirstTmp = GetQueueHead(iQueue);
        pTmp = pFirstTmp;
        printf("Dump queue %d:\n", iQueue);
        while(pTmp)
        {
            iCount++;
            if(bDumpNode)
                printf("%08x ", pTmp);

            pTmp = GetNext(pTmp);
            if(pTmp==pFirstTmp) break;
        }
        printf("\tnumber %d\n", iCount);
    }

    void DumpNode (void* pNode){
        void* pSrcNode = pNode;
        int *pSrcNext = (int*)((char*)pSrcNode+m_iNodeSize-sizeof(int));
        int *pSrcPrev = (int*)((char*)pSrcNext-sizeof(int));
        printf("Dump Node %08x: Prev\t%08x (%d), Next\t%08x (%d)\n", pNode, 
            (char*)m_pMappedPointer+*pSrcPrev, *pSrcPrev,
            (char*)m_pMappedPointer+*pSrcNext, *pSrcNext);
    }


  protected:
  private:

  private: 
      char* m_strMapName;
      int m_iNodeSize; // the size of internal node which contains additional pointers besides user data.
      int m_iNodeCount; // the node count 
      int m_iQueueCount; // the number of queues
      int* m_pQueueArray; // the queue array 
      shared_memory_t m_hMapMem;
      void* m_pMappedPointer;
};

#ifndef _WIN32
// I've found ftok collision too many,
unsigned int GenerateKeyFromName(const char* pathname, int proj_id){
	if(!pathname)
		return -1;

	key_t ret=0;
	const char* p=pathname;
	while(*p)
	{
		ret += *p++;
	}
	ret = (ret << 8) | (proj_id & 0xFF);
	ret = (ret << 8) | (getuid() & 0xFF);	
	return ret;
}
#endif

#endif
