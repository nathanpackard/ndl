#include <ndl/image.h>
#include <ndl/imageIO.h>
#include <ndl/matrix.h>
#include <ndl/projection.h>
#include <ndl/visualize.h>
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
// same exact-adjoint back-projection, same Landweber iteration, but a real
// 128^3 volume, a genuinely PERSPECTIVE (not orthographic) geometry, and a
// 64x64 detector considerably coarser than the volume's own resolution --
// specifically to exercise the two things that only show up once you leave
// 2D parallel-beam: a true cone/fan-beam projection matrix (nontrivial
// homogeneous row, real perspective divide), and automatic anti-aliasing
// whose footprint genuinely varies with a voxel's own distance from the
// source (unlike parallel-beam's spatially-uniform case -- see PART 4
// below). Unlike the 2D demo, PART 5's reconstruction loop here actually
// runs with autoAA on, not just off -- this geometry's own detector/volume
// resolution mismatch is exactly the case automatic anti-aliasing exists
// for (projection.h's own top comment has the matched-filter derivation
// that makes this possible without breaking forward_project()/
// back_project()'s exact adjointness).
//
// This demo is considerably slower than every other demo in this repo --
// 128^3 = ~2.1M voxels, 180 views x 64x64 = ~740k detector pixels, each
// ray marched through up to ~220 voxel-steps -- the same "by far the most
// expensive step in this whole demo" situation demo/motion's sift_flow()
// step flags, just for the whole demo this time. forward_project()/
// back_project() both parallelize over views via std::execution::par
// (projection.h's own top comment) when a parallel STL backend is
// available, which is most of what keeps this demo's own runtime down to
// roughly 1-2 minutes end to end despite autoAA's extra per-sample cost --
// still dominated by PART 5's reconstruction loop.
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

	void saveSlice(const std::string& label, const Image<double, 3>& vol, int axis, int index, const std::string& filename)
	{
		auto slice = vol.slice(axis, index);
		OwnedImage<uint8_t, 2> heat(slice.extent());
		heatmap(slice, heat, (uint8_t)255);
		saveForInspection(label, heat, filename);
	}
}

int main()
{
	outputDir = NDL_CT_RECONSTRUCTION_3D_OUTPUT_DIR;
	fs::create_directories(outputDir);

	std::cout <<
		"This demo is the 3D, cone/fan-beam sibling of demo/ct_reconstruction (2D parallel-beam) --\n"
		"same forward_project()/back_project() (projection.h), a genuinely perspective geometry this\n"
		"time. It's considerably slower (128^3 volume, ~740k cone-beam rays per pass) -- expect roughly\n"
		"1-2 minutes end to end, mostly PART 5's reconstruction loop. Output PNGs land in:\n    " << outputDir << "\n";

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
	std::cout << "\n\n=== PART 2: the 3D Shepp-Logan phantom (128^3) ===\n";

	const int W = 128;
	std::vector<double> phantomData((std::size_t)W * W * W);
	Image<double, 3> phantom(phantomData.data(), { W, W, W });
	step("sheppLoganValue3D(x,y,z)   // ten ellipsoids, additive density",
		"The classic Shepp-Logan phantom, extended to a solid (Kak & Slaney) -- an idealized 3D head\n"
		"             volume built from ten overlapping ellipsoids. Three orthogonal central slices are\n"
		"             saved below (axial/coronal/sagittal) since a whole volume can't be viewed as one PNG.");
	double half = (W - 1) / 2.0;
	for (int z = 0; z < W; z++) for (int y = 0; y < W; y++) for (int x = 0; x < W; x++)
	{
		double nx = (x - half) / half, ny = (y - half) / half, nz = (z - half) / half;
		phantom(x, y, z) = sheppLoganValue3D(nx, -ny, nz);
	}
	saveSlice("phantom, axial slice (z=64)", phantom, 2, W / 2, "01_phantom_axial.png");
	saveSlice("phantom, coronal slice (y=64)", phantom, 1, W / 2, "02_phantom_coronal.png");
	saveSlice("phantom, sagittal slice (x=64)", phantom, 0, W / 2, "03_phantom_sagittal.png");

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
	// magnification (focalLength/sourceDistance = 2x here) combined with
	// a 64-pixel detector deliberately makes the detector considerably
	// coarser than the volume's own 128^3 resolution -- see PART 4.
	auto geometry = buildConeBeamGeometry(numViews, volCenter, /*sourceDistance=*/300, /*detectorDistance=*/300, detW, detH, /*detPixelSpacing=*/7.0);

	step("forward_project(phantom, sinogram, geometry)   // 180 cone-beam views over 360 degrees",
		"Each of the 180 saved views is a full 64x64 cone-beam projection image (not a single 1D row,\n"
		"             unlike the 2D demo's sinogram) -- 04_projection_view0.png shows one representative view;\n"
		"             05_sinogram_slice.png shows the classic sinogram layout (view angle x detector row) for\n"
		"             just the detector's own central row, across all 180 views.");
	std::vector<double> sinogramData((std::size_t)numViews * detW * detH);
	Image<double, 3> sinogram(sinogramData.data(), { numViews, detW, detH });
	forward_project(phantom, sinogram, geometry, Linear{}, /*autoAA=*/false);

	auto view0 = sinogram.slice(0, 0);
	OwnedImage<uint8_t, 2> view0Heat(view0.extent());
	heatmap(view0, view0Heat, (uint8_t)255);
	saveForInspection("cone-beam projection, view 0", view0Heat, "04_projection_view0.png");

	auto sinoSlice = sinogram.slice(2, detH / 2);
	OwnedImage<uint8_t, 2> sinoSliceHeat(sinoSlice.extent());
	heatmap(sinoSlice, sinoSliceHeat, (uint8_t)255);
	saveForInspection("sinogram slice (central detector row, all views)", sinoSliceHeat, "05_sinogram_slice.png");

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
	auto smallGeometry = buildConeBeamGeometry(3, volCenter, 300, 300, detW, detH, 7.0);
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
		"The same algorithm demo/ct_reconstruction uses in 2D, plus two things that particular\n"
		"             demo doesn't need. First: back_project() is forward_project()'s exact adjoint regardless\n"
		"             of DIM, beam geometry, OR whether autoAA is on (the dot-product test in\n"
		"             unitTests/projection_tests.cpp checks all of this directly, to ~1e-15 relative error even\n"
		"             with AA enabled and the data touching the volume's own boundary -- projection.h's own top\n"
		"             comment has the matched-filter construction that makes this possible). Second: this\n"
		"             64x64-detector/180-view setup has fewer measurements (180*64*64 ~= 737k) than volume\n"
		"             unknowns (128^3 ~= 2.1M) -- an underdetermined system, where PLAIN (unconstrained,\n"
		"             unfiltered) Landweber exhibits classic SEMI-CONVERGENCE: RMSE against the ground truth\n"
		"             drops for the first ~10 iterations, then starts RISING again as later iterations fit\n"
		"             increasingly noisy/artifact-prone null-space directions instead of real structure.\n"
		"             Clamping each update to [0, max_density_bound(sinogram)] (projection.h's own comment has\n"
		"             the derivation -- the lower bound is physical non-negativity, the upper bound is read\n"
		"             directly off the sinogram) fixes the semi-convergence outright: box-constrained\n"
		"             (\"projected\") gradient descent doesn't wander into those noisy directions in the first\n"
		"             place, RMSE keeps falling monotonically for at least 25 iterations. autoAA=true goes\n"
		"             further: since this geometry's detector really is coarser than the volume (PART 4 just\n"
		"             confirmed autoAA changes the forward projection here, not a no-op), letting\n"
		"             forward_project() account for that instead of aliasing it away measurably lowers the\n"
		"             RMSE this reaches at every iteration -- not just a display fix, a better-conditioned\n"
		"             problem for the iteration to actually solve. Both parallelized over views\n"
		"             (std::execution::par) is what keeps 25 iterations of a 128^3 cone-beam volume, with AA,\n"
		"             inside this demo's own 1-2 minute budget.");
	std::vector<double> boundData((std::size_t)W * W * W);
	Image<double, 3> bound(boundData.data(), { W, W, W });
	max_density_bound(sinogram, bound, geometry, /*stepSize=*/1.0);

	std::vector<double> reconData((std::size_t)W * W * W, 0.0);
	Image<double, 3> recon(reconData.data(), { W, W, W });
	const double lambda = 0.0005;
	const int numIterations = 25;
	for (int iter = 0; iter < numIterations; iter++)
	{
		std::vector<double> predData((std::size_t)numViews * detW * detH);
		Image<double, 3> pred(predData.data(), { numViews, detW, detH });
		forward_project(recon, pred, geometry, Linear{}, true);

		std::vector<double> residData((std::size_t)numViews * detW * detH);
		for (std::size_t i = 0; i < residData.size(); i++) residData[i] = sinogramData[i] - predData[i];
		Image<double, 3> resid(residData.data(), { numViews, detW, detH });

		std::vector<double> updateData((std::size_t)W * W * W);
		Image<double, 3> update(updateData.data(), { W, W, W });
		back_project(resid, update, geometry, Linear{}, true);

		for (std::size_t i = 0; i < reconData.size(); i++)
		{
			reconData[i] += lambda * updateData[i];
			reconData[i] = std::min(std::max(reconData[i], 0.0), boundData[i]);
		}
		showText("iteration " + std::to_string(iter), "RMSE vs. ground truth = " + std::to_string(rmse(reconData, phantomData)));
	}

	saveSlice("reconstruction, axial slice (z=64)", recon, 2, W / 2, "06_reconstruction_axial.png");
	saveSlice("reconstruction, coronal slice (y=64)", recon, 1, W / 2, "07_reconstruction_coronal.png");
	saveSlice("reconstruction, sagittal slice (x=64)", recon, 0, W / 2, "08_reconstruction_sagittal.png");

	std::cout <<
		"\n\nAll outputs written to: " << outputDir << "\n"
		"01-03 are the ground-truth phantom's three central slices; 04-05 show the cone-beam sinogram\n"
		"(what a real cone-beam CT scanner's detector would actually measure); 06-08 are the\n"
		"reconstruction's matching slices, recovered from ONLY that sinogram (with autoAA on and the\n"
		"max_density_bound() constraint applied -- PART 5's own comment covers both). Compare 06-08\n"
		"against 01-03 -- the background is clean (max_density_bound()'s whole point: no bright\n"
		"stray-pixel overshoot), the overall oval shape and largest internal structure are recognizable,\n"
		"and the ring/grid pattern within the object outline is visibly SOFTER than it would be with AA\n"
		"off (RMSE is measurably lower too, at every iteration -- PART 5's own comment), but it isn't\n"
		"gone. That's expected, not a bug: AA stops the iteration from aliasing detail the sinogram can't\n"
		"support into sharp noise, but it doesn't add information -- the system is still underdetermined\n"
		"(737k detector measurements for 2.1M volume unknowns), and max_density_bound() is only ever tight\n"
		"for background voxels, not interior ones (projection.h's own comment on it has the reasoning), so\n"
		"neither constraint touches the remaining interior ambiguity. demo/ct_reconstruction's 2D/\n"
		"60-iteration result looks cleaner throughout because that problem isn't underdetermined at all\n"
		"(145 detector pixels x 90 views vs. only 10000 pixel unknowns) -- closing the rest of this gap\n"
		"here would take more views, a higher-resolution detector, or real regularization (e.g.\n"
		"total-variation), not just more of what's already implemented.\n";

	return 0;
}
