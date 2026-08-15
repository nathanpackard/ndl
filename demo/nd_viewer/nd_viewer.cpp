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

// A tour of viewer.h's N-D pairwise-slice viewer, on both a plain 3D
// volume (the classic axial/coronal/sagittal case) and a genuinely
// 4-dimensional one (3 spatial axes plus a time axis, so none of its
// C(4,2)=6 pairwise views are a disguised 2D/3D case) -- showing the same
// viewer, unmodified, generalizes across DIM rather than the 3D case being
// some separately-handled special path. step()/showText()/embedNDViewer()/
// outputDir come from demoHelpers.h, shared with every other demo --
// embedNDViewer() specifically is new (see its own comment in
// demoHelpers.h): it writes viewer.h's binary format and prints the marker
// docs/generate_tutorial.py turns into a live, embedded copy of the viewer,
// right in the generated tutorial page.
//
// The 3D volume is a torus -- deliberately not another sphere, so its 3
// panels are visually distinguishable at a glance from the 4D volume's own
// 3 purely-spatial panels below. The 4D volume is a sphere whose center
// drifts and whose radius pulses over the time axis -- so its 3
// purely-spatial views (axes (0,1),(0,2),(1,2), each at a fixed t) look
// like an ordinary static sphere cross-section, while the 3 views involving
// the time axis ((0,3),(1,3),(2,3)) show that motion directly: each is a
// space-vs-time plot where the sphere's radius change and center drift
// trace out a visible curve, not just a static shape repeated across
// columns.
int main()
{
	outputDir = NDL_ND_VIEWER_OUTPUT_DIR;
	fs::create_directories(outputDir);

	std::cout <<
		"This demo tours viewer.h's N-D pairwise-slice primitives on both a plain 3D\n"
		"volume and a genuinely 4D one (3 space + 1 time), then embeds the actual\n"
		"interactive WebGL viewer for each right in this page (if you're reading the\n"
		"generated tutorial -- see docs/generate_tutorial.py) so you can click around\n"
		"them yourself. Output lands in:\n    " << outputDir << "\n";

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

	step("embedNDViewer(\"4D sphere\", vol, \"volume.ndlv\")",
		"Writes the whole 4D volume in ndlviewer.js's binary format. In the generated tutorial page\n"
		"             this becomes a live grid of all 6 pairwise views (space/space and space/time alike) --\n"
		"             click or drag in any panel to move the shared cursor and watch every other panel\n"
		"             (including the ones showing the time axis) jump to match.");
	embedNDViewer("4D sphere (space x time)", vol, "volume.ndlv");

	std::cout <<
		"\n\nAll output written to: " << outputDir << "\n"
		"If you're viewing the generated tutorial page, scroll up to the two embedded viewers: the 3D\n"
		"torus's 3 panels are the classic synchronized axial/coronal/sagittal triple; the 4D sphere's\n"
		"top-left 3 panels (axes 0-1, 0-2, 1-2) are the same kind of synchronized space/space views, while\n"
		"its other 3 (0-3, 1-3, 2-3) plot space against time, so dragging the time axis's own crosshair\n"
		"scrubs through the sphere's drift and pulse directly. Same viewer code, same pairwise_slice()\n"
		"primitive, both DIM=3 and DIM=4 -- nothing about it is special-cased to 3 dimensions.\n";

	return 0;
}
