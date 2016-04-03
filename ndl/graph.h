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

#ifndef SHAPEINTERFACE_H
#define SHAPEINTERFACE_H

#include <string>
#include <list>
#include <vector>
#include <stack>
#include <stdio.h>
#include <iostream>
#include <assert.h>
#include <stdarg.h>
#include <functional>
#define min(a,b) ((a)<(b))?a:b
#define epsilon 0.0000001
//#define epsilon 0

namespace ndl {

#include <hlib/linalg/Vector.h> //Vector class (for use with Matrix class)
#include <hlib/linalg/Matrix.h> //Matrix class

//********************
//HELPER FUNCTIONS
//********************
//GetSubSets recursively generates the bankers sequence.
//It is based on:
//
//Efficiently Enumerating the Subsets of a Set
//J. Loughry, Lockheed Martin Space Systems Company
//J.I. van Hemerty, Leiden Institute of Advanced Computer Science
//Leiden University
//L. Schoofsz, Intelligent Systems Lab
//Department of Mathematics and Computer Science
//University of Antwerp, RUCA
//12 December 2000

//Usage eg:
//
//      std::vector<float> test;
//      test.push_back(1.1);
//      test.push_back(2.2);
//      test.push_back(3.3);
//      test.push_back(4.4);
//      test.push_back(5.5);
//      std::vector<std::vector<float>> output = GetSubSets<3>(test);
//      for (int i = 0; i < output.size(); i++){
//        for (int j = 0; j < subsetsize; j++){
//            printf("%f ",output[i][j]);
//        }
//        printf("\n");
//      }
template<int subsetsize,class thesettype>
std::vector<std::vector<typename thesettype::value_type>>& GetSubSets(thesettype& theset,std::vector<std::vector<typename thesettype::value_type>>& output=std::vector<std::vector<typename thesettype::value_type>>(),std::vector<int>& string=std::vector<int>(),int position=0){
    int tlength = theset.size();
    if (string.size()==0) string.resize(tlength);
    int i;
    if (position < subsetsize){
        if (position==0) i=0;
        else i = string[position - 1] + 1;
        while (i < tlength){
            string[position] = i;
            GetSubSets<subsetsize>(theset,output, string, position + 1);
            i++;
        }
    } else {
        std::vector<typename thesettype::value_type> temp;
        temp.resize(subsetsize);
        int index = 0;
        for (int i = 0; i < tlength; i++){
            if ((index < subsetsize) && (string[index] == i)){
                temp[index] = theset[i];
                index++;
            }
        }
        output.push_back(temp);
    }
    return output;
}
//**********************
//END HELPERS
//**********************

//***********************************
// Helper Classes and Functions
//***********************************
//Comparison Class Used for Sorting
template <class ShapeElement>
struct shapepointerlessthan : std::binary_function<const ShapeElement*, const ShapeElement*, bool>{
public:
    bool operator() (ShapeElement* lhs, ShapeElement* rhs ) const {
        if (m_sortdim==-1) return lhs->index() < rhs->index();
        else return (*lhs)[m_sortdim] < (*rhs)[m_sortdim];
    }
    static int m_sortdim;
};
template<class ShapeElement> int shapepointerlessthan<ShapeElement>::m_sortdim = -1;

//Comparison Class Used for Sorting
template <class ShapeElement>
struct shapepointergreaterthan : std::binary_function<const ShapeElement*, const ShapeElement*, bool>{
public:
    bool operator() (ShapeElement* lhs, ShapeElement* rhs ) const {
        if (m_sortdim==-1) return lhs->index() > rhs->index();
        else return (*lhs)[m_sortdim] > (*rhs)[m_sortdim];
    }
    static int m_sortdim;
};
template<class ShapeElement> int shapepointergreaterthan<ShapeElement>::m_sortdim = -1;

/*!
//Base Class for other shapes
*/
template <class ScalarType,int N>
class Shape : public Vector<ScalarType,N>
{
public:
    typedef Vector<ScalarType,N> value_type;
    
    Shape() { }
    ~Shape() { }
    int size(){ return _v.size(); }
    Vector<ScalarType, N>& operator [] (const int index){ return *_v[index]; }

    Vector<ScalarType, N>& Min(int sortdim = -1){
        shapepointerlessthan<Vector<ScalarType, N>>::m_sortdim = sortdim;
        return **std::min_element(_v.begin(), _v.end(), shapepointerlessthan<Vector<ScalarType, N>>());
    }

    Vector<ScalarType, N>& Max(int sortdim = -1){
        shapepointergreaterthan<Vector<ScalarType, N>>::m_sortdim = sortdim;
        return **std::max_element(_v.begin(), _v.end(), shapepointergreaterthan<Vector<ScalarType, N>>());
    }
    
    void sort(int sortdim = -1){
        shapepointerlessthan<Vector<ScalarType, N>>::m_sortdim = sortdim;
        std::sort(_v.begin(), _v.end(), shapepointerlessthan<Vector<ScalarType, N>>());
    }
    void sort2(int sortdim = -1){
        shapepointergreaterthan<Vector<ScalarType, N>>::m_sortdim = sortdim;
        std::sort(_v.begin(), _v.end(), shapepointergreaterthan<Vector<ScalarType, N>>());
    }
    void print(){ for(int i=0; i < _v.size(); i++) _v[i]->print(); }
    std::vector<Vector<ScalarType, N>*> _v; //holds points of this shape
};

/*!
//An N-D Simplex is stored here
*/
template <class ScalarType, int N>
class Simplex : public Shape<ScalarType, N>{
public:
    Simplex() { }
    Simplex(int npoints,Vector<ScalarType, N>* pts[]){
        for (int i=0;i<npoints;i++) _v.push_back(pts[i]);
    }
    Simplex(Vector<ScalarType, N>& p1){
        _v.push_back(&p1);
    }
    Simplex(Vector<ScalarType, N>& p1,Vector<ScalarType, N>& p2){ 
        _v.push_back(&p1); _v.push_back(&p2);
    }
    Simplex(Vector<ScalarType, N>& p1,Vector<ScalarType, N>& p2,Vector<ScalarType, N>& p3){ 
        _v.push_back(&p1); _v.push_back(&p2); _v.push_back(&p3);
    }
    Simplex(Vector<ScalarType, N>& p1,Vector<ScalarType, N>& p2,Vector<ScalarType, N>& p3,Vector<ScalarType, N>& p4){ 
        _v.push_back(&p1); _v.push_back(&p2); _v.push_back(&p3); _v.push_back(&p4);
    }
    Simplex(Simplex<ScalarType, N>& s){ 
        _v = s._v;
    }
    ~Simplex() { }
    void addPoint(Vector<ScalarType, N>& p){ _v.push_back(&p); }
    void addFrontPoint(Vector<ScalarType, N>* p){ _v.insert(_v.begin(),p); }
    Vector<ScalarType, N>* lastPoint(){ return _v.back(); }
    Vector<ScalarType, N>* frontPoint(){ return _v.front(); }
    void removeLastPoint(){ _v.pop_back(); }
    void removeFrontPoint(){ _v.pop_front(); }
    ScalarType Determinant(){
        int n = min(_v.size(),N+1);
        //determinant for area, volume, and hypervolumes
        Matrix<ScalarType,N+1,N+1> m;
        for(int j=0;j<n;j++){
        for(int i=0;i<n;i++){
            if (j==n-1) (m[j])[i]=1;
            else (m[j])[i]=(*_v[i])[j];
        }}
        return m.Determinant(n);
    }
    ScalarType EdgePointDeterminant(Vector<ScalarType, N>& p){        
        int n = min(_v.size(),N); n++;
        //determinant for area, volume, and hypervolumes
        Matrix<ScalarType,N+1,N+1> m;
        for(int j=0;j<n;j++){
        for(int i=0;i<n;i++){
            if (j==n-1) (m[j])[i]=1;
            else if (i==n-1) (m[j])[i]=p[j];
            else (m[j])[i]=(*_v[i])[j];
        }}
        return m.Determinant(n);
    }
    ScalarType nVolume(){
        //Calculate the n-Volume of the simplex created by connecting 
        //a point to this edge. (Note: 1-volume = length, 2-volume = area, 
        //3-volume = volume, ...)
        int n = min(_v.size(),N+1);
        return Determinant()/(ScalarType)factorial<n>::value;
    }

    //Simplex Vector Operators
    bool operator < (Vector<ScalarType, N>& p){ return EdgePointDeterminant(p) < 0; }
    bool operator > (Vector<ScalarType, N>& p){ return EdgePointDeterminant(p) > 0; }
    bool operator <= (Vector<ScalarType, N>& p){ return EdgePointDeterminant(p) <= 0; }
    bool operator >= (Vector<ScalarType, N>& p){ return EdgePointDeterminant(p) >= 0; }
    bool operator == (Vector<ScalarType, N>& p){ return EdgePointDeterminant(p)==0; }
    bool operator != (Vector<ScalarType, N>& p){ return EdgePointDeterminant(p)!=0; }
    
    //~ bool operator < (Vector<ScalarType, N>& p){ return EdgePointDeterminant(p) < epsilon; }
    //~ bool operator > (Vector<ScalarType, N>& p){ return EdgePointDeterminant(p) > -epsilon; }
    //~ bool operator <= (Vector<ScalarType, N>& p){ return EdgePointDeterminant(p) <= epsilon; }
    //~ bool operator >= (Vector<ScalarType, N>& p){ return EdgePointDeterminant(p) >= -epsilon; }
    //~ bool operator == (Vector<ScalarType, N>& p){ ScalarType t = EdgePointDeterminant(p); return (t <= epsilon && t >= -epsilon); }
    //~ bool operator != (Vector<ScalarType, N>& p){ ScalarType t = EdgePointDeterminant(p); return (t > epsilon || t < -epsilon); }
    
    //Simplex Simplex Operators
    bool operator == (Simplex<ScalarType, N>& e){
        int size = _v.size();
        if (size!=e.size()) return false;
        for (int i=0;i<size;i++){
            bool foundit=false;
            for (int j=0;j<size;j++){
                if (_v[i] == &e[j]){
                    foundit=true;
                    break;
                }
            }
            if (!foundit) return false;
        }
        //~ printf("TRUECOMPARE: \n"); 
        //~ print();
        //~ e.print();
        //~ printf("\n");

        return true;
    }

    Simplex<ScalarType, N>& operator = (Simplex<ScalarType, N>& s){
        _v = s._v;
    }

    //Print out Simplex
    void print(){
        std::cout << index() << ":(";
        int size = _v.size();
        for (int i=0;i<size;i++){
            if (i!=0) std::cout << ","; 
            std::cout << _v[i]->index();
        }
        std::cout << ")\n";
    }
    
};

/*!
//This class handles ND-mesh processing in NDL.
*/
template <class ScalarType,int N>
class Mesh : public Shape<ScalarType, N>{
public:
    Mesh() { destructflag=true; }
    Mesh(Mesh<ScalarType, N>* m){ 
        //This copy constructor does not do a deep copy,
        destructflag=false;
        _v = m->_v;
        _e = m->_e;
    }
    Mesh(Mesh<ScalarType, N>& m){ 
        //This copy constructor does a deep copy,
        destructflag=true;
        for(int i=0; i < _v.size(); i++) addPoint(m[i]);
        //NOTE: STILL NEED TO IMPLEMENT DEEP COPY OF EDGES
    }
    Mesh<ScalarType, N>& operator = (const Mesh<ScalarType, N>* m){
        empty();
        //This assignment does not do a deep copy,
        destructflag=false;
        _v = m->_v;
        _e = m->_e;
        return *this;
    }
    ~Mesh() { empty(); }
    void empty(){
        emptyedges();
        if (destructflag) for(int i=0; i < _v.size(); i++) delete _v[i]; 
        _v.clear();
    }
    void emptyedges(){
        if (destructflag) for(int i=0; i < _e.size(); i++) delete _e[i]; 
        _e.clear();
    }
    int numEdges(){
        return _e.size();
    }
    Simplex<ScalarType, N>& lastEdge(){
        return *_e.back();
    }
    Vector<ScalarType, N>* addPoint(ScalarType x){ 
        Vector<ScalarType, N>* p = new Vector<ScalarType, N>(x);
        p->setIndex(_v.size());
        _v.push_back(p);
        return p;
    }
    Vector<ScalarType, N>* addPoint(ScalarType x,ScalarType y){ 
        Vector<ScalarType, N>* p = new Vector<ScalarType, N>(x,y);
        p->setIndex(_v.size());
        _v.push_back(p);
        return p;
    }
    Vector<ScalarType, N>* addPoint(ScalarType x,ScalarType y,ScalarType z){ 
        Vector<ScalarType, N>* p = new Vector<ScalarType, N>(x,y,z);
        p->setIndex(_v.size());
        _v.push_back(p);
        return p;
    }
    Vector<ScalarType, N>* addPoint(ScalarType x,ScalarType y,ScalarType z,ScalarType w){ 
        Vector<ScalarType, N>* p = new Vector<ScalarType, N>(x,y,z,w);
        p->setIndex(_v.size());
        _v.push_back(p);
        return p;
    }
    Vector<ScalarType, N>* addPoint(ScalarType x[]){
        Vector<ScalarType, N>* p = new Vector<ScalarType, N>(x);
        p->setIndex(_v.size());
        _v.push_back(p);
        return p;
    }
    Vector<ScalarType, N>* addPoint(Vector<ScalarType, N>& point){
        Vector<ScalarType, N>* p = new Vector<ScalarType, N>(point);
        p->setIndex(_v.size());
        _v.push_back(p);
        return p;
    }
    Vector<ScalarType, N>* addPoint(Vector<ScalarType, N>* point){
        _v.push_back(point);
        return point;
    }
    Simplex<ScalarType, N>* addEdge(){
        Simplex<ScalarType, N>* e = new Simplex<ScalarType, N>();
        e->setIndex(_e.size());
        _e.push_back(e);
        return e;
    }
    Simplex<ScalarType, N>* addEdge(Vector<ScalarType, N>& p1){
        Simplex<ScalarType, N>* e = new Simplex<ScalarType, N>(p1);
        e->setIndex(_e.size());
        _e.push_back(e);
        return e;
    }
    Simplex<ScalarType, N>* addEdge(Vector<ScalarType, N>& p1,Vector<ScalarType, N>& p2){
        Simplex<ScalarType, N>* e = new Simplex<ScalarType, N>(p1,p2);
        e->setIndex(_e.size());
        _e.push_back(e);
        return e;
    }
    Simplex<ScalarType, N>* addEdge(Vector<ScalarType, N>& p1,Vector<ScalarType, N>& p2,Vector<ScalarType, N>& p3){
        Simplex<ScalarType, N>* e = new Simplex<ScalarType, N>(p1,p2,p3);
        e->setIndex(_e.size());
        _e.push_back(e);
        return e;
    }
    Simplex<ScalarType, N>* addEdge(Vector<ScalarType, N>& p1,Vector<ScalarType, N>& p2,Vector<ScalarType, N>& p3,Vector<ScalarType, N>& p4){
        Simplex<ScalarType, N>* e = new Simplex<ScalarType, N>(p1,p2,p3,p4);
        e->setIndex(_e.size());
        _e.push_back(e);
        return e;
    }
    Simplex<ScalarType, N>* addEdge(int npoints,Vector<ScalarType, N>* pts[]){
        Simplex<ScalarType, N>* e = new Simplex<ScalarType, N>(npoints,pts);
        e->setIndex(_e.size());
        _e.push_back(e);
        return e;
    }
    Simplex<ScalarType, N>* addEdge(Simplex<ScalarType, N>& p){
        Simplex<ScalarType, N>* e = new Simplex<ScalarType, N>(p);
        e->setIndex(_e.size());
        _e.push_back(e);
        return e;
    }
    Simplex<ScalarType, N>* addEdge(Simplex<ScalarType, N>* p){
        _e.push_back(p);
        return p;
    }
    void print(){
        printf("Points:\n");
        Shape<ScalarType, N>::print();
        printf("Simplexes:\n");
        for(int i=0; i < _e.size(); i++) _e[i]->print();
        int min=-8;
        int max=5;
        for(int y=min;y<max;y++){
            for(int x=min;x<max;x++){
                int index=-1;
                for(int i=0; i < _v.size(); i++){
                    int tx = (*_v[i])[0];
                    int ty = (*_v[i])[1];
                    if (tx==x && ty==y) index=_v[i]->index();
                }
                if (index>-1) printf("%d",index);
                else printf(".");
            }
            printf("\n");
        }
    }

    //~ bool convexHull(){
        //~ emptyedges(); //clear out old edges
        //~ Mesh<N,ScalarType> tmesh = this; //hold the set of points still in the hull
        //~ int numedges=0; int lastnumedges=0;
        //~ do {
            //~ //add min and max points in each dimension to a temp edge,
            //~ //this generates a hypercube like structure, then delete
            //~ //the points from the list of points to consider for next
            //~ //time
            //~ Simplex<ScalarType,N> tedge;
            //~ for (int t=0;t<N;t++){
                //~ tmesh.sort(t);
                
                //~ int index=0;
                //~ while (index < tmesh.size() && std::find(tedge._v.begin(),tedge._v.end(),&tmesh[index])!=tedge._v.end()) index++;
                //~ if (index < tmesh.size()){
                    //~ tedge.addPoint(tmesh[index]);
                    //~ tmesh._v.erase(tmesh._v.begin()+index);
                //~ }

                //~ index=tmesh.size()-1;
                //~ while (index >= 0 && std::find(tedge._v.begin(),tedge._v.end(),&tmesh[index])!=tedge._v.end()) index--;
                //~ if (index >= 0){
                    //~ tedge.addPoint(tmesh[index]);
                    //~ tmesh._v.erase(tmesh._v.begin()+index);
                //~ }
            //~ }
            
            //~ //add the points from previous step
            //~ //to the final hull result if its not already there
            //~ for (int t=0;t<tedge.size();t++){
                //~ bool foundflag=false;
                //~ for(int i=0;i<_e.size();i++){
                    //~ if (&(*_e[i])[0]==&tedge[t]) foundflag=true;
                //~ }
                //~ if (foundflag==false){
                    //~ addEdge(tedge[t]);
                //~ }
            //~ }

            //~ //split the hypercube into subsimplecies, check each one
            //~ //against all points to see if we can trim any points out
            //~ std::vector<std::vector<Vector<ScalarType, N>*>> subsimplecies = GetSubSets<N+1>(tedge._v);
            //~ for(int i=0;i<subsimplecies.size();i++){
                //~ Simplex<ScalarType,N> simplex;
                //~ simplex._v = subsimplecies[i];
                //~ ScalarType det = simplex.Determinant();
                //~ bool sign = (det < 0);
                //~ for(int j=0;j < tmesh.size();j++){
                    //~ int isinside=true;
                    //~ for (int t=0;t<simplex.size();t++){
                        //~ Simplex<ScalarType,N> temp = simplex;
                        //~ temp._v[t]=tmesh._v[j];
                        //~ if (sign != (temp.Determinant() < 0) || (det>-epsilon && det<epsilon)){
                            //~ printf("OUTSIDE: ");tmesh[j].print();
                            //~ isinside=false;
                            //~ break;
                        //~ }
                    //~ }
                    //~ if (isinside){
                        //~ printf("INSIDE: ");tmesh[j].print();
                        //~ tmesh._v.erase(tmesh._v.begin()+j);
                        //~ j--;
                    //~ }
                //~ }
            //~ }
            //~ printf("FINAL\n");print();
            //~ printf("TMESH\n");tmesh.print();
            //~ lastnumedges = numedges;
            //~ numedges = numEdges();
        //~ } while (numedges!=lastnumedges);
        //~ return true;
    //~ }
    
    //~ bool convexHull(){
        //~ emptyedges(); //clear out old edges
        //~ Mesh<N,ScalarType> tmesh[N];
        //~ for(int dim=0;dim<N;dim++){ //sort vertices by each dimension
            //~ tmesh[dim] = this; //not a deep copy
            //~ tmesh[dim].sort(dim);
        //~ }
        //~ int numedges=0; int lastnumedges=0;
        //~ int startindexoffset = 0;
        //~ do {
            //~ for (int t=0;t<N*2;t++){
                //~ int dim = t/2;
                //~ bool newedgeisgood = false;
                //~ if (startindexoffset<tmesh[dim].size()){
                    //~ printf("&");
                    //~ if (t % 2) addEdge(tmesh[dim][startindexoffset]); //add a 1D edge--a point
                    //~ else addEdge(tmesh[dim][tmesh[dim].size()-1-startindexoffset]);
                //~ }
                //~ for (int q=0;q<tmesh[dim].size();q++){
                    //~ if (lastEdge()!=tmesh[dim][q]){ //skip points that are incident,colinear,coplanar,etc.
                        //~ lastEdge().addPoint(tmesh[dim][q]);
                        //~ printf("CONSIDER EDGE: "); lastEdge().print();

                        //~ //check against other points to see if it is valid
                        //~ bool lessflag=false;
                        //~ bool greaterflag=false;
                        //~ for(int j=0;j < size();j++){ //compare new edge to each point
                            //~ if (lessflag==false && lastEdge() < *_v[j]) lessflag=true;
                            //~ if (greaterflag==false && lastEdge() > *_v[j]) greaterflag=true;
                        //~ }
                        //~ if (lessflag ^ greaterflag) newedgeisgood=true;
                        
                        //~ //check against other edges to see if it is valid
                        //~ if (newedgeisgood)
                        //~ for(int j=0;j < _e.size();j++){ //compare new edge to each point
                            //~ if (&lastEdge() != _e[j] && lastEdge() == *_e[j]){
                                //~ printf("EDGE ");
                                //~ newedgeisgood = false;
                                //~ break;
                            //~ }
                        //~ } else printf("POINT ");
                        
                        //~ if (newedgeisgood){
                            //~ printf("ACCEPT: "); lastEdge().print();
                            //~ break;
                        //~ } else {
                            //~ lastEdge().removeLastPoint(); //remove the new point and try again
                            //~ printf("REJECT: "); lastEdge().print();
                        //~ }
                    //~ }
                //~ }
                //~ if (startindexoffset<tmesh[dim].size()){
                    //~ if (!newedgeisgood){ printf("$"); _e.pop_back(); }
                    //~ //if (t % 2) tmesh[dim]._v.erase(tmesh[dim]._v.begin());
                    //~ //else tmesh[dim]._v.erase(tmesh[dim]._v.end());
                //~ }

            //~ }
            //~ lastnumedges = numedges;
            //~ numedges = numEdges();
            //~ if (numedges!=lastnumedges){
                //~ lastnumedges=-1;
                //~ startindexoffset++;
            //~ }
        //~ } while (numedges!=lastnumedges);
        //~ return true;
    //~ }
    
    bool convexHull(){ //uses gift-wrap algorithm
        bool stopflag;
        emptyedges(); //clear out old edges
        Mesh<ScalarType,N> tmesh(this); //process mesh to keep track of points and edges to be processed (this is not a deep copy)
        tmesh.sort(1); //sort the points by their y dimension
        tmesh.addEdge(addEdge(tmesh[0])); //add the first edge to the edge queue (Note: The first edge is a 1D edge--a point)
        printf("Push: "); tmesh.lastEdge().print();
        tmesh.sort2(0); //sort the points by their x dimension
        
        //process edges in queue until all done
        do {
            stopflag=true;
            for(int i=0;i<size();i++){ //scan through all the points
                if (tmesh.lastEdge()!=tmesh[i]){ //skip points that are incident,colinear,coplanar,etc.
                    //add a new point to the edge
                    tmesh.lastEdge().addPoint(tmesh[i]);
                    printf("Push: "); tmesh.lastEdge().print();
                
                    //check against other points to see if it is valid
                    bool newedgeisgood = true;
                    for(int j=1;j < size();j++){ //compare new edge to each point
                        if (tmesh.lastEdge() < *_v[j]){ //if we find a point outside of the new edge
                            printf("badpoint ");
                            newedgeisgood = false;
                            break;
                        }
                    }
                    
                    //check against other edges to see if it is valid
                    if (newedgeisgood)
                    for(int j=0;j < _e.size();j++){ //compare new edge to each point
                        if (&tmesh.lastEdge() != _e[j] && tmesh.lastEdge() == *_e[j]){
                            printf("badedge ");
                            newedgeisgood = false;
                            break;
                        }
                    }
                    
                    if (newedgeisgood){
                        printf("Good!\n");
                        stopflag = false;
                        
                        //If the simplex is complete, pop it and spawn smaller edges
                        if (tmesh.lastEdge().size() == N){
                            Simplex<ScalarType, N>* parentedge = &tmesh.lastEdge();
                            tmesh._e.pop_back();
                            printf("POP: "); parentedge->print();
                            
                            //add children edges
                            for(int i=parentedge->size()-1;i>=0;i--){
                                //remove the point at i from the parentedge
                                //to create a child edge
                                Vector<ScalarType, N>* temp = &(*parentedge)[i];
                                parentedge->_v.erase(parentedge->_v.begin()+i);
                                
                                //ensure the edge is not allready stored
                                bool addedgeflag=true;
                                for(int j=0;j<_e.size();j++){
                                    if (parentedge != _e[j] && *parentedge == *_e[j]){
                                        addedgeflag=false;
                                        break;
                                    }
                                }

                                //add the edge
                                if (addedgeflag){
                                    Simplex<ScalarType, N>* newedge = addEdge(*parentedge);
                                    for(int i=0;i<N;i++) newedge->sort(i);
                                    tmesh.addEdge(newedge);
                                    printf("Push: "); tmesh.lastEdge().print();                        
                                }
                                
                                //put the point back
                                parentedge->_v.insert(parentedge->_v.begin()+i,temp);
                            }
                        }
                        
                        break;
                    } else {
                        tmesh.lastEdge().removeLastPoint(); //remove the new point and try again
                        printf("Pop: "); tmesh.lastEdge().print();
                    }
                } else printf("SKIPPED: %d\n",tmesh[i].index());
            }
        } while (!stopflag && tmesh.numEdges()>0);
        return true;
    }
    
    //member vars
    bool destructflag; //keep track if we need to destruct the points and edges or not
    std::vector<Simplex<ScalarType, N>*> _e; //holds all edges stored in this mesh
};

}; // end of namespace


#endif