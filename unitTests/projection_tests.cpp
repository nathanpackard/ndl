#include <gtest/gtest.h>
#include <cmath>
#include <array>
#include <sstream>
#include <iostream>
#include <random>

#include <ndl/image.h>
#include <ndl/matrix.h>
#include <ndl/projection.h>

#include "testHelpers.h"

using namespace ndl;

namespace
{
	std::vector<ProjectionMatrix<double, 2>> buildParallelGeometry(int numViews, double volCenterX, double volCenterY, double detCenter)
	{
		std::vector<ProjectionMatrix<double, 2>> geometry;
		for (int v = 0; v < numViews; v++)
		{
			double theta = M_PI * v / numViews; // 0..180 degrees
			Matrix<double, 2> rotation;
			make_rotate_matrix(rotation, theta);
			double translation0 = detCenter - (rotation(0, 0) * volCenterX + rotation(0, 1) * volCenterY);
			std::array<double, 1> translation{ translation0 };
			ProjectionMatrix<double, 2> pm;
			make_parallel_projection_matrix(pm, rotation, translation);
			geometry.push_back(pm);
		}
		return geometry;
	}

	// Circular-orbit cone-beam geometry, matching demo/ct_reconstruction_3d's
	// own buildConeBeamGeometry() (kept as an independent copy here rather
	// than shared -- unit tests deliberately don't depend on demo code).
	// See that demo's own comment for the recentering-shift derivation.
	std::vector<ProjectionMatrix<double, 3>> buildConeBeamGeometry(int numViews, const std::array<double, 3>& volCenter, double sourceDistance, double detectorDistance, int detW, int detH, double detPixelSpacing)
	{
		double focalLength = sourceDistance + detectorDistance;
		std::vector<ProjectionMatrix<double, 3>> geometry;
		for (int v = 0; v < numViews; v++)
		{
			double theta = 2 * M_PI * v / numViews;
			std::array<double, 3> d{ std::cos(theta), std::sin(theta), 0.0 };
			std::array<double, 3> eu{ -std::sin(theta), std::cos(theta), 0.0 };
			std::array<double, 3> ev{ 0.0, 0.0, 1.0 };
			std::array<double, 3> source{
				volCenter[0] - sourceDistance * d[0],
				volCenter[1] - sourceDistance * d[1],
				volCenter[2] - sourceDistance * d[2]
			};
			Matrix<double, 3> rotation;
			for (int c = 0; c < 3; c++)
			{
				rotation(0, c) = eu[c] / detPixelSpacing;
				rotation(1, c) = ev[c] / detPixelSpacing;
				rotation(2, c) = d[c];
			}
			ProjectionMatrix<double, 3> pm;
			make_cone_beam_projection_matrix(pm, source, rotation, focalLength);
			for (int c = 0; c < 4; c++)
			{
				pm(0, c) += (detW / 2.0) * pm(2, c);
				pm(1, c) += (detH / 2.0) * pm(2, c);
			}
			geometry.push_back(pm);
		}
		return geometry;
	}
}

TEST(Projection, ProjectionMatrixAndCameraCenter) {
	std::stringstream passfail;
	std::cout << std::endl << "PROJECTION -- ProjectionMatrix / camera_center / ray_for_pixel" << std::endl;

	// Parallel-beam: camera_center() should recover atInfinity=true with
	// a direction matching the rotation's own ray-direction row (up to
	// sign, since it's a null-space vector), and ray_for_pixel() should
	// produce a genuine ray landing on the requested detector coordinate
	// at every ray parameter t, not just at one point.
	{
		double theta = 0.4;
		Matrix<double, 2> rotation;
		make_rotate_matrix(rotation, theta);
		std::array<double, 1> translation{ 1.5 };
		ProjectionMatrix<double, 2> pm;
		make_parallel_projection_matrix(pm, rotation, translation);

		double X[2] = { 3.0, -2.0 };
		double u[1];
		project_point(pm, X, u);
		double expected = rotation(0, 0) * X[0] + rotation(0, 1) * X[1] + translation[0];
		passfail << "parallel-beam project_point matches the detector-axis dot product: " << (std::abs(u[0] - expected) < 1e-9 ? "Pass" : "Fail") << std::endl;

		auto center = camera_center(pm);
		double dot = center.point[0] * rotation(1, 0) + center.point[1] * rotation(1, 1);
		passfail << "parallel-beam camera_center is at infinity, direction matches the ray-direction row: " << (center.atInfinity && std::abs(std::abs(dot) - 1.0) < 1e-6 ? "Pass" : "Fail") << std::endl;

		std::array<double, 1> detCoord{ 2.7 };
		auto ray = ray_for_pixel(pm, center, detCoord);
		bool rayOk = true;
		for (double t : { -5.0, 0.0, 3.0, 10.0 })
		{
			double Xp[2] = { ray.origin[0] + t * ray.direction[0], ray.origin[1] + t * ray.direction[1] };
			double up[1];
			project_point(pm, Xp, up);
			if (std::abs(up[0] - detCoord[0]) > 1e-7) rayOk = false;
		}
		passfail << "parallel-beam ray_for_pixel produces a genuine ray landing on the requested detector coordinate: " << (rayOk ? "Pass" : "Fail") << std::endl;
	}

	// Cone-beam: camera_center() should recover the exact finite source
	// position, and ray_for_pixel()'s origin should match it, with the
	// whole ray converging correctly.
	{
		Matrix<double, 2> rotation;
		make_rotate_matrix(rotation, 0.9);
		std::array<double, 2> source{ 50.0, -30.0 };
		double focalLength = 80.0;
		ProjectionMatrix<double, 2> pm;
		make_cone_beam_projection_matrix(pm, source, rotation, focalLength);

		auto center = camera_center(pm);
		bool centerOk = !center.atInfinity
			&& std::abs(center.point[0] - source[0]) < 1e-6
			&& std::abs(center.point[1] - source[1]) < 1e-6;
		passfail << "cone-beam camera_center recovers the exact finite source position: " << (centerOk ? "Pass" : "Fail") << std::endl;

		std::array<double, 1> detCoord{ -12.3 };
		auto ray = ray_for_pixel(pm, center, detCoord);
		bool originOk = std::abs(ray.origin[0] - source[0]) < 1e-6 && std::abs(ray.origin[1] - source[1]) < 1e-6;
		bool rayOk = true;
		for (double t : { 1.0, 20.0, 60.0, 150.0 })
		{
			double Xp[2] = { ray.origin[0] + t * ray.direction[0], ray.origin[1] + t * ray.direction[1] };
			double up[1];
			project_point(pm, Xp, up);
			if (std::abs(up[0] - detCoord[0]) > 1e-6) rayOk = false;
		}
		passfail << "cone-beam ray_for_pixel originates at the true source: " << (originOk ? "Pass" : "Fail") << std::endl;
		passfail << "cone-beam ray_for_pixel lands on the requested detector coordinate along the whole ray: " << (rayOk ? "Pass" : "Fail") << std::endl;
	}

	reportPassFail(passfail);
}

TEST(Projection, ForwardAndBackProjectAreExactAdjoints) {
	std::stringstream passfail;
	std::cout << std::endl << "PROJECTION -- forward_project()/back_project() ADJOINTNESS" << std::endl;

	// The centerpiece correctness check: <forward_project(x), y> should
	// equal <x, back_project(y)> for random x, y -- see projection.h's own
	// top comment for why this holds to near machine precision (the
	// ray-driven scatter construction, not voxel-driven gather) as long
	// as autoAA doesn't engage (documented scope).
	const int W = 32, H = 32;
	const int numViews = 40;
	const int numDet = 47;
	double volCenterX = (W - 1) / 2.0, volCenterY = (H - 1) / 2.0;
	double detCenter = (numDet - 1) / 2.0;
	auto geometry = buildParallelGeometry(numViews, volCenterX, volCenterY, detCenter);

	std::mt19937 rng(7);
	std::uniform_real_distribution<double> dist(-1.0, 1.0);

	std::vector<double> xData(W * H);
	Image<double, 2> x(xData.data(), { W, H });
	for (auto& v : xData) v = dist(rng);

	std::vector<double> yData(numViews * numDet);
	Image<double, 2> y(yData.data(), { numViews, numDet });
	for (auto& v : yData) v = dist(rng);

	std::vector<double> pxData(numViews * numDet);
	Image<double, 2> px(pxData.data(), { numViews, numDet });
	forward_project(x, px, geometry, Linear{}, /*autoAA=*/false);

	std::vector<double> ptyData(W * H);
	Image<double, 2> pty(ptyData.data(), { W, H });
	back_project(y, pty, geometry, Linear{}, /*autoAA=*/false);

	double lhs = 0;
	for (int i = 0; i < numViews * numDet; i++) lhs += pxData[i] * yData[i];
	double rhs = 0;
	for (int i = 0; i < W * H; i++) rhs += xData[i] * ptyData[i];
	double relErr = std::abs(lhs - rhs) / (std::abs(lhs) + std::abs(rhs) + 1e-30);

	passfail << "<forward_project(x),y> = " << lhs << ", <x,back_project(y)> = " << rhs << ", relative error = " << relErr << std::endl;
	passfail << "forward_project()/back_project() are exact adjoints (relative error < 1e-9): " << (relErr < 1e-9 ? "Pass" : "Fail") << std::endl;

	// AA-enabled forward projection is out of the exact-adjointness scope
	// (documented in projection.h), but should still produce a finite,
	// non-degenerate result.
	std::vector<double> unifData(W * H);
	std::uniform_real_distribution<double> posDist(0, 1);
	for (auto& v : unifData) v = posDist(rng);
	Image<double, 2> unif(unifData.data(), { W, H });
	std::vector<double> sinoData(numViews * numDet);
	Image<double, 2> sino(sinoData.data(), { numViews, numDet });
	forward_project(unif, sino, geometry, Linear{}, /*autoAA=*/true);
	bool aaFinite = true;
	double aaSum = 0;
	for (auto v : sinoData) { if (!std::isfinite(v)) aaFinite = false; aaSum += v; }
	passfail << "AA-enabled forward projection produces a finite, non-degenerate result: " << (aaFinite && aaSum > 0 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Projection, PrecomputedCentersMatchInternal) {
	std::stringstream passfail;
	std::cout << std::endl << "PROJECTION -- compute_camera_centers() OVERLOAD MATCHES THE INTERNAL ONE" << std::endl;

	// forward_project()/back_project()'s plain geometry-only overload
	// computes camera_center() internally, once per view, every call --
	// wasted repeated work across an iterative reconstruction loop that
	// reuses the same geometry. The `centers`-taking overload lets a
	// caller precompute once instead; this only matters if it produces
	// the SAME result as the plain overload, checked directly here for
	// both parallel- and cone-beam geometry, not just assumed from the
	// refactor being "just" a cache. forward_project() is checked for
	// EXACT bit-for-bit equality (embarrassingly parallel, each view
	// writes disjoint output, so there's no accumulation-order sensitivity
	// to begin with); back_project() is checked to a relative-error
	// tolerance instead, since it merges every view's own contribution
	// into a shared accumulator across parallel chunks (this file's own
	// top comment), and floating-point addition isn't associative -- two
	// separate calls, even with byte-identical inputs, can land in a
	// different thread-scheduling order and sum in a different sequence,
	// which is expected (~1e-15 relative, confirmed directly) and not
	// something precomputing centers changes.
	std::mt19937 rng(11);
	std::uniform_real_distribution<double> dist(-1.0, 1.0);

	// Parallel-beam (2D): center.atInfinity == true.
	{
		const int W = 24, H = 24, numViews = 12, numDet = 31;
		double volCenterX = (W - 1) / 2.0, volCenterY = (H - 1) / 2.0;
		double detCenter = (numDet - 1) / 2.0;
		auto geometry = buildParallelGeometry(numViews, volCenterX, volCenterY, detCenter);
		auto centers = compute_camera_centers(geometry);

		std::vector<double> xData(W * H);
		Image<double, 2> x(xData.data(), { W, H });
		for (auto& v : xData) v = dist(rng);

		std::vector<double> pxInternalData(numViews * numDet), pxPrecomputedData(numViews * numDet);
		Image<double, 2> pxInternal(pxInternalData.data(), { numViews, numDet });
		Image<double, 2> pxPrecomputed(pxPrecomputedData.data(), { numViews, numDet });
		forward_project(x, pxInternal, geometry, Linear{}, /*autoAA=*/false);
		forward_project(x, pxPrecomputed, geometry, centers, Linear{}, /*autoAA=*/false);
		bool fwdMatches = pxInternalData == pxPrecomputedData;
		passfail << "parallel-beam: forward_project() with precomputed centers matches the internal-computation overload exactly: " << (fwdMatches ? "Pass" : "Fail") << std::endl;

		std::vector<double> btInternalData(W * H), btPrecomputedData(W * H);
		Image<double, 2> btInternal(btInternalData.data(), { W, H });
		Image<double, 2> btPrecomputed(btPrecomputedData.data(), { W, H });
		back_project(pxInternal, btInternal, geometry, Linear{}, /*autoAA=*/false);
		back_project(pxInternal, btPrecomputed, geometry, centers, Linear{}, /*autoAA=*/false);
		bool backMatches = true;
		for (std::size_t i = 0; i < btInternalData.size(); i++)
		{
			double d = std::abs(btInternalData[i] - btPrecomputedData[i]);
			double rel = d / (std::abs(btInternalData[i]) + std::abs(btPrecomputedData[i]) + 1e-300);
			if (rel > 1e-9) backMatches = false;
		}
		passfail << "parallel-beam: back_project() with precomputed centers matches the internal-computation overload exactly: " << (backMatches ? "Pass" : "Fail") << std::endl;
	}

	// Cone-beam (3D): center.atInfinity == false (a genuine finite source).
	{
		const int W = 24, numViews = 10, detW = 12, detH = 12;
		std::array<double, 3> volCenter{ (W - 1) / 2.0, (W - 1) / 2.0, (W - 1) / 2.0 };
		auto geometry = buildConeBeamGeometry(numViews, volCenter, /*sourceDistance=*/60, /*detectorDistance=*/60, detW, detH, /*detPixelSpacing=*/1.5);
		auto centers = compute_camera_centers(geometry);

		std::vector<double> xData((std::size_t)W * W * W);
		Image<double, 3> x(xData.data(), { W, W, W });
		for (auto& v : xData) v = dist(rng);

		std::vector<double> pxInternalData((std::size_t)numViews * detW * detH), pxPrecomputedData((std::size_t)numViews * detW * detH);
		Image<double, 3> pxInternal(pxInternalData.data(), { numViews, detW, detH });
		Image<double, 3> pxPrecomputed(pxPrecomputedData.data(), { numViews, detW, detH });
		forward_project(x, pxInternal, geometry, Linear{}, /*autoAA=*/false);
		forward_project(x, pxPrecomputed, geometry, centers, Linear{}, /*autoAA=*/false);
		bool fwdMatches = pxInternalData == pxPrecomputedData;
		passfail << "cone-beam: forward_project() with precomputed centers matches the internal-computation overload exactly: " << (fwdMatches ? "Pass" : "Fail") << std::endl;

		std::vector<double> btInternalData((std::size_t)W * W * W), btPrecomputedData((std::size_t)W * W * W);
		Image<double, 3> btInternal(btInternalData.data(), { W, W, W });
		Image<double, 3> btPrecomputed(btPrecomputedData.data(), { W, W, W });
		back_project(pxInternal, btInternal, geometry, Linear{}, /*autoAA=*/false);
		back_project(pxInternal, btPrecomputed, geometry, centers, Linear{}, /*autoAA=*/false);
		bool backMatches = true;
		for (std::size_t i = 0; i < btInternalData.size(); i++)
		{
			double d = std::abs(btInternalData[i] - btPrecomputedData[i]);
			double rel = d / (std::abs(btInternalData[i]) + std::abs(btPrecomputedData[i]) + 1e-300);
			if (rel > 1e-9) backMatches = false;
		}
		passfail << "cone-beam: back_project() with precomputed centers matches the internal-computation overload exactly: " << (backMatches ? "Pass" : "Fail") << std::endl;
	}

	reportPassFail(passfail);
}

TEST(Projection, PointSourceForwardProjectionTracksGeometry) {
	std::stringstream passfail;
	std::cout << std::endl << "PROJECTION -- POINT-SOURCE SANITY CHECK" << std::endl;

	// A single nonzero voxel's forward projection should peak, at every
	// view, exactly where that voxel's own project_point() lands -- the
	// whole ray-marching + accumulation pipeline concentrating its mass
	// at the geometrically correct detector location.
	const int W = 32, H = 32;
	const int numViews = 40;
	const int numDet = 47;
	double volCenterX = (W - 1) / 2.0, volCenterY = (H - 1) / 2.0;
	double detCenter = (numDet - 1) / 2.0;
	auto geometry = buildParallelGeometry(numViews, volCenterX, volCenterY, detCenter);

	std::vector<double> xData(W * H, 0.0);
	Image<double, 2> x(xData.data(), { W, H });
	int vx = 20, vy = 8;
	x(vx, vy) = 1.0;

	std::vector<double> sinoData(numViews * numDet);
	Image<double, 2> sino(sinoData.data(), { numViews, numDet });
	forward_project(x, sino, geometry, Linear{}, /*autoAA=*/false, /*stepSize=*/0.25);

	int mismatches = 0;
	for (int view = 0; view < numViews; view++)
	{
		double p[2] = { (double)vx, (double)vy };
		double expected[1];
		project_point(geometry[view], p, expected);

		int bestDet = 0; double bestVal = -1;
		for (int d = 0; d < numDet; d++) if (sino(view, d) > bestVal) { bestVal = sino(view, d); bestDet = d; }

		if (std::abs(bestDet - expected[0]) > 1.5) mismatches++;
	}
	passfail << "point-source forward-projection peaks track project_point() across all " << numViews << " views (" << mismatches << " mismatches): " << (mismatches == 0 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Projection, ForwardAndBackProject3DConeBeamAreExactAdjoints) {
	std::stringstream passfail;
	std::cout << std::endl << "PROJECTION -- 3D CONE-BEAM ADJOINTNESS" << std::endl;

	// The same dot-product test as ForwardAndBackProjectAreExactAdjoints
	// above, but for a genuinely PERSPECTIVE (cone-beam) 3D geometry --
	// confirms the exact-adjoint guarantee (projection.h's own top
	// comment) doesn't depend on DIM or on the projection being affine
	// (parallel-beam); demo/ct_reconstruction_3d relies on this holding
	// for its own 128^3/cone-beam reconstruction.
	const int W = 24;
	const int numViews = 30, detW = 16, detH = 16;
	std::array<double, 3> volCenter{ (W - 1) / 2.0, (W - 1) / 2.0, (W - 1) / 2.0 };
	auto geometry = buildConeBeamGeometry(numViews, volCenter, /*sourceDistance=*/60, /*detectorDistance=*/60, detW, detH, /*detPixelSpacing=*/1.5);

	std::mt19937 rng(11);
	std::uniform_real_distribution<double> dist(-1.0, 1.0);

	std::vector<double> xData((std::size_t)W * W * W);
	Image<double, 3> x(xData.data(), { W, W, W });
	for (auto& v : xData) v = dist(rng);

	std::vector<double> yData((std::size_t)numViews * detW * detH);
	Image<double, 3> y(yData.data(), { numViews, detW, detH });
	for (auto& v : yData) v = dist(rng);

	std::vector<double> pxData((std::size_t)numViews * detW * detH);
	Image<double, 3> px(pxData.data(), { numViews, detW, detH });
	forward_project(x, px, geometry, Linear{}, /*autoAA=*/false);

	std::vector<double> ptyData((std::size_t)W * W * W);
	Image<double, 3> pty(ptyData.data(), { W, W, W });
	back_project(y, pty, geometry, Linear{}, /*autoAA=*/false);

	double lhs = 0;
	for (std::size_t i = 0; i < pxData.size(); i++) lhs += pxData[i] * yData[i];
	double rhs = 0;
	for (std::size_t i = 0; i < xData.size(); i++) rhs += xData[i] * ptyData[i];
	double relErr = std::abs(lhs - rhs) / (std::abs(lhs) + std::abs(rhs) + 1e-30);

	passfail << "<forward_project(x),y> = " << lhs << ", <x,back_project(y)> = " << rhs << ", relative error = " << relErr << std::endl;
	passfail << "3D cone-beam forward_project()/back_project() are exact adjoints (relative error < 1e-9): " << (relErr < 1e-9 ? "Pass" : "Fail") << std::endl;

	// A voxel exactly on the source-detector axis (the volume's own
	// center) must land on the detector's own center pixel at every view,
	// regardless of the perspective divide -- the cone-beam analog of the
	// parallel-beam point-source check above, and specifically what
	// catches the "shift the numerator's translation entry instead of
	// scaling by the whole w-row" mistake described in
	// demo/ct_reconstruction_3d's buildConeBeamGeometry() comment.
	bool centeredOk = true;
	for (int view = 0; view < numViews; view++)
	{
		double p[3] = { volCenter[0], volCenter[1], volCenter[2] };
		double u[2];
		project_point(geometry[view], p, u);
		if (std::abs(u[0] - detW / 2.0) > 1e-6 || std::abs(u[1] - detH / 2.0) > 1e-6) centeredOk = false;
	}
	passfail << "a voxel on the source-detector axis projects to the detector's own center at every view: " << (centeredOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Projection, ForwardAndBackProjectWithAAAreExactAdjoints) {
	std::stringstream passfail;
	std::cout << std::endl << "PROJECTION -- AA-ENABLED ADJOINTNESS" << std::endl;

	// The centerpiece check for the matched-AA-filter pair (projection.h's
	// own top comment has the derivation): with autoAA on, back_project()
	// swaps in box_filter_scatter_add() (summed_area_table.h) in place of
	// scatter_add() at exactly the same per-sample point forward_project()
	// swapped in box_filter_query() in place of sample() -- built to be
	// box_filter_query()'s own exact adjoint directly (verified in
	// isolation in unitTests/summed_area_table_tests.cpp), not by assuming
	// the resulting box blur is a symmetric operator. detPixelSpacing=3.5
	// at magnification 2 here (unlike the unfiltered tests above, whose
	// geometry happens to have matched voxel/detector resolution and so
	// never actually exercises the AA path at all) makes one detector
	// pixel correspond to 1.75 volume-units -- confirmed, not just
	// assumed, to actually trigger autoAA's filtering below.
	const int W = 48;
	const int numViews = 40, detW = 24, detH = 24;
	std::array<double, 3> volCenter{ (W - 1) / 2.0, (W - 1) / 2.0, (W - 1) / 2.0 };
	auto geometry = buildConeBeamGeometry(numViews, volCenter, /*sourceDistance=*/120, /*detectorDistance=*/120, detW, detH, /*detPixelSpacing=*/3.5);

	std::mt19937 rng(13);
	std::uniform_real_distribution<double> dist(-1.0, 1.0);

	// Confirm AA actually triggers for this geometry first -- a point
	// source's forward projection should differ between autoAA on/off; if
	// it doesn't, the rest of this test would silently just be re-testing
	// the already-covered unfiltered path.
	std::vector<double> pointData((std::size_t)W * W * W, 0.0);
	pointData[(std::size_t)(W / 2) * W * W + (std::size_t)(W / 2) * W + (W / 2)] = 1.0;
	Image<double, 3> pointSrc(pointData.data(), { W, W, W });
	std::vector<double> pxAAData((std::size_t)numViews * detW * detH), pxNoAAData((std::size_t)numViews * detW * detH);
	Image<double, 3> pxAA(pxAAData.data(), { numViews, detW, detH }), pxNoAA(pxNoAAData.data(), { numViews, detW, detH });
	forward_project(pointSrc, pxAA, geometry, Linear{}, /*autoAA=*/true);
	forward_project(pointSrc, pxNoAA, geometry, Linear{}, /*autoAA=*/false);
	double aaMaxDiff = 0;
	for (std::size_t i = 0; i < pxAAData.size(); i++) aaMaxDiff = std::max(aaMaxDiff, std::abs(pxAAData[i] - pxNoAAData[i]));
	passfail << "autoAA genuinely changes the forward projection for this geometry (max diff " << aaMaxDiff << "), confirming the test below exercises the AA path: " << (aaMaxDiff > 1e-6 ? "Pass" : "Fail") << std::endl;

	// Case A: data with margin from the volume's own boundary.
	{
		std::vector<double> xData((std::size_t)W * W * W, 0.0);
		Image<double, 3> x(xData.data(), { W, W, W });
		for (int z = 8; z < W - 8; z++) for (int y = 8; y < W - 8; y++) for (int xx = 8; xx < W - 8; xx++) x(xx, y, z) = dist(rng);
		std::vector<double> yData((std::size_t)numViews * detW * detH);
		for (auto& v : yData) v = dist(rng);
		Image<double, 3> y(yData.data(), { numViews, detW, detH });

		std::vector<double> pxData((std::size_t)numViews * detW * detH);
		Image<double, 3> px(pxData.data(), { numViews, detW, detH });
		forward_project(x, px, geometry, Linear{}, /*autoAA=*/true);
		std::vector<double> ptyData((std::size_t)W * W * W);
		Image<double, 3> pty(ptyData.data(), { W, W, W });
		back_project(y, pty, geometry, Linear{}, /*autoAA=*/true);

		double lhs = 0;
		for (std::size_t i = 0; i < pxData.size(); i++) lhs += pxData[i] * yData[i];
		double rhs = 0;
		for (std::size_t i = 0; i < xData.size(); i++) rhs += xData[i] * ptyData[i];
		double relErr = std::abs(lhs - rhs) / (std::abs(lhs) + std::abs(rhs) + 1e-30);
		passfail << "AA-enabled, volume with margin from boundary: relative error = " << relErr << (relErr < 1e-9 ? "  Pass" : "  Fail") << std::endl;
	}

	// Case B: data filling the whole volume, including its own boundary --
	// isolates whether box_filter_query()'s boundary-clamped window
	// (asymmetric as a raw operator, see projection.h's own comment)
	// still yields an exact adjoint once box_filter_scatter_add() is
	// built as its literal transpose rather than assumed self-adjoint.
	{
		std::vector<double> xData((std::size_t)W * W * W);
		for (auto& v : xData) v = dist(rng);
		Image<double, 3> x(xData.data(), { W, W, W });
		std::vector<double> yData((std::size_t)numViews * detW * detH);
		for (auto& v : yData) v = dist(rng);
		Image<double, 3> y(yData.data(), { numViews, detW, detH });

		std::vector<double> pxData((std::size_t)numViews * detW * detH);
		Image<double, 3> px(pxData.data(), { numViews, detW, detH });
		forward_project(x, px, geometry, Linear{}, /*autoAA=*/true);
		std::vector<double> ptyData((std::size_t)W * W * W);
		Image<double, 3> pty(ptyData.data(), { W, W, W });
		back_project(y, pty, geometry, Linear{}, /*autoAA=*/true);

		double lhs = 0;
		for (std::size_t i = 0; i < pxData.size(); i++) lhs += pxData[i] * yData[i];
		double rhs = 0;
		for (std::size_t i = 0; i < xData.size(); i++) rhs += xData[i] * ptyData[i];
		double relErr = std::abs(lhs - rhs) / (std::abs(lhs) + std::abs(rhs) + 1e-30);
		passfail << "AA-enabled, volume touches its own boundary: relative error = " << relErr << (relErr < 1e-9 ? "  Pass" : "  Fail") << std::endl;
	}

	reportPassFail(passfail);
}

TEST(Projection, MaxDensityBoundFromSinogram) {
	std::stringstream passfail;
	std::cout << std::endl << "PROJECTION -- MAX_DENSITY_BOUND" << std::endl;

	// A small dense disc, well clear of the volume's own corners/edges.
	const int W = 60, H = 60;
	std::vector<double> volData(W * H, 0.0);
	Image<double, 2> vol(volData.data(), { W, H });
	double cx = 30, cy = 30, r = 8, density = 3.0;
	for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
	{
		double dx = x - cx, dy = y - cy;
		if (dx * dx + dy * dy <= r * r) vol(x, y) = density;
	}

	const int numViews = 60, numDet = 90;
	double detCenter = (numDet - 1) / 2.0;
	auto geometry = buildParallelGeometry(numViews, cx, cy, detCenter);

	std::vector<double> sinoData(numViews * numDet);
	Image<double, 2> sino(sinoData.data(), { numViews, numDet });
	forward_project(vol, sino, geometry, Linear{}, /*autoAA=*/false, /*stepSize=*/1.0);

	std::vector<double> boundData(W * H);
	Image<double, 2> bound(boundData.data(), { W, H });
	max_density_bound(sino, bound, geometry, /*stepSize=*/1.0);

	// A far-corner background voxel (never overlaps the disc at any
	// rotation) should get a bound close to 0 -- tight enough to actually
	// suppress a reconstruction overshoot artifact there.
	double cornerBound = bound(2, 2);
	passfail << "far-background voxel gets a tight near-zero bound (" << cornerBound << "): " << (cornerBound < 0.5 ? "Pass" : "Fail") << std::endl;

	// A voxel truly inside the disc must never be excluded by its own
	// bound -- the bound must always be >= the true density there, or the
	// constraint would corrupt genuine structure, not just artifacts.
	double interiorBound = bound((int)cx, (int)cy);
	passfail << "interior voxel's bound (" << interiorBound << ") never excludes its own true density (" << density << "): " << (interiorBound >= density - 1e-9 ? "Pass" : "Fail") << std::endl;

	bool allNonNegative = true;
	for (auto v : boundData) if (v < 0) allNonNegative = false;
	passfail << "every voxel's bound is non-negative: " << (allNonNegative ? "Pass" : "Fail") << std::endl;

	// Applying the bound as a clamp to a deliberately-corrupted
	// reconstruction (a synthetic background overshoot spike) suppresses it.
	std::vector<double> reconData = volData;
	reconData[5 * W + 5] = 50.0;
	reconData[5 * W + 5] = std::min(std::max(reconData[5 * W + 5], 0.0), boundData[5 * W + 5]);
	passfail << "clamping to [0,bound] suppresses a synthetic background overshoot spike (was 50.0, now " << reconData[5 * W + 5] << "): " << (reconData[5 * W + 5] < 1.0 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Projection, AAHalfWidthVariesWithDepthAlongRay) {
	std::stringstream passfail;
	std::cout << std::endl << "PROJECTION -- PER-SAMPLE AA HALF-WIDTH VARIES WITH DEPTH" << std::endl;

	// A direct (white-box) check of the specific bug this test exists to
	// catch a regression of: an earlier version of forward_project()/
	// back_project() evaluated the AA footprint ONCE per view, at the
	// volume's own center, and reused that single constant for every ray
	// sample -- silently wrong for a true perspective (cone-beam) matrix,
	// where a voxel's real footprint on the detector depends on its own
	// distance from the source. The adjointness tests elsewhere in this
	// file would pass even with that bug (forward_project() and
	// back_project() were still exact adjoints of EACH OTHER, just both
	// wrong relative to the true continuous footprint in the same way) --
	// this test checks the footprint itself against the closed-form
	// pinhole-camera magnification formula instead, at multiple depths
	// along one ray, which the adjointness tests can't do.
	//
	// detail::sampleAAHalfWidth() is reached into directly here (not
	// exercised only indirectly through forward_project()'s own
	// observable output) because it's the one function responsible for
	// this specific correctness property, and a direct check of it is
	// far more precise than trying to infer per-sample filter width from
	// a sinogram's own aggregate output.
	const int W = 128;
	const int numViews = 4, detW = 8, detH = 8;
	std::array<double, 3> volCenter{ (W - 1) / 2.0, (W - 1) / 2.0, (W - 1) / 2.0 };
	double sourceDistance = 300, detectorDistance = 300, detPixelSpacing = 7.0;
	auto geometry = buildConeBeamGeometry(numViews, volCenter, sourceDistance, detectorDistance, detW, detH, detPixelSpacing);

	const auto& pm = geometry[0]; // view angle 0: ray direction is +x
	auto center = camera_center(pm);
	ASSERT_FALSE(center.atInfinity); // cone-beam: must be a finite source

	auto halfWidthAt = [&](double depthOffset) {
		double p[3] = { volCenter[0] + depthOffset, volCenter[1], volCenter[2] };
		return detail::sampleAAHalfWidth(pm, center, p);
	};

	auto nearSource = halfWidthAt(-(W - 1) / 2.0);
	auto atCenter = halfWidthAt(0.0);
	auto nearDetector = halfWidthAt((W - 1) / 2.0);

	// Index 1 (not 0): view 0's own ray direction is +x (index 0), and
	// blur ALONG the ray direction is meaningless here -- the ray march
	// itself already integrates along that axis -- so halfWidth[0] is
	// always ~0 by construction; the real, physically meaningful
	// footprint shows up on the axes PERPENDICULAR to the ray (1 and 2).
	passfail << "half-width near source / at center / near detector: " << nearSource[1] << " / " << atCenter[1] << " / " << nearDetector[1] << std::endl;

	// Physical direction: magnification = focalLength / distanceFromSource,
	// and volume-space footprint = detPixelSpacing / magnification, so
	// footprint should INCREASE monotonically with distance from the
	// source (matches the classic pinhole-camera relationship, not
	// specific to this library).
	bool monotonic = nearSource[1] < atCenter[1] && atCenter[1] < nearDetector[1];
	passfail << "half-width increases monotonically from near-source to near-detector: " << (monotonic ? "Pass" : "Fail") << std::endl;

	// Cross-check against the closed-form pinhole magnification formula
	// directly, not just internal self-consistency.
	auto expectedHalfWidth = [&](double depthOffset) {
		double distFromSource = sourceDistance + depthOffset; // ray direction is +x, source is at -sourceDistance along x from volCenter
		double magnification = (sourceDistance + detectorDistance) / distFromSource;
		return 0.5 * (detPixelSpacing / magnification);
	};
	bool matchesClosedForm = true;
	for (double depthOffset : { -(W - 1) / 2.0, 0.0, (W - 1) / 2.0 })
	{
		double got = halfWidthAt(depthOffset)[1];
		double expected = expectedHalfWidth(depthOffset);
		if (std::abs(got - expected) > 1e-6) matchesClosedForm = false;
	}
	passfail << "matches the closed-form pinhole-camera magnification formula at all three depths: " << (matchesClosedForm ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}
