//// Copyright (C) 2009   Nathan Packard   <nathanpackard@gmail.com>
////
//// This program is free software; you can redistribute it and/or modify
//// it under the terms of the GNU Lesser General Public License as 
//// published by the Free Software Foundation; either version 3 of the 
//// License, or (at your option) any later version.
////
//// This program is distributed in the hope that it will be useful,
//// but WITHOUT ANY WARRANTY; without even the implied warranty of
//// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//// GNU Lesser General Public License for more details.
////
//// You should have received a copy of the GNU Lesser General Public 
//// License along with this program; if not, see 
//// <http://www.gnu.org/licenses/>.
//
//#ifndef SHAPEINTERFACE_H
//#define SHAPEINTERFACE_H
//
//#include <string>
//#include <list>
//#include <vector>
//#include <stack>
//#include <stdio.h>
//#include <iostream>
//#include <assert.h>
//#include <stdarg.h>
//#include <functional>
//
//namespace ndl {
//
//#include "matrix.h" //Matrix class
//
////Comparison Class Used for Sorting
//template <class ShapeElement>
//struct shapepointerlessthan : std::binary_function<const ShapeElement*, const ShapeElement*, bool>{
//public:
//    bool operator() (ShapeElement* lhs, ShapeElement* rhs ) const {
//        if (m_sortdim==-1) return lhs->index() < rhs->index();
//        else return (*lhs)[m_sortdim] < (*rhs)[m_sortdim];
//    }
//    static int m_sortdim;
//};
//template<class ShapeElement> int shapepointerlessthan<ShapeElement>::m_sortdim = -1;
//
////Comparison Class Used for Sorting
//template <class ShapeElement>
//struct shapepointergreaterthan : std::binary_function<const ShapeElement*, const ShapeElement*, bool>{
//public:
//    bool operator() (ShapeElement* lhs, ShapeElement* rhs ) const {
//        if (m_sortdim==-1) return lhs->index() > rhs->index();
//        else return (*lhs)[m_sortdim] > (*rhs)[m_sortdim];
//    }
//    static int m_sortdim;
//};
//template<class ShapeElement> int shapepointergreaterthan<ShapeElement>::m_sortdim = -1;
//
////template <class ScalarType, int N>
////struct Mesh
////{
////	template <int VERTICES> std::vector<std::array<int, VERTICES>> edges;
////	std::vector<std::array<ScalarType, N>> Points;
////};
//
//
//
//
///*!
////Base Class for other shapes
//*/
//template <class ScalarType,int N>
//class Shape : public std::array<ScalarType,N>
//{
//public:
//	Shape() {}
//    Shape(std::vector<std::array<ScalarType, N>>& points) : _points(points) { }
//    ~Shape() { }
//    int size(){ return _indices.size(); }
//	std::array<ScalarType, N>& operator [] (const int index){ return *_points[index]; }
//	std::array<ScalarType, N>& Min(int sortdim = -1){
//        shapepointerlessthan<std::array<ScalarType, N>>::m_sortdim = sortdim;
//        return **std::min_element(_points.begin(), _points.end(), shapepointerlessthan<std::array<ScalarType, N>>());
//    }
//	std::array<ScalarType, N>& Max(int sortdim = -1){
//        shapepointergreaterthan<std::array<ScalarType, N>>::m_sortdim = sortdim;
//        return **std::max_element(_points.begin(), _points.end(), shapepointergreaterthan<std::array<ScalarType, N>>());
//    }
//    void sort(bool descending = false, int sortdim = -1){
//		if (!descending)
//		{
//			shapepointerlessthan<std::array<ScalarType, N>>::m_sortdim = sortdim;
//			std::sort(_points.begin(), _points.end(), shapepointerlessthan<std::array<ScalarType, N>>());
//		}
//		else {
//			shapepointergreaterthan<std::array<ScalarType, N>>::m_sortdim = sortdim;
//			std::sort(_points.begin(), _points.end(), shapepointergreaterthan<std::array<ScalarType, N>>());
//		}
//    }
//    void print(){ for(int i=0; i < _points.size(); i++) _points[i]->print(); }
//
//    std::vector<int> _indices; //holds a vector of point indices that make up this shape
//	std::vector<std::array<ScalarType, N>>* _points;
//};
//
///*!
//An N-D Simplex is stored here.
//A 0-simplex is a point.
//A 1-simplex is a line segment.
//A 2-simplex is a triangle.
//A 3-simplex is a tetrahedron.
//*/
//template <class ScalarType, int N>
//class Simplex : public Shape<ScalarType, N>{
//public:
//    Simplex() { }
//    Simplex(int npoints, std::array<ScalarType, N>* pts[]){
//        for (int i=0;i<npoints;i++) _points.push_back(pts[i]);
//    }
//    Simplex(std::array<ScalarType, N>& p1){
//        _points.push_back(&p1);
//    }
//    Simplex(std::array<ScalarType, N>& p1, std::array<ScalarType, N>& p2){
//        _points.push_back(&p1); _points.push_back(&p2);
//    }
//    Simplex(std::array<ScalarType, N>& p1, std::array<ScalarType, N>& p2, std::array<ScalarType, N>& p3){
//        _points.push_back(&p1); _points.push_back(&p2); _points.push_back(&p3);
//    }
//    Simplex(std::array<ScalarType, N>& p1, std::array<ScalarType, N>& p2, std::array<ScalarType, N>& p3, std::array<ScalarType, N>& p4){
//        _points.push_back(&p1); _points.push_back(&p2); _points.push_back(&p3); _points.push_back(&p4);
//    }
//    Simplex(Simplex<ScalarType, N>& s){ 
//        _points = s._points;
//    }
//    ~Simplex() { }
//    void addPoint(std::array<ScalarType, N>& p){ _points.push_back(&p); }
//    void addFrontPoint(std::array<ScalarType, N>* p){ _points.insert(_points.begin(),p); }
//	std::array<ScalarType, N>* lastPoint(){ return _points.back(); }
//	std::array<ScalarType, N>* frontPoint(){ return _points.front(); }
//    void removeLastPoint(){ _points.pop_back(); }
//    void removeFrontPoint(){ _points.pop_front(); }
//    ScalarType Determinant(){
//        int n = min(_points.size(),N+1);
//        //determinant for area, volume, and hypervolumes
//        Matrix<ScalarType,N+1,N+1> m;
//        for(int j=0;j<n;j++){
//        for(int i=0;i<n;i++){
//            if (j==n-1) (m[j])[i]=1;
//            else (m[j])[i]=(*_points[i])[j];
//        }}
//        return m.Determinant(n);
//    }
//    ScalarType EdgePointDeterminant(std::array<ScalarType, N>& p){
//        int n = min(_points.size(),N); n++;
//        //determinant for area, volume, and hypervolumes
//        Matrix<ScalarType,N+1,N+1> m;
//        for(int j=0;j<n;j++){
//        for(int i=0;i<n;i++){
//            if (j==n-1) (m[j])[i]=1;
//            else if (i==n-1) (m[j])[i]=p[j];
//            else (m[j])[i]=(*_points[i])[j];
//        }}
//        return m.Determinant(n);
//    }
//    ScalarType nVolume(){
//        //Calculate the n-Volume of the simplex created by connecting 
//        //a point to this edge. (Note: 1-volume = length, 2-volume = area, 
//        //3-volume = volume, ...)
//        int n = min(_points.size(),N+1);
//        return Determinant()/(ScalarType)factorial<n>::value;
//    }
//
//    //Simplex Vector Operators
//    bool operator < (std::array<ScalarType, N>& p){ return EdgePointDeterminant(p) < 0; }
//    bool operator > (std::array<ScalarType, N>& p){ return EdgePointDeterminant(p) > 0; }
//    bool operator <= (std::array<ScalarType, N>& p){ return EdgePointDeterminant(p) <= 0; }
//    bool operator >= (std::array<ScalarType, N>& p){ return EdgePointDeterminant(p) >= 0; }
//    bool operator == (std::array<ScalarType, N>& p){ return EdgePointDeterminant(p)==0; }
//    bool operator != (std::array<ScalarType, N>& p){ return EdgePointDeterminant(p)!=0; }
//    
//    //Simplex Simplex Operators
//    bool operator == (Simplex<ScalarType, N>& e){
//        int size = _points.size();
//        if (size!=e.size()) return false;
//        for (int i=0;i<size;i++){
//            bool foundit=false;
//            for (int j=0;j<size;j++){
//                if (_points[i] == &e[j]){
//                    foundit=true;
//                    break;
//                }
//            }
//            if (!foundit) return false;
//        }
//        //~ printf("TRUECOMPARE: \n"); 
//        //~ print();
//        //~ e.print();
//        //~ printf("\n");
//
//        return true;
//    }
//
//    Simplex<ScalarType, N>& operator = (Simplex<ScalarType, N>& s){
//        _points = s._points;
//    }
//
//    //Print out Simplex
//    void print(){
//        std::cout << index() << ":(";
//        int size = _points.size();
//        for (int i=0;i<size;i++){
//            if (i!=0) std::cout << ","; 
//            std::cout << _points[i]->index();
//        }
//        std::cout << ")\n";
//    }
//};
//
///*!
////This class handles ND-mesh processing in NDL.
//*/
//template <class ScalarType,int N>
//class Mesh : public Shape<ScalarType, N>{
//public:
//    Mesh() { destructflag=true; }
//    Mesh(Mesh<ScalarType, N>* m){ 
//        //This copy constructor does not do a deep copy,
//        destructflag=false;
//        _points = m->_points;
//        _e = m->_e;
//    }
//    Mesh(Mesh<ScalarType, N>& m){ 
//        //This copy constructor does a deep copy,
//        destructflag=true;
//        for(int i=0; i < _points.size(); i++) addPoint(m[i]);
//        //NOTE: STILL NEED TO IMPLEMENT DEEP COPY OF EDGES
//    }
//    Mesh<ScalarType, N>& operator = (const Mesh<ScalarType, N>* m){
//        empty();
//        //This assignment does not do a deep copy,
//        destructflag=false;
//        _points = m->_points;
//        _e = m->_e;
//        return *this;
//    }
//    ~Mesh() { empty(); }
//    void empty(){
//        emptyedges();
//        if (destructflag) for(int i=0; i < _points.size(); i++) delete _points[i]; 
//        _points.clear();
//    }
//    void emptyedges(){
//        if (destructflag) for(int i=0; i < _e.size(); i++) delete _e[i]; 
//        _e.clear();
//    }
//    int numEdges(){
//        return _e.size();
//    }
//    Simplex<ScalarType, N>& lastEdge(){
//        return *_e.back();
//    }
//	std::array<ScalarType, N>* addPoint(ScalarType x){
//		std::array<ScalarType, N>* p = new std::array<ScalarType, N>(x);
//        p->setIndex(_points.size());
//        _points.push_back(p);
//        return p;
//    }
//	std::array<ScalarType, N>* addPoint(ScalarType x,ScalarType y){
//		std::array<ScalarType, N>* p = new std::array<ScalarType, N>(x,y);
//        p->setIndex(_points.size());
//        _points.push_back(p);
//        return p;
//    }
//	std::array<ScalarType, N>* addPoint(ScalarType x,ScalarType y,ScalarType z){
//		std::array<ScalarType, N>* p = new std::array<ScalarType, N>(x,y,z);
//        p->setIndex(_points.size());
//        _points.push_back(p);
//        return p;
//    }
//	std::array<ScalarType, N>* addPoint(ScalarType x,ScalarType y,ScalarType z,ScalarType w){
//		std::array<ScalarType, N>* p = new std::array<ScalarType, N>(x,y,z,w);
//        p->setIndex(_points.size());
//        _points.push_back(p);
//        return p;
//    }
//	std::array<ScalarType, N>* addPoint(ScalarType x[]){
//		std::array<ScalarType, N>* p = new std::array<ScalarType, N>(x);
//        p->setIndex(_points.size());
//        _points.push_back(p);
//        return p;
//    }
//	std::array<ScalarType, N>* addPoint(std::array<ScalarType, N>& point){
//		std::array<ScalarType, N>* p = new std::array<ScalarType, N>(point);
//        p->setIndex(_points.size());
//        _points.push_back(p);
//        return p;
//    }
//	std::array<ScalarType, N>* addPoint(std::array<ScalarType, N>* point){
//        _points.push_back(point);
//        return point;
//    }
//    Simplex<ScalarType, N>* addEdge(){
//        Simplex<ScalarType, N>* e = new Simplex<ScalarType, N>();
//        e->setIndex(_e.size());
//        _e.push_back(e);
//        return e;
//    }
//    Simplex<ScalarType, N>* addEdge(std::array<ScalarType, N>& p1){
//        Simplex<ScalarType, N>* e = new Simplex<ScalarType, N>(p1);
//        e->setIndex(_e.size());
//        _e.push_back(e);
//        return e;
//    }
//    Simplex<ScalarType, N>* addEdge(std::array<ScalarType, N>& p1, std::array<ScalarType, N>& p2){
//        Simplex<ScalarType, N>* e = new Simplex<ScalarType, N>(p1,p2);
//        e->setIndex(_e.size());
//        _e.push_back(e);
//        return e;
//    }
//    Simplex<ScalarType, N>* addEdge(std::array<ScalarType, N>& p1, std::array<ScalarType, N>& p2, std::array<ScalarType, N>& p3){
//        Simplex<ScalarType, N>* e = new Simplex<ScalarType, N>(p1,p2,p3);
//        e->setIndex(_e.size());
//        _e.push_back(e);
//        return e;
//    }
//    Simplex<ScalarType, N>* addEdge(std::array<ScalarType, N>& p1, std::array<ScalarType, N>& p2, std::array<ScalarType, N>& p3, std::array<ScalarType, N>& p4){
//        Simplex<ScalarType, N>* e = new Simplex<ScalarType, N>(p1,p2,p3,p4);
//        e->setIndex(_e.size());
//        _e.push_back(e);
//        return e;
//    }
//    Simplex<ScalarType, N>* addEdge(int npoints, std::array<ScalarType, N>* pts[]){
//        Simplex<ScalarType, N>* e = new Simplex<ScalarType, N>(npoints,pts);
//        e->setIndex(_e.size());
//        _e.push_back(e);
//        return e;
//    }
//    Simplex<ScalarType, N>* addEdge(Simplex<ScalarType, N>& p){
//        Simplex<ScalarType, N>* e = new Simplex<ScalarType, N>(p);
//        e->setIndex(_e.size());
//        _e.push_back(e);
//        return e;
//    }
//    Simplex<ScalarType, N>* addEdge(Simplex<ScalarType, N>* p){
//        _e.push_back(p);
//        return p;
//    }
//    void print(){
//        printf("Points:\n");
//        Shape<ScalarType, N>::print();
//        printf("Simplexes:\n");
//        for(int i=0; i < _e.size(); i++) _e[i]->print();
//        int min=-8;
//        int max=5;
//        for(int y=min;y<max;y++){
//            for(int x=min;x<max;x++){
//                int index = -1;
//                for(int i=0; i < _points->size(); i++)
//				{
//                    int tx = ((*_points)[i])[0];
//                    int ty = ((*_points)[i])[1];
//					if (tx == x && ty == y) index = i;// _points[i]->index();
//                }
//                if (index > -1) printf("%d",index);
//                else printf(".");
//            }
//            printf("\n");
//        }
//    }
//
//    bool convexHull(){ //uses gift-wrap algorithm
//        bool stopflag;
//        emptyedges(); //clear out old edges
//        Mesh<ScalarType,N> tmesh(this); //process mesh to keep track of points and edges to be processed (this is not a deep copy)
//        tmesh.sort(1); //sort the points by their y dimension
//        tmesh.addEdge(addEdge(tmesh[0])); //add the first edge to the edge queue (Note: The first edge is a 1D edge--a point)
//        printf("Push: "); tmesh.lastEdge().print();
//        tmesh.sort2(0); //sort the points by their x dimension
//        
//        //process edges in queue until all done
//        do {
//            stopflag=true;
//            for(int i=0;i<size();i++){ //scan through all the points
//                if (tmesh.lastEdge()!=tmesh[i]){ //skip points that are incident,colinear,coplanar,etc.
//                    //add a new point to the edge
//                    tmesh.lastEdge().addPoint(tmesh[i]);
//                    printf("Push: "); tmesh.lastEdge().print();
//                
//                    //check against other points to see if it is valid
//                    bool newedgeisgood = true;
//                    for(int j=1;j < size();j++){ //compare new edge to each point
//                        if (tmesh.lastEdge() < *_points[j]){ //if we find a point outside of the new edge
//                            printf("badpoint ");
//                            newedgeisgood = false;
//                            break;
//                        }
//                    }
//                    
//                    //check against other edges to see if it is valid
//                    if (newedgeisgood)
//                    for(int j=0;j < _e.size();j++){ //compare new edge to each point
//                        if (&tmesh.lastEdge() != _e[j] && tmesh.lastEdge() == *_e[j]){
//                            printf("badedge ");
//                            newedgeisgood = false;
//                            break;
//                        }
//                    }
//                    
//                    if (newedgeisgood){
//                        printf("Good!\n");
//                        stopflag = false;
//                        
//                        //If the simplex is complete, pop it and spawn smaller edges
//                        if (tmesh.lastEdge().size() == N){
//                            Simplex<ScalarType, N>* parentedge = &tmesh.lastEdge();
//                            tmesh._e.pop_back();
//                            printf("POP: "); parentedge->print();
//                            
//                            //add children edges
//                            for(int i=parentedge->size()-1;i>=0;i--){
//                                //remove the point at i from the parentedge
//                                //to create a child edge
//								std::array<ScalarType, N>* temp = &(*parentedge)[i];
//                                parentedge->_points.erase(parentedge->_points.begin()+i);
//                                
//                                //ensure the edge is not allready stored
//                                bool addedgeflag=true;
//                                for(int j=0;j<_e.size();j++){
//                                    if (parentedge != _e[j] && *parentedge == *_e[j]){
//                                        addedgeflag=false;
//                                        break;
//                                    }
//                                }
//
//                                //add the edge
//                                if (addedgeflag){
//                                    Simplex<ScalarType, N>* newedge = addEdge(*parentedge);
//                                    for(int i=0;i<N;i++) newedge->sort(i);
//                                    tmesh.addEdge(newedge);
//                                    printf("Push: "); tmesh.lastEdge().print();                        
//                                }
//                                
//                                //put the point back
//                                parentedge->_points.insert(parentedge->_points.begin()+i,temp);
//                            }
//                        }
//                        
//                        break;
//                    } else {
//                        tmesh.lastEdge().removeLastPoint(); //remove the new point and try again
//                        printf("Pop: "); tmesh.lastEdge().print();
//                    }
//                } else printf("SKIPPED: %d\n",tmesh[i].index());
//            }
//        } while (!stopflag && tmesh.numEdges()>0);
//        return true;
//    }
//    
//    //member vars
//    bool destructflag; //keep track if we need to destruct the points and edges or not
//    std::vector<Simplex<ScalarType, N>*> _e; //holds all edges stored in this mesh
//};
//
//}; // end of namespace
//
//
//#endif