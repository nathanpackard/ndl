#pragma once
#include <ndl/image.h>
#include <ndl/imageIO.h>
#include <ndl/viewer/viewer.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>

// Shared infrastructure for every demo/*/*.cpp tutorial: each demo prints a
// step-by-step walkthrough (step()'s code/explanation, then the actual
// input/output data or saved PNG), which docs/generate_tutorial.py turns
// into a real Doxygen page from the captured output -- see that script's
// own comment for the exact structured convention (step()/showArray()/
// showText()/saveForInspection()) it depends on staying consistent across
// every demo. This header exists so that convention only has one real
// definition, not one accidentally-drifting copy per demo -- the same
// reasoning unitTests/testHelpers.h already follows for the test suite.
//
// Every function here is `inline` -- this header is included by several
// independent demo executables' translation units, and while each one
// today happens to include it exactly once, `inline` is what actually
// makes that safe (no multiple-definition risk if that ever changes)
// rather than relying on it staying true by convention. Deliberately not
// `using namespace ndl;` at this header's scope, unlike each demo's own
// explicit using-directive -- a shared header pulling a namespace into
// every includer's global scope is a surprise callers didn't ask for, so
// Image/image_io are qualified as ndl::Image/ndl::image_io here instead.

inline int stepNumber = 0;

inline void step(const std::string& code, const std::string& explanation)
{
	std::cout << "\n[" << ++stepNumber << "] code:    " << code << "\n";
	std::cout << "    explain: " << explanation << "\n";
}

// Same as step() above, but for a call worth showing as more than one line
// -- e.g. a demo helper function whose whole point is to stand in for a few
// lines of real library calls, which is exactly the part worth seeing
// spelled out rather than hidden behind the helper's name. codeLines[0]
// prints on the "code:" line itself; codeLines[1..] print indented to line
// up under it, using the same 13-space continuation convention as the
// "explain:" text below (and recognized by docs/generate_tutorial.py as
// still being part of the same code block, not a new one).
inline void step(const std::vector<std::string>& codeLines, const std::string& explanation)
{
	std::cout << "\n[" << ++stepNumber << "] code:    " << codeLines[0] << "\n";
	for (std::size_t i = 1; i < codeLines.size(); i++)
		std::cout << "             " << codeLines[i] << "\n";
	std::cout << "    explain: " << explanation << "\n";
}

template<class ImageT>
void showArray(const std::string& label, const ImageT& img) { std::cout << "    " << label << ":\n" << img; }
inline void showText(const std::string& label, const std::string& text) { std::cout << "    " << label << ":   " << text << "\n"; }

inline std::string outputDir;

// Saves img as a PNG under the demo's output folder and prints where it
// went plus min/max/mean, so the numbers and the picture can be checked
// against each other rather than eyeballing the picture alone. The
// human-readable label line intentionally prints `filename` alone, not the
// full `path` -- `path` is built from `outputDir`, which is an ABSOLUTE
// path baked in at compile time from CMAKE_CURRENT_BINARY_DIR (see each
// demo's own CMakeLists.txt), i.e. whoever's local machine happened to
// build it. This line's own text ends up verbatim in the committed
// docs/tutorials/*.md (unlike the SAVING_RE-matched "saving output file:
// <path>" trace right below, which generate_tutorial.py rewrites into a
// relative image link and never echoes as-is) -- printing the bare
// filename here is what keeps a contributor's own home-directory path out
// of the public docs/git history.
template<class T, int DIM>
void saveForInspection(const std::string& label, const ndl::Image<T, DIM>& img, const std::string& filename)
{
	std::string path = outputDir + "/" + filename;
	ndl::image_io::save(img, path);
	std::cout << "    " << label << ": " << filename << "\n";
	std::cout << "        extent = {";
	for (int i = 0; i < DIM; i++) std::cout << (i ? ", " : "") << img.extent()[i];
	std::cout << "}   min=" << (int)img.min() << "  max=" << (int)img.max() << "  mean=" << img.mean() << "\n";
}

// Writes img in ndlviewer.js's binary format (ndl::write_web_volume(), see
// viewer.h) under the demo's output folder and prints where it went, in
// the same "saving output file: <path>" shape ndl::image_io::save() itself
// prints (imageIO.h) -- docs/generate_tutorial.py's EMBED_RE recognizes
// this exact marker and turns it into an embedded <canvas>+<script> block
// in the generated tutorial page, the same way SAVING_RE turns an
// image_io::save() trace into an embedded `![]()`. spacing is optional
// per-axis physical calibration (ndl::VoxelSpacing<DIM>, see viewer.h's own
// comment) -- passed straight through to write_web_volume(); omit it (or
// pass nullptr) for a volume with no physical units, exactly as before.
// panelSize is an optional override for NDLViewer.create()'s own
// options.panelSize (its largest-axis pixel size, see ndlviewer.js's own
// comment) -- 0 (the default) means "don't override, let the viewer use
// its own default." colorAxis is an optional override for
// options.colorAxis (which axis holds RGB channels -- see ndlviewer.js's
// own comment on it) -- -1 (the default) means "no color axis, every panel
// grayscale," exactly as before this parameter existed. Both are appended
// to the marker line (` panelSize=N`/` colorAxis=N`) only when actually
// set (nonzero/non-negative respectively), in that fixed order, so every
// existing "embedding viewer: <path>" marker (and EMBED_RE's own match
// against it) stays byte-for-byte unchanged for every caller that doesn't
// need either.
template<class T, int DIM>
void embedNDViewer(const std::string& label, const ndl::Image<T, DIM>& img, const std::string& filename, const ndl::VoxelSpacing<DIM>* spacing = nullptr, int panelSize = 0, int colorAxis = -1)
{
	std::string path = outputDir + "/" + filename;
	std::ofstream out(path, std::ios::binary);
	ndl::write_web_volume(img, out, spacing);
	out.close();
	// filename, not path, in the human-readable line -- same reasoning as
	// saveForInspection()'s own comment above. The "embedding viewer:
	// <path>" marker line right below is left as the real `path`
	// deliberately: EMBED_RE needs a path it can actually open to read the
	// file's bytes, and (unlike this line above it) that marker is
	// rewritten into the embedded viewer block, never echoed into the
	// final doc as-is.
	std::cout << "    " << label << ": " << filename << "\n";
	std::cout << "embedding viewer: " << path
		<< (panelSize > 0 ? " panelSize=" + std::to_string(panelSize) : "")
		<< (colorAxis >= 0 ? " colorAxis=" + std::to_string(colorAxis) : "")
		<< std::endl;
}

// How many positions two same-shaped minimal-interface images disagree at,
// and the largest single disagreement -- the "how close are these two
// independently-computed results" check several demos make (a
// denoised-vs-ground-truth threshold, two box blurs computed two different
// ways, ...). `long` throughout so the subtraction can't itself
// overflow/wrap for a narrow unsigned value_type like uint8_t.
struct MismatchResult { long count = 0; long maxDiff = 0; };

template<class ImageT1, class ImageT2>
MismatchResult countMismatches(const ImageT1& a, const ImageT2& b)
{
	MismatchResult result;
	for (const auto& coord : a.coordinates())
	{
		long diff = std::abs((long)a.at(coord) - (long)b.at(coord));
		if (diff != 0) { result.count++; result.maxDiff = std::max(result.maxDiff, diff); }
	}
	return result;
}
