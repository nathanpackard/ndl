#include <gtest/gtest.h>
#include <cmath>
#include <array>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/matrix.h>

#include "testHelpers.h"

using namespace ndl;

TEST(Matrix, MatrixConstruction) {
	std::stringstream passfail;

	std::cout << std::endl << "MATRIX CONSTRUCTION" << std::endl;

	Matrix<double, 3> m; // default constructor -> identity
	bool isIdentity = true;
	for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) if (m(i, j) != (i == j ? 1.0 : 0.0)) isIdentity = false;
	passfail << "default constructor produces the identity: " << (isIdentity ? "Pass" : "Fail") << std::endl;

	double raw[9] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	Matrix<double, 3> m2(raw);
	bool matches = true;
	for (int i = 0; i < 9; i++) if (m2.data()[i] != raw[i]) matches = false;
	passfail << "explicit constructor copies from a raw array: " << (matches ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(Matrix, MatrixElementAccess) {
	std::stringstream passfail;

	std::cout << std::endl << "MATRIX ELEMENT ACCESS" << std::endl;

	Matrix<double, 3> m;
	m(0, 1) = 5;
	passfail << "operator()(i,j) writes and reads: " << (m(0, 1) == 5 ? "Pass" : "Fail") << std::endl;
	passfail << "operator[] bracket-chain access matches operator(): " << (m[0][1] == 5 ? "Pass" : "Fail") << std::endl;

	const Matrix<double, 3>& cm = m;
	passfail << "const operator()(i,j): " << (cm(0, 1) == 5 ? "Pass" : "Fail") << std::endl;
	passfail << "const operator[]: " << (cm[0][1] == 5 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

// determinant()/cofactor() (matrix/decomposition.h) are free functions, not
// Matrix members -- the same "core class + toolkit" split convolve()/
// erode()/etc. have relative to Image, per matrix.h's own umbrella comment.
TEST(Matrix, MatrixDeterminant) {
	std::stringstream passfail;

	std::cout << std::endl << "MATRIX DETERMINANT" << std::endl;

	Matrix<double, 2> m2;
	m2(0, 0) = 4; m2(0, 1) = 3; m2(1, 0) = 6; m2(1, 1) = 3;
	passfail << "2x2 determinant: " << (std::abs(determinant(m2) - (-6.0)) < 1e-9 ? "Pass" : "Fail") << std::endl;

	// classic worked example (det = -306)
	Matrix<double, 3> m3;
	double vals3[9] = { 6, 1, 1, 4, -2, 5, 2, 8, 7 };
	int k = 0; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m3(i, j) = vals3[k++];
	passfail << "3x3 determinant (non-diagonal): " << (std::abs(determinant(m3) - (-306.0)) < 1e-6 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 4> m4;
	for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) m4(i, j) = (i == j) ? (i + 1) : 0;
	passfail << "4x4 determinant (diagonal): " << (std::abs(determinant(m4) - 24.0) < 1e-9 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 5> m5;
	for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++) m5(i, j) = (i == j) ? (i + 1) : 0;
	passfail << "5x5 determinant (Laplace expansion): " << (std::abs(determinant(m5) - 120.0) < 1e-9 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(Matrix, MatrixCofactor) {
	std::stringstream passfail;

	std::cout << std::endl << "MATRIX COFACTOR" << std::endl;

	Matrix<double, 3> m;
	double vals[9] = { 1, 2, 3, 0, 4, 5, 1, 0, 6 };
	int k = 0; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m(i, j) = vals[k++];

	// cofactor(0,0) = +det[[4,5],[0,6]] = 24
	passfail << "cofactor(0,0): " << (std::abs(cofactor(m, 0, 0) - 24.0) < 1e-9 ? "Pass" : "Fail") << std::endl;
	// cofactor(0,1) = -det[[0,5],[1,6]] = -(0*6-5*1) = 5 -- also checks the sign alternation
	passfail << "cofactor(0,1) sign alternates correctly: " << (std::abs(cofactor(m, 0, 1) - 5.0) < 1e-9 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(Matrix, MatrixArithmeticOperators) {
	std::stringstream passfail;

	std::cout << std::endl << "MATRIX ARITHMETIC OPERATORS" << std::endl;

	Matrix<double, 2> a, b;
	a(0, 0) = 1; a(0, 1) = 2; a(1, 0) = 3; a(1, 1) = 4;
	b(0, 0) = 5; b(0, 1) = 6; b(1, 0) = 7; b(1, 1) = 8;

	Matrix<double, 2> sum = a; sum += b;
	passfail << "operator+= (matrix): " << (sum(0, 0) == 6 && sum(0, 1) == 8 && sum(1, 0) == 10 && sum(1, 1) == 12 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> diff = b; diff -= a;
	passfail << "operator-= (matrix): " << (diff(0, 0) == 4 && diff(0, 1) == 4 && diff(1, 0) == 4 && diff(1, 1) == 4 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> scaled = a; scaled *= 2;
	passfail << "operator*= (scalar): " << (scaled(0, 0) == 2 && scaled(1, 1) == 8 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> divided = scaled; divided /= 2;
	passfail << "operator/= (scalar): " << (divided == a ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> product = a * b; // [1,2;3,4] * [5,6;7,8] = [19,22;43,50]
	passfail << "operator* (matrix*matrix): "
		<< (product(0, 0) == 19 && product(0, 1) == 22 && product(1, 0) == 43 && product(1, 1) == 50 ? "Pass" : "Fail") << std::endl;

	std::array<double, 2> v = { 1, 1 };
	auto r = a * v; // [1*1+2*1, 3*1+4*1] = [3,7]
	passfail << "operator* (matrix*array): " << (std::abs(r[0] - 3.0) < 1e-9 && std::abs(r[1] - 7.0) < 1e-9 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> neg = -a;
	passfail << "unary operator-: " << (neg(0, 0) == -1 && neg(1, 1) == -4 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> plusScalar = a + 10;
	passfail << "operator+ (scalar): " << (plusScalar(0, 0) == 11 && plusScalar(1, 1) == 14 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> minusScalar = a - 1;
	passfail << "operator- (scalar): " << (minusScalar(0, 0) == 0 && minusScalar(1, 1) == 3 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> mulScalar = a * 3;
	passfail << "operator* (scalar): " << (mulScalar(0, 0) == 3 && mulScalar(1, 1) == 12 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> divScalar = a / 2;
	passfail << "operator/ (scalar): " << (std::abs(divScalar(0, 0) - 0.5) < 1e-9 && divScalar(1, 1) == 2 ? "Pass" : "Fail") << std::endl;

	passfail << "operator==: " << (a == a ? "Pass" : "Fail") << std::endl;
	passfail << "operator!=: " << (a != b ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(Matrix, MatrixSetters) {
	std::stringstream passfail;

	std::cout << std::endl << "MATRIX SETTERS" << std::endl;

	Matrix<double, 3> m;
	m(0, 0) = 9;
	m.set_zero();
	bool ok1 = true; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) if (m(i, j) != 0) ok1 = false;
	passfail << "set_zero(): " << (ok1 ? "Pass" : "Fail") << std::endl;

	m.set_identity();
	bool ok2 = true; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) if (m(i, j) != (i == j ? 1.0 : 0.0)) ok2 = false;
	passfail << "set_identity(): " << (ok2 ? "Pass" : "Fail") << std::endl;

	double col[3] = { 1, 2, 3 };
	m.set_column(1, col);
	passfail << "set_column(): " << (m(0, 1) == 1 && m(1, 1) == 2 && m(2, 1) == 3 ? "Pass" : "Fail") << std::endl;

	double row[3] = { 7, 8, 9 };
	m.set_row(0, row);
	passfail << "set_row(): " << (m(0, 0) == 7 && m(0, 1) == 8 && m(0, 2) == 9 ? "Pass" : "Fail") << std::endl;

	double diag[3] = { 4, 5, 6 };
	m.set_diagonal(diag);
	passfail << "set_diagonal(): " << (m(0, 0) == 4 && m(1, 1) == 5 && m(2, 2) == 6 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(Matrix, MatrixTranspose) {
	std::stringstream passfail;

	std::cout << std::endl << "MATRIX TRANSPOSE" << std::endl;

	Matrix<double, 2> m;
	m(0, 0) = 1; m(0, 1) = 2; m(1, 0) = 3; m(1, 1) = 4;

	Matrix<double, 2> t = m.transpose();
	passfail << "transpose() returns the transposed matrix: " << (t(0, 0) == 1 && t(0, 1) == 3 && t(1, 0) == 2 && t(1, 1) == 4 ? "Pass" : "Fail") << std::endl;
	passfail << "transpose() does not mutate the original: " << (m(0, 1) == 2 ? "Pass" : "Fail") << std::endl;

	m.transpose_in_place();
	passfail << "transpose_in_place() mutates the original: " << (m(0, 0) == 1 && m(0, 1) == 3 && m(1, 0) == 2 && m(1, 1) == 4 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(Matrix, MatrixInverse) {
	std::stringstream passfail;

	std::cout << std::endl << "MATRIX INVERSE" << std::endl;

	// general (non-rotation) 2x2: det=10, inverse = 1/10 * [6,-7;-2,4]
	Matrix<double, 2> m;
	m(0, 0) = 4; m(0, 1) = 7; m(1, 0) = 2; m(1, 1) = 6;
	Matrix<double, 2> inv = inverse(m);
	bool ok1 = std::abs(inv(0, 0) - 0.6) < 1e-9 && std::abs(inv(0, 1) - (-0.7)) < 1e-9
		&& std::abs(inv(1, 0) - (-0.2)) < 1e-9 && std::abs(inv(1, 1) - 0.4) < 1e-9;
	passfail << "inverse() of a general 2x2 matrix: " << (ok1 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> identity2 = m * inv;
	bool ok2 = std::abs(identity2(0, 0) - 1) < 1e-9 && std::abs(identity2(0, 1)) < 1e-9
		&& std::abs(identity2(1, 0)) < 1e-9 && std::abs(identity2(1, 1) - 1) < 1e-9;
	passfail << "m * inverse(m) == identity: " << (ok2 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> m2 = m;
	invert(m2);
	bool ok3 = true;
	for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) if (std::abs(m2(i, j) - inv(i, j)) > 1e-9) ok3 = false;
	passfail << "invert() matches inverse(): " << (ok3 ? "Pass" : "Fail") << std::endl;

	// a rotation matrix is orthogonal: its inverse equals its transpose
	Matrix<double, 3> rot;
	make_rotate_matrix(rot, M_PI / 4, 0, 1);
	Matrix<double, 3> rotInv = inverse(rot);
	Matrix<double, 3> rotTranspose = rot.transpose();
	bool ok4 = true;
	for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) if (std::abs(rotInv(i, j) - rotTranspose(i, j)) > 1e-9) ok4 = false;
	passfail << "rotation matrix inverse() == transpose(): " << (ok4 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

// make_scale_matrix()/make_translate_matrix()/make_rotate_matrix()/
// make_shear_matrix()/make_projection_matrix()/make_ortho_projection_matrix()/
// transform_point() (matrix/transform.h) are free functions that MUTATE a
// caller-provided `out` (matching threshold()/make_box_kernel()'s own
// "pre-existing destination" convention) rather than Matrix members --
// see matrix/transform.h's own comment for why (it's what makes N itself
// deducible for the N-1-component homogeneous-coordinate overloads).
TEST(Matrix, MatrixTransforms) {
	std::stringstream passfail;

	std::cout << std::endl << "MATRIX TRANSFORMS" << std::endl;

	Matrix<double, 3> scaleNonHomog;
	std::array<double, 3> s3 = { 2, 3, 4 };
	make_scale_matrix(scaleNonHomog, s3);
	passfail << "make_scale_matrix() (non-homogeneous, N components): "
		<< (scaleNonHomog(0, 0) == 2 && scaleNonHomog(1, 1) == 3 && scaleNonHomog(2, 2) == 4 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 4> scaleHomog;
	std::array<double, 3> s2 = { 2, 3, 4 };
	make_scale_matrix(scaleHomog, s2);
	passfail << "make_scale_matrix() (homogeneous, N-1 components, last stays 1): "
		<< (scaleHomog(0, 0) == 2 && scaleHomog(1, 1) == 3 && scaleHomog(2, 2) == 4 && scaleHomog(3, 3) == 1 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 4> translate;
	std::array<double, 3> t = { 5, 6, 7 };
	make_translate_matrix(translate, t);
	double p[4] = { 0, 0, 0, 1 };
	transform_point(translate, p);
	passfail << "make_translate_matrix() + transform_point(): "
		<< (std::abs(p[0] - 5) < 1e-9 && std::abs(p[1] - 6) < 1e-9 && std::abs(p[2] - 7) < 1e-9 ? "Pass" : "Fail") << std::endl;

	// make_rotate_matrix() on a plain (non-homogeneous) matrix is a direct linear transform,
	// applied via matrix*vector -- transform_point is for homogeneous coordinates only.
	Matrix<double, 2> rot;
	make_rotate_matrix(rot, M_PI / 2, 0, 1);
	std::array<double, 2> v = { 1, 0 };
	auto rotated = rot * v;
	passfail << "make_rotate_matrix() rotates (1,0) by 90 degrees to (0,1): "
		<< (std::abs(rotated[0]) < 1e-9 && std::abs(rotated[1] - 1) < 1e-9 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> shear;
	make_shear_matrix(shear, 2.0, 0, 1);
	passfail << "make_shear_matrix(): " << (shear(0, 1) == 2 && shear(0, 0) == 1 && shear(1, 1) == 1 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 3> proj;
	make_projection_matrix(proj, 5.0, 0);
	passfail << "make_projection_matrix(): "
		<< (proj(0, 0) == 5 && proj(1, 1) == 5 && proj(2, 0) == 1 && proj(2, 1) == 0 && proj(2, 2) == 0 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 3> orthoProj;
	make_ortho_projection_matrix(orthoProj, 5.0, 0);
	passfail << "make_ortho_projection_matrix(): "
		<< (orthoProj(0, 0) == 0 && orthoProj(0, 2) == 5 && orthoProj(1, 1) == 1 && orthoProj(2, 2) == 1 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(Matrix, MatrixDotOuterProduct) {
	std::stringstream passfail;

	std::cout << std::endl << "MATRIX DOT/OUTER PRODUCT" << std::endl;

	Matrix<double, 2> m;
	m(0, 0) = 1; m(0, 1) = 2; m(1, 0) = 3; m(1, 1) = 4;
	std::array<double, 2> v = { 1, 1 };
	std::array<double, 2> result;
	m.dot_product(v, result); // result[i] = sum_j m(i,j)*v[j]
	passfail << "dot_product(): " << (std::abs(result[0] - 3) < 1e-9 && std::abs(result[1] - 7) < 1e-9 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> outer;
	std::array<double, 2> a = { 2, 3 };
	std::array<double, 2> b = { 5, 7 };
	outer.outer_product(a, b); // outer(i,j) = a[i]*b[j]
	passfail << "outer_product(): " << (outer(0, 0) == 10 && outer(0, 1) == 14 && outer(1, 0) == 15 && outer(1, 1) == 21 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

// eigen_decomposition() (matrix/decomposition.h) is a real implementation
// (classic Jacobi rotation algorithm, symmetric matrices) now, not the
// throwing stub it used to be -- see matrix/decomposition.h's own comment
// for the algorithm and its documented limits (eigenvector sign/ordering
// ambiguity).
TEST(Matrix, MatrixEigenDecomposition) {
	std::stringstream passfail;

	std::cout << std::endl << "MATRIX EIGEN DECOMPOSITION" << std::endl;

	// Diagonal matrix: eigenvalues are just the diagonal entries themselves,
	// hand-checkable, and sorted descending per eigen_decomposition()'s own
	// documented convention.
	Matrix<double, 3> diag;
	diag.set_zero();
	diag(0, 0) = 5; diag(1, 1) = 2; diag(2, 2) = 9;
	std::array<double, 3> diagVals;
	Matrix<double, 3> diagVecs;
	eigen_decomposition(diag, diagVals, diagVecs);
	bool diagOk = std::abs(diagVals[0] - 9) < 1e-9 && std::abs(diagVals[1] - 5) < 1e-9 && std::abs(diagVals[2] - 2) < 1e-9;
	passfail << "eigenvalues of a diagonal matrix are its own diagonal, sorted descending: " << (diagOk ? "Pass" : "Fail") << std::endl;

	// Known symmetric 2x2 [[2,1],[1,2]]: eigenvalues 3 and 1 (hand-checkable
	// via the characteristic polynomial (2-L)^2 - 1 = 0).
	double raw[4] = { 2, 1, 1, 2 };
	Matrix<double, 2> A(raw);
	std::array<double, 2> vals;
	Matrix<double, 2> vecs;
	eigen_decomposition(A, vals, vecs);
	passfail << "known 2x2 eigenvalues (3, 1): " << (std::abs(vals[0] - 3) < 1e-9 && std::abs(vals[1] - 1) < 1e-9 ? "Pass" : "Fail") << std::endl;

	// The defining property A*v = lambda*v, checked directly rather than
	// just trusting the eigenvalues alone.
	std::array<double, 2> v0 = { vecs(0, 0), vecs(1, 0) };
	std::array<double, 2> Av0 = A * v0;
	bool eigenRelationOk = std::abs(Av0[0] - vals[0] * v0[0]) < 1e-9 && std::abs(Av0[1] - vals[0] * v0[1]) < 1e-9;
	passfail << "A * v0 == lambda0 * v0: " << (eigenRelationOk ? "Pass" : "Fail") << std::endl;

	// Eigenvectors of a symmetric matrix are orthonormal: V^T * V == I.
	Matrix<double, 2> orthoCheck = vecs.transpose() * vecs;
	bool orthoOk = std::abs(orthoCheck(0, 0) - 1) < 1e-9 && std::abs(orthoCheck(1, 1) - 1) < 1e-9 && std::abs(orthoCheck(0, 1)) < 1e-9;
	passfail << "eigenvectors are orthonormal (V^T*V == I): " << (orthoOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Matrix, CompositeTransformChain) {
	std::stringstream passfail;

	std::cout << std::endl << "COMPOSITE: TRANSFORM CHAIN (scale, then rotate, then translate)" << std::endl;

	Matrix<double, 3> scale;
	std::array<double, 2> s = { 2, 2 };
	make_scale_matrix(scale, s);

	Matrix<double, 3> rotate;
	make_rotate_matrix(rotate, M_PI / 2, 0, 1);

	Matrix<double, 3> translate;
	std::array<double, 2> t = { 10, 0 };
	make_translate_matrix(translate, t);

	// matrix multiplication associates right-to-left when applied to a point:
	// (translate * rotate * scale) * p == translate * (rotate * (scale * p))
	Matrix<double, 3> combined = translate * rotate * scale;

	double p[3] = { 1, 0, 1 }; // homogeneous point (1,0)
	transform_point(combined, p);
	// scale (1,0)->(2,0); rotate 90 deg (2,0)->(0,2); translate +(10,0) -> (10,2)
	passfail << "chained transform applies scale, rotate, translate in the expected order: "
		<< (std::abs(p[0] - 10) < 1e-6 && std::abs(p[1] - 2) < 1e-6 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}
