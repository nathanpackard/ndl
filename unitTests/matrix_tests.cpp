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

TEST(Matrix, MatrixDeterminant) {
	std::stringstream passfail;

	std::cout << std::endl << "MATRIX DETERMINANT" << std::endl;

	Matrix<double, 2> m2;
	m2(0, 0) = 4; m2(0, 1) = 3; m2(1, 0) = 6; m2(1, 1) = 3;
	passfail << "2x2 determinant: " << (std::abs(m2.determinant() - (-6.0)) < 1e-9 ? "Pass" : "Fail") << std::endl;

	// classic worked example (det = -306)
	Matrix<double, 3> m3;
	double vals3[9] = { 6, 1, 1, 4, -2, 5, 2, 8, 7 };
	int k = 0; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m3(i, j) = vals3[k++];
	passfail << "3x3 determinant (non-diagonal): " << (std::abs(m3.determinant() - (-306.0)) < 1e-6 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 4> m4;
	for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) m4(i, j) = (i == j) ? (i + 1) : 0;
	passfail << "4x4 determinant (diagonal): " << (std::abs(m4.determinant() - 24.0) < 1e-9 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 5> m5;
	for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++) m5(i, j) = (i == j) ? (i + 1) : 0;
	passfail << "5x5 determinant (Laplace expansion): " << (std::abs(m5.determinant() - 120.0) < 1e-9 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(Matrix, MatrixCofactor) {
	std::stringstream passfail;

	std::cout << std::endl << "MATRIX COFACTOR" << std::endl;

	Matrix<double, 3> m;
	double vals[9] = { 1, 2, 3, 0, 4, 5, 1, 0, 6 };
	int k = 0; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m(i, j) = vals[k++];

	// cofactor(0,0) = +det[[4,5],[0,6]] = 24
	passfail << "cofactor(0,0): " << (std::abs(m.cofactor(0, 0) - 24.0) < 1e-9 ? "Pass" : "Fail") << std::endl;
	// cofactor(0,1) = -det[[0,5],[1,6]] = -(0*6-5*1) = 5 -- also checks the sign alternation
	passfail << "cofactor(0,1) sign alternates correctly: " << (std::abs(m.cofactor(0, 1) - 5.0) < 1e-9 ? "Pass" : "Fail") << std::endl;
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
	Matrix<double, 2> inv = m.inverse();
	bool ok1 = std::abs(inv(0, 0) - 0.6) < 1e-9 && std::abs(inv(0, 1) - (-0.7)) < 1e-9
		&& std::abs(inv(1, 0) - (-0.2)) < 1e-9 && std::abs(inv(1, 1) - 0.4) < 1e-9;
	passfail << "inverse() of a general 2x2 matrix: " << (ok1 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> identity2 = m * inv;
	bool ok2 = std::abs(identity2(0, 0) - 1) < 1e-9 && std::abs(identity2(0, 1)) < 1e-9
		&& std::abs(identity2(1, 0)) < 1e-9 && std::abs(identity2(1, 1) - 1) < 1e-9;
	passfail << "m * m.inverse() == identity: " << (ok2 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> m2 = m;
	m2.invert();
	bool ok3 = true;
	for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) if (std::abs(m2(i, j) - inv(i, j)) > 1e-9) ok3 = false;
	passfail << "invert() matches inverse(): " << (ok3 ? "Pass" : "Fail") << std::endl;

	// a rotation matrix is orthogonal: its inverse equals its transpose
	Matrix<double, 3> rot;
	rot.set_rotate(M_PI / 4, 0, 1);
	Matrix<double, 3> rotInv = rot.inverse();
	Matrix<double, 3> rotTranspose = rot.transpose();
	bool ok4 = true;
	for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) if (std::abs(rotInv(i, j) - rotTranspose(i, j)) > 1e-9) ok4 = false;
	passfail << "rotation matrix inverse() == transpose(): " << (ok4 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(Matrix, MatrixTransforms) {
	std::stringstream passfail;

	std::cout << std::endl << "MATRIX TRANSFORMS" << std::endl;

	Matrix<double, 3> scaleNonHomog;
	std::array<double, 3> s3 = { 2, 3, 4 };
	scaleNonHomog.set_scale(s3);
	passfail << "set_scale() (non-homogeneous, N components): "
		<< (scaleNonHomog(0, 0) == 2 && scaleNonHomog(1, 1) == 3 && scaleNonHomog(2, 2) == 4 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 4> scaleHomog;
	std::array<double, 3> s2 = { 2, 3, 4 };
	scaleHomog.set_scale(s2);
	passfail << "set_scale() (homogeneous, N-1 components, last stays 1): "
		<< (scaleHomog(0, 0) == 2 && scaleHomog(1, 1) == 3 && scaleHomog(2, 2) == 4 && scaleHomog(3, 3) == 1 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 4> translate;
	std::array<double, 3> t = { 5, 6, 7 };
	translate.set_translate(t);
	double p[4] = { 0, 0, 0, 1 };
	translate.transform_point(p);
	passfail << "set_translate() + transform_point(): "
		<< (std::abs(p[0] - 5) < 1e-9 && std::abs(p[1] - 6) < 1e-9 && std::abs(p[2] - 7) < 1e-9 ? "Pass" : "Fail") << std::endl;

	// set_rotate() on a plain (non-homogeneous) matrix is a direct linear transform,
	// applied via matrix*vector -- transform_point is for homogeneous coordinates only.
	Matrix<double, 2> rot;
	rot.set_rotate(M_PI / 2, 0, 1);
	std::array<double, 2> v = { 1, 0 };
	auto rotated = rot * v;
	passfail << "set_rotate() rotates (1,0) by 90 degrees to (0,1): "
		<< (std::abs(rotated[0]) < 1e-9 && std::abs(rotated[1] - 1) < 1e-9 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 2> shear;
	shear.set_shear(2, 0, 1);
	passfail << "set_shear(): " << (shear(0, 1) == 2 && shear(0, 0) == 1 && shear(1, 1) == 1 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 3> proj;
	proj.set_projection(5.0, 0);
	passfail << "set_projection(): "
		<< (proj(0, 0) == 5 && proj(1, 1) == 5 && proj(2, 0) == 1 && proj(2, 1) == 0 && proj(2, 2) == 0 ? "Pass" : "Fail") << std::endl;

	Matrix<double, 3> orthoProj;
	orthoProj.set_ortho_projection(5.0, 0);
	passfail << "set_ortho_projection(): "
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

TEST(Matrix, MatrixEigenDecomposition) {
	std::stringstream passfail;

	std::cout << std::endl << "MATRIX EIGEN DECOMPOSITION" << std::endl;

	bool threw = false;
	try {
		Matrix<double, 3> m;
		std::array<double, 3> eigenvalues;
		Matrix<double, 3> eigenvectors;
		m.eigen_decomposition(eigenvalues, eigenvectors);
	} catch (const std::logic_error&) {
		threw = true;
	}
	passfail << "eigen_decomposition() reports unimplemented rather than silently returning zeros: " << (threw ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(Matrix, CompositeTransformChain) {
	std::stringstream passfail;

	std::cout << std::endl << "COMPOSITE: TRANSFORM CHAIN (scale, then rotate, then translate)" << std::endl;

	Matrix<double, 3> scale;
	std::array<double, 2> s = { 2, 2 };
	scale.set_scale(s);

	Matrix<double, 3> rotate;
	rotate.set_rotate(M_PI / 2, 0, 1);

	Matrix<double, 3> translate;
	std::array<double, 2> t = { 10, 0 };
	translate.set_translate(t);

	// matrix multiplication associates right-to-left when applied to a point:
	// (translate * rotate * scale) * p == translate * (rotate * (scale * p))
	Matrix<double, 3> combined = translate * rotate * scale;

	double p[3] = { 1, 0, 1 }; // homogeneous point (1,0)
	combined.transform_point(p);
	// scale (1,0)->(2,0); rotate 90 deg (2,0)->(0,2); translate +(10,0) -> (10,2)
	passfail << "chained transform applies scale, rotate, translate in the expected order: "
		<< (std::abs(p[0] - 10) < 1e-6 && std::abs(p[1] - 2) < 1e-6 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

