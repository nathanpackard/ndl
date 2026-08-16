#include <ndl/image.h>
#include <ndl/viewer.h>
#include <iostream>
#include <filesystem>
#include <cstdint>
#include <cmath>
#include <vector>
#include <array>
#include <string>
#include "../demoHelpers.h"

using namespace ndl;
namespace fs = std::filesystem;

// A tour of viewer.h's N-D pairwise-slice viewer, on a plain 3D volume (the
// classic axial/coronal/sagittal case), a genuinely 4-dimensional one (3
// spatial axes plus a time axis, so none of its C(4,2)=6 pairwise views are
// a disguised 2D/3D case), and a genuinely 5-dimensional one (3 spatial
// axes, time, and a channel axis, so its C(5,2)=10 pairwise views include
// one -- time vs. channel -- that isn't spatial at all) -- showing the same
// viewer, unmodified, generalizes across DIM rather than the 3D case being
// some separately-handled special path. step()/showText()/embedNDViewer()/
// outputDir come from demoHelpers.h, shared with every other demo --
// embedNDViewer() specifically is new (see its own comment in
// demoHelpers.h): it writes viewer.h's binary format and prints the marker
// docs/generate_tutorial.py turns into a live, embedded copy of the viewer,
// right in the generated tutorial page.
//
// The 3D volume is a torus -- deliberately not another sphere, so its 3
// panels are visually distinguishable at a glance from the 4D and 5D
// volumes' own purely-spatial panels below. The 4D volume is a sphere whose
// center drifts and whose radius pulses over the time axis -- so its 3
// purely-spatial views (axes (0,1),(0,2),(1,2), each at a fixed t) look
// like an ordinary static sphere cross-section, while the 3 views involving
// the time axis ((0,3),(1,3),(2,3)) show that motion directly: each is a
// space-vs-time plot where the sphere's radius change and center drift
// trace out a visible curve, not just a static shape repeated across
// columns. The 5D volume adds a channel axis whose own phase differs from
// time's, so the one panel showing both (3,4) traces a genuinely 2D
// Lissajous-style curve rather than a straight line.
int main()
{
	outputDir = NDL_ND_VIEWER_OUTPUT_DIR;
	fs::create_directories(outputDir);

	std::cout <<
		"This demo tours viewer.h's N-D pairwise-slice primitives on a plain 3D volume,\n"
		"a genuinely 4D one (3 space + 1 time), and a genuinely 5D one (3 space + time +\n"
		"channel), then embeds the actual interactive viewer for each right in this page\n"
		"(if you're reading the generated tutorial -- see docs/generate_tutorial.py) so\n"
		"you can click around them yourself. Output lands in:\n    build/demo/nd_viewer/output\n";

	std::cout << "\n\n=== Building a synthetic 3D volume ===\n";

	step("Image<uint8_t,3> vol3d(data, {n,n,n})",
		"A torus (donut shape), the classic axial/coronal/sagittal case viewer.h generalizes:\n"
		"             DIM=3 has exactly C(3,2)=3 pairwise views, one per pair of the 3 spatial axes -- the\n"
		"             same axial/coronal/sagittal triple a clinical volume viewer shows, just synchronized\n"
		"             through pairwise_slice()'s one shared cursor instead of 3 separately-coded planes.");

	const int n3 = 40;
	std::vector<uint8_t> storage3(static_cast<std::size_t>(n3) * n3 * n3);
	Image<uint8_t, 3> vol3d(storage3.data(), { n3, n3, n3 });
	{
		double cx = n3 * 0.5, cy = n3 * 0.5, cz = n3 * 0.5;
		double majorRadius = n3 * 0.28, minorRadius = n3 * 0.12;
		for (const auto& c : vol3d.coordinates())
		{
			double dx = c[0] - cx, dy = c[1] - cy, dz = c[2] - cz;
			double planarDist = std::sqrt(dx * dx + dy * dy) - majorRadius;
			double d = std::sqrt(planarDist * planarDist + dz * dz);
			vol3d.at(c) = static_cast<uint8_t>(d < minorRadius ? 220 : 30);
		}
	}
	showText("vol3d.extent()", "{" + std::to_string(n3) + "," + std::to_string(n3) + "," + std::to_string(n3) + "}");

	step("embedNDViewer(\"3D torus\", vol3d, \"volume3d.ndlv\")",
		"In the generated tutorial page this becomes a live grid of all 3 pairwise views -- click or\n"
		"             drag in any panel to move the shared cursor and watch the other two panels jump to\n"
		"             match, exactly the clinical triple-view interaction, generalized.");
	embedNDViewer("3D torus", vol3d, "volume3d.ndlv");

	std::cout << "\n\n=== Building a synthetic 4D (space x time) volume ===\n";

	step("Image<uint8_t,4> vol(data, {n,n,n,nt})",
		"A sphere whose center drifts diagonally and whose radius pulses sinusoidally over a 4th\n"
		"             (time) axis -- deliberately not a static 3D shape just repeated across time, so the\n"
		"             views involving the time axis show something genuinely different from the 3 purely-\n"
		"             spatial ones below.");

	const int n = 28, nt = 16;
	std::vector<uint8_t> storage(static_cast<std::size_t>(n) * n * n * nt);
	Image<uint8_t, 4> vol(storage.data(), { n, n, n, nt });
	for (const auto& c : vol.coordinates())
	{
		double t = static_cast<double>(c[3]) / (nt - 1); // 0..1
		double cx = n * 0.35 + n * 0.3 * t;
		double cy = n * 0.5;
		double cz = n * 0.5;
		double radius = n * 0.15 + n * 0.10 * std::sin(2.0 * 3.14159265358979323846 * t);
		double d = std::sqrt((c[0] - cx) * (c[0] - cx) + (c[1] - cy) * (c[1] - cy) + (c[2] - cz) * (c[2] - cz));
		vol.at(c) = static_cast<uint8_t>(d < radius ? 220 : 30);
	}
	showText("extent", "{" + std::to_string(n) + "," + std::to_string(n) + "," + std::to_string(n) + "," + std::to_string(nt) + "}");

	std::cout << "\n\n=== Checking one pairwise slice by hand ===\n";

	step("auto slice03 = pairwise_slice<0,3>(vol, cursor);   // space axis 0 vs. time",
		"pairwise_slice<AxisA,AxisB>() extracts the zero-copy 2D view spanning two axes, fixed\n"
		"             elsewhere at cursor -- here axis 0 (an X position) against axis 3 (time), fixed at the\n"
		"             volume's spatial center along Y/Z. Since the sphere's radius pulses over time, this\n"
		"             slice should show a wavy band (wide where the pulsing sphere is large at this X\n"
		"             position, narrow or absent where it's small), not a straight bar -- exactly the\n"
		"             \"space vs. time trace\" pairwise_slice() makes viewable at all, generalizing the\n"
		"             classic axial/coronal/sagittal synchronized 3-plane viewer to a 4th (or Nth) axis\n"
		"             that isn't spatial at all.");
	std::array<int, 4> cursor = { n / 2, n / 2, n / 2, nt / 2 };
	auto slice03 = pairwise_slice<0, 3>(vol, cursor);
	showText("slice03.extent()", "{" + std::to_string(slice03.extent()[0]) + "," + std::to_string(slice03.extent()[1]) + "}  (expected {" + std::to_string(n) + "," + std::to_string(nt) + "})");

	std::cout << "\n\n=== Embedding the interactive viewer ===\n";

	step("VoxelSpacing<4> spacing;   // 3 spatial axes in mm, the time axis in s",
		"viewer.h's VoxelSpacing<DIM> is optional per-axis physical calibration -- deliberately not\n"
		"             part of Image itself (see VoxelSpacing's own comment for why), so it's supplied\n"
		"             separately, right where it's actually used. Axes 0-2 (space) share one unit (\"mm\"), axis 3\n"
		"             (time) uses a different one (\"s\") entirely -- this is exactly the case a single shared\n"
		"             physical scale can't handle: 2.0mm x 28 voxels is a 56mm cube, meaningfully compared\n"
		"             between the 3 spatial axes, but there's no sane way to compare 56mm against 0.5s x 16\n"
		"             frames = 8s, so the two stay on separate physical scales (see ndlviewer.js's own\n"
		"             computePerAxisPixelSizes() comment for how the viewer resolves that).");
	VoxelSpacing<4> spacing;
	spacing.spacing = { 2.0, 2.0, 2.0, 0.5 };
	spacing.unit = { "mm", "mm", "mm", "s" };

	step("embedNDViewer(\"4D sphere\", vol, \"volume.ndlv\", &spacing)",
		"Writes the whole 4D volume in ndlviewer.js's binary format, spacing included. In the generated\n"
		"             tutorial page this becomes a live grid of all 6 pairwise views (space/space and space/time\n"
		"             alike) -- click or drag in any panel to move the shared cursor and watch every other panel\n"
		"             (including the ones showing the time axis) jump to match. Two visible effects of `spacing`\n"
		"             specifically: the space/space panels are now perfect squares (all 3 spatial axes share\n"
		"             \"mm\" at the same 2.0mm spacing, so their physical extents match exactly, unlike a\n"
		"             voxel-count-only viewer that would already draw them square anyway since their voxel\n"
		"             counts happen to match here too) -- but the space/time panels are visibly NOT square\n"
		"             (56mm of physical space against 8s of physical time is not a 1:1 ratio), and hovering\n"
		"             any panel now shows a second \"physical (...)\" line under the voxel line, in mm/s rather\n"
		"             than raw indices.");
	embedNDViewer("4D sphere (space x time)", vol, "volume.ndlv", &spacing);

	std::cout << "\n\n=== Building a synthetic 5D (space x time x channel) volume ===\n";

	step("Image<uint8_t,5> vol5d(data, {n,n,n,nt,nc})",
		"A sphere whose center drifts over a time axis (axis 3) and whose radius pulses over a 5th\n"
		"             \"channel\" axis (axis 4) at a different phase -- so axes 3 and 4 aren't just two copies\n"
		"             of the same signal: the (3,4) pairwise view traces a genuinely 2D Lissajous-style curve,\n"
		"             not a straight diagonal, and this is the first example where none of the C(5,2)=10\n"
		"             pairwise views are a disguised lower-DIM case.");

	const int n5 = 20, nt5 = 10, nc5 = 8;
	std::vector<uint8_t> storage5(static_cast<std::size_t>(n5) * n5 * n5 * nt5 * nc5);
	Image<uint8_t, 5> vol5d(storage5.data(), { n5, n5, n5, nt5, nc5 });
	for (const auto& c : vol5d.coordinates())
	{
		double t = static_cast<double>(c[3]) / (nt5 - 1);   // 0..1
		double ch = static_cast<double>(c[4]) / (nc5 - 1);  // 0..1
		double cx = n5 * 0.5 + n5 * 0.2 * std::sin(2.0 * 3.14159265358979323846 * t);
		double cy = n5 * 0.5 + n5 * 0.2 * std::cos(2.0 * 3.14159265358979323846 * ch);
		double cz = n5 * 0.5;
		double radius = n5 * 0.12 + n5 * 0.08 * std::sin(2.0 * 3.14159265358979323846 * (t + 0.5 * ch));
		double dx = c[0] - cx, dy = c[1] - cy, dz = c[2] - cz;
		double d = std::sqrt(dx * dx + dy * dy + dz * dz);
		vol5d.at(c) = static_cast<uint8_t>(d < radius ? 220 : 30);
	}
	showText("extent", "{" + std::to_string(n5) + "," + std::to_string(n5) + "," + std::to_string(n5) + "," + std::to_string(nt5) + "," + std::to_string(nc5) + "}");

	step("embedNDViewer(\"5D sphere\", vol5d, \"volume5d.ndlv\")",
		"Writes the whole 5D volume in ndlviewer.js's binary format. In the generated tutorial page this\n"
		"             becomes a live grid of all 10 pairwise views: 3 purely spatial (0-1, 0-2, 1-2), 6 mixing\n"
		"             one spatial axis with time or channel, and the 1 pure time-vs-channel view (3-4) where\n"
		"             the Lissajous curve is most visible. Same viewer code as the 3D and 4D examples above,\n"
		"             still nothing special-cased about DIM.");
	embedNDViewer("5D sphere (space x time x channel)", vol5d, "volume5d.ndlv");

	std::cout <<
		"\n\nAll output written to: build/demo/nd_viewer/output\n"
		"If you're viewing the generated tutorial page, scroll up to the three embedded viewers: the 3D\n"
		"torus's 3 panels are the classic synchronized axial/coronal/sagittal triple; the 4D sphere's\n"
		"top-left 3 panels (axes 0-1, 0-2, 1-2) are the same kind of synchronized space/space views, while\n"
		"its other 3 (0-3, 1-3, 2-3) plot space against time, so dragging the time axis's own crosshair\n"
		"scrubs through the sphere's drift and pulse directly; the 5D sphere adds a channel axis on top of\n"
		"that, so its (3,4) panel is the one pure time-vs-channel view. Same viewer code, same\n"
		"pairwise_slice() primitive, DIM=3 through DIM=5 alike -- nothing about it is special-cased to any\n"
		"particular dimension count.\n";

	return 0;
}
