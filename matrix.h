#pragma once
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

#include <stdio.h>
#include <math.h>
#include <cmath>
#include <memory.h>
#include <algorithm>

template <class Real,int N>
class SVD;

/*! 
//This class handles NxN matricies where N is specified at compile time,
// This allows the compiler to optimize and speed up computation
*/
template<class Real, int N> 
class Matrix {
    public:    
        //Constructors
        Matrix(){ SetIdentity(); };
        Matrix(Real *values){ memcpy(m_data, values, N*N*sizeof(Real)); };
        
        //Methods
        bool operator==(const Matrix<Real,N> &m) const {
            bool result = true;
            for (unsigned int i=0; i<N*N && result; i++) result = (m_data[i]==m.m_data[i]);
            return result;
        };
        
        bool operator!=(const Matrix<Real,N> &m) const {
            bool result = false;
            for (unsigned int i=0; i<N*N && !result; i++) result = (m_data[i]!=m.m_data[i]);
            return result;
        };

        inline Real& ElementAt(unsigned int i, unsigned int j){
            return m_data[i*N+j];
        };

        //Calculate the Determinant (Also allows for computation 
        //of the determinant of a submatrix)
        Real Determinant(unsigned int subsize=N) const {
            switch (subsize){
                case 2: {
                    return m_data[0]*m_data[N+1]-m_data[1]*m_data[N];
                    break;
                };
                case 3: {
                    return	(m_data[0]*(m_data[N+1]*m_data[2*N+2]-m_data[N+2]*m_data[2*N+1]))
                          - (m_data[1]*(m_data[N]*m_data[2*N+2]-m_data[N+2]*m_data[2*N]))
                          + (m_data[2]*(m_data[N]*m_data[2*N+1]-m_data[N+1]*m_data[2*N]));
                };
                case 4: {
                    return m_data[3] * m_data[N+2] * m_data[2*N+1] * m_data[3*N] - m_data[2] * m_data[N+3] * m_data[2*N+1] * m_data[3*N]-
                    m_data[3] * m_data[N+1] * m_data[2*N+2] * m_data[3*N]+m_data[1] * m_data[N+3] * m_data[2*N+2] * m_data[3*N]+
                    m_data[2] * m_data[N+1] * m_data[2*N+3] * m_data[3*N]-m_data[1] * m_data[N+2] * m_data[2*N+3] * m_data[3*N]-
                    m_data[3] * m_data[N+2] * m_data[2*N] * m_data[3*N+1]+m_data[2] * m_data[N+3] * m_data[2*N] * m_data[3*N+1]+
                    m_data[3] * m_data[N] * m_data[2*N+2] * m_data[3*N+1]-m_data[0] * m_data[N+3] * m_data[2*N+2] * m_data[3*N+1]-
                    m_data[2] * m_data[N] * m_data[2*N+3] * m_data[3*N+1]+m_data[0] * m_data[N+2] * m_data[2*N+3] * m_data[3*N+1]+
                    m_data[3] * m_data[N+1] * m_data[2*N] * m_data[3*N+2]-m_data[1] * m_data[N+3] * m_data[2*N] * m_data[3*N+2]-
                    m_data[3] * m_data[N] * m_data[2*N+1] * m_data[3*N+2]+m_data[0] * m_data[N+3] * m_data[2*N+1] * m_data[3*N+2]+
                    m_data[1] * m_data[N] * m_data[2*N+3] * m_data[3*N+2]-m_data[0] * m_data[N+1] * m_data[2*N+3] * m_data[3*N+2]-
                    m_data[2] * m_data[N+1] * m_data[2*N] * m_data[3*N+3]+m_data[1] * m_data[N+2] * m_data[2*N] * m_data[3*N+3]+
                    m_data[2] * m_data[N] * m_data[2*N+1] * m_data[3*N+3]-m_data[0] * m_data[N+2] * m_data[2*N+1] * m_data[3*N+3]-
                    m_data[1] * m_data[N] * m_data[2*N+2] * m_data[3*N+3]+m_data[0] * m_data[N+1] * m_data[2*N+2] * m_data[3*N+3];
                };
                default: {
                    Real det = 0;
                    for (unsigned int j=0; j<subsize; j++){
                        if (m_data[j]!=0) det += m_data[j]*this->Cofactor(0, j,subsize);
                    }
                    return det;
                }
            };
        };
        
        Real Cofactor(unsigned int i, unsigned int j,unsigned int subsize=N) const {
            Real values[N*N];
            memset(values,0,N*N*sizeof(Real));
            int ac=0;
            for (int a=0;a<subsize;a++){
                if (a==i) continue;
                int bc=0;
                for (int b=0;b<subsize;b++){
                    if (b==j) continue;
                    values[ac*N+bc] = m_data[a*N+b];
                    bc++;
                }
                ac++;
            }
            Matrix<Real,N> temp(values);
            return (pow(Real(-1), Real(i+j))*temp.Determinant(subsize-1));
        };
        
        inline Real* operator[](const unsigned int i){
            return m_data + i*N;
        };

        Matrix<Real,N>& operator=(const Matrix<Real,N> &m){
            for (unsigned int i=0; i<N*N; i++) m_data[i] = m.m_data[i];
            return *this;
        };

        Matrix<Real,N>& operator=(Real s){
            for (unsigned int i=0; i<N*N; i++) m_data[i] = s;
            return *this;
        };
        
        Matrix<Real,N>& operator+=(const Matrix<Real,N> &m){
            for (unsigned int i=0; i<N*N; i++) m_data[i] += m.m_data[i];
            return *this;
        };

        Matrix<Real,N>& operator-=(const Matrix<Real,N> &m){
            for (unsigned int i=0; i<N*N; i++) m_data[i] -= m.m_data[i];
            return *this;
        };

        Matrix<Real,N>& operator+=(const Real k){
            for (unsigned int i=0; i<N*N; i++) m_data[i] += k;
            return *this;
        };

        Matrix<Real,N>& operator-=(const Real k){
            for (unsigned int i=0; i<N*N; i++) m_data[i] -= k;
            return *this;
        };

        Matrix<Real,N>& operator*=(const Real k){
            for (unsigned int i=0; i<N*N; i++) m_data[i] *= k;
            return *this;
        };

        Matrix<Real,N>& operator/=(const Real k){
            for (unsigned int i=0; i<N*N; i++) m_data[i] /= k;
            return *this;
        };

        Matrix<Real,N> operator*(Matrix<Real,N> &m) const {
            Matrix<Real,N> result;
            int p=0;
            for (int i=0; i<N; i++){
                for (int j=0; j<N; j++) {
                    Real temp = 0;
                    int q=0;
                    for (int k=0; k<N; k++){
                        temp+=(m_data[p+k]*m.m_data[q+j]);
                        q+=N;
                    }
                    result.m_data[p+j] = temp;
                }
                p+=N;
            }
            return result;
        };
        
       std::array<Real,N> operator*(std::array<Real,N>& p) const {
		   std::array<Real,N> result;
            int i=0;
            for (int r=0; r<N; ++r){
            for (int c=0; c<N; ++c){
                result[r] += m_data[i]*p[c];
                ++i;
            }}
            return result;
        };

      void transformpoint(Real* p) {
            Real result[N];
            Real t=0;
            int i=N*N-1;
            for (int c=N-1; c>=0; --c){
                t += m_data[i]*p[c];
                --i;
            }
            t=1/t;
            
            result[N-1]=1;
            for (int r=N-2; r>=0; --r){
                Real t2=0;
                for (int c=N-1; c>=0; --c){
                    t2 += m_data[i]*p[c];
                    --i;
                }
                result[r]=t2*t;
            }
            for(int i=0;i<N;++i) p[i]=result[i];
      };

        void DotProduct(std::array<Real,N> &m, std::array<Real,N> &result){
            unsigned int i, j,  p,  r;
            for (i=0, p=0, r=0; i<N; i++){ 
                result[i]=0;
                for (j=0; j<N; j++) result[i]+=(*this)[i][j]*m[j];
            }
        };

        void OuterProduct(std::array<Real,N> a, std::array<Real,N> b){
            Matrix<Real,N> result;
            for (int i=0; i<N; i++){
            for (int j=0; j<N; j++){
                (*this)[i][j] = a[i] * b[j];
            }}
        };

        Matrix<Real,N> operator+(const Real k){
            Matrix<Real,N> result;
            for (unsigned int i=0; i<N*N; i++) result.m_data[i] =  m_data[i]+k;
            return result;
        };

        Matrix<Real,N> operator-(const Real k){
            Matrix<Real,N> result;
            for (unsigned int i=0; i<N*N; i++) result.m_data[i] =  m_data[i]-k;
            return result;
        };

        Matrix<Real,N> operator-() const {
            Matrix<Real,N> result(m_data);
            for (unsigned int i=0; i<N*N; i++) result.m_data[i] = -1*m_data[i];
            return result;
        };

        Matrix<Real,N> operator*(const Real k) const {
            Matrix<Real,N> result;
            for (unsigned int i=0; i<N*N; i++) result.m_data[i] =  m_data[i]*k;
            return result;
        };

        Matrix<Real,N> operator/(const Real k){
            Matrix<Real,N> result;
            for (unsigned int i=0; i<N*N; i++) result.m_data[i] =  m_data[i]/k;
            return result;
        };

        void SetZero(){
            for (unsigned int i=0; i<N*N; i++) m_data[i] = Real(0.0);
        };

        void SetIdentity(){
            SetZero();
            for (unsigned int i=0, p=0; i<N; i++, p+=N) m_data[p+i] = Real(1.0);
        };

        void SetColumn(const unsigned int j, Real* v){
            unsigned int i, p;
            for (i=0, p=j; i<N; i++, p+=N) m_data[p] = v[i];
        };

        void SetRow(const unsigned int i, Real* v){
            unsigned int j, p;
            for (j=0, p=i*N; j<N; j++, p++) m_data[p] = v[j];
        };

        void SetDiagonal(Real *v){
            for (unsigned int i=0, p=0; i<N; i++, p+=N) m_data[p+i] = v[i];
        };
        
        //transpose the matrix (must be square)
        void Transpose(){
            Real temp[N*N];
            unsigned int i, j, p, q;
            for (i=0, p=0; i<N; i++, p+=N){
            for (j=0, q=0; j<N; j++, q+=N){
                temp[q+i] = m_data[p+j];
            }}
            std::swap(m_data, temp);
        };
        
        //return the transpose of the matrix
        Matrix<Real,N> GetTranspose(){
            Matrix<Real,N> result;
            unsigned int i, j, p, q;
            for (i=0, p=0; i<N; i++, p+=N){
            for (j=0, q=0; j<N; j++, q+=N){
                result.m_data[q+i] = m_data[p+j];
            }}
            return result;
        };
        
        //Invert the matrix (must be square)
        void Invert(){
            *this = GetInverse();
        }
        
        //Return the Inverse Matrix
        Matrix<Real,N> GetInverse(){
            Matrix<Real,N> U;
            Matrix<Real,N> V;
			std::array<Real,N> W;

            SVD<Real,N> svd(*this);
            svd.getU(U);
            svd.getV(V);
            svd.getSingularValues(W);
            
            unsigned  int i,j;
            for (i=0; i<N; i++){
            for (j=0; j<N; j++){
                V[i][j]/= W[j];
            }}
            return V * U.GetTranspose();
        }
        
        //Return the Eigenvalues and Eigenvectors of the matrix
        int EigenDecomposition(std::array<Real,N>& eigenvalues,Matrix<Real,N>& eigenvectors){
            Matrix<Real,N> W = (*this);
            int nrot=0;
            
            //Jacobi(W,eigenvalues,eigenvectors,nrot);
            
            return nrot; //return the number of jacobi rotate operations
        }
        

        //For Non-Homogeneous Coords
        Matrix<Real,N> &SetScale(std::array<Real,N> &t){
            SetIdentity();
            for (int i=0;i<N;i++) (*this)[i][i] = t[i];
            return *this;
        }

        //For Homogeneous Coords
        Matrix<Real,N> &SetScale(std::array<Real,N-1> &t){
            SetIdentity();
            for (int i=0;i<N-1;i++) (*this)[i][i] = t[i];
            return *this;
        }

        //For Homogeneous Coords
        Matrix<Real,N> &SetTranslate(std::array<Real,N-1> &t){
            SetIdentity();
            for (int i=0;i<N-1;i++) (*this)[i][N-1] = t[i];
            return *this;
        }

        //For either Homogeneous Coords or Non-Homogeneous Coords
        Matrix<Real,N> &SetRotate(Real AngleRad,int Axis1=0,int Axis2=1) {
            //Does a main rotation from Axis1 to Axis2
            for (int i=0; i<N; i++){
            for (int j=0; j<N; j++){
                if (i==j){
                    if (i==Axis1) (*this)[i][j] = cos(AngleRad);
                    else if (i==Axis2) (*this)[i][j] = cos(AngleRad);
                    else (*this)[i][j]=1;
                }
                else if (i==Axis1 && j==Axis2)  (*this)[i][j] = -sin(AngleRad);
                else if (i==Axis2 && j==Axis1)  (*this)[i][j] = sin(AngleRad);
                else (*this)[i][j] = 0;
            }}
            return *this;
        }
        
        Matrix<Real,N> &SetShear(Real amount,int Axis1=0,int Axis2=1){
            SetIdentity();
            if (Axis1!=Axis2) (*this)[Axis1][Axis2]=amount;
            return *this;
        }
        
        //For Homogeneous Coords
        Matrix<Real,N> &SetProjection(Real Dist,int Axis=0){
            //Perspective projection from the orgin along the specified 
            //axis onto a hyperplane at a distance Dist away
            SetZero();
            for (int i=0; i<N-1; i++) (*this)[i][i] = Dist;
            (*this)[N-1][Axis] = 1;
            return *this;
        }

        //For Homogeneous Coords
        Matrix<Real,N> &SetOrthoProjection(Real Dist,int Axis=0){
            //Perspective projection from the orgin along the specified 
            //axis onto a hyperplane at a distance Dist away
            SetIdentity();
            (*this)[Axis][Axis] = 0;
            (*this)[Axis][N-1] = Dist;
            return *this;
        }
        
        //Print the Matrix
        void print(char* msg=0){
            if (msg) printf("****************\n%s\n****************\n",msg);
            for (int j=0; j<N; j++){
                for (int i=0; i<N; i++) printf("% .4f ",(float)m_data[j*N+i]);
                std::cout << std::endl;
            }
            std::cout << "\n\n";
        };

        Real m_data[N*N];
    };    

    
    

//Singular Value Decomposition. Taken and modified from:
/* Template Numerical Toolkit (TNT): Linear Algebra Module
*
* Mathematical and Computational Sciences Division
* National Institute of Technology,
* Gaithersburg, MD USA
*
*
* This software was developed at the National Institute of Standards and
* Technology (NIST) by employees of the Federal Government in the course
* of their official duties. Pursuant to title 17 Section 105 of the
* United States Code, this software is not subject to copyright protection
* and is in the public domain. NIST assumes no responsibility whatsoever for
* its use by other parties, and makes no guarantees, expressed or implied,
* about its quality, reliability, or any other characteristic.
*/

template <class Real,int N>
class SVD {
	Matrix<Real,N> U;
    Matrix<Real,N> V;
	std::array<Real,N> s;
  public:
   SVD (const Matrix<Real,N> &Arg) {
      U.SetZero();
      V.SetZero();
       
	  std::array<Real,N> e;
	  std::array<Real,N> work;
	  Matrix<Real,N> A;
      A = Arg;
       
      const int wantu = 1;  					/* boolean */
      const int wantv = 1;  					/* boolean */
	  int i=0, j=0, k=0;

      // Reduce A to bidiagonal form, storing the diagonal elements
      // in s and the super-diagonal elements in e.
      const int nrt = N-2; //THIS ASSUMES N>1, OTHERWISE DO MAX(0,N-2)
      for (k = 0; k < N; k++) {
         if (k < N) {
            // Compute the transformation for the k-th column and
            // place the k-th diagonal in s[k].
            // Compute 2-norm of k-th column without under/overflow.
            s[k] = 0;
            for (i = k; i < N; i++) s[k] = hypot(s[k],A[i][k]);
            if (s[k] != Real(0.0)) {
               if (A[k][k] < Real(0.0)) s[k] = -s[k];
               for (i = k; i < N; i++) A[i][k] /= s[k];
               A[k][k] += Real(1.0);
            }
            s[k] = -s[k];
         }
         for (j = k+1; j < N; j++) {
            if ((k < N) && (s[k] != Real(0.0)))  {
               // Apply the transformation.
               double t = 0;
               for (i = k; i < N; i++) t += A[i][k]*A[i][j];
               t = -t/A[k][k];
               for (i = k; i < N; i++) A[i][j] += t*A[i][k];
            }

            // Place the k-th row of A into e for the
            // subsequent calculation of the row transformation.
            e[j] = A[k][j];
         }
         if (wantu & (k < N)) {
            // Place the transformation in U for subsequent back
            // multiplication.
            for (i = k; i < N; i++) U[i][k] = A[i][k];
         }
         if (k < nrt) {
            // Compute the k-th row transformation and place the
            // k-th super-diagonal in e[k].
            // Compute 2-norm without under/overflow.
            e[k] = 0;
            for (i = k+1; i < N; i++) e[k] = hypot(e[k],e[i]);
            if (e[k] != Real(0.0)) {
               if (e[k+1] < Real(0.0)) e[k] = -e[k];
               for (i = k+1; i < N; i++) e[i] /= e[k];
               e[k+1] += Real(1.0);
            }
            e[k] = -e[k];
            if ((k+1 < N) & (e[k] != Real(0.0))) {
               // Apply the transformation.
               for (i = k+1; i < N; i++) work[i] = Real(0.0);
                   
               for (j = k+1; j < N; j++){
               for (i = k+1; i < N; i++){
                  work[i] += e[j]*A[i][j];
               }}
               
               for (j = k+1; j < N; j++) {
                  double t = -e[j]/e[k+1];
                  for (i = k+1; i < N; i++) A[i][j] += t*work[i];
               }
            }
            if (wantv) {
               // Place the transformation in V for subsequent
               // back multiplication.
               for (i = k+1; i < N; i++) V[i][k] = e[i];
            }
         }
      }

      // Set up the final bidiagonal matrix or order p.
      int p = N;
      if (N < p) s[p-1] = Real(0.0);
      if (nrt+1 < p) e[nrt] = A[nrt][p-1];
      e[p-1] = Real(0.0);

      // If required, generate U.
      if (wantu) {
         for (k = N-1; k >= 0; k--) {
            if (s[k] != Real(0.0)) {
               for (j = k+1; j < N; j++) {
                  double t = 0;
                  for (i = k; i < N; i++) t += U[i][k]*U[i][j];
                  t = -t/U[k][k];
                  for (i = k; i < N; i++) U[i][j] += t*U[i][k];
               }
               for (i = k; i < N; i++ ) U[i][k] = -U[i][k];
               U[k][k] = Real(1.0) + U[k][k];
               for (i = 0; i < k-1; i++) U[i][k] = Real(0.0);
            } else {
               for (i = 0; i < N; i++) U[i][k] = Real(0.0);
               U[k][k] = Real(1.0);
            }
         }
      }

      // If required, generate V.
      if (wantv) {
         for (k = N-1; k >= 0; k--) {
            if ((k < nrt) & (e[k] != Real(0.0))) {
               for (j = k+1; j < N; j++) {
                  double t = 0;
                  for (i = k+1; i < N; i++) t += V[i][k]*V[i][j];
                  t = -t/V[k+1][k];
                  for (i = k+1; i < N; i++) V[i][j] += t*V[i][k];
               }
            }
            for (i = 0; i < N; i++) V[i][k] = Real(0.0);
            V[k][k] = Real(1.0);
         }
      }

      // Main iteration loop for the singular values.

      int pp = p-1;
      int iter = 0;
      double eps = pow(2.0,-52.0);
      while (p > 0) {
         int k=0;
		 int kase=0;

         // Here is where a test for too many iterations would go.

         // This section of the program inspects for
         // negligible elements in the s and e arrays.  On
         // completion the variables kase and k are set as follows.

         // kase = 1     if s(p) and e[k-1] are negligible and k<p
         // kase = 2     if s(k) is negligible and k<p
         // kase = 3     if e[k-1] is negligible, k<p, and
         //              s(k), ..., s(p) are not negligible (qr step).
         // kase = 4     if e(p-1) is negligible (convergence).

         for (k = p-2; k >= -1; k--) {
            if (k == -1) break;
            if (abs(e[k]) <= eps*(abs(s[k]) + abs(s[k+1]))) {
               e[k] = Real(0.0);
               break;
            }
         }
         if (k == p-2) kase = 4;
         else {
            int ks;
            for (ks = p-1; ks >= k; ks--) {
               if (ks == k) break;
               double t = (ks != p ? abs(e[ks]) : 0.) + (ks != k+1 ? abs(e[ks-1]) : 0.);
               if (abs(s[ks]) <= eps*t)  {
                  s[ks] = Real(0.0);
                  break;
               }
            }
            if (ks == k) kase = 3;
            else if (ks == p-1) kase = 1;
            else {
               kase = 2;
               k = ks;
            }
         }
         k++;

         // Perform the task indicated by kase.
         switch (kase) {
            // Deflate negligible s(p).
            case 1: {
               double f = e[p-2];
               e[p-2] = Real(0.0);
               for (j = p-2; j >= k; j--) {
                  double t = hypot(s[j],f);
                  double cs = s[j]/t;
                  double sn = f/t;
                  s[j] = t;
                  if (j != k) {
                     f = -sn*e[j-1];
                     e[j-1] = cs*e[j-1];
                  }
                  if (wantv) {
                     for (i = 0; i < N; i++) {
                        t = cs*V[i][j] + sn*V[i][p-1];
                        V[i][p-1] = -sn*V[i][j] + cs*V[i][p-1];
                        V[i][j] = t;
                     }
                  }
               }
            }
            break;
            // Split at negligible s(k).
            case 2: {
               double f = e[k-1];
               e[k-1] = Real(0.0);
               for (j = k; j < p; j++) {
                  double t = hypot(s[j],f);
                  double cs = s[j]/t;
                  double sn = f/t;
                  s[j] = t;
                  f = -sn*e[j];
                  e[j] = cs*e[j];
                  if (wantu) {
                     for (i = 0; i < N; i++) {
                        t = cs*U[i][j] + sn*U[i][k-1];
                        U[i][k-1] = -sn*U[i][j] + cs*U[i][k-1];
                        U[i][j] = t;
                     }
                  }
               }
            }
            break;
            // Perform one qr step.
            case 3: {
               // Calculate the shift.
               double scale = (std::max)((std::max)((std::max)((std::max)(abs(s[p-1]),abs(s[p-2])),abs(e[p-2])), abs(s[k])),abs(e[k]));
               double sp = s[p-1]/scale;
               double spm1 = s[p-2]/scale;
               double epm1 = e[p-2]/scale;
               double sk = s[k]/scale;
               double ek = e[k]/scale;
               double b = ((spm1 + sp)*(spm1 - sp) + epm1*epm1)/2.0;
               double c = (sp*epm1)*(sp*epm1);
               double shift = Real(0.0);
               if ((b != Real(0.0)) || (c != Real(0.0))) {
                  shift = sqrt(b*b + c);
                  if (b < Real(0.0)) shift = -shift;
                  shift = c/(b + shift);
               }
               double f = (sk + sp)*(sk - sp) + shift;
               double g = sk*ek;
   
               // Chase zeros.
               for (j = k; j < p-1; j++) {
                  double t = hypot(f,g);
                  double cs = f/t;
                  double sn = g/t;
                  if (j != k) {
                     e[j-1] = t;
                  }
                  f = cs*s[j] + sn*e[j];
                  e[j] = cs*e[j] - sn*s[j];
                  g = sn*s[j+1];
                  s[j+1] = cs*s[j+1];
                  if (wantv) {
                     for (i = 0; i < N; i++) {
                        t = cs*V[i][j] + sn*V[i][j+1];
                        V[i][j+1] = -sn*V[i][j] + cs*V[i][j+1];
                        V[i][j] = t;
                     }
                  }
                  t = hypot(f,g);
                  cs = f/t;
                  sn = g/t;
                  s[j] = t;
                  f = cs*e[j] + sn*s[j+1];
                  s[j+1] = -sn*e[j] + cs*s[j+1];
                  g = sn*e[j+1];
                  e[j+1] = cs*e[j+1];
                  if (wantu && (j < N-1)) {
                     for (i = 0; i < N; i++) {
                        t = cs*U[i][j] + sn*U[i][j+1];
                        U[i][j+1] = -sn*U[i][j] + cs*U[i][j+1];
                        U[i][j] = t;
                     }
                  }
               }
               e[p-2] = f;
               iter = iter + 1;
            }
            break;
            // Convergence.
            case 4: {
               // Make the singular values positive.
               if (s[k] <= Real(0.0)) {
                  s[k] = (s[k] < Real(0.0) ? -s[k] : Real(0.0));
                  if (wantv) {
                     for (i = 0; i <= pp; i++) {
                        V[i][k] = -V[i][k];
                     }
                  }
               }
               // Order the singular values.
               while (k < pp) {
                  if (s[k] >= s[k+1]) break;
                  double t = s[k];
                  s[k] = s[k+1];
                  s[k+1] = t;
                  if (wantv && (k < N-1)) {
                     for (i = 0; i < N; i++) {
                        t = V[i][k+1]; V[i][k+1] = V[i][k]; V[i][k] = t;
                     }
                  }
                  if (wantu && (k < N-1)) {
                     for (i = 0; i < N; i++) {
                        t = U[i][k+1]; U[i][k+1] = U[i][k]; U[i][k] = t;
                     }
                  }
                  k++;
               }
               iter = 0;
               p--;
            }
            break;
         }
      }
   }

   //Get U
   void getU(Matrix<Real,N> &A){
        int minm = N;
        for (int i=0; i<N; i++){
        for (int j=0; j<minm; j++){
            A[i][j] = U[i][j];
        }}
   }

   //Return the right singular vectors
   void getV(Matrix<Real,N> &A){ A = V; }

   //Return the one-dimensional array of singular values
   void getSingularValues(std::array<Real,N> &x){ x = s; }

   //Return the diagonal matrix of singular values
   void getS(Matrix<Real,N> &A) {
      for (int i = 0; i < N; i++) {
         for (int j = 0; j < N; j++) A[i][j] = Real(0.0);
         A[i][i] = s[i];
      }
   }

   //Two norm  (max(S))
   double norm2() { return s[0]; }

   //Two norm of condition number (max(S)/min(S))
   double cond() { return s[0]/s[N-1]; }

   //Effective numerical matrix rank
   int rank(){
      double eps = pow(2.0,-52.0);
      double tol = N*s[0]*eps;
      int r = 0;
      for (int i = 0; i < s.dim(); i++) {
         if (s[i] > tol) r++;
      }
      return r;
   }
   
    Real hypot(const Real &a, const Real &b){
        if (a== 0) return abs(b);
        else {
            Real c = b/a;
            return abs(a) * sqrt(1 + c*c);
        }
    }
   
};
    






//~ /** 

    //~ Computes eigenvalues and eigenvectors of a real (non-complex)
    //~ matrix. 
//~ <P>
    //~ If A is symmetric, then A = V*D*V' where the eigenvalue matrix D is
    //~ diagonal and the eigenvector matrix V is orthogonal. That is,
	//~ the diagonal values of D are the eigenvalues, and
    //~ V*V' = I, where I is the identity matrix.  The columns of V 
    //~ represent the eigenvectors in the sense that A*V = V*D.
    
//~ <P>
    //~ If A is not symmetric, then the eigenvalue matrix D is block diagonal
    //~ with the real eigenvalues in 1-by-1 blocks and any complex eigenvalues,
    //~ a + i*b, in 2-by-2 blocks, [a, b; -b, a].  That is, if the complex
    //~ eigenvalues look like
//~ <pre>

          //~ u + iv     .        .          .      .    .
            //~ .      u - iv     .          .      .    .
            //~ .        .      a + ib       .      .    .
            //~ .        .        .        a - ib   .    .
            //~ .        .        .          .      x    .
            //~ .        .        .          .      .    y
//~ </pre>
        //~ then D looks like
//~ <pre>

            //~ u        v        .          .      .    .
           //~ -v        u        .          .      .    . 
            //~ .        .        a          b      .    .
            //~ .        .       -b          a      .    .
            //~ .        .        .          .      x    .
            //~ .        .        .          .      .    y
//~ </pre>
    //~ This keeps V a real matrix in both symmetric and non-symmetric
    //~ cases, and A*V = V*D.
    
    
    
    //~ <p>
    //~ The matrix V may be badly
    //~ conditioned, or even singular, so the validity of the equation
    //~ A = V*D*inverse(V) depends upon the condition number of V.

   //~ <p>
	//~ (Adapted from JAMA, a Java Matrix Library, developed by jointly 
	//~ by the Mathworks and NIST (see  http://math.nist.gov/javanumerics/jama),
	//~ which in turn, were based on original EISPACK routines.
//~ **/

//~ template <class Real>
//~ class Eigenvalue
//~ {


   //~ /** Row and column dimension (square matrix).  */
    //~ int n;

   //~ int issymmetric; /* boolean*/

   //~ /** Arrays for internal storage of eigenvalues. */

   //~ Vector<Real> d;         /* real part */
   //~ Vector<Real> e;         /* img part */

   //~ /** Array for internal storage of eigenvectors. */
   //~ Matrix<Real> V;

   //~ /* Array for internal storage of nonsymmetric Hessenberg form.
   //~ @serial internal storage of nonsymmetric Hessenberg form.
   //~ */
   //~ Matrix<Real> H;
   

   //~ /* Working storage for nonsymmetric algorithm.
   //~ @serial working storage for nonsymmetric algorithm.
   //~ */
   //~ std::array<Real> ort;


   //~ // Symmetric Householder reduction to tridiagonal form.

   //~ void tred2() {

   //~ //  This is derived from the Algol procedures tred2 by
   //~ //  Bowdler, Martin, Reinsch, and Wilkinson, Handbook for
   //~ //  Auto. Comp., Vol.ii-Linear Algebra, and the corresponding
   //~ //  Fortran subroutine in EISPACK.

      //~ for (int j = 0; j < n; j++) {
         //~ d[j] = V[n-1][j];
      //~ }

      //~ // Householder reduction to tridiagonal form.
   
      //~ for (int i = n-1; i > 0; i--) {
   
         //~ // Scale to avoid under/overflow.
   
         //~ Real scale = Real(0.0);
         //~ Real h = Real(0.0);
         //~ for (int k = 0; k < i; k++) {
            //~ scale = scale + abs(d[k]);
         //~ }
         //~ if (scale == Real(0.0)) {
            //~ e[i] = d[i-1];
            //~ for (int j = 0; j < i; j++) {
               //~ d[j] = V[i-1][j];
               //~ V[i][j] = Real(0.0);
               //~ V[j][i] = Real(0.0);
            //~ }
         //~ } else {
   
            //~ // Generate Householder vector.
   
            //~ for (int k = 0; k < i; k++) {
               //~ d[k] /= scale;
               //~ h += d[k] * d[k];
            //~ }
            //~ Real f = d[i-1];
            //~ Real g = sqrt(h);
            //~ if (f > 0) {
               //~ g = -g;
            //~ }
            //~ e[i] = scale * g;
            //~ h = h - f * g;
            //~ d[i-1] = f - g;
            //~ for (int j = 0; j < i; j++) {
               //~ e[j] = Real(0.0);
            //~ }
   
            //~ // Apply similarity transformation to remaining columns.
   
            //~ for (int j = 0; j < i; j++) {
               //~ f = d[j];
               //~ V[j][i] = f;
               //~ g = e[j] + V[j][j] * f;
               //~ for (int k = j+1; k <= i-1; k++) {
                  //~ g += V[k][j] * d[k];
                  //~ e[k] += V[k][j] * f;
               //~ }
               //~ e[j] = g;
            //~ }
            //~ f = Real(0.0);
            //~ for (int j = 0; j < i; j++) {
               //~ e[j] /= h;
               //~ f += e[j] * d[j];
            //~ }
            //~ Real hh = f / (h + h);
            //~ for (int j = 0; j < i; j++) {
               //~ e[j] -= hh * d[j];
            //~ }
            //~ for (int j = 0; j < i; j++) {
               //~ f = d[j];
               //~ g = e[j];
               //~ for (int k = j; k <= i-1; k++) {
                  //~ V[k][j] -= (f * e[k] + g * d[k]);
               //~ }
               //~ d[j] = V[i-1][j];
               //~ V[i][j] = Real(0.0);
            //~ }
         //~ }
         //~ d[i] = h;
      //~ }
   
      //~ // Accumulate transformations.
   
      //~ for (int i = 0; i < n-1; i++) {
         //~ V[n-1][i] = V[i][i];
         //~ V[i][i] = Real(1.0);
         //~ Real h = d[i+1];
         //~ if (h != Real(0.0)) {
            //~ for (int k = 0; k <= i; k++) {
               //~ d[k] = V[k][i+1] / h;
            //~ }
            //~ for (int j = 0; j <= i; j++) {
               //~ Real g = Real(0.0);
               //~ for (int k = 0; k <= i; k++) {
                  //~ g += V[k][i+1] * V[k][j];
               //~ }
               //~ for (int k = 0; k <= i; k++) {
                  //~ V[k][j] -= g * d[k];
               //~ }
            //~ }
         //~ }
         //~ for (int k = 0; k <= i; k++) {
            //~ V[k][i+1] = Real(0.0);
         //~ }
      //~ }
      //~ for (int j = 0; j < n; j++) {
         //~ d[j] = V[n-1][j];
         //~ V[n-1][j] = Real(0.0);
      //~ }
      //~ V[n-1][n-1] = Real(1.0);
      //~ e[0] = Real(0.0);
   //~ } 

   //~ // Symmetric tridiagonal QL algorithm.
   
   //~ void tql2 () {

   //~ //  This is derived from the Algol procedures tql2, by
   //~ //  Bowdler, Martin, Reinsch, and Wilkinson, Handbook for
   //~ //  Auto. Comp., Vol.ii-Linear Algebra, and the corresponding
   //~ //  Fortran subroutine in EISPACK.
   
      //~ for (int i = 1; i < n; i++) {
         //~ e[i-1] = e[i];
      //~ }
      //~ e[n-1] = Real(0.0);
   
      //~ Real f = Real(0.0);
      //~ Real tst1 = Real(0.0);
      //~ Real eps = pow(2.0,-52.0);
      //~ for (int l = 0; l < n; l++) {

         //~ // Find small subdiagonal element
   
         //~ tst1 = max(tst1,abs(d[l]) + abs(e[l]));
         //~ int m = l;

        //~ // Original while-loop from Java code
         //~ while (m < n) {
            //~ if (abs(e[m]) <= eps*tst1) {
               //~ break;
            //~ }
            //~ m++;
         //~ }

   
         //~ // If m == l, d[l] is an eigenvalue,
         //~ // otherwise, iterate.
   
         //~ if (m > l) {
            //~ int iter = 0;
            //~ do {
               //~ iter = iter + 1;  // (Could check iteration count here.)
   
               //~ // Compute implicit shift
   
               //~ Real g = d[l];
               //~ Real p = (d[l+1] - g) / (2.0 * e[l]);
               //~ Real r = hypot(p, static_cast<Real>(Real(1.0)));
               //~ if (p < 0) {
                  //~ r = -r;
               //~ }
               //~ d[l] = e[l] / (p + r);
               //~ d[l+1] = e[l] * (p + r);
               //~ Real dl1 = d[l+1];
               //~ Real h = g - d[l];
               //~ for (int i = l+2; i < n; i++) {
                  //~ d[i] -= h;
               //~ }
               //~ f = f + h;
   
               //~ // Implicit QL transformation.
   
               //~ p = d[m];
               //~ Real c = Real(1.0);
               //~ Real c2 = c;
               //~ Real c3 = c;
               //~ Real el1 = e[l+1];
               //~ Real s = Real(0.0);
               //~ Real s2 = Real(0.0);
               //~ for (int i = m-1; i >= l; i--) {
                  //~ c3 = c2;
                  //~ c2 = c;
                  //~ s2 = s;
                  //~ g = c * e[i];
                  //~ h = c * p;
                  //~ r = hypot(p,e[i]);
                  //~ e[i+1] = s * r;
                  //~ s = e[i] / r;
                  //~ c = p / r;
                  //~ p = c * d[i] - s * g;
                  //~ d[i+1] = h + s * (c * g + s * d[i]);
   
                  //~ // Accumulate transformation.
   
                  //~ for (int k = 0; k < n; k++) {
                     //~ h = V[k][i+1];
                     //~ V[k][i+1] = s * V[k][i] + c * h;
                     //~ V[k][i] = c * V[k][i] - s * h;
                  //~ }
               //~ }
               //~ p = -s * s2 * c3 * el1 * e[l] / dl1;
               //~ e[l] = s * p;
               //~ d[l] = c * p;
   
               //~ // Check for convergence.
   
            //~ } while (abs(e[l]) > eps*tst1);
         //~ }
         //~ d[l] = d[l] + f;
         //~ e[l] = Real(0.0);
      //~ }
     
      //~ // Sort eigenvalues and corresponding vectors.
   
      //~ for (int i = 0; i < n-1; i++) {
         //~ int k = i;
         //~ Real p = d[i];
         //~ for (int j = i+1; j < n; j++) {
            //~ if (d[j] < p) {
               //~ k = j;
               //~ p = d[j];
            //~ }
         //~ }
         //~ if (k != i) {
            //~ d[k] = d[i];
            //~ d[i] = p;
            //~ for (int j = 0; j < n; j++) {
               //~ p = V[j][i];
               //~ V[j][i] = V[j][k];
               //~ V[j][k] = p;
            //~ }
         //~ }
      //~ }
   //~ }

   //~ // Nonsymmetric reduction to Hessenberg form.

   //~ void orthes () {
   
      //~ //  This is derived from the Algol procedures orthes and ortran,
      //~ //  by Martin and Wilkinson, Handbook for Auto. Comp.,
      //~ //  Vol.ii-Linear Algebra, and the corresponding
      //~ //  Fortran subroutines in EISPACK.
   
      //~ int low = 0;
      //~ int high = n-1;
   
      //~ for (int m = low+1; m <= high-1; m++) {
   
         //~ // Scale column.
   
         //~ Real scale = Real(0.0);
         //~ for (int i = m; i <= high; i++) {
            //~ scale = scale + abs(H[i][m-1]);
         //~ }
         //~ if (scale != Real(0.0)) {
   
            //~ // Compute Householder transformation.
   
            //~ Real h = Real(0.0);
            //~ for (int i = high; i >= m; i--) {
               //~ ort[i] = H[i][m-1]/scale;
               //~ h += ort[i] * ort[i];
            //~ }
            //~ Real g = sqrt(h);
            //~ if (ort[m] > 0) {
               //~ g = -g;
            //~ }
            //~ h = h - ort[m] * g;
            //~ ort[m] = ort[m] - g;
   
            //~ // Apply Householder similarity transformation
            //~ // H = (I-u*u'/h)*H*(I-u*u')/h)
   
            //~ for (int j = m; j < n; j++) {
               //~ Real f = Real(0.0);
               //~ for (int i = high; i >= m; i--) {
                  //~ f += ort[i]*H[i][j];
               //~ }
               //~ f = f/h;
               //~ for (int i = m; i <= high; i++) {
                  //~ H[i][j] -= f*ort[i];
               //~ }
           //~ }
   
           //~ for (int i = 0; i <= high; i++) {
               //~ Real f = Real(0.0);
               //~ for (int j = high; j >= m; j--) {
                  //~ f += ort[j]*H[i][j];
               //~ }
               //~ f = f/h;
               //~ for (int j = m; j <= high; j++) {
                  //~ H[i][j] -= f*ort[j];
               //~ }
            //~ }
            //~ ort[m] = scale*ort[m];
            //~ H[m][m-1] = scale*g;
         //~ }
      //~ }
   
      //~ // Accumulate transformations (Algol's ortran).

      //~ for (int i = 0; i < n; i++) {
         //~ for (int j = 0; j < n; j++) {
            //~ V[i][j] = (i == j ? Real(1.0) : Real(0.0));
         //~ }
      //~ }

      //~ for (int m = high-1; m >= low+1; m--) {
         //~ if (H[m][m-1] != Real(0.0)) {
            //~ for (int i = m+1; i <= high; i++) {
               //~ ort[i] = H[i][m-1];
            //~ }
            //~ for (int j = m; j <= high; j++) {
               //~ Real g = Real(0.0);
               //~ for (int i = m; i <= high; i++) {
                  //~ g += ort[i] * V[i][j];
               //~ }
               //~ // Double division avoids possible underflow
               //~ g = (g / ort[m]) / H[m][m-1];
               //~ for (int i = m; i <= high; i++) {
                  //~ V[i][j] += g * ort[i];
               //~ }
            //~ }
         //~ }
      //~ }
   //~ }


   //~ // Complex scalar division.

   //~ Real cdivr, cdivi;
   //~ void cdiv(Real xr, Real xi, Real yr, Real yi) {
      //~ Real r,d;
      //~ if (abs(yr) > abs(yi)) {
         //~ r = yi/yr;
         //~ d = yr + r*yi;
         //~ cdivr = (xr + r*xi)/d;
         //~ cdivi = (xi - r*xr)/d;
      //~ } else {
         //~ r = yr/yi;
         //~ d = yi + r*yr;
         //~ cdivr = (r*xr + xi)/d;
         //~ cdivi = (r*xi - xr)/d;
      //~ }
   //~ }


   //~ // Nonsymmetric reduction from Hessenberg to real Schur form.

   //~ void hqr2 () {
   
      //~ //  This is derived from the Algol procedure hqr2,
      //~ //  by Martin and Wilkinson, Handbook for Auto. Comp.,
      //~ //  Vol.ii-Linear Algebra, and the corresponding
      //~ //  Fortran subroutine in EISPACK.
   
      //~ // Initialize
   
      //~ int nn = this->n;
      //~ int n = nn-1;
      //~ int low = 0;
      //~ int high = nn-1;
      //~ Real eps = pow(2.0,-52.0);
      //~ Real exshift = Real(0.0);
      //~ Real p=0,q=0,r=0,s=0,z=0,t,w,x,y;
   
      //~ // Store roots isolated by balanc and compute matrix norm
   
      //~ Real norm = Real(0.0);
      //~ for (int i = 0; i < nn; i++) {
         //~ if ((i < low) || (i > high)) {
            //~ d[i] = H[i][i];
            //~ e[i] = Real(0.0);
         //~ }
         //~ for (int j = max(i-1,0); j < nn; j++) {
            //~ norm = norm + abs(H[i][j]);
         //~ }
      //~ }
   
      //~ // Outer loop over eigenvalue index
   
      //~ int iter = 0;
      //~ while (n >= low) {
   
         //~ // Look for single small sub-diagonal element
   
         //~ int l = n;
         //~ while (l > low) {
            //~ s = abs(H[l-1][l-1]) + abs(H[l][l]);
            //~ if (s == Real(0.0)) {
               //~ s = norm;
            //~ }
            //~ if (abs(H[l][l-1]) < eps * s) {
               //~ break;
            //~ }
            //~ l--;
         //~ }
       
         //~ // Check for convergence
         //~ // One root found
   
         //~ if (l == n) {
            //~ H[n][n] = H[n][n] + exshift;
            //~ d[n] = H[n][n];
            //~ e[n] = Real(0.0);
            //~ n--;
            //~ iter = 0;
   
         //~ // Two roots found
   
         //~ } else if (l == n-1) {
            //~ w = H[n][n-1] * H[n-1][n];
            //~ p = (H[n-1][n-1] - H[n][n]) / 2.0;
            //~ q = p * p + w;
            //~ z = sqrt(abs(q));
            //~ H[n][n] = H[n][n] + exshift;
            //~ H[n-1][n-1] = H[n-1][n-1] + exshift;
            //~ x = H[n][n];
   
            //~ // Real pair
   
            //~ if (q >= 0) {
               //~ if (p >= 0) {
                  //~ z = p + z;
               //~ } else {
                  //~ z = p - z;
               //~ }
               //~ d[n-1] = x + z;
               //~ d[n] = d[n-1];
               //~ if (z != Real(0.0)) {
                  //~ d[n] = x - w / z;
               //~ }
               //~ e[n-1] = Real(0.0);
               //~ e[n] = Real(0.0);
               //~ x = H[n][n-1];
               //~ s = abs(x) + abs(z);
               //~ p = x / s;
               //~ q = z / s;
               //~ r = sqrt(p * p+q * q);
               //~ p = p / r;
               //~ q = q / r;
   
               //~ // Row modification
   
               //~ for (int j = n-1; j < nn; j++) {
                  //~ z = H[n-1][j];
                  //~ H[n-1][j] = q * z + p * H[n][j];
                  //~ H[n][j] = q * H[n][j] - p * z;
               //~ }
   
               //~ // Column modification
   
               //~ for (int i = 0; i <= n; i++) {
                  //~ z = H[i][n-1];
                  //~ H[i][n-1] = q * z + p * H[i][n];
                  //~ H[i][n] = q * H[i][n] - p * z;
               //~ }
   
               //~ // Accumulate transformations
   
               //~ for (int i = low; i <= high; i++) {
                  //~ z = V[i][n-1];
                  //~ V[i][n-1] = q * z + p * V[i][n];
                  //~ V[i][n] = q * V[i][n] - p * z;
               //~ }
   
            //~ // Complex pair
   
            //~ } else {
               //~ d[n-1] = x + p;
               //~ d[n] = x + p;
               //~ e[n-1] = z;
               //~ e[n] = -z;
            //~ }
            //~ n = n - 2;
            //~ iter = 0;
   
         //~ // No convergence yet
   
         //~ } else {
   
            //~ // Form shift
   
            //~ x = H[n][n];
            //~ y = Real(0.0);
            //~ w = Real(0.0);
            //~ if (l < n) {
               //~ y = H[n-1][n-1];
               //~ w = H[n][n-1] * H[n-1][n];
            //~ }
   
            //~ // Wilkinson's original ad hoc shift
   
            //~ if (iter == 10) {
               //~ exshift += x;
               //~ for (int i = low; i <= n; i++) {
                  //~ H[i][i] -= x;
               //~ }
               //~ s = abs(H[n][n-1]) + abs(H[n-1][n-2]);
               //~ x = y = 0.75 * s;
               //~ w = -0.4375 * s * s;
            //~ }

            //~ // MATLAB's new ad hoc shift

            //~ if (iter == 30) {
                //~ s = (y - x) / 2.0;
                //~ s = s * s + w;
                //~ if (s > 0) {
                    //~ s = sqrt(s);
                    //~ if (y < x) {
                       //~ s = -s;
                    //~ }
                    //~ s = x - w / ((y - x) / 2.0 + s);
                    //~ for (int i = low; i <= n; i++) {
                       //~ H[i][i] -= s;
                    //~ }
                    //~ exshift += s;
                    //~ x = y = w = 0.964;
                //~ }
            //~ }
   
            //~ iter = iter + 1;   // (Could check iteration count here.)
   
            //~ // Look for two consecutive small sub-diagonal elements
   
            //~ int m = n-2;
            //~ while (m >= l) {
               //~ z = H[m][m];
               //~ r = x - z;
               //~ s = y - z;
               //~ p = (r * s - w) / H[m+1][m] + H[m][m+1];
               //~ q = H[m+1][m+1] - z - r - s;
               //~ r = H[m+2][m+1];
               //~ s = abs(p) + abs(q) + abs(r);
               //~ p = p / s;
               //~ q = q / s;
               //~ r = r / s;
               //~ if (m == l) {
                  //~ break;
               //~ }
               //~ if (abs(H[m][m-1]) * (abs(q) + abs(r)) <
                  //~ eps * (abs(p) * (abs(H[m-1][m-1]) + abs(z) +
                  //~ abs(H[m+1][m+1])))) {
                     //~ break;
               //~ }
               //~ m--;
            //~ }
   
            //~ for (int i = m+2; i <= n; i++) {
               //~ H[i][i-2] = Real(0.0);
               //~ if (i > m+2) {
                  //~ H[i][i-3] = Real(0.0);
               //~ }
            //~ }
   
            //~ // Double QR step involving rows l:n and columns m:n
   
            //~ for (int k = m; k <= n-1; k++) {
               //~ int notlast = (k != n-1);
               //~ if (k != m) {
                  //~ p = H[k][k-1];
                  //~ q = H[k+1][k-1];
                  //~ r = (notlast ? H[k+2][k-1] : Real(0.0));
                  //~ x = abs(p) + abs(q) + abs(r);
                  //~ if (x != Real(0.0)) {
                     //~ p = p / x;
                     //~ q = q / x;
                     //~ r = r / x;
                  //~ }
               //~ }
               //~ if (x == Real(0.0)) {
                  //~ break;
               //~ }
               //~ s = sqrt(p * p + q * q + r * r);
               //~ if (p < 0) {
                  //~ s = -s;
               //~ }
               //~ if (s != 0) {
                  //~ if (k != m) {
                     //~ H[k][k-1] = -s * x;
                  //~ } else if (l != m) {
                     //~ H[k][k-1] = -H[k][k-1];
                  //~ }
                  //~ p = p + s;
                  //~ x = p / s;
                  //~ y = q / s;
                  //~ z = r / s;
                  //~ q = q / p;
                  //~ r = r / p;
   
                  //~ // Row modification
   
                  //~ for (int j = k; j < nn; j++) {
                     //~ p = H[k][j] + q * H[k+1][j];
                     //~ if (notlast) {
                        //~ p = p + r * H[k+2][j];
                        //~ H[k+2][j] = H[k+2][j] - p * z;
                     //~ }
                     //~ H[k][j] = H[k][j] - p * x;
                     //~ H[k+1][j] = H[k+1][j] - p * y;
                  //~ }
   
                  //~ // Column modification
   
                  //~ for (int i = 0; i <= min(n,k+3); i++) {
                     //~ p = x * H[i][k] + y * H[i][k+1];
                     //~ if (notlast) {
                        //~ p = p + z * H[i][k+2];
                        //~ H[i][k+2] = H[i][k+2] - p * r;
                     //~ }
                     //~ H[i][k] = H[i][k] - p;
                     //~ H[i][k+1] = H[i][k+1] - p * q;
                  //~ }
   
                  //~ // Accumulate transformations
   
                  //~ for (int i = low; i <= high; i++) {
                     //~ p = x * V[i][k] + y * V[i][k+1];
                     //~ if (notlast) {
                        //~ p = p + z * V[i][k+2];
                        //~ V[i][k+2] = V[i][k+2] - p * r;
                     //~ }
                     //~ V[i][k] = V[i][k] - p;
                     //~ V[i][k+1] = V[i][k+1] - p * q;
                  //~ }
               //~ }  // (s != 0)
            //~ }  // k loop
         //~ }  // check convergence
      //~ }  // while (n >= low)
      
      //~ // Backsubstitute to find vectors of upper triangular form

      //~ if (norm == Real(0.0)) {
         //~ return;
      //~ }
   
      //~ for (n = nn-1; n >= 0; n--) {
         //~ p = d[n];
         //~ q = e[n];
   
         //~ // Real vector
   
         //~ if (q == 0) {
            //~ int l = n;
            //~ H[n][n] = Real(1.0);
            //~ for (int i = n-1; i >= 0; i--) {
               //~ w = H[i][i] - p;
               //~ r = Real(0.0);
               //~ for (int j = l; j <= n; j++) {
                  //~ r = r + H[i][j] * H[j][n];
               //~ }
               //~ if (e[i] < Real(0.0)) {
                  //~ z = w;
                  //~ s = r;
               //~ } else {
                  //~ l = i;
                  //~ if (e[i] == Real(0.0)) {
                     //~ if (w != Real(0.0)) {
                        //~ H[i][n] = -r / w;
                     //~ } else {
                        //~ H[i][n] = -r / (eps * norm);
                     //~ }
   
                  //~ // Solve real equations
   
                  //~ } else {
                     //~ x = H[i][i+1];
                     //~ y = H[i+1][i];
                     //~ q = (d[i] - p) * (d[i] - p) + e[i] * e[i];
                     //~ t = (x * s - z * r) / q;
                     //~ H[i][n] = t;
                     //~ if (abs(x) > abs(z)) {
                        //~ H[i+1][n] = (-r - w * t) / x;
                     //~ } else {
                        //~ H[i+1][n] = (-s - y * t) / z;
                     //~ }
                  //~ }
   
                  //~ // Overflow control
   
                  //~ t = abs(H[i][n]);
                  //~ if ((eps * t) * t > 1) {
                     //~ for (int j = i; j <= n; j++) {
                        //~ H[j][n] = H[j][n] / t;
                     //~ }
                  //~ }
               //~ }
            //~ }
   
         //~ // Complex vector
   
         //~ } else if (q < 0) {
            //~ int l = n-1;

            //~ // Last vector component imaginary so matrix is triangular
   
            //~ if (abs(H[n][n-1]) > abs(H[n-1][n])) {
               //~ H[n-1][n-1] = q / H[n][n-1];
               //~ H[n-1][n] = -(H[n][n] - p) / H[n][n-1];
            //~ } else {
               //~ cdiv(Real(0.0),-H[n-1][n],H[n-1][n-1]-p,q);
               //~ H[n-1][n-1] = cdivr;
               //~ H[n-1][n] = cdivi;
            //~ }
            //~ H[n][n-1] = Real(0.0);
            //~ H[n][n] = Real(1.0);
            //~ for (int i = n-2; i >= 0; i--) {
               //~ Real ra,sa,vr,vi;
               //~ ra = Real(0.0);
               //~ sa = Real(0.0);
               //~ for (int j = l; j <= n; j++) {
                  //~ ra = ra + H[i][j] * H[j][n-1];
                  //~ sa = sa + H[i][j] * H[j][n];
               //~ }
               //~ w = H[i][i] - p;
   
               //~ if (e[i] < Real(0.0)) {
                  //~ z = w;
                  //~ r = ra;
                  //~ s = sa;
               //~ } else {
                  //~ l = i;
                  //~ if (e[i] == 0) {
                     //~ cdiv(-ra,-sa,w,q);
                     //~ H[i][n-1] = cdivr;
                     //~ H[i][n] = cdivi;
                  //~ } else {
   
                     //~ // Solve complex equations
   
                     //~ x = H[i][i+1];
                     //~ y = H[i+1][i];
                     //~ vr = (d[i] - p) * (d[i] - p) + e[i] * e[i] - q * q;
                     //~ vi = (d[i] - p) * 2.0 * q;
                     //~ if ((vr == Real(0.0)) && (vi == Real(0.0))) {
                        //~ vr = eps * norm * (abs(w) + abs(q) +
                        //~ abs(x) + abs(y) + abs(z));
                     //~ }
                     //~ cdiv(x*r-z*ra+q*sa,x*s-z*sa-q*ra,vr,vi);
                     //~ H[i][n-1] = cdivr;
                     //~ H[i][n] = cdivi;
                     //~ if (abs(x) > (abs(z) + abs(q))) {
                        //~ H[i+1][n-1] = (-ra - w * H[i][n-1] + q * H[i][n]) / x;
                        //~ H[i+1][n] = (-sa - w * H[i][n] - q * H[i][n-1]) / x;
                     //~ } else {
                        //~ cdiv(-r-y*H[i][n-1],-s-y*H[i][n],z,q);
                        //~ H[i+1][n-1] = cdivr;
                        //~ H[i+1][n] = cdivi;
                     //~ }
                  //~ }
   
                  //~ // Overflow control

                  //~ t = max(abs(H[i][n-1]),abs(H[i][n]));
                  //~ if ((eps * t) * t > 1) {
                     //~ for (int j = i; j <= n; j++) {
                        //~ H[j][n-1] = H[j][n-1] / t;
                        //~ H[j][n] = H[j][n] / t;
                     //~ }
                  //~ }
               //~ }
            //~ }
         //~ }
      //~ }
   
      //~ // Vectors of isolated roots
   
      //~ for (int i = 0; i < nn; i++) {
         //~ if (i < low || i > high) {
            //~ for (int j = i; j < nn; j++) {
               //~ V[i][j] = H[i][j];
            //~ }
         //~ }
      //~ }
   
      //~ // Back transformation to get eigenvectors of original matrix
   
      //~ for (int j = nn-1; j >= low; j--) {
         //~ for (int i = low; i <= high; i++) {
            //~ z = Real(0.0);
            //~ for (int k = low; k <= min(j,high); k++) {
               //~ z = z + V[i][k] * H[k][j];
            //~ }
            //~ V[i][j] = z;
         //~ }
      //~ }
   //~ }

//~ public:


   //~ /** Check for symmetry, then construct the eigenvalue decomposition
   //~ @param A    Square real (non-complex) matrix
   //~ */

   //~ Eigenvalue(const Matrix<Real> &A) {
      //~ n = A.num_cols();
      //~ V = Matrix<Real>(n,n);
      //~ d = std::array<Real>(n);
      //~ e = std::array<Real>(n);

      //~ issymmetric = 1;
      //~ for (int j = 0; (j < n) && issymmetric; j++) {
         //~ for (int i = 0; (i < n) && issymmetric; i++) {
            //~ issymmetric = (A[i][j] == A[j][i]);
         //~ }
      //~ }

      //~ if (issymmetric) {
         //~ for (int i = 0; i < n; i++) {
            //~ for (int j = 0; j < n; j++) {
               //~ V[i][j] = A[i][j];
            //~ }
         //~ }
   
         //~ // Tridiagonalize.
         //~ tred2();
   
         //~ // Diagonalize.
         //~ tql2();

      //~ } else {
         //~ H = Matrix<Real>(n,n);
         //~ ort = std::array<Real>(n);
         
         //~ for (int j = 0; j < n; j++) {
            //~ for (int i = 0; i < n; i++) {
               //~ H[i][j] = A[i][j];
            //~ }
         //~ }
   
         //~ // Reduce to Hessenberg form.
         //~ orthes();
   
         //~ // Reduce Hessenberg to real Schur form.
         //~ hqr2();
      //~ }
   //~ }


   //~ /** Return the eigenvector matrix
   //~ @return     V
   //~ */

   //~ void getV (Matrix<Real> &V_) {
      //~ V_ = V;
      //~ return;
   //~ }

   //~ /** Return the real parts of the eigenvalues
   //~ @return     real(diag(D))
   //~ */

   //~ void getRealEigenvalues (std::array<Real> &d_) {
      //~ d_ = d;
      //~ return ;
   //~ }

   //~ /** Return the imaginary parts of the eigenvalues
   //~ in parameter e_.

   //~ @param e_: new matrix with imaginary parts of the eigenvalues.
   //~ */
   //~ void getImagEigenvalues (std::array<Real> &e_) {
      //~ e_ = e;
      //~ return;
   //~ }

   
//~ /** 
	//~ Computes the block diagonal eigenvalue matrix.
    //~ If the original matrix A is not symmetric, then the eigenvalue 
	//~ matrix D is block diagonal with the real eigenvalues in 1-by-1 
	//~ blocks and any complex eigenvalues,
    //~ a + i*b, in 2-by-2 blocks, [a, b; -b, a].  That is, if the complex
    //~ eigenvalues look like
//~ <pre>

          //~ u + iv     .        .          .      .    .
            //~ .      u - iv     .          .      .    .
            //~ .        .      a + ib       .      .    .
            //~ .        .        .        a - ib   .    .
            //~ .        .        .          .      x    .
            //~ .        .        .          .      .    y
//~ </pre>
        //~ then D looks like
//~ <pre>

            //~ u        v        .          .      .    .
           //~ -v        u        .          .      .    . 
            //~ .        .        a          b      .    .
            //~ .        .       -b          a      .    .
            //~ .        .        .          .      x    .
            //~ .        .        .          .      .    y
//~ </pre>
    //~ This keeps V a real matrix in both symmetric and non-symmetric
    //~ cases, and A*V = V*D.

	//~ @param D: upon return, the matrix is filled with the block diagonal 
	//~ eigenvalue matrix.
	
//~ */
   //~ void getD (Matrix<Real> &D) {
      //~ D = Matrix<Real>(n,n);
      //~ for (int i = 0; i < n; i++) {
         //~ for (int j = 0; j < n; j++) {
            //~ D[i][j] = Real(0.0);
         //~ }
         //~ D[i][i] = d[i];
         //~ if (e[i] > 0) {
            //~ D[i][i+1] = e[i];
         //~ } else if (e[i] < 0) {
            //~ D[i][i-1] = e[i];
         //~ }
      //~ }
   //~ }
//~ };
