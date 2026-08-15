#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/morphology.h>

#include "testHelpers.h"

using namespace ndl;

TEST(ImageMorphology, ImageMorphology) {
	std::stringstream passfail;

	std::cout << std::endl << "IMAGE MORPHOLOGY" << std::endl;

	std::vector<int> gridData(25);
	Image<int, 2> grid(gridData.data(), { 5, 5 });
	{ int i = 0; for (auto it = grid.begin(); it != grid.end(); ++it) *it = ++i; }

	std::vector<double> boxData(9);
	Image<double, 2> box(boxData.data(), { 3, 3 });
	make_box_kernel(box);
	std::vector<double> crossData(9);
	Image<double, 2> cross(crossData.data(), { 3, 3 });
	make_cross_kernel(cross);

	std::vector<int> outData(25);
	Image<int, 2> out(outData.data(), { 5, 5 });

	ndl::erode(grid, out, box);
	passfail << "erode(box) takes the minimum of the 3x3 neighborhood: " << (out(2, 2) == 7 ? "Pass" : "Fail") << std::endl;
	ndl::dilate(grid, out, box);
	passfail << "dilate(box) takes the maximum of the 3x3 neighborhood: " << (out(2, 2) == 19 ? "Pass" : "Fail") << std::endl;

	ndl::median_filter(grid, out, box);
	passfail << "median_filter(box) takes the middle of the sorted 3x3 neighborhood: " << (out(2, 2) == 13 ? "Pass" : "Fail") << std::endl;

	ndl::erode(grid, out, cross);
	passfail << "erode(cross) considers only the 5-tap plus-sign neighborhood: " << (out(2, 2) == 8 ? "Pass" : "Fail") << std::endl;
	ndl::dilate(grid, out, cross);
	passfail << "dilate(cross) considers only the 5-tap plus-sign neighborhood: " << (out(2, 2) == 18 ? "Pass" : "Fail") << std::endl;

	ndl::percentile_filter(grid, out, box, 0.0);
	passfail << "percentile_filter(0) equals erode() (the minimum): " << (out(2, 2) == 7 ? "Pass" : "Fail") << std::endl;
	ndl::percentile_filter(grid, out, box, 100.0);
	passfail << "percentile_filter(100) equals dilate() (the maximum): " << (out(2, 2) == 19 ? "Pass" : "Fail") << std::endl;
	ndl::percentile_filter(grid, out, box, 50.0);
	passfail << "percentile_filter(50) equals median_filter(): " << (out(2, 2) == 13 ? "Pass" : "Fail") << std::endl;

	// Dilating a single point reproduces the structuring element's own shape
	// (the dilation of a point by a shape is just that shape, translated) --
	// box gives a filled square, cross gives only its 4 axis-aligned arms,
	// not a filled diamond.
	std::vector<int> dotData(25, 0);
	Image<int, 2> dot(dotData.data(), { 5, 5 });
	dot(2, 2) = 1;
	std::vector<int> dotOutData(25);
	Image<int, 2> dotOut(dotOutData.data(), { 5, 5 });

	ndl::dilate(dot, dotOut, box);
	int boxOnes = 0;
	for (auto it = dotOut.begin(); it != dotOut.end(); ++it) if (*it == 1) boxOnes++;
	passfail << "dilate(box) of a single point turns on all 9 taps of the box (filled square): " << (boxOnes == 9 ? "Pass" : "Fail") << std::endl;

	ndl::dilate(dot, dotOut, cross);
	int crossOnes = 0;
	for (auto it = dotOut.begin(); it != dotOut.end(); ++it) if (*it == 1) crossOnes++;
	passfail << "dilate(cross) of a single point turns on only the 5 cross taps (plus sign, not a filled diamond): " << (crossOnes == 5 ? "Pass" : "Fail") << std::endl;

	// morphological_open()/morphological_close() are just erode()+dilate()/
	// dilate()+erode() collapsed into one call (morphology.h's own comment
	// on them) -- checked here by comparing directly against the same two
	// manual steps demo/morphology.cpp's own PART 6 spells out by hand.
	Image<int, 2> manualStage(outData.data(), { 5, 5 });
	ndl::erode(grid, manualStage, box);
	std::vector<int> manualOpenData(25);
	Image<int, 2> manualOpen(manualOpenData.data(), { 5, 5 });
	ndl::dilate(manualStage, manualOpen, box);

	std::vector<int> openIntermediateData(25), openData(25);
	Image<int, 2> openIntermediate(openIntermediateData.data(), { 5, 5 });
	Image<int, 2> opened(openData.data(), { 5, 5 });
	ndl::morphological_open(grid, opened, openIntermediate, box);
	passfail << "morphological_open() matches erode() then dilate() done by hand: " << (opened == manualOpen ? "Pass" : "Fail") << std::endl;

	ndl::dilate(grid, manualStage, box);
	std::vector<int> manualCloseData(25);
	Image<int, 2> manualClose(manualCloseData.data(), { 5, 5 });
	ndl::erode(manualStage, manualClose, box);

	std::vector<int> closeIntermediateData(25), closeData(25);
	Image<int, 2> closeIntermediate(closeIntermediateData.data(), { 5, 5 });
	Image<int, 2> closed(closeData.data(), { 5, 5 });
	ndl::morphological_close(grid, closed, closeIntermediate, box);
	passfail << "morphological_close() matches dilate() then erode() done by hand: " << (closed == manualClose ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(ImageMorphology, OtsuThreshold) {
	std::stringstream passfail;

	std::cout << std::endl << "OTSU THRESHOLD" << std::endl;

	// A clean bimodal split: 1000 pixels at 50, 1000 at 200. Any cutoff in
	// (50,200) separates them perfectly, so this checks both that
	// otsu_threshold() lands somewhere in that range AND that threshold()
	// applied with it actually reproduces the two original clusters exactly.
	std::vector<uint8_t> data(2000);
	for (int i = 0; i < 1000; i++) data[i] = 50;
	for (int i = 1000; i < 2000; i++) data[i] = 200;
	Image<uint8_t, 1> img(data.data(), { 2000 });

	uint8_t t = ndl::otsu_threshold(img);
	passfail << "otsu_threshold() on a clean bimodal split lands strictly between the two clusters: " << (t >= 50 && t < 200 ? "Pass" : "Fail") << std::endl;

	std::vector<uint8_t> outData(2000);
	Image<uint8_t, 1> out(outData.data(), { 2000 });
	ndl::threshold(img, out, t); // default on/off = 1/0
	bool separated = true;
	for (int i = 0; i < 1000; i++) if (out(i) != 0) separated = false;
	for (int i = 1000; i < 2000; i++) if (out(i) != 1) separated = false;
	passfail << "threshold() with the Otsu cutoff exactly reproduces the two original clusters (default 0/1): " << (separated ? "Pass" : "Fail") << std::endl;

	std::vector<uint8_t> out255Data(2000);
	Image<uint8_t, 1> out255(out255Data.data(), { 2000 });
	ndl::threshold(img, out255, t, (uint8_t)255, (uint8_t)0);
	bool separated255 = true;
	for (int i = 0; i < 1000; i++) if (out255(i) != 0) separated255 = false;
	for (int i = 1000; i < 2000; i++) if (out255(i) != 255) separated255 = false;
	passfail << "threshold() with explicit on/off values (0/255) still separates correctly: " << (separated255 ? "Pass" : "Fail") << std::endl;

	// A skewed split (800 background, 200 foreground) -- Otsu should still
	// find a cutoff between the two clusters despite the imbalance.
	std::vector<uint8_t> skewedData(1000);
	for (int i = 0; i < 800; i++) skewedData[i] = 30;
	for (int i = 800; i < 1000; i++) skewedData[i] = 220;
	Image<uint8_t, 1> skewed(skewedData.data(), { 1000 });
	uint8_t tSkewed = ndl::otsu_threshold(skewed);
	passfail << "otsu_threshold() on a skewed (800/200) split still lands between the clusters: " << (tSkewed >= 30 && tSkewed < 220 ? "Pass" : "Fail") << std::endl;

	// A uniform (single-value) image is degenerate -- nothing to split --
	// but must not crash, and the only sane answer is that single value.
	std::vector<uint8_t> uniformData(100, 128);
	Image<uint8_t, 1> uniform(uniformData.data(), { 100 });
	uint8_t tUniform = ndl::otsu_threshold(uniform);
	passfail << "otsu_threshold() on a uniform image returns that single value rather than crashing: " << (tUniform == 128 ? "Pass" : "Fail") << std::endl;

	// Generalization: the same bimodal idea, but on float data with a
	// completely different range -- otsu_threshold() derives the histogram
	// range from min()/max() rather than assuming [0,255], so this must work
	// the same way.
	std::vector<float> floatData(2000);
	for (int i = 0; i < 1000; i++) floatData[i] = 0.2f;
	for (int i = 1000; i < 2000; i++) floatData[i] = 0.8f;
	Image<float, 1> floatImg(floatData.data(), { 2000 });
	float floatT = ndl::otsu_threshold(floatImg);
	passfail << "otsu_threshold() generalizes to float data (threshold lands in (0.2,0.8)): " << (floatT > 0.2f && floatT < 0.8f ? "Pass" : "Fail") << std::endl;

	std::vector<float> floatOutData(2000);
	Image<float, 1> floatOut(floatOutData.data(), { 2000 });
	ndl::threshold(floatImg, floatOut, floatT, 1.0f, 0.0f);
	bool floatSeparated = true;
	for (int i = 0; i < 1000; i++) if (floatOut(i) != 0.0f) floatSeparated = false;
	for (int i = 1000; i < 2000; i++) if (floatOut(i) != 1.0f) floatSeparated = false;
	passfail << "threshold() on float data with the Otsu cutoff separates the two clusters exactly: " << (floatSeparated ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

TEST(ImageMorphology, TwoMeansThreshold) {
	std::stringstream passfail;

	std::cout << std::endl << "TWO-MEANS THRESHOLD" << std::endl;

	// Same clean bimodal split OtsuThreshold's first case uses -- both
	// algorithms should agree on a clean, well-separated split.
	std::vector<uint8_t> data(2000);
	for (int i = 0; i < 1000; i++) data[i] = 50;
	for (int i = 1000; i < 2000; i++) data[i] = 200;
	Image<uint8_t, 1> img(data.data(), { 2000 });

	uint8_t t = ndl::two_means_threshold(img);
	passfail << "two_means_threshold() on a clean bimodal split lands strictly between the two clusters: " << (t >= 50 && t < 200 ? "Pass" : "Fail") << std::endl;

	std::vector<uint8_t> outData(2000);
	Image<uint8_t, 1> out(outData.data(), { 2000 });
	ndl::threshold(img, out, t);
	bool separated = true;
	for (int i = 0; i < 1000; i++) if (out(i) != 0) separated = false;
	for (int i = 1000; i < 2000; i++) if (out(i) != 1) separated = false;
	passfail << "threshold() with the two-means cutoff exactly reproduces the two original clusters: " << (separated ? "Pass" : "Fail") << std::endl;

	// A uniform (single-value) image is degenerate the same way it is for
	// Otsu -- must not crash or loop, and the only sane answer is that
	// single value.
	std::vector<uint8_t> uniformData(100, 128);
	Image<uint8_t, 1> uniform(uniformData.data(), { 100 });
	uint8_t tUniform = ndl::two_means_threshold(uniform);
	passfail << "two_means_threshold() on a uniform image returns that single value rather than looping/crashing: " << (tUniform == 128 ? "Pass" : "Fail") << std::endl;

	// A tiny maxIterations still returns a usable (if less-converged)
	// value rather than an error/garbage -- the "always returns something
	// sane" contract documented on the function itself.
	uint8_t tCapped = ndl::two_means_threshold(img, 256, 1);
	passfail << "two_means_threshold() with maxIterations=1 still returns a value between the clusters: " << (tCapped >= 50 && tCapped < 200 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(ImageMorphology, KernelShapes) {
	std::stringstream passfail;

	std::cout << std::endl << "KERNEL SHAPES (make_box_kernel / make_cross_kernel)" << std::endl;

	std::vector<double> boxData(9);
	Image<double, 2> box(boxData.data(), { 3, 3 });
	make_box_kernel(box);
	bool boxOk = true;
	for (auto it = box.begin(); it != box.end(); ++it) if (*it != 1.0) boxOk = false;
	passfail << "make_box_kernel() sets every tap to 1: " << (boxOk ? "Pass" : "Fail") << std::endl;

	std::vector<double> crossData(9);
	Image<double, 2> cross(crossData.data(), { 3, 3 });
	make_cross_kernel(cross);
	double expectedCross[9] = { 0,1,0, 1,1,1, 0,1,0 };
	bool crossOk = true;
	{ int i = 0; for (auto it = cross.begin(); it != cross.end(); ++it, ++i) if (*it != expectedCross[i]) crossOk = false; }
	passfail << "make_cross_kernel() (3x3) produces the expected plus-sign pattern: " << (crossOk ? "Pass" : "Fail") << std::endl;

	int crossNonzero = 0;
	for (auto it = cross.begin(); it != cross.end(); ++it) if (*it != 0) crossNonzero++;
	passfail << "make_cross_kernel() (3x3) has exactly 5 nonzero taps (center + 4 arms): " << (crossNonzero == 5 ? "Pass" : "Fail") << std::endl;

	// 3D: center + one pair of arms per axis = 1 + 3*2 = 7 nonzero taps -- the
	// "jack" generalization the doc comment describes.
	std::vector<double> cross3dData(27);
	Image<double, 3> cross3d(cross3dData.data(), { 3, 3, 3 });
	make_cross_kernel(cross3d);
	int cross3dNonzero = 0;
	for (auto it = cross3d.begin(); it != cross3d.end(); ++it) if (*it != 0) cross3dNonzero++;
	passfail << "make_cross_kernel() generalizes to 3D with 7 nonzero taps (a \"jack\"): " << (cross3dNonzero == 7 ? "Pass" : "Fail") << std::endl;
	reportPassFail(passfail);
}

