#pragma once
#include <cstring>
#include <cmath>
#include <array>
#include <cstddef>
#include "core.h"
#include "decomposition.h"

// Projective-geometry toolkit: ProjectionMatrix<Real,D> (a D x (D+1)
// homogeneous matrix mapping D-dimensional source points to
// (D-1)-dimensional destination points -- the standard computer-vision
// "camera matrix" shape, generalized to any D), project_point(), and the
// machinery projection.h's forward_project()/back_project() need to turn
// a detector pixel back into a ray through the source volume. A sibling
// of matrix/decomposition.h/matrix/transform.h, not part of matrix/core.h
// -- #include this directly if you use it, or #include "matrix.h" (the
// umbrella header) for everything at once.
//
// ONE matrix shape covers both parallel-beam (orthographic -- constant
// ray direction, no perspective divide) and cone/fan-beam (perspective --
// rays converge at a single source point) projection geometry, exactly
// the same way a single homogeneous matrix already covers both affine and
// perspective transforms elsewhere in this library. What distinguishes
// them is only whether the matrix's own homogeneous (last) row is trivial
// ([0,...,0,1], parallel-beam) or not (cone/fan-beam) -- camera_center()
// below recovers which case a given matrix is, and the corresponding
// finite source point or shared ray direction, from the matrix ALONE, via
// the matrix's own null space (the classic "camera center from a
// projection matrix" result from multi-view geometry: the null space of
// a camera matrix is exactly its own center of projection, and comes back
// as a point at infinity precisely when there isn't a finite one -- i.e.
// parallel projection). Computed by padding the D x (D+1) matrix to
// square with one zero row (which adds no constraint) and reusing this
// library's own SVD<Real,N> (matrix/decomposition.h) to find the smallest
// singular vector, rather than a separate null-space solver -- real reuse
// of existing internal machinery, not new math.
//
// This is what lets forward_project()/back_project() (projection.h) share
// ONE code path for both beam geometries: both are driven by literally
// the same ProjectionMatrix per view, and ray_for_pixel() below resolves
// a detector pixel back into a ray (an origin and a direction) uniformly,
// without either of them needing to know or care which geometry kind
// they're looking at.

namespace ndl
{
	/// A D x (D+1) homogeneous projective matrix mapping D-dimensional source points to (D-1)-dimensional destination points -- the shape of a computer-vision "camera matrix", generalized to any D. See this file's own top comment for why one shape covers both parallel- and cone/fan-beam projection geometry.
	/// @tparam Real Element type (float or double).
	/// @tparam D    Source (volume) dimension; the destination (detector) dimension is D-1.
	/// @ingroup matrix
	template<class Real, int D>
	class ProjectionMatrix
	{
	public:
		static constexpr int Rows = D;
		static constexpr int Cols = D + 1;

		/// Constructs a zero matrix (there's no canonical "identity" for a non-square shape).
		ProjectionMatrix() { set_zero(); }
		/// Constructs from a row-major array of Rows*Cols values.
		/// @param values Row-major array of exactly Rows*Cols elements, copied in.
		explicit ProjectionMatrix(const Real* values) { memcpy(data_, values, Rows * Cols * sizeof(Real)); }

		inline Real& operator()(unsigned int i, unsigned int j) { return data_[i * Cols + j]; }
		inline const Real& operator()(unsigned int i, unsigned int j) const { return data_[i * Cols + j]; }

		void set_zero() { for (unsigned int i = 0; i < Rows * Cols; i++) data_[i] = Real(0); }

		Real* data() { return data_; }
		const Real* data() const { return data_; }

	private:
		Real data_[Rows * Cols];
	};

	/// Projects `srcPoint` (D components) through `m`, writing the (D-1)-component perspective-divided result into `dstPoint`.
	/// @ingroup matrix
	template<class Real, int D>
	void project_point(const ProjectionMatrix<Real, D>& m, const Real* srcPoint, Real* dstPoint)
	{
		Real w = m(D - 1, D);
		for (int c = 0; c < D; c++) w += m(D - 1, c) * srcPoint[c];
		Real invW = Real(1) / w;
		for (int r = 0; r < D - 1; r++)
		{
			Real t = m(r, D);
			for (int c = 0; c < D; c++) t += m(r, c) * srcPoint[c];
			dstPoint[r] = t * invW;
		}
	}

	// M (the std::array's own size) is an independent template parameter
	// from D, reconciled below via a static_assert, rather than reusing D
	// for both -- the same int-vs-size_t deduction pitfall (and fix)
	// documented on matrix/decomposition.h's eigen_decomposition() and
	// matrix/transform.h's make_translate_matrix(): Matrix/ProjectionMatrix's
	// own D is int, but std::array's own size is std::size_t, and a single
	// template parameter can't be deduced as both simultaneously.
	/// Builds a parallel-beam (orthographic) projection matrix: a constant ray direction, no perspective divide.
	/// @param out         Destination; overwritten.
	/// @param rotation    A full D x D orthonormal frame: rows 0..D-2 are the detector's own axes (in detector-pixel-axis order), row D-1 is the ray direction (the axis being integrated away). Build via make_rotate_matrix() (matrix/transform.h) chains.
	/// @param translation Additive detector-space offset, D-1 components.
	/// @ingroup matrix
	template<class Real, int D, std::size_t M>
	void make_parallel_projection_matrix(ProjectionMatrix<Real, D>& out, const Matrix<Real, D>& rotation, const std::array<Real, M>& translation)
	{
		static_assert(M == (std::size_t)(D - 1), "ndl::make_parallel_projection_matrix() requires translation to have exactly D-1 elements");
		for (int r = 0; r < D - 1; r++)
		{
			for (int c = 0; c < D; c++) out(r, c) = rotation(r, c);
			out(r, D) = translation[r];
		}
		for (int c = 0; c < D; c++) out(D - 1, c) = Real(0);
		out(D - 1, D) = Real(1);
	}

	/// Builds a cone/fan-beam (perspective) projection matrix: rays converge at `sourcePosition`.
	/// @param out            Destination; overwritten.
	/// @param sourcePosition The (finite) source/focal point, D components.
	/// @param rotation       A full D x D orthonormal frame: rows 0..D-2 are the detector's own axes, row D-1 is the view direction (source toward the detector).
	/// @param focalLength    Source-to-detector-plane distance along the view direction.
	/// @ingroup matrix
	template<class Real, int D, std::size_t M>
	void make_cone_beam_projection_matrix(ProjectionMatrix<Real, D>& out, const std::array<Real, M>& sourcePosition, const Matrix<Real, D>& rotation, Real focalLength)
	{
		static_assert(M == (std::size_t)D, "ndl::make_cone_beam_projection_matrix() requires sourcePosition to have exactly D elements");
		for (int r = 0; r < D - 1; r++)
		{
			Real dotES = 0;
			for (int c = 0; c < D; c++) dotES += rotation(r, c) * sourcePosition[c];
			for (int c = 0; c < D; c++) out(r, c) = focalLength * rotation(r, c);
			out(r, D) = -focalLength * dotES;
		}
		Real dotDS = 0;
		for (int c = 0; c < D; c++) dotDS += rotation(D - 1, c) * sourcePosition[c];
		for (int c = 0; c < D; c++) out(D - 1, c) = rotation(D - 1, c);
		out(D - 1, D) = -dotDS;
	}

	/// The result of camera_center(): either the finite point every ray converges at (cone/fan-beam), or the direction every ray shares (parallel-beam), distinguished by `atInfinity`.
	/// @ingroup matrix
	template<class Real, int D>
	struct ProjectionCenter
	{
		std::array<Real, D> point; // finite camera center, or (if atInfinity) the shared unit ray direction
		bool atInfinity;
	};

	// See this file's own top comment for the derivation: pad to square
	// with one (constraint-free) zero row, then find the padded matrix's
	// own null space via SVD -- the classic multi-view-geometry "camera
	// center from a projection matrix" construction.
	/// Recovers a ProjectionMatrix's own center of projection: a finite source point for cone/fan-beam geometry, or a direction at infinity (the shared ray direction) for parallel-beam geometry.
	/// @ingroup matrix
	template<class Real, int D>
	ProjectionCenter<Real, D> camera_center(const ProjectionMatrix<Real, D>& m)
	{
		Matrix<Real, D + 1> padded;
		padded.set_zero();
		for (int r = 0; r < D; r++)
			for (int c = 0; c <= D; c++)
				padded(r, c) = m(r, c);

		SVD<Real, D + 1> svd(padded);
		Matrix<Real, D + 1> V;
		svd.v(V);
		// Singular values come back sorted descending (SVD<Real,N>'s own
		// convention), so the null space (smallest singular value) is the
		// LAST column.
		std::array<Real, D + 1> nullVec;
		for (int i = 0; i <= D; i++) nullVec[i] = V(i, D);

		Real norm = 0;
		for (int i = 0; i <= D; i++) norm += nullVec[i] * nullVec[i];
		norm = std::sqrt(norm);

		ProjectionCenter<Real, D> result;
		Real w = nullVec[D];
		if (std::abs(w) < Real(1e-9) * norm)
		{
			result.atInfinity = true;
			Real dirNorm = 0;
			for (int i = 0; i < D; i++) dirNorm += nullVec[i] * nullVec[i];
			dirNorm = std::sqrt(dirNorm);
			for (int i = 0; i < D; i++) result.point[i] = nullVec[i] / dirNorm;
		}
		else
		{
			result.atInfinity = false;
			for (int i = 0; i < D; i++) result.point[i] = nullVec[i] / w;
		}
		return result;
	}

	/// A ray in source space: `origin` + t*`direction` for t in (-inf, inf), with `direction` a unit vector.
	/// @ingroup matrix
	template<class Real, int D>
	struct ProjectionRay
	{
		std::array<Real, D> origin;
		std::array<Real, D> direction;
	};

	// Cross-multiplying the perspective divide turns "find X with
	// project_point(m,X) == detectorCoord" into a LINEAR system (D-1
	// equations in D unknowns -- the ray through X) regardless of whether
	// m is a true perspective (cone/fan-beam) matrix or a trivial
	// (parallel-beam) one: numerator_r(X) = detectorCoord[r] * w(X), and
	// both numerator_r and w are themselves affine in X. One shared
	// closed-form solve for both geometries, no iteration needed. A probe
	// row is added to pin down one specific point on that 1-parameter
	// family: for parallel-beam, "zero component along the already-known
	// shared ray direction"; for cone/fan-beam, "the homogeneous
	// denominator w(X) equals 1" (any point strictly away from the source
	// itself, where w is exactly 0 -- see camera_center()'s own comment).
	/// Resolves the ray through source space that lands on `detectorCoord` when projected through `m`, given `m`'s own already-computed camera_center().
	/// @ingroup matrix
	template<class Real, int D, std::size_t M>
	ProjectionRay<Real, D> ray_for_pixel(const ProjectionMatrix<Real, D>& m, const ProjectionCenter<Real, D>& center, const std::array<Real, M>& detectorCoord)
	{
		static_assert(M == (std::size_t)(D - 1), "ndl::ray_for_pixel() requires detectorCoord to have exactly D-1 elements");
		Matrix<Real, D> augmented;
		std::array<Real, D> rhs;
		for (int r = 0; r < D - 1; r++)
		{
			for (int c = 0; c < D; c++) augmented(r, c) = m(r, c) - detectorCoord[r] * m(D - 1, c);
			rhs[r] = detectorCoord[r] * m(D - 1, D) - m(r, D);
		}
		if (center.atInfinity)
		{
			for (int c = 0; c < D; c++) augmented(D - 1, c) = center.point[c];
			rhs[D - 1] = Real(0);
		}
		else
		{
			for (int c = 0; c < D; c++) augmented(D - 1, c) = m(D - 1, c);
			rhs[D - 1] = Real(1) - m(D - 1, D);
		}
		// fast_inverse() (matrix/decomposition.h), not the general
		// SVD-based inverse(): called once per detector pixel per view --
		// tens of millions of times in a real reconstruction -- and
		// `augmented` is always well-conditioned by construction (rows 0..
		// D-2 come from the projection matrix's own cross-multiplied
		// linear system, row D-1 is a probe row deliberately chosen to be
		// independent of them), not arbitrary caller-supplied data.
		Matrix<Real, D> inv = fast_inverse(augmented);
		std::array<Real, D> X = inv * rhs;

		ProjectionRay<Real, D> ray;
		if (center.atInfinity)
		{
			ray.origin = X;
			ray.direction = center.point;
		}
		else
		{
			ray.origin = center.point;
			Real norm = 0;
			std::array<Real, D> dir;
			for (int c = 0; c < D; c++) { dir[c] = X[c] - center.point[c]; norm += dir[c] * dir[c]; }
			norm = std::sqrt(norm);
			for (int c = 0; c < D; c++) dir[c] /= norm;
			ray.direction = dir;
		}
		return ray;
	}

	namespace detail
	{
		// Exact Jacobian (d detectorCoord / d sourcePoint) of project_point()
		// at a given source point p -- same closed-form quotient-rule
		// differentiation as sampling.h's perspectiveJacobian(), adapted to
		// ProjectionMatrix's rectangular (D-1 output rows, D input columns)
		// shape. Used by projection.h to size the (per-view, evaluated once
		// rather than per ray-sample -- see that file's own comment on the
		// scope of automatic anti-aliasing implemented there) volume-space
		// anti-aliasing footprint. jacobianOut is (D-1)*D, row-major.
		template<class Real, int D>
		void projectionJacobian(const ProjectionMatrix<Real, D>& m, const Real* p, Real* jacobianOut)
		{
			Real w = m(D - 1, D);
			for (int c = 0; c < D; c++) w += m(D - 1, c) * p[c];
			Real out[D - 1];
			for (int r = 0; r < D - 1; r++)
			{
				Real t = m(r, D);
				for (int c = 0; c < D; c++) t += m(r, c) * p[c];
				out[r] = t / w;
			}
			for (int r = 0; r < D - 1; r++)
				for (int j = 0; j < D; j++)
					jacobianOut[r * D + j] = (m(r, j) - out[r] * m(D - 1, j)) / w;
		}
	}
}
