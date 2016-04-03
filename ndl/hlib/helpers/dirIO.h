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

#ifndef DIRENT_INCLUDED
#define DIRENT_INCLUDED

#include <cstdio>

#ifndef _WIN32
#include <dirent.h>
#include <algorithm> 
//~ #undef max
//~ #undef min
#else

#include <errno.h>
#include <io.h> /* _findfirst and _findnext set errno iff they return -1 */
#include <stdlib.h>
#include <string.h>

/*

    Declaration of POSIX directory browsing functions and types for Win32.

    Author:  Kevlin Henney (kevlin@acm.org, kevlin@curbralan.com)
    History: Created March 1997. Updated June 2003.
    Rights:  See end of file.
    
*/
struct dirent{
    char *d_name;
};

struct DIR {
    long                handle; /* -1 for failed rewind */
    struct _finddata_t  info;
    struct dirent       result; /* d_name null iff first time */
    char                *name;  /* null-terminated char string */
};

DIR *opendir(const char *name){
    DIR *dir = 0;
    if(name && name[0]){
        size_t base_length = strlen(name);
        const char *all = strchr("/\\", name[base_length - 1]) ? "*" : "/*"; /* search pattern must end with suitable wildcard */
        if((dir = (DIR *) malloc(sizeof *dir)) != 0 && (dir->name = (char *) malloc(base_length + strlen(all) + 1)) != 0){
            strcat(strcpy(dir->name, name), all);
            if((dir->handle = (long) _findfirst(dir->name, &dir->info)) != -1){
                dir->result.d_name = 0;
            } else {/* rollback */
                free(dir->name);
                free(dir);
                dir = 0;
            }
        } else { /* rollback */
            free(dir);
            dir   = 0;
            errno = ENOMEM;
        }
    }
    else errno = EINVAL;
    return dir;
}

int closedir(DIR *dir){
    int result = -1;
    if(dir){
        if(dir->handle != -1) result = _findclose(dir->handle);
        free(dir->name);
        free(dir);
    }
    if(result == -1) errno = EBADF; /* map all errors to EBADF */
    return result;
}

struct dirent *readdir(DIR *dir){
    struct dirent *result = 0;
    if(dir && dir->handle != -1){
        if(!dir->result.d_name || _findnext(dir->handle, &dir->info) != -1){
            result         = &dir->result;
            result->d_name = dir->info.name;
        }
    } else errno = EBADF;
    return result;
}

void rewinddir(DIR *dir){
    if(dir && dir->handle != -1){
        _findclose(dir->handle);
        dir->handle = (long) _findfirst(dir->name, &dir->info);
        dir->result.d_name = 0;
    } else errno = EBADF;
}

/*

    Copyright Kevlin Henney, 1997, 2003. All rights reserved.

    Permission to use, copy, modify, and distribute this software and its
    documentation for any purpose is hereby granted without fee, provided
    that this copyright and permissions notice appear in all copies and
    derivatives.
    
    This software is supplied "as is" without express or implied warranty.

    But that said, if there are any problems please get in touch.

*/
#endif

void fixslashes(std::string& filename){
    for(int i=filename.size();i>=0;i--){
        if (filename[i]=='\\') filename[i]='/';
    }
}

bool isdir(std::string dir){
    DIR *dp = opendir(dir.c_str());
    if(dp != NULL){ 
        closedir(dp);
        return true;
    }        
    return false;
}

int getfiles(std::string dir, std::vector<std::string> &files){
    fixslashes(dir);
    DIR *dp;
    struct dirent *dirp;
    if((dp  = opendir(dir.c_str())) == NULL) { std::cout << "Error(" << errno << ") opening folder '" << dir << "'\n"; return errno; }
    while ((dirp = readdir(dp)) != NULL){
        std::string path = std::string(dirp->d_name);
        std::string fullpath = dir + "//" + path;
        if (!isdir(fullpath)) files.push_back(path);
    }
    closedir(dp);
    return 0;
}

int getsubdirs(std::string dir, std::vector<std::string> &subdirs){
    fixslashes(dir);
    DIR *dp;
    struct dirent *dirp;
    if((dp  = opendir(dir.c_str())) == NULL) { std::cout << "Error(" << errno << ") opening " << dir << std::endl; return errno; }
    while ((dirp = readdir(dp)) != NULL){
        std::string path = std::string(dirp->d_name);
        std::string fullpath = dir + "//" + path;
        if (path!=".." && path!="." && isdir(fullpath)) subdirs.push_back(path);
    }
    closedir(dp);
    return 0;
}

std::string getextension(std::string filename){
    std::string extension = "";
    int pos = filename.find_last_of('.');
    if ( pos != filename.npos ){
        extension = filename.substr(pos+1);
    }
    return extension;
}

bool fileexists(std::string filename){
    FILE* f = fopen(filename.c_str(),"rb");
    if (f) fclose(f);
    else return false;
    return true;
}


//~ void StringSplit(std::string str, std::string delim, std::vector<std::string>& results){
    //~ int cutAt;
    //~ while( (cutAt = str.find_first_of(delim)) != str.npos ){
        //~ if(cutAt > 0){
            //~ results.push_back(str.substr(0,cutAt));
        //~ }
        //~ str = str.substr(cutAt+1);
    //~ }
    //~ if(str.length() > 0){
        //~ results.push_back(str);
    //~ }
//~ }

    //~ FILE* fp = fopen(filename,"rb");
	//~ if (fp==0) return false;
	//~ fseek (fp , 0 , SEEK_END);
	//~ long lSize = ftell(fp);

std::string printfilename(std::string filename,int size=50){
    fixslashes(filename);
    size = std::max(size,10);
    if (filename.size()-3>size){
        int nstartchars = 5;
        int nendchars = std::max(0,size - nstartchars - 3);
        std::string start = filename.substr(0,nstartchars);
        std::string end = filename.substr(filename.size() - nendchars,nendchars);
        return start + "..." + end;
    } else return filename;
}


bool isnumeric(std::string str){
    if (str.size()==0) return false;
    for(int i=0;i<str.size();i++){
        if (!isdigit(str[i])) return false;
    }
    return true;
}

std::string getfolderfromfile(std::string filename){
    fixslashes(filename);
    int c=0;
    for(int i=filename.size();i>=0;i--){
        if (filename[i]=='\\' || filename[i]=='/'){
            return filename.substr(0,i);
        }
    }
    return std::string(".");
}

std::string stripfolder(std::string filename){
    fixslashes(filename);
    int c=0;
    for(int i=filename.size();i>=0;i--){
        if (filename[i]=='\\' || filename[i]=='/'){
            return filename.substr(i+1);
        }
    }
    return filename;
}

int getfilesrecursive(std::string dir, std::vector<std::string> &files){
    std::vector<std::string> subdirs;
    getfiles(dir,files);
    getsubdirs(dir,subdirs);
    for(int i=0;i<subdirs.size();i++){
        std::vector<std::string> subfiles;
        getfilesrecursive(dir + "/" + subdirs[i],subfiles);
        for(int j=0;j<subfiles.size();j++){
            files.push_back(subdirs[i] + "/" + subfiles[j]);
        }
    }
    return 0;
}


std::string getparamstring(char** argv,int startarg,int endarg){
    std::string params;
    for(int i=startarg;i<=endarg;i++){
        if (argv[i][0]=='-'){
            std::string t = &argv[i][1];
            if (t.find_first_of(" ")==std::string::npos) params = params + "-" + t + " ";
            else params = params + "\"-" + t + "\" ";
        }
    }
    return params;
}

template<class T>
void getoptionalparams(const std::string &str,std::map<std::string,T> &result,bool verbose=false){
    
    bool inquotes=false;
    int state = 0; //0 means searching, 1 means getting left, 2 means getting right
    std::string leftside;
    std::string rightside;
    for(int i=0;i<str.length();i++){
        if (str[i]=='\"'){
            //track if we are in quotes or not
            inquotes=!inquotes;
        } else
        if (state==0 && str[i]=='-'){
            //start getting leftside
            state=1;
        } else
        if (state==1 && str[i]==':'){
            //start getting rightside
            state=2;
        } else 
        if ((!inquotes || state==0) && (str[i]==' ' || str[i]=='\t' || str[i]=='\n')){
            //finished getting leftside and rightside, save it and reset state
            if (state>0 && leftside.length()>0){
                result[leftside] = rightside;
                state=0;
                leftside="";
                rightside="";
            }
        } else {
            //add text to leftside or rightside if need be
            if (state==1) leftside += str[i];
            if (state==2) rightside += str[i];
        }
    }
    if (state==1) result[leftside];
    if (state==2) result[leftside] = rightside;
        
    if (verbose){
        std::cout << "inputstr:" << str << std::endl;
        bool printend=false;
        for (std::map<std::string,T>::iterator i = result.begin();i != result.end();i++){
            if (i==result.begin()){ printf("*************\n"); printend=true; }
            std::cout << "result[" << i->first << "] = " << i->second << std::endl;
        }
        if (printend) printf("*************\n");
    }
}



#endif
