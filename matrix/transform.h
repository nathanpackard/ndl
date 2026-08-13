#pragma once
#include <cmath>
#include <array>
#include <cstddef>
#include <type_traits>
#include "core.h"

// Geometric transform toolkit: make_scale_matrix()/make_translate_matrix()/
// make_rotate_matrix()/make_shear_matrix()/make_projection_matrix()/
// make_ortho_projection_matrix()/transform_point() -- free functions
// building/applying homogeneous-coordinate transform matrices, the same
// "core class + toolkit" split matrix/decomposition.h has relative to
// Matrix<Real,N> itself. #include this directly if you use any of it;
// #include "matrix/core.h" alone does not pull it in -- #include
// "matrix.h" (the umbrella header) for everything at once.
//
// Every builder here MUTATES a caller-provided `out` (matching
// threshold()/make_box_kernel()/etc.'s own "pre-existing destination"
// convention throughout this library) rather than returning a fresh
// Matrix by value -- not just for consistency, but because it's what makes
// N itself deducible at all for the homogeneous-coordinate overloads below:
// N can't be recovered from an N-1-length std::array argument alone (that's
// not an invertible deduction), but it's immediately available from `out`'s
// own Matrix<Real,N>& type.

namespace ndl
{
	// Both make_scale_matrix() overloads below need an M template parameter
	// independent from N (rather than writing std::array<Real,N> / N-1
	// directly): Matrix's own N is `int`, but std::array's own size
	// parameter is `std::size_t`, so trying to deduce ONE symbol N as both
	// simultaneously fails outright (the same reason
	// matrix/decomposition.h's eigen_decomposition() takes an independent
	// M too) -- reconciled here via enable_if instead of a plain
	// static_assert specifically because there are two overloads to
	// disambiguate: a static_assert only fires after overload resolution
	// already picked a candidate, so with two candidates both nominally
	// matching (Matrix<Real,N>&, std::array<Real,M>&) for any M, resolution
	// itself would be ambiguous; enable_if removes whichever candidate
	// doesn't actually fit from consideration before that happens. The
	// enable_if has to sit in the RETURN TYPE specifically, not in an extra
	// defaulted template parameter -- two templates whose only difference
	// is a defaulted type-template-parameter's own default value are
	// treated as redeclarations of the SAME template (their default
	// doesn't participate in what makes two templates distinct), not as
	// separate overloads.
	/// Sets `out` to a scale matrix with per-axis factors `t` (N components, homogeneous scale included).
	/// @param out Destination; overwritten.
	/// @param t   Per-axis scale factors, N components.
	/// @ingroup matrix
	template<class Real, int N, std::size_t M>
	std::enable_if_t<M == (std::size_t)N, void> make_scale_matrix(Matrix<Real,N>& out, const std::array<Real,M> &t){
		out.set_identity();
		for (int i=0;i<N;i++) out(i,i) = t[i];
	}

	/// Sets `out` to a scale matrix with per-axis factors `t` (N-1 components; the homogeneous component is left at 1).
	/// @param out Destination; overwritten.
	/// @param t   Per-axis scale factors, N-1 components.
	/// @ingroup matrix
	template<class Real, int N, std::size_t M>
	std::enable_if_t<M == (std::size_t)(N-1), void> make_scale_matrix(Matrix<Real,N>& out, const std::array<Real,M> &t){
		out.set_identity();
		for (int i=0;i<N-1;i++) out(i,i) = t[i];
	}

	/// Sets `out` to a translation matrix by `t` (N-1 components, for an N-dimensional homogeneous matrix).
	/// @param out Destination; overwritten.
	/// @param t   Per-axis translation offsets, N-1 components.
	/// @ingroup matrix
	template<class Real, int N, std::size_t M>
	void make_translate_matrix(Matrix<Real,N>& out, const std::array<Real,M> &t){
		static_assert(M == (std::size_t)(N-1), "ndl::make_translate_matrix() requires t to have exactly N-1 elements, matching the matrix's own dimension");
		out.set_identity();
		for (int i=0;i<N-1;i++) out(i,N-1) = t[i];
	}

	/// Sets `out` to a rotation by `angleRad` in the plane spanned by `axis1`/`axis2` -- the fundamental "simple rotation" any N-D rotation decomposes into (see feature_detection.h's own comment on structure-tensor orientation frames for why this matters beyond just 2D/3D).
	/// @param out      Destination; overwritten.
	/// @param angleRad Rotation angle, in radians.
	/// @param axis1    First axis of the rotation plane. Defaults to 0.
	/// @param axis2    Second axis of the rotation plane. Defaults to 1.
	/// @ingroup matrix
	template<class Real, int N>
	void make_rotate_matrix(Matrix<Real,N>& out, Real angleRad, int axis1=0, int axis2=1) {
		for (int i=0; i<N; i++){
		for (int j=0; j<N; j++){
			if (i==j){
				if (i==axis1) out(i,j) = std::cos(angleRad);
				else if (i==axis2) out(i,j) = std::cos(angleRad);
				else out(i,j)=1;
			}
			else if (i==axis1 && j==axis2)  out(i,j) = -std::sin(angleRad);
			else if (i==axis2 && j==axis1)  out(i,j) = std::sin(angleRad);
			else out(i,j) = 0;
		}}
	}

	/// Sets `out` to a shear matrix.
	/// @param out    Destination; overwritten.
	/// @param amount Shear factor.
	/// @param axis1  Axis whose value is offset by the shear. Defaults to 0.
	/// @param axis2  Axis the shear amount is proportional to. Defaults to 1.
	/// @ingroup matrix
	template<class Real, int N>
	void make_shear_matrix(Matrix<Real,N>& out, Real amount, int axis1=0, int axis2=1){
		out.set_identity();
		if (axis1!=axis2) out(axis1,axis2)=amount;
	}

	/// Sets `out` to a perspective projection matrix: projects from the origin along `axis` onto a hyperplane at distance `dist`.
	/// @param out  Destination; overwritten.
	/// @param dist Distance from the origin to the projection hyperplane.
	/// @param axis Axis the projection is along. Defaults to 0.
	/// @ingroup matrix
	template<class Real, int N>
	void make_projection_matrix(Matrix<Real,N>& out, Real dist, int axis=0){
		out.set_zero();
		for (int i=0; i<N-1; i++) out(i,i) = dist;
		out(N-1,axis) = 1;
	}

	/// Sets `out` to an orthographic projection matrix along `axis`, at distance `dist`.
	/// @param out  Destination; overwritten.
	/// @param dist Offset along `axis` in the resulting matrix.
	/// @param axis Axis the projection flattens. Defaults to 0.
	/// @ingroup matrix
	template<class Real, int N>
	void make_ortho_projection_matrix(Matrix<Real,N>& out, Real dist, int axis=0){
		out.set_identity();
		out(axis,axis) = 0;
		out(axis,N-1) = dist;
	}

	/// Transforms the N-element point `p` in place by `m`, treating it as a homogeneous coordinate (perspective divide included -- see make_projection_matrix()).
	/// @param m Transform matrix.
	/// @param p N-element point, overwritten with the transformed result.
	/// @ingroup matrix
	template<class Real, int N>
	void transform_point(const Matrix<Real,N>& m, Real* p){
		Real result[N];

		// Row N-1 is the homogeneous (w) row; dotting it with p and
		// dividing every other transformed component by it is exactly the
		// perspective divide make_projection_matrix() relies on.
		Real w = 0;
		for (int c = 0; c < N; c++) w += m(N-1,c) * p[c];
		Real invW = Real(1) / w;

		result[N-1] = Real(1);
		for (int r = 0; r < N-1; r++){
			Real t = 0;
			for (int c = 0; c < N; c++) t += m(r,c) * p[c];
			result[r] = t * invW;
		}
		for (int i = 0; i < N; i++) p[i] = result[i];
	}
}
