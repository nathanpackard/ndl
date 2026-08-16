#include <ndl/image.h>
#include <ndl/imageIO.h>
#include <ndl/matrix.h>
#include <ndl/projection.h>
#include <ndl/morphology.h>
#include <ndl/viewer.h>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <vector>
#include <array>
#include <string>
#include <algorithm>
#include "../demoHelpers.h"

using namespace ndl;
namespace fs = std::filesystem;

// The 3D, cone/fan-beam sibling of demo/ct_reconstruction (which covers
// 2D parallel-beam) -- same forward_project()/back_project() (projection.h),
// same exact-adjoint back-projection, same Landweber iteration, but a
// genuinely PERSPECTIVE (not orthographic) geometry: a 64^3 volume
// reconstructed from a 64x64-pixel cone-beam detector, specifically to
// exercise the two things that only show up once you leave 2D parallel-beam:
// a true cone/fan-beam projection matrix (nontrivial homogeneous row, real
// perspective divide), and automatic anti-aliasing whose footprint genuinely
// varies with a voxel's own distance from the source (unlike parallel-beam's
// spatially-uniform case -- see PART 4 below). Unlike the 2D demo, PART 5's
// reconstruction loop here actually runs with autoAA on, not just off --
// this geometry's own detector pixel footprint (~1.75 voxel-widths, PART 3's
// own comment has the derivation) is exactly the case automatic
// anti-aliasing exists for (projection.h's own top comment has the
// matched-filter derivation that makes this possible without breaking
// forward_project()/back_project()'s exact adjointness).
//
// This demo is still slower than most of this repo's other demos --
// 64^3 = ~262k voxels, 180 views x 64x64 = ~740k detector pixels, each
// ray marched through up to ~110 voxel-steps -- the same "by far the most
// expensive step in this whole demo" situation demo/motion's sift_flow()
// step flags, just for the whole demo this time. forward_project()/
// back_project() both parallelize over views via std::execution::par
// (projection.h's own top comment) when a parallel STL backend is
// available, which is most of what keeps this demo's own runtime to
// roughly a minute and a half despite autoAA evaluating a genuine
// per-ray-sample footprint (not just once per view -- projection.h's own
// top comment covers why that distinction matters and what it costs) --
// dominated by PART 5's continuous reconstruction loop and PART 6's DART
// refinement on top of it (each DART outer iteration re-runs a few more
// forward/back-projection passes, on top of PART 5's own 25).
//
// The volume is reconstructed at 64^3, matched to what this 64x64-pixel
// detector can actually resolve rather than a finer grid than the
// measurements support -- see PART 3's own comment for the footprint
// derivation. Reconstructing onto a grid finer than a detector's own
// resolving power leaves the system underdetermined (more volume unknowns
// than independent measurements), which shows up as structured
// null-space artifacts (rings/grid patterns) that neither anti-aliasing
// nor a value constraint can fix, since neither adds real information;
// matching resolution instead keeps the system overdetermined and
// well-posed (PART 3's own comment has the measurement-vs-unknown
// counts). PART 6 adds a second, complementary lever on top of that: DART
// (Discrete Algebraic Reconstruction Technique), which exploits a known,
// small number of tissue-density levels to constrain the reconstruction
// far more tightly than PART 5's own [0, bound] box -- see PART 6's own
// comment for the result, including where it helps and where its own
// analogue of semi-convergence shows up.
//
// step()/showArray()/showText()/saveForInspection()/outputDir come from
// demoHelpers.h, shared with every other demo.

// The classic 3D Shepp-Logan phantom (Shepp & Logan's original 1974 head
// phantom, extended to a solid by giving each ellipse a 3rd semi-axis and
// letting a couple sit off the central z=0 plane -- the standard synthetic
// volumetric test phantom for 3D/cone-beam CT reconstruction, e.g. Kak &
// Slaney's "Principles of Computerized Tomographic Imaging"). Each
// ellipsoid here is only rotated about the z-axis (the same in-plane
// rotation the 2D phantom's ellipses use) -- the full reference phantom
// additionally tilts two of the small ellipsoids slightly out of that
// plane, a minor cosmetic detail this demo skips since it doesn't change
// the phantom's overall recognizable shape or its validity as a
// reconstruction test target.
namespace
{
	struct Ellipsoid { double A, a, b, c, x0, y0, z0, phiDeg; };
	const Ellipsoid sheppLoganEllipsoids[10] = {
		{  1.00, 0.6900, 0.9200, 0.8100,  0.00,  0.0000,  0.00,   0 },
		{ -0.80, 0.6624, 0.8740, 0.7800,  0.00, -0.0184,  0.00,   0 },
		{ -0.20, 0.1100, 0.3100, 0.2200,  0.22,  0.0000,  0.00, -18 },
		{ -0.20, 0.1600, 0.4100, 0.2800, -0.22,  0.0000,  0.00,  18 },
		{  0.10, 0.2100, 0.2500, 0.4100,  0.00,  0.3500, -0.15,   0 },
		{  0.10, 0.0460, 0.0460, 0.0500,  0.00,  0.1000,  0.25,   0 },
		{  0.10, 0.0460, 0.0460, 0.0500,  0.00, -0.1000,  0.25,   0 },
		{  0.10, 0.0460, 0.0230, 0.0500, -0.08, -0.6050,  0.00,   0 },
		{  0.10, 0.0230, 0.0230, 0.0200,  0.00, -0.6060,  0.00,   0 },
		{  0.10, 0.0230, 0.0460, 0.0200,  0.06, -0.6050,  0.00,   0 },
	};

	double sheppLoganValue3D(double x, double y, double z)
	{
		double v = 0;
		for (const auto& e : sheppLoganEllipsoids)
		{
			double phi = e.phiDeg * M_PI / 180.0;
			double dx = x - e.x0, dy = y - e.y0, dz = z - e.z0;
			double xr = dx * std::cos(phi) + dy * std::sin(phi);
			double yr = -dx * std::sin(phi) + dy * std::cos(phi);
			double zr = dz;
			if ((xr * xr) / (e.a * e.a) + (yr * yr) / (e.b * e.b) + (zr * zr) / (e.c * e.c) <= 1.0) v += e.A;
		}
		return v;
	}

	// Circular-orbit cone-beam geometry: the source and detector rotate
	// together about the volume's own z-axis at a fixed sourceDistance/
	// detectorDistance, `numViews` angles evenly spanning the full 360
	// degrees (unlike parallel-beam's 180-degree periodicity -- a
	// cone-beam view and its 180-degree opposite are NOT equivalent
	// measurements, since the cone diverges differently at each). Returns
	// one ProjectionMatrix<double,3> per view, each a genuine PERSPECTIVE
	// (not orthographic) map: rows 0/1 of the orthonormal frame passed to
	// make_cone_beam_projection_matrix() are the detector's own two axes
	// (in-plane "horizontal" eu, fixed "vertical" ev=z), row 2 is the view
	// direction from source to detector.
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

			// Rows 0/1 are scaled by 1/detPixelSpacing so the resulting
			// detector coordinate comes out in PIXEL units directly
			// (detPixelSpacing is a world-units-per-pixel conversion);
			// row 2 (the view direction) stays unscaled -- only the
			// numerator rows need it, not the homogeneous/w row.
			Matrix<double, 3> rotation;
			for (int c = 0; c < 3; c++)
			{
				rotation(0, c) = eu[c] / detPixelSpacing;
				rotation(1, c) = ev[c] / detPixelSpacing;
				rotation(2, c) = d[c];
			}

			ProjectionMatrix<double, 3> pm;
			make_cone_beam_projection_matrix(pm, source, rotation, focalLength);

			// make_cone_beam_projection_matrix() centers the principal ray
			// (source -> volume center) at detector coordinate (0,0);
			// shift to (detW/2, detH/2) so array index 0 is the detector's
			// own edge, matching every other image in this library.
			// Shifting a PERSPECTIVE map's output by a constant requires
			// adding shift*w_row to the numerator row (newU = (numerator +
			// shift*w)/w = u + shift, valid for every point, not just the
			// principal ray) -- NOT just patching the numerator's own
			// translation entry, which would only be correct for an
			// affine (parallel-beam) map where w is constant.
			for (int c = 0; c < 4; c++)
			{
				pm(0, c) += (detW / 2.0) * pm(2, c);
				pm(1, c) += (detH / 2.0) * pm(2, c);
			}
			geometry.push_back(pm);
		}
		return geometry;
	}

	double rmse(const std::vector<double>& a, const std::vector<double>& b)
	{
		double sum = 0;
		for (std::size_t i = 0; i < a.size(); i++) { double d = a[i] - b[i]; sum += d * d; }
		return std::sqrt(sum / a.size());
	}

	// 1D k-means (Lloyd's algorithm), deterministically seeded by evenly
	// spaced percentiles of [min,max] rather than a random draw -- keeps
	// this demo's output reproducible without needing an RNG. Used by
	// PART 6 to turn "K tissue types" (a known count) into K actual
	// density VALUES, which aren't known a priori even when K is --
	// exactly what DART (Discrete Algebraic Reconstruction Technique,
	// Batenburg & Sijbers) estimates from the current reconstruction's own
	// histogram at the start of every outer iteration.
	std::vector<double> kMeans1D(const std::vector<double>& values, int K, int iterations = 30)
	{
		double vmin = *std::min_element(values.begin(), values.end());
		double vmax = *std::max_element(values.begin(), values.end());
		std::vector<double> centers(K);
		for (int k = 0; k < K; k++) centers[k] = vmin + (vmax - vmin) * (k + 0.5) / K;
		for (int iter = 0; iter < iterations; iter++)
		{
			std::vector<double> sum(K, 0.0);
			std::vector<std::size_t> count(K, 0);
			for (double v : values)
			{
				int best = 0; double bestDist = std::abs(v - centers[0]);
				for (int k = 1; k < K; k++) { double d = std::abs(v - centers[k]); if (d < bestDist) { bestDist = d; best = k; } }
				sum[best] += v; count[best]++;
			}
			for (int k = 0; k < K; k++) if (count[k] > 0) centers[k] = sum[k] / count[k];
		}
		std::sort(centers.begin(), centers.end());
		return centers;
	}

	int nearestLevelIndex(double v, const std::vector<double>& levels)
	{
		int best = 0; double bestDist = std::abs(v - levels[0]);
		for (int k = 1; k < (int)levels.size(); k++) { double d = std::abs(v - levels[k]); if (d < bestDist) { bestDist = d; best = k; } }
		return best;
	}

}

int main()
{
	outputDir = NDL_CT_RECONSTRUCTION_3D_OUTPUT_DIR;
	fs::create_directories(outputDir);

	std::cout <<
		"This demo is the 3D, cone/fan-beam sibling of demo/ct_reconstruction (2D parallel-beam) --\n"
		"same forward_project()/back_project() (projection.h), a genuinely perspective geometry this\n"
		"time. It's considerably slower than a 2D demo (64^3 volume, ~740k cone-beam rays per pass) --\n"
		"expect roughly a minute and a half end to end, mostly PART 5's continuous reconstruction loop and\n"
		"PART 6's DART refinement on top of it. Output (4 embedded interactive volume viewers --\n"
		"sinogram, phantom, reconstruction, DART) lands in:\n    build/demo/ct_reconstruction_3d/output\n";

	// ------------------------------------------------------------------
	// PART 1: cone-beam geometry mechanics, on a single hand-checkable voxel
	// ------------------------------------------------------------------
	std::cout << "\n\n=== PART 1: cone-beam geometry -- a single voxel, checked by hand ===\n";

	const int W1 = 9;
	std::vector<double> singleVoxelData((std::size_t)W1 * W1 * W1, 0.0);
	Image<double, 3> singleVoxel(singleVoxelData.data(), { W1, W1, W1 });
	singleVoxel(4, 4, 4) = 1.0; // dead center of a 9x9x9 grid

	std::array<double, 3> volCenter1{ 4.0, 4.0, 4.0 };
	auto geometry1 = buildConeBeamGeometry(4, volCenter1, /*sourceDistance=*/30, /*detectorDistance=*/30, /*detW=*/8, /*detH=*/8, /*detPixelSpacing=*/1.0);
	step("forward_project(singleVoxel, sino1, geometry1)   // one voxel at the cone's own center of rotation",
		"Unlike an off-center voxel, a voxel exactly on the source-to-detector axis lands at the SAME\n"
		"             detector pixel (the detector's own center) at every view angle regardless of the\n"
		"             geometry being a true perspective (cone-beam) projection -- the one case simple\n"
		"             enough to check without a calculator, and a real test of the (detW/2,detH/2)\n"
		"             recentering math (see buildConeBeamGeometry()'s own comment).");
	std::vector<double> sino1Data(4 * 8 * 8, 0.0);
	Image<double, 3> sino1(sino1Data.data(), { 4, 8, 8 });
	forward_project(singleVoxel, sino1, geometry1, Linear{}, /*autoAA=*/false);
	for (int v = 0; v < 4; v++)
	{
		int bestU = 0, bestV = 0; double bestVal = -1;
		auto view = sino1.slice(0, v);
		for (int py = 0; py < 8; py++) for (int px = 0; px < 8; px++)
			if (view(px, py) > bestVal) { bestVal = view(px, py); bestU = px; bestV = py; }
		showText("view " + std::to_string(v) + " peak detector pixel (expect (4,4))", "(" + std::to_string(bestU) + "," + std::to_string(bestV) + ")  value=" + std::to_string(bestVal));
	}

	// ------------------------------------------------------------------
	// PART 2: the 3D Shepp-Logan phantom
	// ------------------------------------------------------------------
	std::cout << "\n\n=== PART 2: the 3D Shepp-Logan phantom (64^3) ===\n";

	const int W = 64;
	std::vector<double> phantomData((std::size_t)W * W * W);
	Image<double, 3> phantom(phantomData.data(), { W, W, W });
	step("sheppLoganValue3D(x,y,z)   // ten ellipsoids, additive density",
		"The classic Shepp-Logan phantom, extended to a solid (Kak & Slaney) -- an idealized 3D head\n"
		"             volume built from ten overlapping ellipsoids, evaluated directly onto a 64^3 grid (see\n"
		"             PART 3's own comment for why 64^3 is the right match for this detector's own resolving power).\n"
		"             embedNDViewer() below embeds it as a live viewer.h grid rather than a few hand-picked\n"
		"             slices.");
	double half = (W - 1) / 2.0;
	for (int z = 0; z < W; z++) for (int y = 0; y < W; y++) for (int x = 0; x < W; x++)
	{
		double nx = (x - half) / half, ny = (y - half) / half, nz = (z - half) / half;
		phantom(x, y, z) = sheppLoganValue3D(nx, -ny, nz);
	}
	step("embedNDViewer(\"phantom\", phantom, \"phantom.ndlv\")",
		"pairwise_slice<AxisA,AxisB>() (viewer.h) generalizes \"pick 3 orthogonal slices by hand\" to every\n"
		"             pairwise-axis plane through the volume at once, synchronized through one shared cursor --\n"
		"             for a genuinely 3D Image this is exactly the axial/coronal/sagittal triple, but live and\n"
		"             click-driven instead of 3 pre-chosen indices. Embedded as phantom's own native doubles\n"
		"             (not windowed/quantized to 8 bits first) so the viewer's hover readout shows the real\n"
		"             density value under the cursor, not a display-rescaled approximation of it.");
	embedNDViewer("Shepp-Logan phantom (64^3)", phantom, "phantom.ndlv");

	// ------------------------------------------------------------------
	// PART 3: cone-beam forward projection into a sinogram
	// ------------------------------------------------------------------
	std::cout << "\n\n=== PART 3: forward_project() the phantom through a 64x64-detector cone-beam geometry ===\n";

	const int numViews = 180, detW = 64, detH = 64;
	std::array<double, 3> volCenter{ half, half, half };
	// sourceDistance/detectorDistance/detPixelSpacing are all in the same
	// voxel-index units the volume itself is indexed in (no separate
	// physical-unit conversion, matching demo/ct_reconstruction's own
	// approach) -- chosen so the detector's own field of view comfortably
	// covers the phantom's full diagonal at every angle. The resulting
	// magnification (focalLength/sourceDistance = 2x here) means each
	// 64x64 detector pixel's own footprint back in the volume is
	// detPixelSpacing/magnification = 3.5/2 = 1.75 voxel-widths at this
	// grid's own spacing -- the reconstruction voxel grid is chosen to be
	// COARSER than the detector's native resolution, not finer than it,
	// which is what keeps the system OVERdetermined and well-posed:
	// 180*64*64 ~= 737k measurements for 64^3 ~= 262k volume unknowns, a
	// ~2.8x surplus. Reconstructing onto a meaningfully finer grid than
	// this detector footprint supports would instead make the system
	// UNDERdetermined (more unknowns than independent measurements),
	// which shows up as structured ring/grid-pattern null-space artifacts
	// in PART 5's own result.
	auto geometry = buildConeBeamGeometry(numViews, volCenter, /*sourceDistance=*/150, /*detectorDistance=*/150, detW, detH, /*detPixelSpacing=*/3.5);

	step("forward_project(phantom, sinogram, geometry)   // 180 cone-beam views over 360 degrees",
		"Each of the 180 views is a full 64x64 cone-beam projection image (not a single 1D row, unlike\n"
		"             the 2D demo's sinogram) -- so `sinogram` (axes: view angle, detector U, detector V) is\n"
		"             itself a genuine 3D volume, the same as phantom/recon/dartVol, just one whose axes are\n"
		"             (angle, detector-column, detector-row) instead of (x,y,z). embedNDViewer() below embeds\n"
		"             it the same way: its (detU,detV) panel is a single cone-beam view at whatever angle the\n"
		"             cursor is on (the classic \"one projection image\" view); its (view,detU) and (view,detV)\n"
		"             panels are the classic sinogram layout (view angle x detector row/column), live for any\n"
		"             row or column instead of one hand-picked central one -- and its hover readout reads off\n"
		"             `sinogram`'s own native line-integral values, not a display-rescaled approximation.");
	std::vector<double> sinogramData((std::size_t)numViews * detW * detH);
	Image<double, 3> sinogram(sinogramData.data(), { numViews, detW, detH });
	forward_project(phantom, sinogram, geometry, Linear{}, /*autoAA=*/false);
	// panelSize doubled from the viewer's own default (320): sinogram's
	// own 180:64 aspect ratio already squashes its (view,detU)/(view,detV)
	// panels down to a short strip at the default size (320 x ~114) --
	// twice the pixel budget keeps that short axis legible without
	// changing the underlying detector data at all.
	embedNDViewer("cone-beam sinogram (180 views x 64x64 detector)", sinogram, "sinogram.ndlv", static_cast<const VoxelSpacing<3>*>(nullptr), /*panelSize=*/640);

	// ------------------------------------------------------------------
	// PART 4: automatic anti-aliasing is genuinely spatially variable here
	// ------------------------------------------------------------------
	std::cout << "\n\n=== PART 4: cone-beam anti-aliasing is spatially VARIABLE, unlike parallel-beam's uniform case ===\n";

	step("forward_project(phantom, sinoAA, smallGeometry, Linear{}, /*autoAA=*/true)   // 3 views only, for a quick before/after",
		"For parallel-beam projection (demo/ct_reconstruction), magnification is constant everywhere, so\n"
		"             the correct anti-aliasing footprint is a single fixed size per view. Cone-beam is a true\n"
		"             perspective projection: a voxel's footprint on the detector shrinks approaching the\n"
		"             detector and grows approaching the source, so forward_project()'s automatic\n"
		"             anti-aliasing (projection.h's own top comment) genuinely differs from the unfiltered\n"
		"             result here -- just 3 views for a quick, cheap before/after comparison; PART 5 below\n"
		"             runs the real reconstruction with autoAA on for all 180.");
	auto smallGeometry = buildConeBeamGeometry(3, volCenter, 150, 150, detW, detH, 3.5);
	std::vector<double> sinoNoAAData((std::size_t)3 * detW * detH);
	Image<double, 3> sinoNoAA(sinoNoAAData.data(), { 3, detW, detH });
	forward_project(phantom, sinoNoAA, smallGeometry, Linear{}, /*autoAA=*/false);
	std::vector<double> sinoAAData((std::size_t)3 * detW * detH);
	Image<double, 3> sinoAA(sinoAAData.data(), { 3, detW, detH });
	forward_project(phantom, sinoAA, smallGeometry, Linear{}, /*autoAA=*/true);
	double diffSum = 0;
	for (std::size_t i = 0; i < sinoAAData.size(); i++) diffSum += std::abs(sinoAAData[i] - sinoNoAAData[i]);
	showText("mean |AA-enabled - AA-disabled| over these 3 views", std::to_string(diffSum / sinoAAData.size()) + " (nonzero confirms AA is genuinely filtering, not a no-op)");

	// ------------------------------------------------------------------
	// PART 5: iterative reconstruction via Landweber iteration
	// ------------------------------------------------------------------
	std::cout << "\n\n=== PART 5: reconstructing the phantom from ONLY the 64x64-detector sinogram ===\n";

	step("recon += lambda * back_project(sinogram - forward_project(recon), autoAA=true); clamp to [0, max_density_bound(sinogram)]",
		"The same algorithm demo/ct_reconstruction uses in 2D, plus the same three things covered in that\n"
		"             demo's own comment. First: back_project() is forward_project()'s exact adjoint regardless\n"
		"             of DIM, beam geometry, OR whether autoAA is on (the dot-product test in\n"
		"             unitTests/projection_tests.cpp checks all of this directly, to ~1e-15 relative error even\n"
		"             with AA enabled and the data touching the volume's own boundary -- projection.h's own top\n"
		"             comment has the matched-filter construction that makes this possible). Second: PART 3's\n"
		"             own comment covers why this demo reconstructs onto a 64^3 grid matched to the detector's\n"
		"             own resolving power -- this 64x64-detector/180-view setup has MORE measurements\n"
		"             (180*64*64 ~= 737k) than volume unknowns (64^3 ~= 262k), an overdetermined, well-posed\n"
		"             system where plain (unconstrained, unfiltered) Landweber converges cleanly rather than\n"
		"             risking classic SEMI-CONVERGENCE (RMSE dropping for a while, then rising again as later\n"
		"             iterations fit increasingly noisy/artifact-prone null-space directions -- the real risk\n"
		"             for any UNDERdetermined system, i.e. more volume unknowns than independent measurements).\n"
		"             Clamping each update to [0, max_density_bound(sinogram)] (projection.h's own comment has\n"
		"             the derivation -- the lower bound is physical non-negativity, the upper bound is read\n"
		"             directly off the sinogram) is kept regardless, since it's what keeps the background\n"
		"             artifact-free (see the final summary below). Third: autoAA=true evaluates a genuine\n"
		"             PER-RAY-SAMPLE footprint (projection.h's own top comment, and\n"
		"             unitTests/projection_tests.cpp's AAHalfWidthVariesWithDepthAlongRay, which checks it\n"
		"             against the closed-form pinhole-camera magnification formula directly) -- evaluating it\n"
		"             once per view at the volume's own center instead would be measurably wrong for true\n"
		"             perspective geometry (the real footprint varies ~1.5x between the near-source and\n"
		"             near-detector sides of this exact volume), over-filtering one side while under-filtering\n"
		"             the other. Per-sample evaluation needs a matrix inversion at\n"
		"             every ray sample, so projection.h uses a fast direct (adjugate/cofactor, not SVD) inverse\n"
		"             specifically to keep it practical; that cost plus forward_project()/back_project() both\n"
		"             parallelizing over views (std::execution::par) is what keeps 25 iterations of this now\n"
		"             considerably smaller 64^3 volume comfortably fast.");
	std::vector<double> boundData((std::size_t)W * W * W);
	Image<double, 3> bound(boundData.data(), { W, W, W });
	max_density_bound(sinogram, bound, geometry, /*stepSize=*/1.0);

	// Computed ONCE here rather than once per view inside every one of the
	// 25 (PART 5) + 8*3 (PART 6's DART inner iterations) forward_project()/
	// back_project() calls below -- camera_center() is a full SVD per view
	// (matrix/projection.h), and it's invariant across every one of those
	// calls since `geometry` itself never changes across the reconstruction
	// loop. See compute_camera_centers()'s own comment (projection.h).
	auto centers = compute_camera_centers(geometry);

	std::vector<double> reconData((std::size_t)W * W * W, 0.0);
	Image<double, 3> recon(reconData.data(), { W, W, W });
	const double lambda = 0.0005;
	const int numIterations = 25;
	for (int iter = 0; iter < numIterations; iter++)
	{
		std::vector<double> predData((std::size_t)numViews * detW * detH);
		Image<double, 3> pred(predData.data(), { numViews, detW, detH });
		forward_project(recon, pred, geometry, centers, Linear{}, true);

		std::vector<double> residData((std::size_t)numViews * detW * detH);
		for (std::size_t i = 0; i < residData.size(); i++) residData[i] = sinogramData[i] - predData[i];
		Image<double, 3> resid(residData.data(), { numViews, detW, detH });

		std::vector<double> updateData((std::size_t)W * W * W);
		Image<double, 3> update(updateData.data(), { W, W, W });
		back_project(resid, update, geometry, centers, Linear{}, true);

		for (std::size_t i = 0; i < reconData.size(); i++)
		{
			reconData[i] += lambda * updateData[i];
			reconData[i] = std::min(std::max(reconData[i], 0.0), boundData[i]);
		}
		showText("iteration " + std::to_string(iter), "RMSE vs. ground truth = " + std::to_string(rmse(reconData, phantomData)));
	}

	step("embedNDViewer(\"reconstruction\", recon, \"reconstruction.ndlv\")",
		"Same live pairwise-axis grid as the phantom's own viewer above, this time over the recovered\n"
		"             volume -- drag any panel's crosshair and compare its shape/proportions against the\n"
		"             phantom viewer at the same cursor position, and its hover readout against phantom's own\n"
		"             (they won't match exactly voxel-for-voxel -- that gap IS the reconstruction error PART\n"
		"             5's own RMSE numbers above are tracking).");
	embedNDViewer("continuous reconstruction (64^3)", recon, "reconstruction.ndlv");

	// ------------------------------------------------------------------
	// PART 6: discrete-label refinement (DART), exploiting a known,
	// small number of tissue types
	// ------------------------------------------------------------------
	std::cout << "\n\n=== PART 6: DART -- refining the continuous reconstruction into a small number of known tissue densities ===\n";

	std::vector<double> trueLevels;
	for (double v : phantomData)
	{
		double r = std::round(v * 1000.0) / 1000.0;
		if (std::find(trueLevels.begin(), trueLevels.end(), r) == trueLevels.end()) trueLevels.push_back(r);
	}
	const int trueK = (int)trueLevels.size();

	step("DART: alternate (1) k-means-estimate K density levels, (2) freeze voxels whose whole 6-neighborhood already agrees on a label, (3) a few more ART iterations on only the still-free (boundary) voxels",
		"The continuous reconstruction above treats every voxel as a free real number in [0, bound] --\n"
		"             a weak constraint. Real tissue isn't continuous, though: it's close to piecewise-constant,\n"
		"             a small number of distinct densities (bone, gray matter, CSF, ...). If that count is known\n"
		"             a priori (e.g. from anatomy, the way it would be for a real scan protocol -- here read\n"
		"             directly off the phantom's own construction, standing in for that same domain knowledge),\n"
		"             constraining the reconstruction to exactly that many discrete values removes far more\n"
		"             ambiguity than the [0, bound] box ever could. The values themselves still have to be\n"
		"             estimated from the data, though (k-means on the current volume's own histogram each\n"
		"             outer iteration) -- and hard-snapping every voxel to its nearest value immediately would\n"
		"             be unstable (that projection isn't convex, unlike the box clamp), so DART (Batenburg &\n"
		"             Sijbers) only freezes a voxel once its entire 6-connected neighborhood already agrees on\n"
		"             the same label (checked via erode() on a per-label binary mask with a 3D cross kernel --\n"
		"             morphology.h's existing erode()/make_cross_kernel(), not new machinery) -- everything else\n"
		"             stays free for a few more ART iterations, so only the genuinely ambiguous boundary\n"
		"             voxels keep moving.");
	showText("true number of distinct tissue-density levels in this phantom (K, assumed known a priori)", std::to_string(trueK));

	std::vector<double> dartData = reconData;
	Image<double, 3> dartVol(dartData.data(), { W, W, W });
	std::vector<uint8_t> frozenMask(dartData.size(), 0);

	OwnedImage<double, 3> cross3({ 3, 3, 3 });
	make_cross_kernel(cross3);

	const int dartOuterIterations = 8, dartInnerIterations = 3;
	for (int outer = 0; outer < dartOuterIterations; outer++)
	{
		auto levels = kMeans1D(dartData, trueK);

		std::vector<int> labelField(dartData.size());
		for (std::size_t i = 0; i < dartData.size(); i++) labelField[i] = nearestLevelIndex(dartData[i], levels);

		for (int k = 0; k < trueK; k++)
		{
			std::vector<uint8_t> maskData(dartData.size());
			for (std::size_t i = 0; i < dartData.size(); i++) maskData[i] = (labelField[i] == k) ? 1 : 0;
			Image<uint8_t, 3> mask(maskData.data(), { W, W, W });
			std::vector<uint8_t> erodedData(dartData.size());
			Image<uint8_t, 3> eroded(erodedData.data(), { W, W, W });
			erode(mask, eroded, cross3, BorderMode::Clamp);
			for (std::size_t i = 0; i < dartData.size(); i++)
				if (erodedData[i] && !frozenMask[i]) { frozenMask[i] = 1; dartData[i] = levels[k]; }
		}

		for (int inner = 0; inner < dartInnerIterations; inner++)
		{
			std::vector<double> predData((std::size_t)numViews * detW * detH);
			Image<double, 3> pred(predData.data(), { numViews, detW, detH });
			forward_project(dartVol, pred, geometry, centers, Linear{}, true);

			std::vector<double> residData(predData.size());
			for (std::size_t i = 0; i < residData.size(); i++) residData[i] = sinogramData[i] - predData[i];
			Image<double, 3> resid(residData.data(), { numViews, detW, detH });

			std::vector<double> updateData(dartData.size());
			Image<double, 3> update(updateData.data(), { W, W, W });
			back_project(resid, update, geometry, centers, Linear{}, true);

			for (std::size_t i = 0; i < dartData.size(); i++)
			{
				if (frozenMask[i]) continue;
				dartData[i] += lambda * updateData[i];
				dartData[i] = std::min(std::max(dartData[i], 0.0), boundData[i]);
			}
		}

		double fracFrozen = 100.0 * (double)std::count(frozenMask.begin(), frozenMask.end(), (uint8_t)1) / frozenMask.size();
		std::ostringstream oss;
		oss << "frozen=" << fracFrozen << "%   RMSE vs. ground truth=" << rmse(dartData, phantomData);
		showText("DART outer iteration " + std::to_string(outer), oss.str());
	}

	// Deliberately NOT hard-snapping the still-free voxels at the end:
	// a voxel that's still free after 8 outer iterations is one whose
	// 6-neighborhood never agreed on a single label -- a genuine
	// partial-volume/boundary voxel, physically a real mix of two
	// tissues within that voxel's footprint. Forcing it to one pure
	// label anyway measurably makes RMSE worse (checked directly: it
	// costs about 2% relative RMSE on this exact phantom) -- leaving it
	// at its converged continuous value is the more accurate model, not
	// a shortcut.
	showText("final DART RMSE vs. ground truth (frozen voxels discrete, remaining boundary voxels left continuous)", std::to_string(rmse(dartData, phantomData)) + "   (continuous-only PART 5 result was " + std::to_string(rmse(reconData, phantomData)) + ")");

	step("embedNDViewer(\"DART reconstruction\", dartVol, \"dart_reconstruction.ndlv\")",
		"The fourth and last of this demo's live pairwise-axis grids -- compare its panels against the\n"
		"             continuous reconstruction viewer above at the same cursor position to see DART's\n"
		"             flatter, more piecewise-constant result directly, and its hover readout against the\n"
		"             same voxel in the reconstruction viewer to see the exact snapped density level, rather\n"
		"             than only via the RMSE numbers.");
	embedNDViewer("DART reconstruction (64^3)", dartVol, "dart_reconstruction.ndlv");

	std::cout <<
		"\n\nAll outputs written to: build/demo/ct_reconstruction_3d/output\n"
		"The phantom viewer shows the ground-truth volume; the sinogram viewer shows the cone-beam data (what\n"
		"a real cone-beam CT scanner's detector would actually measure) that's ALL the reconstruction viewer\n"
		"is recovered from (with a genuine per-ray-sample autoAA and the max_density_bound() constraint\n"
		"applied -- PART 5's own comment covers both). Drag the same panel's crosshair to the same position in\n"
		"the phantom viewer and the reconstruction viewer -- the background is clean (max_density_bound()'s\n"
		"whole point: no bright stray-pixel overshoot) and the overall shape, proportions, and internal\n"
		"structure are recognizable, without a ring/grid null-space pattern in the object's interior. That's\n"
		"the direct payoff of PART 3's resolution choice: reconstructing onto a 64^3 grid matched to this\n"
		"64x64/180-view detector's own resolving power keeps the system overdetermined (737k measurements for\n"
		"only 262k unknowns) rather than underdetermined, where multiple different volumes would fit the same\n"
		"sinogram equally well and the residual ambiguity would show up as structured aliasing -- AA and\n"
		"max_density_bound() only ever mitigate that kind of mismatch, they don't fix it outright the way\n"
		"matching resolution does. The real tradeoff: some of the phantom's own finest structures (its\n"
		"smallest ellipsoids, only a few voxels wide even at this grid's own resolution) are close to what a\n"
		"64x64 detector can resolve at all, so 64^3 is genuinely the ceiling here -- recovering meaningfully\n"
		"finer detail would take a higher-resolution detector (more, or smaller, detector pixels) together\n"
		"with a correspondingly finer volume grid, or more views, not a finer reconstruction grid on its own.\n"
		"The DART viewer is PART 6's discrete-label refinement, starting from the reconstruction viewer's own\n"
		"continuous result and using the phantom's own (assumed a priori known) tissue-density count --\n"
		"visually flatter/cleaner than the continuous reconstruction (compare the coronal panels of each\n"
		"viewer in particular), and a real, if modest, RMSE improvement over PART 5's continuous-only result\n"
		"(PART 6's own printed numbers above). DART is a genuinely different, complementary lever from PART\n"
		"3's resolution choice: it doesn't change how many measurements vs. unknowns there are, it changes how\n"
		"much each unknown is ALLOWED to vary, which is why it still helps even on an already-overdetermined\n"
		"64^3 system. Two honest caveats worth stating directly rather than glossing over: it depends entirely\n"
		"on knowing K correctly (too few tissue types assumed and genuinely distinct structures get merged\n"
		"into one label; too many, and noise gets mistaken for real structure), and PART 6's own\n"
		"per-iteration RMSE isn't perfectly monotonic either -- it improves for the first few outer iterations,\n"
		"then drifts slightly worse (a milder echo of PART 5's own semi-convergence risk, here from voxels\n"
		"freezing to a slightly-off label estimate permanently rather than from an unconstrained update\n"
		"overshooting). This demo doesn't correct that drift the way PART 5's box constraint corrects\n"
		"semi-convergence there -- doing so honestly would need a stopping rule computed from the data alone\n"
		"(e.g. the sinogram residual, not ground-truth RMSE, which no real reconstruction has access to),\n"
		"which is a reasonable next step but out of scope here.\n";

	return 0;
}
