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

#ifndef MATHHELPERS_INCLUDED
#define MATHHELPERS_INCLUDED

//  Redefine the manifest constants in math.h to properly implement
//  long double precision. This fixes an oversight (or a misguided
//  commitment to 53-bit double precision) in many versions of math.h
#undef M_E             /*  e           */
#undef M_LOG2E         /*  log_2(e)    */
#undef M_LOG10E        /*  log_10(e)   */
#undef M_LN2           /*  ln(2)       */
#undef M_LN10          /*  ln(10)      */
#undef M_PI            /*  pi          */
#undef M_PI_2          /*  pi/2        */
#undef M_PI_4          /*  pi/4        */
#undef M_1_PI          /*  1/pi        */
#undef M_2_PI          /*  2/pi        */
#undef M_2_SQRTPI      /*  2/sqrt(pi)  */
#undef M_SQRT2         /*  sqrt(2)     */
#undef M_SQRT1_2       /*  sqrt(1/2)   */
#undef PI              /*  in accordance with C99 */
#undef PI2             /*  in accordance with C99 */

#include <math.h>

// Corrected values are given to 50 decimal places (at least 165-bit precision).
#define M_E             2.71828182845904523536028747135266249775724709369996L
#define M_LOG2E	        1.44269504088896340735992468100189213742664595415299L
#define M_LOG10E        0.43429448190325182765112891891660508229439700580367L
#define M_LN2           0.69314718055994530941723212145817656807550013436026L
#define M_LN10          2.30258509299404568401799145468436420760110148862877L
#define M_PI            3.14159265358979323846264338327950288419716939937511L
#define M_PI_2          1.57079632679489661923132169163975144209858469968755L
#define M_PI_4          0.78539816339744830961566084581987572104929234984378L
#define M_1_PI          0.31830988618379067153776752674502872406891929148091L
#define M_2_PI          0.63661977236758134307553505349005744813783858296183L
#define M_2_SQRTPI      1.12837916709551257389615890312154517168810125865800L
#define M_SQRT2         1.41421356237309504880168872420969807856967187537695L
#define M_SQRT1_2       0.70710678118654752440084436210484903928483593768847L


//**********************************
//HELPERS
//**********************************

/** taken from (then modified):
    operators : Basic arithmetic operation using INFTY numbers
 * David Coeurjolly (david.coeurjolly@liris.cnrs.fr) - Sept. 2004
**/

#define I_INFTY 100000001

// The sum of a and b handling I_INFTY
int inf_sum(int a, int b){
  if ((a==I_INFTY) || (b==I_INFTY)) return I_INFTY;
  else return a+b;
}

//The product of a and b handling I_INFTY
int inf_prod(int a, int b){
  if ((a==I_INFTY) || (b==I_INFTY)) return I_INFTY;  
  else return a*b;
}

//The opposite of a handling I_INFTY
int inf_opp(int a){
  if (a == I_INFTY) return I_INFTY;
  else return -a;
}

// The division (integer) of divid out of divis handling I_INFTY
int inf_intdivint(int divid, int divis) {
  if (divis == 0) return  I_INFTY;
  if (divid == I_INFTY) return  I_INFTY;
  else return  divid / divis;
}
    
//Functions SDT_F and SDT_Sep for the SDT labelling
//return Definition of a parabola
long SDT_F(int x, int i, long gi2){
  return inf_sum((x-i)*(x-i), gi2);
}

//return The abscissa of the intersection point between two parabolas
long SDT_Sep(int i, int u, long gi2, long gu2){
  return inf_intdivint(inf_sum( inf_sum((long) (u*u - i*i),gu2), inf_opp(gi2) ), 2*(u-i));
}

//MORE HELPERS
int nextpowof2(int val){
    int powof2 = 1;
    while( powof2 < val ) powof2 <<= 1;
    return powof2;
}

//Template based power function (for positive integers)
//eg:  printf("%d\n",Power<3,5>::value);
template <int X,int N>
struct Power{
    enum { value = X * Power<X,N - 1>::value };
};
template <int X>
struct Power<X,0>{
    enum { value = 1 };
};

//Template based power factorial function
//eg:  printf("%d\n",Factorial<4>::value);
template <int N>
struct Factorial{
    enum { value = N * Factorial<N - 1>::value };
};
template <>
struct Factorial<0>{
    enum { value = 1 };
};

//~ template <int MANTISSA, int EXPONENT = 0>
//~ struct Float {
    //~ enum { 
        //~ mantissa = MANTISSA,
        //~ exponent = EXPONENT
    //~ };
//~ };

//~ template <class X,int N>
//~ struct FPower{
    //~ enum { 
        //~ mantissa = X::mantissa * FPower<X,N - 1>::mantissa,
        //~ exponent = X::exponent + FPower<X,N - 1>::exponent
    //~ };
//~ };

//~ template <class X>
//~ struct FPower<X,0>{
    //~ enum { 
        //~ mantissa = 1,
        //~ exponent = 0
    //~ };
//~ };

//~ template <class X>
//~ struct Sin {
    //~ enum {
        //~ mantissa = X::mantissa*Factorial<3>::value,
        //~ exponent = X::exponent+Factorial<3>::value
    //~ };
    
    //~ X::value - FPower<X,3>/Factorial<3>::value + FPower<X,5>/Factorial<5>::value - FPower<X,7>/Factorial<7>::value + FPower<X,9>/Factorial<9>::value - FPower<X,11>/Factorial<11>::value
    
    //~ X - X^3/3! + X^5/5! - X^7/7! + X^9/9! - X^11/11!
    
//~ };

//~ sine
//~ Y = X - X^3/3! + X^5/5! - X^7/7! + X^9/9! - X^11/11!

//~ cosine
//~ Y = 1 - X^2/2! + x^4/4! - X^6/6! + X^8/8! - X^10/10!

//used to determine how many 1D interpolations are needed in an ND space
template <int X,int N>
struct NumLines{
    enum { value = X * NumLines<X,N - 1>::value + 1};
};
template <int X>
struct NumLines<X,1>{
    enum { value = 1 };
};

//power function for integers
int powx(int a, int b){
    int base=a, res=1;
    while(b){
        if(b&1) res*=base;
        b>>=1;
        base*=base;
    }
    return res;
}

template<class T>T linear_interpolation(double p,T v1,T v2){
    return v1*(1-p) + v2*p;
}

template<class T>T cubic_interpolation(double p,T v1,T v2,T v3,T v4){
    double p2 = p*p;
    double p3 = p2*p;
    double w = p2 + p2;
    double r = p3 + p3;
    double s= w + p2;
    double a0 = r - s + 1;
    double a1 = p3 - w + p;
    double a2 = p3 - p2;
    double a3 = s - r;
    //This is a Hermite cubic w/0 bias and 0 tension
    double v3mv2 = v3 - v2;
    double m0  = v3mv2 + v2 - v1;
    double m1  = v3mv2 + v4 - v3;
    return a0*v2 + 0.5*(a1*m0 + a2*m1) + a3*v3;
}

//comparison function for sorting the second element of std::pair elemnets
template<class T> struct less_second : std::binary_function<T,T,bool>{
   inline bool operator()(const T& lhs, const T& rhs){ return lhs.second < rhs.second; }
};
template<class T> struct greater_second : std::binary_function<T,T,bool>{
   inline bool operator()(const T& lhs, const T& rhs){ return lhs.second > rhs.second; }
};


#define CLAMP(x, l, h) (((x) > (h)) ? (h) : (((x) < (l)) ? (l) : (x))) 
#define MAX2(l, h) ((l) > (h) ? (l) : (h))

//~ template <class T>
//~ T CLAMP(T x, T l, T h){
    //~ return (x > h) ? h : ((x < l) ? l : x);
//~ }

//******************************
//END HELPERS
//******************************


#endif