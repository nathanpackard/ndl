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

// A step-by-step tour of ndl::forward_project()/ndl::back_project()
// (projection.h) -- ray-marched CT-style forward projection and its exact
// adjoint -- ending in a real (if small) iterative CT reconstruction: an
// analytic 2D Shepp-Logan phantom forward-projected into a sinogram, then
// recovered from just that sinogram via Landweber iteration
// (x += lambda * back_project(y - forward_project(x))), the simplest
// member of the same family of algorithms (ART/SART/OS-SEM/...) real CT
// scanners actually use. See projection.h's own top comment for why
// back_project() is built to be forward_project()'s EXACT adjoint (not
// merely a good approximation of one) -- that exactness is what makes
// this iteration converge cleanly rather than drift.
//
// step()/showArray()/showText()/saveForInspection()/outputDir come from
// demoHelpers.h, shared with every other demo.

// The classic Shepp-Logan phantom (Shepp & Logan, "The Fourier
// Reconstruction of a Head Section", IEEE Trans. Nuclear Science, 1974):
// ten ellipses, each adding (or, for the negative-amplitude ones,
// subtracting) a constant density over its own interior -- the standard
// synthetic test image for CT reconstruction algorithms, chosen because
// its sharp boundaries and varied feature sizes stress a reconstruction
// the way a real head CT scan's anatomy does, without needing real patient
// data. Coordinates are normalized to fit within the unit disk.
namespace
{
	struct Ellipse { double A, a, b, x0, y0, phiDeg; };
	const Ellipse sheppLoganEllipses[10] = {
		{  2.00, 0.6900, 0.9200,  0.0000,  0.0000,   0 },
		{ -0.98, 0.6624, 0.8740,  0.0000, -0.0184,   0 },
		{ -0.02, 0.1100, 0.3100,  0.2200,  0.0000, -18 },
		{ -0.02, 0.1600, 0.4100, -0.2200,  0.0000,  18 },
		{  0.01, 0.2100, 0.2500,  0.0000,  0.3500,   0 },
		{  0.01, 0.0460, 0.0460,  0.0000,  0.1000,   0 },
		{  0.01, 0.0460, 0.0460,  0.0000, -0.1000,   0 },
		{  0.01, 0.0460, 0.0230, -0.0800, -0.6050,   0 },
		{  0.01, 0.0230, 0.0230,  0.0000, -0.6060,   0 },
		{  0.01, 0.0230, 0.0460,  0.0600, -0.6050,   0 },
	};

	double sheppLoganValue(double x, double y)
	{
		double v = 0;
		for (const auto& e : sheppLoganEllipses)
		{
			double phi = e.phiDeg * M_PI / 180.0;
			double dx = x - e.x0, dy = y - e.y0;
			double xr = dx * std::cos(phi) + dy * std::sin(phi);
			double yr = -dx * std::sin(phi) + dy * std::cos(phi);
			if ((xr * xr) / (e.a * e.a) + (yr * yr) / (e.b * e.b) <= 1.0) v += e.A;
		}
		return v;
	}

	// Standard parallel-beam geometry: `numViews` angles evenly spanning
	// 0..180 degrees (a parallel-beam sinogram is periodic over 180, not
	// 360 -- a ray and its exact opposite measure the same line integral).
	// Each view's ProjectionMatrix maps volume voxel coordinates to a 1D
	// detector coordinate, centered so the volume's own center projects to
	// the detector's own center at every angle.
	std::vector<ProjectionMatrix<double, 2>> buildParallelGeometry(int numViews, double volCenterX, double volCenterY, double detCenter)
	{
		std::vector<ProjectionMatrix<double, 2>> geometry;
		for (int v = 0; v < numViews; v++)
		{
			double theta = M_PI * v / numViews;
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

	double rmse(const std::vector<double>& a, const std::vector<double>& b)
	{
		double sum = 0;
		for (std::size_t i = 0; i < a.size(); i++) { double d = a[i] - b[i]; sum += d * d; }
		return std::sqrt(sum / a.size());
	}
}

int main()
{
	outputDir = NDL_CT_RECONSTRUCTION_OUTPUT_DIR;
	fs::create_directories(outputDir);

	std::cout <<
		"This demo teaches ndl::forward_project()/ndl::back_project() (projection.h) the same\n"
		"way demo/convolution teaches convolve(): each step shows the code, explains it, and\n"
		"shows the result -- first on small numbers you can check by hand, then on a real (if\n"
		"synthetic) CT reconstruction problem. Output PNGs land in:\n    " << outputDir << "\n";

	// ------------------------------------------------------------------
	// PART 1: forward_project() mechanics, on a single hand-checkable voxel
	// ------------------------------------------------------------------
	std::cout << "\n\n=== PART 1: forward_project() -- a single voxel, checked by hand ===\n";

	const int W1 = 9, H1 = 9;
	std::vector<double> singleVoxelData(W1 * H1, 0.0);
	Image<double, 2> singleVoxel(singleVoxelData.data(), { W1, H1 });
	singleVoxel(4, 4) = 1.0; // dead center of a 9x9 grid (indices 0..8)

	auto geometry1 = buildParallelGeometry(4, 4.0, 4.0, 6.0); // 4 views, 0/45/90/135 deg; detector center = 6
	std::vector<double> sino1Data(4 * 13);
	Image<double, 2> sino1(sino1Data.data(), { 4, 13 });
	step("forward_project(singleVoxel, sino1, geometry1)   // one voxel at the volume's own center",
		"A voxel exactly at the geometry's own center of rotation projects to the SAME detector\n"
		"             coordinate (6, the detector's own center) at every view angle -- the one case simple\n"
		"             enough to check without a calculator.");
	forward_project(singleVoxel, sino1, geometry1, Linear{}, /*autoAA=*/false);
	for (int v = 0; v < 4; v++)
	{
		int bestDet = 0; double bestVal = -1;
		for (int d = 0; d < 13; d++) if (sino1(v, d) > bestVal) { bestVal = sino1(v, d); bestDet = d; }
		showText("view " + std::to_string(v) + " peak detector index (expect 6)", std::to_string(bestDet) + "  (value " + std::to_string(bestVal) + ")");
	}

	// ------------------------------------------------------------------
	// PART 2: the Shepp-Logan phantom
	// ------------------------------------------------------------------
	std::cout << "\n\n=== PART 2: the Shepp-Logan phantom ===\n";

	const int W = 100, H = 100;
	std::vector<double> phantomData(W * H);
	Image<double, 2> phantom(phantomData.data(), { W, H });
	step("sheppLoganValue(x,y)   // ten ellipses, additive density -- see this file's own comment",
		"The classic Shepp-Logan phantom (Shepp & Logan, 1974) -- the standard synthetic test image\n"
		"             for CT reconstruction, an idealized head cross-section built from ten overlapping\n"
		"             ellipses (some subtracting density, carving out the brain's internal structure).");
	for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
	{
		double nx = (x - (W - 1) / 2.0) / ((W - 1) / 2.0);
		double ny = (y - (H - 1) / 2.0) / ((H - 1) / 2.0);
		phantom(x, y) = sheppLoganValue(nx, -ny);
	}
	OwnedImage<uint8_t, 2> phantomHeatmap({ W, H });
	heatmap(phantom, phantomHeatmap, (uint8_t)255);
	saveForInspection("Shepp-Logan phantom (ground truth)", phantomHeatmap, "01_phantom.png");

	// ------------------------------------------------------------------
	// PART 3: forward projection into a sinogram
	// ------------------------------------------------------------------
	std::cout << "\n\n=== PART 3: forward_project() the phantom into a sinogram ===\n";

	const int numViews = 90, numDet = 145; // numDet >= W*sqrt(2), so every view sees the whole phantom
	double volCenterX = (W - 1) / 2.0, volCenterY = (H - 1) / 2.0;
	double detCenter = (numDet - 1) / 2.0;
	auto geometry = buildParallelGeometry(numViews, volCenterX, volCenterY, detCenter);

	step("forward_project(phantom, sinogram, geometry)   // 90 parallel-beam views over 180 degrees",
		"Each row of 02_sinogram.png is one view's own 1D projection (a line integral through the\n"
		"             phantom at that angle); stacking all 90 rows gives the classic sinogram -- named for\n"
		"             the sinusoidal trace a single off-center point source (see PART 1 above, and\n"
		"             unitTests/projection_tests.cpp's point-source test) would trace across it.");
	std::vector<double> sinogramData(numViews * numDet);
	Image<double, 2> sinogram(sinogramData.data(), { numViews, numDet });
	forward_project(phantom, sinogram, geometry, Linear{}, /*autoAA=*/false);
	OwnedImage<uint8_t, 2> sinogramHeatmap({ numViews, numDet });
	heatmap(sinogram, sinogramHeatmap, (uint8_t)255);
	saveForInspection("sinogram (one row per view)", sinogramHeatmap, "02_sinogram.png");

	// ------------------------------------------------------------------
	// PART 4: iterative reconstruction via Landweber iteration
	// ------------------------------------------------------------------
	std::cout << "\n\n=== PART 4: reconstructing the phantom from ONLY the sinogram ===\n";

	step("recon += lambda * back_project(sinogram - forward_project(recon)); clamp to [0, max_density_bound(sinogram)]",
		"Starting from an all-zero volume, repeatedly forward-project the current estimate, compare\n"
		"             against the real sinogram, and back-project the residual to correct the estimate -- the\n"
		"             simplest member of the ART/SART/OS-SEM family of iterative CT reconstruction\n"
		"             algorithms real scanners use. That back_project() is forward_project()'s EXACT adjoint\n"
		"             (projection.h's own top comment explains why -- verified directly by\n"
		"             unitTests/projection_tests.cpp's dot-product test) is what makes this a well-behaved\n"
		"             gradient-descent step on ||forward_project(x) - sinogram||^2, rather than an ad-hoc\n"
		"             update rule that happens to sort of work.\n"
		"             Each update is also CLAMPED to [0, max_density_bound(sinogram)] -- a box constraint\n"
		"             (\"projected\" Landweber/gradient descent), not just a display fix. The lower bound (0) is\n"
		"             physical: density/attenuation can't be negative. The upper bound is derived per-voxel\n"
		"             directly from the measured sinogram itself (max_density_bound(), projection.h's own\n"
		"             comment has the derivation) -- background voxels the sinogram shows a clear line of\n"
		"             sight through (a near-zero ray sum at some angle) get clamped tightly near zero, which is\n"
		"             exactly what suppresses the bright stray-pixel overshoot artifacts unconstrained Landweber\n"
		"             is otherwise prone to in the background.");

	std::vector<double> boundData(W * H);
	Image<double, 2> bound(boundData.data(), { W, H });
	max_density_bound(sinogram, bound, geometry, /*stepSize=*/1.0);

	std::vector<double> reconData(W * H, 0.0);
	Image<double, 2> recon(reconData.data(), { W, H });
	const double lambda = 0.0002;
	const int numIterations = 60;
	for (int iter = 0; iter < numIterations; iter++)
	{
		std::vector<double> predData(numViews * numDet);
		Image<double, 2> pred(predData.data(), { numViews, numDet });
		forward_project(recon, pred, geometry, Linear{}, false);

		std::vector<double> residData(numViews * numDet);
		for (int i = 0; i < numViews * numDet; i++) residData[i] = sinogramData[i] - predData[i];
		Image<double, 2> resid(residData.data(), { numViews, numDet });

		std::vector<double> updateData(W * H);
		Image<double, 2> update(updateData.data(), { W, H });
		back_project(resid, update, geometry, Linear{}, false);

		for (int i = 0; i < W * H; i++)
		{
			reconData[i] += lambda * updateData[i];
			reconData[i] = std::min(std::max(reconData[i], 0.0), boundData[i]);
		}

		if (iter % 10 == 0 || iter == numIterations - 1)
			showText("iteration " + std::to_string(iter), "RMSE vs. ground truth = " + std::to_string(rmse(reconData, phantomData)));
	}

	OwnedImage<uint8_t, 2> reconHeatmap({ W, H });
	heatmap(recon, reconHeatmap, (uint8_t)255);
	saveForInspection("reconstruction after " + std::to_string(numIterations) + " box-constrained Landweber iterations", reconHeatmap, "03_reconstruction.png");

	std::cout <<
		"\n\nAll outputs written to: " << outputDir << "\n"
		"01 is the ground-truth phantom; 02 is its sinogram (what a real CT scanner would actually\n"
		"measure); 03 is the reconstruction recovered from ONLY that sinogram, via forward_project()/\n"
		"back_project() alone (plus the sinogram-derived box constraint described above) -- compare 03\n"
		"against 01 to judge reconstruction quality, and the RMSE numbers above to see it decrease\n"
		"monotonically as the iteration progresses.\n";

	return 0;
}
