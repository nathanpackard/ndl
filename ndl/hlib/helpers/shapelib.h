#include <math.h>

#ifndef SHAPELIB_H
#define SHAPELIB_H
/*-------------------------------*/
/* Shape Lib                     */
/*-------------------------------*/

//a little helper function
template <int X,int N>
struct Shapelib_Power{
    enum { value = X * Power<X,N - 1>::value };
};
template <int X>
struct Shapelib_Power<X,0>{
    enum { value = 1 };
};


//determine if a point lies on another point
template<class VoxelType,int DIM>
bool pointonpoint(VoxelType point[DIM],VoxelType point1[DIM],double thresholdsquared=9){
    double temp1[DIM];
    double distsquared=0;
    for(int i=0;i<DIM;i++){
        temp1[i]=point[i]-point1[i];
        distsquared+=temp1[i]*temp1[i];
    }
    if (distsquared<=thresholdsquared) return true;
    return false;
}

//determine if a point lies on a line
template<class VoxelType,int DIM>
bool pointonline(VoxelType point[DIM],VoxelType point1[DIM],VoxelType point2[DIM],double thresholdsquared=9){
    double dp[DIM];
    double temp1[DIM];
    double u_numerator=0;
    double u_denominator=0;
    for(int i=0;i<DIM;i++){
        u_numerator+=(point[i]-point1[i])*(point2[i]-point1[i]);
        dp[i] = point2[i]-point1[i];
        u_denominator+=dp[i]*dp[i];
    }
    double u;
    if (u_denominator==0) u=0;
    else u=u_numerator/u_denominator;
    double distsquared=0;
    for(int i=0;i<DIM;i++){
        temp1[i]=point[i]-point1[i]-u*dp[i];
        distsquared+=temp1[i]*temp1[i];
    }
    
    //printf("distsquared: %f\nthresholdsquared: %f\n",distsquared,thresholdsquared);
    if (u>=0 && u<=1 && distsquared<=thresholdsquared) return true;
    return false;
}

//If point is equal to an endpoint (point1 or point2), set point2 to the selected endpoint and set point1 to the other point
template<class VoxelType,int DIM>
bool getlineendpoint(VoxelType point[DIM],VoxelType point1[DIM],VoxelType point2[DIM],double thresholdsquared=9){
    if (pointonpoint<VoxelType,DIM>(point,point2)){
        for(int i=0;i<DIM;i++) point2[i] = point[i];
    } else
    if (pointonpoint<VoxelType,DIM>(point,point1)){
        for(int i=0;i<DIM;i++){
            point1[i] = point2[i];
            point2[i] = point[i];
        }
    } else return false;
    return true;
}

//determine if a point lies within a cube
template<class VoxelType,int DIM>
bool pointincube(VoxelType point[DIM],VoxelType point1[DIM],VoxelType point2[DIM],double threshold=0){
    bool test=1;
    for(int i=0;i<DIM;i++){
        test = (test && ((point[i]>=point1[i]-threshold && point[i]<=point2[i]+threshold) || (point[i]>=point2[i]-threshold && point[i]<=point1[i]+threshold)));
    }
    return test;
}

//If point is equal to a corner of the cube defined by point1 and point2, set point2 to the selected corner and set point1 to the most distant point from point2
template<class VoxelType,int DIM>
bool getcubecorner(VoxelType point[DIM],VoxelType point1[DIM],VoxelType point2[DIM],double thresholdsquared=9){
    VoxelType temp1[DIM],temp2[DIM];
    for(int a=0;a<Shapelib_Power<2,DIM>::value;a++){
        for(int n=0;n<DIM;n++){
            int i = ((a >> n) & 1);
            if (i){
                temp1[n] = point1[n];
                temp2[n] = point2[n];
            } else {
                temp1[n] = point2[n];
                temp2[n] = point1[n];
            }
        }
        if (pointonpoint<VoxelType,DIM>(point,temp2)){
            for(int i=0;i<DIM;i++){
                point1[i] = temp1[i];
                point2[i] = temp2[i];
            }
            return true;
        }
    }
    return false;
}

//NOT DONE YET!!!
template<class VoxelType,int DIM>
int getcubesideindex(VoxelType point[DIM],VoxelType point1[DIM], VoxelType point2[DIM],int* sidedim,int* side,double threshold=3){
    VoxelType sideapoint1[DIM],sideapoint2[DIM],sidebpoint1[DIM],sidebpoint2[DIM];
    for(int a=0;a<DIM;a++){
        for(int n=0;n<DIM;n++){
            if (n!=a){
                sideapoint1[n] = sidebpoint1[n] = point1[n];
                sideapoint2[n] = sidebpoint2[n] = point2[n];
            } else {
                sideapoint1[n] = point1[n];
                sideapoint2[n] = point1[n];
                sidebpoint1[n] = point2[n];
                sidebpoint2[n] = point2[n];
            }
        }
        
        if (pointincube<VoxelType,DIM>(point,sideapoint1,sideapoint2,threshold)){
            if (point1[a]>=point2[a]){
                for(int n=0;n<DIM;n++){
                    VoxelType t = point1[n];
                    point1[n] = point2[n];
                    point2[n] = t;
                }
            }
            *side = 0;
            *sidedim = a;
            return true;
        }
        if (pointincube<VoxelType,DIM>(point,sidebpoint1,sidebpoint2,threshold)){
            if (point1[a]>=point2[a]){
                for(int n=0;n<DIM;n++){
                    VoxelType t = point1[n];
                    point1[n] = point2[n];
                    point2[n] = t;
                }
            }
            *side = 1;
            *sidedim = a;
            return true;
        }
    }
    return false;
}

template<class VoxelType,int DIM>
bool pointinsphere(VoxelType point[DIM],VoxelType center[DIM],double radius){
    double sumofsquares=0;
    for(int i=0;i<DIM;i++){
        double diff = point[i]-center[i];
        sumofsquares+=diff*diff;
    }
    return (sqrt(sumofsquares)<=radius);
}

template<class VoxelType,int DIM>
bool pointonsphereedge(VoxelType point[DIM],VoxelType center[DIM], double radius,double threshold=3){
    double sumofsquares=0;
    for(int i=0;i<DIM;i++){
        double diff = point[i]-center[i];
        sumofsquares+=diff*diff;
    }
    return abs(radius-sqrt(sumofsquares))<=threshold;
}

template<class VoxelType,int DIM>
double getradius(VoxelType point1[DIM],VoxelType point2[DIM]){
    double sumofsquares=0;
    for(int i=0;i<DIM;i++){
        double diff = (point1[i]-point2[i])/2.0;
        sumofsquares+=diff*diff;
    }
    return sqrt(sumofsquares);
}

//get radius of a (n-1)-sphere a distance d from the center of an n-sphere
//(eg: radius of a circle cutting through a sphere at a distance d from the center of the sphere)
double radiusfromnsphere(double dist, double radius){
    if (abs(dist)>abs(radius)) return 0;
    return sqrt(radius*radius-dist*dist);
}

#endif
