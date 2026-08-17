#pragma once
#include <cstring>
#include <cmath>
#include <array>
#include <iostream>

namespace ndl
{
	/// A fixed-size, compile-time-dimensioned NxN matrix. Independent of Image<T,DIM> --
	/// used for the geometric transforms (matrix/transform.h) that operate on individual
	/// points, not per-pixel over a whole Image, and for the structure-tensor / local
	/// linear-algebra math optical_flow.h and feature_detection.h build on
	/// (matrix/decomposition.h).
	///
	/// This is the CORE class only: construction, element access, arithmetic, and
	/// transpose -- the same scope image/core.h's own Image<T,DIM> has (no convolution or
	/// morphology there either). Determinant, inverse, and eigendecomposition
	/// (matrix/decomposition.h) and the scale/translate/rotate/etc. transform builders
	/// (matrix/transform.h) are toolkits built ON TOP of this class, as free functions,
	/// not members -- the same "core class + free-function toolkits" split this whole
	/// library uses for Image itself (convolve()/erode()/etc. aren't Image members
	/// either). #include "matrix.h" (the umbrella header) to get everything at once.
	///
	/// @tparam Real Element type (float or double).
	/// @tparam N    Matrix dimension (NxN); also the coordinate dimension for
	///              matrix/transform.h's homogeneous-coordinate helpers, where an
	///              N-dimensional Matrix transforms (N-1)-dimensional points.
	/// @ingroup matrix
	template<class Real, int N>
	class Matrix {
		public:
			//Constructors
			/// Constructs an identity matrix.
			Matrix(){ set_identity(); };
			/// Constructs from a row-major array of N*N values.
			/// @param values Row-major array of exactly N*N elements, copied in.
			explicit Matrix(const Real *values){ memcpy(data_, values, N*N*sizeof(Real)); };

			/// Builds a matrix from a COLUMN-major array of N*N values -- the layout convention
			/// OpenGL/WebGL's own uniform-matrix upload (gl.uniformMatrix3fv et al.) and this
			/// project's own browser-side code (web/ndlviewer.js's mat3Multiply()/rotationMatrix())
			/// use, as opposed to this class's own row-major storage/constructor above. A named
			/// static factory rather than a second same-signature constructor (C++ can't overload
			/// on layout alone) -- named so a call site reads as "the SAME matrix a column-major
			/// caller means," not a different matrix: `m(row,col) = values[col*N+row]` maps each
			/// column-major entry onto the mathematically identical element of this row-major
			/// storage, so every operator (`*`, `transpose()`, ...) afterward computes exactly what
			/// the column-major caller's own convention would.
			/// @param values Column-major array of exactly N*N elements.
			/// @return A new matrix, mathematically identical to `values` under its own column-major reading.
			static Matrix<Real,N> from_column_major(const Real *values)
			{
				Matrix<Real,N> m;
				for (int col = 0; col < N; col++)
					for (int row = 0; row < N; row++)
						m(row, col) = values[col * N + row];
				return m;
			}

			//Methods
			/// Element-wise equality.
			/// @param m Matrix to compare against.
			/// @return True if every element matches exactly (no tolerance).
			bool operator==(const Matrix<Real,N> &m) const {
				bool result = true;
				for (unsigned int i=0; i<N*N && result; i++) result = (data_[i]==m.data_[i]);
				return result;
			};

			bool operator!=(const Matrix<Real,N> &m) const {
				bool result = false;
				for (unsigned int i=0; i<N*N && !result; i++) result = (data_[i]!=m.data_[i]);
				return result;
			};

			// element access, matching the operator()(i,j) convention typical
			// matrix libraries (Eigen, Armadillo, etc.) use
			inline Real& operator()(unsigned int i, unsigned int j) { return data_[i*N+j]; }
			inline const Real& operator()(unsigned int i, unsigned int j) const { return data_[i*N+j]; }

			inline Real* operator[](const unsigned int i){
				return data_ + i*N;
			};
			inline const Real* operator[](const unsigned int i) const {
				return data_ + i*N;
			};

			Matrix<Real,N>& operator=(const Matrix<Real,N> &m){
				for (unsigned int i=0; i<N*N; i++) data_[i] = m.data_[i];
				return *this;
			};

			Matrix<Real,N>& operator=(Real s){
				for (unsigned int i=0; i<N*N; i++) data_[i] = s;
				return *this;
			};

			Matrix<Real,N>& operator+=(const Matrix<Real,N> &m){
				for (unsigned int i=0; i<N*N; i++) data_[i] += m.data_[i];
				return *this;
			};

			Matrix<Real,N>& operator-=(const Matrix<Real,N> &m){
				for (unsigned int i=0; i<N*N; i++) data_[i] -= m.data_[i];
				return *this;
			};

			Matrix<Real,N>& operator+=(const Real k){
				for (unsigned int i=0; i<N*N; i++) data_[i] += k;
				return *this;
			};

			Matrix<Real,N>& operator-=(const Real k){
				for (unsigned int i=0; i<N*N; i++) data_[i] -= k;
				return *this;
			};

			Matrix<Real,N>& operator*=(const Real k){
				for (unsigned int i=0; i<N*N; i++) data_[i] *= k;
				return *this;
			};

			Matrix<Real,N>& operator/=(const Real k){
				for (unsigned int i=0; i<N*N; i++) data_[i] /= k;
				return *this;
			};

			/// Matrix product.
			/// @param m Matrix to multiply by (this * m).
			/// @return The product.
			Matrix<Real,N> operator*(const Matrix<Real,N> &m) const {
				Matrix<Real,N> result;
				int p=0;
				for (int i=0; i<N; i++){
					for (int j=0; j<N; j++) {
						Real temp = 0;
						int q=0;
						for (int k=0; k<N; k++){
							temp+=(data_[p+k]*m.data_[q+j]);
							q+=N;
						}
						result.data_[p+j] = temp;
					}
					p+=N;
				}
				return result;
			};

		   /// Matrix-vector product.
		   /// @param p N-element vector.
		   /// @return this * p.
		   std::array<Real,N> operator*(const std::array<Real,N>& p) const {
				   std::array<Real,N> result{};
				int i=0;
				for (int r=0; r<N; ++r){
				for (int c=0; c<N; ++c){
					result[r] += data_[i]*p[c];
					++i;
				}}
				return result;
			};

			void dot_product(const std::array<Real,N> &m, std::array<Real,N> &result) const {
				unsigned int i, j;
				for (i=0; i<N; i++){
					result[i]=0;
					for (j=0; j<N; j++) result[i]+=(*this)[i][j]*m[j];
				}
			};

			/// Sets this matrix to the outer product a * b^T.
			/// @param a Column vector (left factor).
			/// @param b Row vector, used transposed (right factor).
			void outer_product(std::array<Real,N> a, std::array<Real,N> b){
				for (int i=0; i<N; i++){
				for (int j=0; j<N; j++){
					(*this)[i][j] = a[i] * b[j];
				}}
			};

			Matrix<Real,N> operator+(const Real k) const {
				Matrix<Real,N> result;
				for (unsigned int i=0; i<N*N; i++) result.data_[i] =  data_[i]+k;
				return result;
			};

			Matrix<Real,N> operator-(const Real k) const {
				Matrix<Real,N> result;
				for (unsigned int i=0; i<N*N; i++) result.data_[i] =  data_[i]-k;
				return result;
			};

			Matrix<Real,N> operator-() const {
				Matrix<Real,N> result(data_);
				for (unsigned int i=0; i<N*N; i++) result.data_[i] = -1*data_[i];
				return result;
			};

			/// Scalar multiplication.
			/// @param k Factor applied to every element.
			/// @return A new, scaled matrix.
			Matrix<Real,N> operator*(const Real k) const {
				Matrix<Real,N> result;
				for (unsigned int i=0; i<N*N; i++) result.data_[i] =  data_[i]*k;
				return result;
			};

			Matrix<Real,N> operator/(const Real k) const {
				Matrix<Real,N> result;
				for (unsigned int i=0; i<N*N; i++) result.data_[i] =  data_[i]/k;
				return result;
			};

			void set_zero(){
				for (unsigned int i=0; i<N*N; i++) data_[i] = Real(0.0);
			};

			/// Resets to the identity matrix.
			void set_identity(){
				set_zero();
				for (unsigned int i=0, p=0; i<N; i++, p+=N) data_[p+i] = Real(1.0);
			};

			void set_column(const unsigned int j, Real* v){
				unsigned int i, p;
				for (i=0, p=j; i<N; i++, p+=N) data_[p] = v[i];
			};

			void set_row(const unsigned int i, Real* v){
				unsigned int j, p;
				for (j=0, p=i*N; j<N; j++, p++) data_[p] = v[j];
			};

			void set_diagonal(Real *v){
				for (unsigned int i=0, p=0; i<N; i++, p+=N) data_[p+i] = v[i];
			};

			//return the transpose of the matrix (does not mutate *this)
			/// Returns the transpose.
			/// @return A new matrix; this one is unchanged (see transpose_in_place() to mutate instead).
			Matrix<Real,N> transpose() const {
				Matrix<Real,N> result;
				unsigned int i, j, p, q;
				for (i=0, p=0; i<N; i++, p+=N){
				for (j=0, q=0; j<N; j++, q+=N){
					result.data_[q+i] = data_[p+j];
				}}
				return result;
			};

			//transpose the matrix in place (must be square)
			void transpose_in_place(){
				*this = transpose();
			};

			//Print the Matrix
			void print(const char* msg=0) const {
				if (msg) printf("****************\n%s\n****************\n",msg);
				for (int j=0; j<N; j++){
					for (int i=0; i<N; i++) printf("% .4f ",(float)data_[j*N+i]);
					std::cout << std::endl;
				}
				std::cout << "\n\n";
			};

			// raw access to the underlying row-major storage, matching the
			// std::vector/std::array convention
			Real* data() { return data_; }
			const Real* data() const { return data_; }

		private:
			Real data_[N*N];
		};
}
