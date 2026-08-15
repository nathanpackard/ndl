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

// A tour of viewer.h's N-D pairwise-slice viewer, on a genuinely
// 4-dimensional volume: 3 spatial axes plus a time axis, so none of the
// C(4,2)=6 pairwise views are a disguised 2D/3D case. step()/showText()/
// embedNDViewer()/outputDir come from demoHelpers.h, shared with every
// other demo -- embedNDViewer() specifically is new (see its own comment
// in demoHelpers.h): it writes viewer.h's binary format and prints the
// marker docs/generate_tutorial.py turns into a live, embedded copy of
// this exact viewer, right in the generated tutorial page.
//
// The synthetic volume: a sphere whose center drifts and whose radius
// pulses over the time axis -- so the 3 purely-spatial views (axes
// (0,1),(0,2),(1,2), each at a fixed t) look like an ordinary static
// sphere cross-section, while the 3 views involving the time axis
// ((0,3),(1,3),(2,3)) show that motion directly: each is a space-vs-time
// plot where the sphere's radius change and center drift trace out a
// visible curve, not just a static shape repeated across columns.
int main()
{
	outputDir = NDL_ND_VIEWER_OUTPUT_DIR;
	fs::create_directories(outputDir);

	std::cout <<
		"This demo tours viewer.h's N-D pairwise-slice primitives on a genuinely 4D\n"
		"volume (3 space + 1 time), then embeds the actual interactive WebGL viewer\n"
		"right in this page (if you're reading the generated tutorial -- see\n"
		"docs/generate_tutorial.py) so you can click around it yourself. Output lands\n"
		"in:\n    " << outputDir << "\n";

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
		"If you're viewing the generated tutorial page, scroll up to the embedded viewer: the top-left\n"
		"3 panels (axes 0-1, 0-2, 1-2) are the familiar synchronized space/space views; the other 3 (0-3,\n"
		"1-3, 2-3) plot space against time, so dragging the time axis's own crosshair scrubs through the\n"
		"sphere's drift and pulse directly.\n";

	return 0;
}
