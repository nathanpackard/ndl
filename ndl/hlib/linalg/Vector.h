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

#ifndef Vector_H
#define Vector_H

#include <assert.h>
#include <map>

/*! 
//This class can only have 1 variable, which is the static data array
// This allows for it to use minimal memory, and because the size is
// determined at compile time, the compiler can optimize performance.
*/
template<class ScalarType,int N>
class Vector {
public:
    //Provide Static access to template paramters
    const static int SIZE = N;
    typedef ScalarType VOXELTYPE;

    //Default Constructor
    Vector(){ for (int i=0;i<N;i++) m_data[i]=0; }
    
    //General Constructor
    Vector(ScalarType* x){ for (int i=0;i<N;i++) m_data[i]=x[i]; }
    
    //Convenience Constructors For N 1->4 to set the values during construction
    Vector(ScalarType x){ m_data[0]=x; for(int i=1;i<N;i++) m_data[i]=0; };
    Vector(ScalarType x,ScalarType y){ m_data[0]=x; m_data[1]=y; for(int i=2;i<N;i++) m_data[i]=0; };
    Vector(ScalarType x,ScalarType y,ScalarType z){ m_data[0]=x; m_data[1]=y; m_data[2]=z; for(int i=3;i<N;i++) m_data[i]=0; };
    Vector(ScalarType x,ScalarType y,ScalarType z,ScalarType w){ m_data[0]=x; m_data[1]=y; m_data[2]=z; m_data[3]=w; for(int i=4;i<N;i++) m_data[i]=0; };
 
    //copy constructor from vectors of other types and sizes
    template<class T,int N2>
    Vector<ScalarType,N>(Vector<T,N2> &x){
        //std::cout << "VECTOR copyconstructor" << endl;
        for(int i=0;i<N;i++){
            if (i<N2) m_data[i]=ScalarType(x.m_data[i]);
            else m_data[i]=ScalarType(0);
        }
    }

    //assignment for generic types
    template<class T>
    Vector<ScalarType,N>& operator=(T& x){
        for(int i=0;i<N;i++){
            m_data[i]=ScalarType(x[i]);
        }
        return *this;
    }
    
    //accessor
    ScalarType& operator[](const unsigned int i){
        return m_data[i];
    }
    
    //operators
    Vector<ScalarType,N> operator-(){
        return Vector<ScalarType,N>(*this)*=-1;
    }
    Vector<ScalarType,N> operator+(ScalarType value){ return Vector<ScalarType,N>(*this)+=value; }
    Vector<ScalarType,N>& operator+=(ScalarType value){
        for (int i = 0; i<N; i++){m_data[i] += value;}
        return *this;
    }

    Vector<ScalarType,N> operator-(ScalarType value){ return Vector<ScalarType,N>(*this)-=value; }
    Vector<ScalarType,N>& operator-=(ScalarType value){
        for (int i = 0; i<N; i++){m_data[i] -= value;}
        return *this;
    }
    
    Vector<ScalarType,N> operator*(ScalarType value){ return Vector<ScalarType,N>(*this)*=value; }
    Vector<ScalarType,N>& operator*=(ScalarType value){
        for (int i = 0; i<N; i++){m_data[i] *= value;}
        return *this;
    }
    
    Vector<ScalarType,N> operator/(ScalarType value){ return Vector<ScalarType,N>(*this)/=value; }
    Vector<ScalarType,N>& operator/=(ScalarType value){
        for (int i = 0; i<N; i++){m_data[i] /= value;}
        return *this;
    }
    
    Vector<ScalarType,N> operator&(ScalarType value){ return Vector<ScalarType,N>(*this)&=value; }
    Vector<ScalarType,N>& operator&=(ScalarType value){
        for (int i = 0; i<N; i++){m_data[i] &= value;}
        return *this;
    }

    Vector<ScalarType,N> operator|(ScalarType value){ return Vector<ScalarType,N>(*this)|=value; }
    Vector<ScalarType,N>& operator|=(ScalarType value){
        for (int i = 0; i<N; i++){m_data[i] |= value;}
        return *this;
    }

    Vector<ScalarType,N> operator^(ScalarType value){ return Vector<ScalarType,N>(*this)^=value; }
    Vector<ScalarType,N>& operator^=(ScalarType value){
        for (int i = 0; i<N; i++){m_data[i] ^= value;}
        return *this;
    }
    
    Vector<ScalarType,N> operator+(Vector<ScalarType,N> &im){ return Vector<ScalarType,N>(*this)+=im; }
    Vector<ScalarType,N>& operator+=(Vector<ScalarType,N> &im){
        ScalarType* d = im.m_data;
        for (int i = 0; i<N; i++){m_data[i] += d[i];}
        return *this;
    }

    Vector<ScalarType,N> operator-(Vector<ScalarType,N> &im){ return Vector<ScalarType,N>(*this)-=im; }
    Vector<ScalarType,N>& operator-=(Vector<ScalarType,N> &im){
        ScalarType* d = im.m_data;
        for (int i = 0; i<N; i++){m_data[i] -= d[i];}
        return *this;
    }

    Vector<ScalarType,N> operator*(Vector<ScalarType,N> &im){ return Vector<ScalarType,N>(*this)*=im; }
    Vector<ScalarType,N>& operator*=(Vector<ScalarType,N> &im){
        ScalarType* d = im.m_data;
        for (int i = 0; i<N; i++){m_data[i] *= d[i];}
        return *this;
    }
    
    Vector<ScalarType,N> operator/(Vector<ScalarType,N> &im){ return Vector<ScalarType,N>(*this)/=im; }
    Vector<ScalarType,N>& operator/=(Vector<ScalarType,N> &im){
        ScalarType* d = im.m_data;
        for (int i = 0; i<N; i++){m_data[i] /= d[i];}
        return *this;
    }
    
    Vector<ScalarType,N> operator&(Vector<ScalarType,N> &im){ return Vector<ScalarType,N>(*this)&=im; }
    Vector<ScalarType,N>& operator&=(Vector<ScalarType,N> &im){
        ScalarType* d = im.m_data;
        for (int i = 0; i<N; i++){m_data[i] &= d[i];}
        return *this;
    }

    Vector<ScalarType,N> operator|(Vector<ScalarType,N> &im){ return Vector<ScalarType,N>(*this)|=im; }
    Vector<ScalarType,N>& operator|=(Vector<ScalarType,N> &im){
        ScalarType* d = im.m_data;
        for (int i = 0; i<N; i++){m_data[i] |= d[i];}
        return *this;
    }

    Vector<ScalarType,N> operator^(Vector<ScalarType,N> &im){ return Vector<ScalarType,N>(*this)^=im; }
    Vector<ScalarType,N>& operator^=(Vector<ScalarType,N> &im){
        ScalarType* d = im.m_data;
        for (int i = 0; i<N; i++){m_data[i] ^= d[i];}
        return *this;
    }

    bool operator == (ScalarType &value){
        bool retval=true;
        for (int i = 0; i<N; i++){ if(m_data[i]!=value){ retval=false; break; } }
        return retval;
    }

    bool operator == (Vector<ScalarType,N> &im){
        ScalarType* d = im.m_data[i];
        bool retval=true;
        for (int i = 0; i<N; i++){ if(m_data[i]!=d[i]){ retval=false; break; } }
        return retval;
    }

    bool operator != (ScalarType &value){
        bool retval=false;
        for (int i = 0; i<N; i++){ if(m_data[i]!=value){ retval=true; break; } }
        return retval;
    }

    bool operator != (Vector<ScalarType,N> &im){
        ScalarType* d = im.m_data[i];
        bool retval=false;
        for (int i = 0; i<N; i++){ if(m_data[i]!=d[i]){ retval=true; break; } }
        return retval;
    }

    bool operator < (ScalarType &value){
        return (value < SquaredNorm());
    }

    bool operator <= (ScalarType &value){
        return (value <= SquaredNorm());
    }

    bool operator > (ScalarType &value){
        return (value > SquaredNorm());
    }

    bool operator >= (ScalarType &value){
        return (value >= SquaredNorm());
    }

    bool operator < (Vector<ScalarType,N> &im){
        return (im.SquaredNorm() < SquaredNorm());
    }

    bool operator <= (Vector<ScalarType,N> &im){
        return (im.SquaredNorm() <= SquaredNorm());
    }

    bool operator > (Vector<ScalarType,N> &im){
        return (im.SquaredNorm() > SquaredNorm());
    }

    bool operator >= (Vector<ScalarType,N> &im){
        return (im.SquaredNorm() >= SquaredNorm());
    }

	ScalarType Sum() {
        ScalarType result=0;
        for (int i=0;i<N;i++) result+=m_data[i];
        return result;
    }
    
	ScalarType Ave() {
        ScalarType result=0;
        for (int i=0;i<N;i++) result+=m_data[i];
        return result/N;
    }

	ScalarType Norm(){
        ScalarType result=0;
        for (int i=0;i<N;i++) result+=m_data[i]*m_data[i];
        return sqrt(result);
    }

	ScalarType SquaredNorm(){
        ScalarType result=0;
        for (int i=0;i<N;i++) result+=m_data[i]*m_data[i];
        return result;
    }

	Vector<ScalarType,N>& HomoNormalize(){
        ScalarType oneover = 1/m_data[N-1];
        for (int i=0;i<N-1;i++) m_data[i]*=oneover;
        m_data[N-1]=1;
        return *this;
    }
    
	Vector<ScalarType,N>& Abs(){
        for (int i=0;i<N;i++) m_data[i]=abs(m_data[i]);
        return *this;
    }
    
	Vector<ScalarType,N>& Sqrt(){
        for (int i=0;i<N;i++) m_data[i]=sqrt(m_data[i]);
        return *this;
    }
    
	Vector<ScalarType,N>& Normalize(){
        ScalarType oneovernfactor = 1/t->Norm();
        for (int i=0;i<s;i++) d[i]*=oneovernfactor;
        return *this;
    }

    void minmax(ScalarType &minvalue,ScalarType& maxvalue){
        //Get Min and Max Values
        ScalarType temp;
        minvalue=maxvalue=m_data[0];
        for (int j=0;j<N;j++){
            temp = m_data[j];
            if (temp <minvalue) minvalue = temp;
            if (temp >maxvalue) maxvalue = temp;
        }
    }
    
    //enable casting to scalar types
    template<class T>operator T() { return Norm(); }
    
    //Convenience Methods to access elements N 1->4 as (X,Y,Z,W)
    // W is in any case the last coordinate. (in a 2D point, W() == Y(). 
    // In a 3D point, W()==Z() in a 4D point, W() is a separate component)
	inline ScalarType &X() { return m_data[0]; }
	inline ScalarType &Y() { return m_data[1]; }
	inline ScalarType &Z() { return m_data[2]; }
	inline ScalarType &W() { return m_data[N-1]; }
        
    //Handle setting, retrieving, and printing indecies
    int& index(){ return m_indecies[long long(this)]; }
    int setIndex(int index){ return m_indecies[long long(this)]=index; }
    void print(char* msg=0){
        if (msg) printf("****************\n%s\n****************\n",msg);
        std::cout << index() << ":(";
        for (int i=0;i<N;i++){
            if (i!=0) std::cout << ","; 
            std::cout << m_data[i];
        }
        std::cout << ")\n\n";
    }
    
    //data and size implementations
    ScalarType* data(){ return m_data; }
    int size(){ return N; }
    static std::map<long long,int> m_indecies;
//~ private:
    ScalarType m_data[N];
};

//declare static vars
template<class ScalarType,int N> std::map<long long,int> Vector<ScalarType,N>::m_indecies;

#endif