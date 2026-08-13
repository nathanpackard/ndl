#pragma once
#include <cmath>
#include <array>
#include <algorithm>
#include "core.h"

// Matrix decomposition/factorization toolkit: determinant()/inverse(),
// SVD<Real,N>, and eigen_decomposition() -- free functions (plus one class
// that genuinely needs to hold decomposition state, SVD) built on top of
// Matrix<Real,N> rather than members of it, the same "core class + toolkit"
// split convolution.h/morphology.h/etc. have relative to Image. #include
// this directly if you use any of it; #include "matrix/core.h" alone does
// not pull it in (matching how #include <ndl/image.h> alone doesn't pull in
// convolution.h either) -- #include "matrix.h" (the umbrella header) for
// everything at once.

namespace ndl
{
	template<class Real, int N>
	Real cofactor(const Matrix<Real,N>& m, unsigned int i, unsigned int j, unsigned int subsize = N);

	namespace detail
	{
		template<class Real, int N>
		Real matrixDeterminant(const Matrix<Real,N>& m, unsigned int subsize)
		{
			switch (subsize){
				case 2: {
					return m[0][0]*m[1][1]-m[0][1]*m[1][0];
				};
				case 3: {
					return	(m[0][0]*(m[1][1]*m[2][2]-m[1][2]*m[2][1]))
						  - (m[0][1]*(m[1][0]*m[2][2]-m[1][2]*m[2][0]))
						  + (m[0][2]*(m[1][0]*m[2][1]-m[1][1]*m[2][0]));
				};
				case 4: {
					return m[0][3] * m[1][2] * m[2][1] * m[3][0] - m[0][2] * m[1][3] * m[2][1] * m[3][0]-
					m[0][3] * m[1][1] * m[2][2] * m[3][0]+m[0][1] * m[1][3] * m[2][2] * m[3][0]+
					m[0][2] * m[1][1] * m[2][3] * m[3][0]-m[0][1] * m[1][2] * m[2][3] * m[3][0]-
					m[0][3] * m[1][2] * m[2][0] * m[3][1]+m[0][2] * m[1][3] * m[2][0] * m[3][1]+
					m[0][3] * m[1][0] * m[2][2] * m[3][1]-m[0][0] * m[1][3] * m[2][2] * m[3][1]-
					m[0][2] * m[1][0] * m[2][3] * m[3][1]+m[0][0] * m[1][2] * m[2][3] * m[3][1]+
					m[0][3] * m[1][1] * m[2][0] * m[3][2]-m[0][1] * m[1][3] * m[2][0] * m[3][2]-
					m[0][3] * m[1][0] * m[2][1] * m[3][2]+m[0][0] * m[1][3] * m[2][1] * m[3][2]+
					m[0][1] * m[1][0] * m[2][3] * m[3][2]-m[0][0] * m[1][1] * m[2][3] * m[3][2]-
					m[0][2] * m[1][1] * m[2][0] * m[3][3]+m[0][1] * m[1][2] * m[2][0] * m[3][3]+
					m[0][2] * m[1][0] * m[2][1] * m[3][3]-m[0][0] * m[1][2] * m[2][1] * m[3][3]-
					m[0][1] * m[1][0] * m[2][2] * m[3][3]+m[0][0] * m[1][1] * m[2][2] * m[3][3];
				};
				default: {
					Real det = 0;
					for (unsigned int j=0; j<subsize; j++){
						if (m[0][j]!=0) det += m[0][j]*cofactor(m, 0, j, subsize);
					}
					return det;
				}
			};
		}
	}

	/// Determinant of the leading `subsize`x`subsize` submatrix (the whole matrix by default), via Laplace/cofactor expansion.
	/// @param m       Matrix to compute the determinant of.
	/// @param subsize Submatrix size; defaults to the full N.
	/// @return The determinant.
	/// @ingroup matrix
	template<class Real, int N>
	Real determinant(const Matrix<Real,N>& m, unsigned int subsize = N)
	{
		return detail::matrixDeterminant(m, subsize);
	}

	/// Cofactor of element (i,j) in the leading `subsize`x`subsize` submatrix (the whole matrix by default) -- determinant()'s own recursive building block (Laplace expansion), also independently useful (e.g. for an adjugate-matrix-based inverse).
	/// @param m       Matrix to compute the cofactor of.
	/// @param i       Row to exclude.
	/// @param j       Column to exclude.
	/// @param subsize Submatrix size; defaults to the full N.
	/// @return The (i,j) cofactor: (-1)^(i+j) times the determinant of the submatrix with row i and column j removed.
	/// @ingroup matrix
	template<class Real, int N>
	Real cofactor(const Matrix<Real,N>& m, unsigned int i, unsigned int j, unsigned int subsize)
	{
		Real values[N*N];
		memset(values,0,N*N*sizeof(Real));
		unsigned int ac=0;
		for (unsigned int a=0;a<subsize;a++){
			if (a==i) continue;
			unsigned int bc=0;
			for (unsigned int b=0;b<subsize;b++){
				if (b==j) continue;
				values[ac*N+bc] = m[a][b];
				bc++;
			}
			ac++;
		}
		Matrix<Real,N> temp(values);
		return (std::pow(Real(-1), Real(i+j))*detail::matrixDeterminant(temp, subsize-1));
	}

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
	/// @ingroup matrix
	template <class Real,int N>
	class SVD {
		Matrix<Real,N> U;
		Matrix<Real,N> V;
		std::array<Real,N> s;
	  public:
	   SVD (const Matrix<Real,N> &Arg) {
		  U.set_zero();
		  V.set_zero();

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
				for (i = k; i < N; i++) s[k] = std::hypot(s[k],A[i][k]);
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
				for (i = k+1; i < N; i++) e[k] = std::hypot(e[k],e[i]);
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
		  double eps = std::pow(2.0,-52.0);
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
				if (std::abs(e[k]) <= eps*(std::abs(s[k]) + std::abs(s[k+1]))) {
				   e[k] = Real(0.0);
				   break;
				}
			 }
			 if (k == p-2) kase = 4;
			 else {
				int ks;
				for (ks = p-1; ks >= k; ks--) {
				   if (ks == k) break;
				   double t = (ks != p ? std::abs(e[ks]) : 0.) + (ks != k+1 ? std::abs(e[ks-1]) : 0.);
				   if (std::abs(s[ks]) <= eps*t)  {
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
					  double t = std::hypot(s[j],f);
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
					  double t = std::hypot(s[j],f);
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
				   double scale = (std::max)((std::max)((std::max)((std::max)(std::abs(s[p-1]),std::abs(s[p-2])),std::abs(e[p-2])), std::abs(s[k])),std::abs(e[k]));
				   double sp = s[p-1]/scale;
				   double spm1 = s[p-2]/scale;
				   double epm1 = e[p-2]/scale;
				   double sk = s[k]/scale;
				   double ek = e[k]/scale;
				   double b = ((spm1 + sp)*(spm1 - sp) + epm1*epm1)/2.0;
				   double c = (sp*epm1)*(sp*epm1);
				   double shift = Real(0.0);
				   if ((b != Real(0.0)) || (c != Real(0.0))) {
					  shift = std::sqrt(b*b + c);
					  if (b < Real(0.0)) shift = -shift;
					  shift = c/(b + shift);
				   }
				   double f = (sk + sp)*(sk - sp) + shift;
				   double g = sk*ek;

				   // Chase zeros.
				   for (j = k; j < p-1; j++) {
					  double t = std::hypot(f,g);
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
					  t = std::hypot(f,g);
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

	   //left singular vectors
	   void u(Matrix<Real,N> &A) const {
			int minm = N;
			for (int i=0; i<N; i++){
			for (int j=0; j<minm; j++){
				A[i][j] = U[i][j];
			}}
	   }

	   //right singular vectors
	   void v(Matrix<Real,N> &A) const { A = V; }

	   //one-dimensional array of singular values
	   void singular_values(std::array<Real,N> &x) const { x = s; }

	   //diagonal matrix of singular values
	   void singular_value_matrix(Matrix<Real,N> &A) const {
		  for (int i = 0; i < N; i++) {
			 for (int j = 0; j < N; j++) A[i][j] = Real(0.0);
			 A[i][i] = s[i];
		  }
	   }

	   //two norm (max(S))
	   double norm() const { return s[0]; }

	   //two norm of condition number (max(S)/min(S))
	   double cond() const { return s[0]/s[N-1]; }

	   //effective numerical matrix rank
	   int rank() const {
		  double eps = std::pow(2.0,-52.0);
		  double tol = N*s[0]*eps;
		  int r = 0;
		  for (unsigned int i = 0; i < s.size(); i++) {
			 if (s[i] > tol) r++;
		  }
		  return r;
	   }

	};

	/// Returns the matrix inverse, computed via SVD.
	/// @param m Matrix to invert.
	/// @return A new matrix; `m` is unchanged (see invert() to mutate in place instead).
	/// @ingroup matrix
	template<class Real, int N>
	Matrix<Real,N> inverse(const Matrix<Real,N>& m)
	{
		Matrix<Real,N> U;
		Matrix<Real,N> V;
		std::array<Real,N> W;

		SVD<Real,N> svd(m);
		svd.u(U);
		svd.v(V);
		svd.singular_values(W);

		unsigned  int i,j;
		for (i=0; i<N; i++){
		for (j=0; j<N; j++){
			V[i][j]/= W[j];
		}}
		return V * U.transpose();
	}

	/// Inverts `m` in place, via inverse() (SVD-based).
	/// @param m Matrix to invert in place.
	/// @ingroup matrix
	template<class Real, int N>
	void invert(Matrix<Real,N>& m)
	{
		m = inverse(m);
	}

	// Classic cyclic Jacobi eigenvalue algorithm: repeatedly zeroes the
	// largest off-diagonal pair via a plane (Givens-style) rotation chosen
	// to eliminate exactly that pair, accumulating the rotations into
	// `eigenvectors`; the diagonal of the fully-rotated working copy is then
	// the eigenvalues. Requires a SYMMETRIC input (only ever reads the upper
	// triangle plus diagonal) -- guaranteed to converge to real eigenvalues
	// and an orthogonal eigenvector matrix for any real symmetric input,
	// unlike the general (possibly-complex-eigenvalue) non-symmetric case,
	// which this deliberately doesn't attempt. Well suited to the small N
	// (2-4) this library actually needs it for (optical_flow.h's and
	// feature_detection.h's structure tensors, both always symmetric
	// positive-semidefinite by construction, being sums of gradient outer
	// products) -- a handful of sweeps converges to machine precision for N
	// this small, not the hundreds a large N might need.
	//
	// Eigenpairs are returned sorted by DESCENDING eigenvalue (largest
	// first) -- the most immediately useful convention for this library's
	// own use (feature_detection.h wants the dominant-gradient-energy axis
	// first, the direct N-D generalization of SIFT's single dominant
	// orientation), and a single fixed convention every caller can rely on
	// rather than re-deriving. Eigenvectors are only ever defined up to
	// sign (v and -v are both valid for the same eigenvalue), and up to an
	// arbitrary rotation within any degenerate (repeated-eigenvalue)
	// subspace -- neither is resolved here; a caller needing a canonical
	// sign (e.g. feature_detection.h's orientation assignment) must
	// disambiguate it themselves from other information (there isn't any
	// more here to use).
	/// Eigendecomposition of a symmetric matrix via the classic Jacobi rotation algorithm. Eigenpairs are sorted by descending eigenvalue.
	/// @param m            Symmetric matrix (only the upper triangle + diagonal are read).
	/// @param eigenvalues  Output: the N eigenvalues, descending.
	/// @param eigenvectors Output: the corresponding eigenvectors as columns (eigenvectors[i][k] is component i of the k-th eigenvector), orthonormal.
	/// @param maxSweeps    Safety cap on the number of full sweeps. Defaults to 100 -- far more than the small N this library uses ever needs; only matters if convergence somehow stalls.
	/// @ingroup matrix
	template<class Real, int N, std::size_t M>
	void eigen_decomposition(const Matrix<Real,N>& m, std::array<Real,M>& eigenvalues, Matrix<Real,N>& eigenvectors, int maxSweeps = 100)
	{
		// N and M are independently-deduced template parameters (Matrix's
		// own N is int, std::array's own size is std::size_t -- the same
		// value can't be deduced as both simultaneously, the same reason
		// summed_area_table.h's rectangle_sum() takes its DIM as
		// std::size_t rather than int) rather than one shared symbol,
		// reconciled here instead of at deduction time.
		static_assert(M == (std::size_t)N, "ndl::eigen_decomposition() requires eigenvalues to have exactly N elements, matching the matrix's own dimension");
		Matrix<Real,N> a = m;
		eigenvectors.set_identity();

		for (int sweep = 0; sweep < maxSweeps; sweep++)
		{
			Real offDiagSum = 0;
			for (int p = 0; p < N; p++)
				for (int q = p + 1; q < N; q++)
					offDiagSum += std::abs(a(p, q));
			if (offDiagSum == Real(0)) break;

			for (int p = 0; p < N - 1; p++)
			{
				for (int q = p + 1; q < N; q++)
				{
					Real apq = a(p, q);
					if (apq == Real(0)) continue;

					Real app = a(p, p), aqq = a(q, q);
					Real theta = (aqq - app) / (Real(2) * apq);
					Real t = (theta >= Real(0) ? Real(1) : Real(-1)) / (std::abs(theta) + std::sqrt(theta * theta + Real(1)));
					Real c = Real(1) / std::sqrt(t * t + Real(1));
					Real s = t * c;
					Real tau = s / (Real(1) + c);

					a(p, p) = app - t * apq;
					a(q, q) = aqq + t * apq;
					a(p, q) = Real(0);
					a(q, p) = Real(0);

					for (int i = 0; i < N; i++)
					{
						if (i == p || i == q) continue;
						Real aip = a(i, p), aiq = a(i, q);
						a(i, p) = aip - s * (aiq + tau * aip);
						a(p, i) = a(i, p);
						a(i, q) = aiq + s * (aip - tau * aiq);
						a(q, i) = a(i, q);
					}
					for (int i = 0; i < N; i++)
					{
						Real vip = eigenvectors(i, p), viq = eigenvectors(i, q);
						eigenvectors(i, p) = vip - s * (viq + tau * vip);
						eigenvectors(i, q) = viq + s * (vip - tau * viq);
					}
				}
			}
		}

		for (int i = 0; i < N; i++) eigenvalues[i] = a(i, i);

		// Sort descending (a simple selection sort -- N is always tiny here).
		for (int i = 0; i < N - 1; i++)
		{
			int best = i;
			for (int j = i + 1; j < N; j++) if (eigenvalues[j] > eigenvalues[best]) best = j;
			if (best != i)
			{
				std::swap(eigenvalues[i], eigenvalues[best]);
				for (int r = 0; r < N; r++) std::swap(eigenvectors(r, i), eigenvectors(r, best));
			}
		}
	}
}
