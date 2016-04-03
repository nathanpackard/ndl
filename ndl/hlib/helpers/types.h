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

#ifndef TYPES_H
#define TYPES_H

#include <string>

//Data Types
#define G_CHAR                0x0010
#define G_UNSIGNED_CHAR       0x0020
#define G_SHORT               0x0030
#define G_UNSIGNED_SHORT      0x0040
#define G_INT                 0x0050
#define G_UNSIGNED_INT        0x0060
#define G_LONGLONG            0x0070
#define G_UNSIGNED_LONGLONG   0x0080
#define G_FLOAT               0x0090
#define G_DOUBLE              0x00A0
    
//declare helper classes for type identification
template< class T > struct is_char{static bool const ans=false; };
template<> struct is_char<char> {static bool const ans=true; };
template< class T > struct is_uchar{static bool const ans=false; };
template<> struct is_uchar<unsigned char> {static bool const ans=true; };

template< class T > struct is_short{static bool const ans=false; };
template<> struct is_short<short> {static bool const ans=true; };
template< class T > struct is_ushort{static bool const ans=false; };
template<> struct is_ushort<unsigned short> {static bool const ans=true; };

template< class T > struct is_int{static bool const ans=false; };
template<> struct is_int<int> {static bool const ans=true; };
template< class T > struct is_uint{static bool const ans=false; };
template<> struct is_uint<unsigned int> {static bool const ans=true; };

template< class T > struct is_longlong{static bool const ans=false; };
template<> struct is_longlong<long long> {static bool const ans=true; };
template< class T > struct is_ulonglong{static bool const ans=false; };
template<> struct is_ulonglong<unsigned long long> {static bool const ans=true; };

template< class T > struct is_float{static bool const ans=false; };
template<> struct is_float<float> {static bool const ans=true; };
template< class T > struct is_double{static bool const ans=false; };
template<> struct is_double<double> {static bool const ans=true; };

template<class T>
int getdatatype(){
    if (is_char<T>::ans) return G_CHAR;
    if (is_uchar<T>::ans) return G_UNSIGNED_CHAR;
    if (is_short<T>::ans) return G_SHORT;
    if (is_ushort<T>::ans) return G_UNSIGNED_SHORT;
    if (is_int<T>::ans) return G_INT;
    if (is_uint<T>::ans) return G_UNSIGNED_INT;
    if (is_longlong<T>::ans) return G_LONGLONG;
    if (is_ulonglong<T>::ans) return G_UNSIGNED_LONGLONG;
    if (is_float<T>::ans) return G_FLOAT;
    if (is_double<T>::ans) return G_DOUBLE;
    return 0;
}

std::string getdatatypestring(int datatype){
    switch(datatype){
        case G_CHAR: return "char";
        case G_UNSIGNED_CHAR: return "uchar";
        case G_SHORT: return "short";
        case G_UNSIGNED_SHORT: return "ushort";
        case G_INT: return "int";
        case G_UNSIGNED_INT: return "uint";
        case G_LONGLONG: return "longlong";
        case G_UNSIGNED_LONGLONG: return "ulonglong";
        case G_FLOAT: return "float";
        case G_DOUBLE: return "double";
        default: return "";
    }
}

int getdatatype(std::string datatypestr){
    if (datatypestr=="char") return G_CHAR;
    else if (datatypestr=="uchar") return G_UNSIGNED_CHAR;
    else if (datatypestr=="short") return G_SHORT;
    else if (datatypestr=="ushort") return G_UNSIGNED_SHORT;
    else if (datatypestr=="int") return G_INT;
    else if (datatypestr=="uint") return G_UNSIGNED_INT;
    else if (datatypestr=="longlong") return G_LONGLONG;
    else if (datatypestr=="ulonglong") return G_UNSIGNED_LONGLONG;
    else if (datatypestr=="float") return G_FLOAT;
    else if (datatypestr=="double") return G_DOUBLE;
    else return 0;
}

int datatypesize(int datatype){
    switch(datatype){
        case G_CHAR: return sizeof(char);
        case G_UNSIGNED_CHAR: return sizeof(unsigned char);
        case G_SHORT: return sizeof(short);
        case G_UNSIGNED_SHORT: return sizeof(unsigned short);
        case G_INT: return sizeof(int);
        case G_UNSIGNED_INT: return sizeof(unsigned int);
        case G_LONGLONG: return sizeof(long long);
        case G_UNSIGNED_LONGLONG: return sizeof(unsigned long long);
        case G_FLOAT: return sizeof(float);
        case G_DOUBLE: return sizeof(double);
        default: return 0;
    }
}

#endif