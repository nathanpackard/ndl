#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/processing/histogram.h>

#include "testHelpers.h"

using namespace ndl;

TEST(Histogram, ScalarHistogram) {
	std::stringstream passfail;

	std::cout << std::endl << "HISTOGRAM (VDIM==1)" << std::endl;

	// Same clean bimodal split ImageMorphology.OtsuThreshold uses: 1000 at
	// 50, 1000 at 200. Every sample at the low end should land in bucket 0,
	// every sample at the high end in the last bucket (closed-inclusive at
	// hi), and nothing in between.
	std::vector<uint8_t> data(2000);
	for (int i = 0; i < 1000; i++) data[i] = 50;
	for (int i = 1000; i < 2000; i++) data[i] = 200;
	Image<uint8_t, 1> img(data.data(), { 2000 });

	Histogram<1> hist(img, 256);
	passfail << "total() equals the image's own element count: " << (hist.total() == 2000 ? "Pass" : "Fail") << std::endl;
	passfail << "lo()/hi() auto-range to the image's own [min,max]: " << (hist.lo()[0] == 50.0 && hist.hi()[0] == 200.0 ? "Pass" : "Fail") << std::endl;
	passfail << "bucketOf(50) is bucket 0: " << (hist.bucketOf(50.0) == 0 ? "Pass" : "Fail") << std::endl;
	passfail << "bucketOf(200) is the last bucket (closed-inclusive at hi): " << (hist.bucketOf(200.0) == 255 ? "Pass" : "Fail") << std::endl;
	passfail << "count(0) == 1000, count(255) == 1000: " << (hist.count(0) == 1000 && hist.count(255) == 1000 ? "Pass" : "Fail") << std::endl;
	std::size_t middleSum = 0;
	for (int b = 1; b < 255; b++) middleSum += hist.count(b);
	passfail << "every bucket strictly between the two clusters is empty: " << (middleSum == 0 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Histogram, DegenerateAxis) {
	std::stringstream passfail;

	std::cout << std::endl << "HISTOGRAM DEGENERATE AXIS" << std::endl;

	// A uniform image (lo==hi): every sample must land in bucket 0 rather
	// than dividing by a zero range, and every OTHER bucket stays empty.
	std::vector<uint8_t> data(100, 42);
	Image<uint8_t, 1> img(data.data(), { 100 });
	Histogram<1> hist(img, 10);

	passfail << "degenerate axis: total() still counts every sample: " << (hist.total() == 100 ? "Pass" : "Fail") << std::endl;
	passfail << "degenerate axis: bucketOf() always returns 0, for any value: " << (hist.bucketOf(42.0) == 0 && hist.bucketOf(999.0) == 0 && hist.bucketOf(-999.0) == 0 ? "Pass" : "Fail") << std::endl;
	passfail << "degenerate axis: count(0) holds every sample: " << (hist.count(0) == 100 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Histogram, JointHistogram) {
	std::stringstream passfail;

	std::cout << std::endl << "JOINT HISTOGRAM (VDIM==2)" << std::endl;

	// Two co-registered 1D "channels" (16 samples each) whose values are
	// every (x,y) in {0..3}x{0..3} exactly once -- a perfectly uniform
	// joint distribution, so every one of the 16 joint bins should hold
	// exactly 1, and building it from two 1D sources (spatial DIM=1) into a
	// VDIM=2 histogram exercises that VDIM (the number of joint value axes)
	// is independent of the sources' own spatial dimensionality.
	std::vector<uint8_t> chanAdata(16), chanBdata(16);
	for (int i = 0; i < 16; i++) { chanAdata[i] = i % 4; chanBdata[i] = i / 4; }
	Image<uint8_t, 1> chanA(chanAdata.data(), { 16 });
	Image<uint8_t, 1> chanB(chanBdata.data(), { 16 });

	Histogram<2> joint(std::array<int, 2>{4, 4}, std::array<const Image<uint8_t, 1>*, 2>{&chanA, &chanB});
	passfail << "joint histogram total() equals the shared sample count: " << (joint.total() == 16 ? "Pass" : "Fail") << std::endl;

	bool everyBinIsOne = true;
	for (int x = 0; x < 4; x++)
		for (int y = 0; y < 4; y++)
			if (joint.count({ x, y }) != 1) everyBinIsOne = false;
	passfail << "every (x,y) combination appears in its own bin exactly once: " << (everyBinIsOne ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Histogram, HistogramEqualize) {
	std::stringstream passfail;

	std::cout << std::endl << "HISTOGRAM EQUALIZE" << std::endl;

	std::vector<uint8_t> data(2000);
	for (int i = 0; i < 1000; i++) data[i] = 50;
	for (int i = 1000; i < 2000; i++) data[i] = 200;
	Image<uint8_t, 1> img(data.data(), { 2000 });

	std::vector<uint8_t> eqData(2000);
	Image<uint8_t, 1> eq(eqData.data(), { 2000 });
	histogram_equalize(img, eq);

	// cdf(bucket 0) = 1000/2000 = 0.5, so the low cluster should map to the
	// range's own midpoint (50 + 150*0.5 = 125); the high cluster's cdf
	// reaches 1.0, so it should map to the range's own top (200).
	passfail << "equalize() maps the low cluster to the range's midpoint: " << ((int)eq(0) == 125 ? "Pass" : "Fail") << std::endl;
	passfail << "equalize() maps the high cluster to the range's own top: " << ((int)eq(1999) == 200 ? "Pass" : "Fail") << std::endl;

	// Degenerate source: nothing to equalize, so it's just copied through.
	std::vector<uint8_t> uniData(50, 7);
	Image<uint8_t, 1> uni(uniData.data(), { 50 });
	std::vector<uint8_t> uniEqData(50);
	Image<uint8_t, 1> uniEq(uniEqData.data(), { 50 });
	histogram_equalize(uni, uniEq);
	bool unchanged = true;
	for (int i = 0; i < 50; i++) if (uniEq(i) != 7) unchanged = false;
	passfail << "equalize() on a degenerate (uniform) source copies it through unchanged: " << (unchanged ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Histogram, HistogramImage) {
	std::stringstream passfail;

	std::cout << std::endl << "HISTOGRAM_IMAGE (RENDERED, NOT ASCII)" << std::endl;

	// Same bimodal split used throughout this file: counts should be all
	// in bucket 0 and bucket 255 (see Histogram.ScalarHistogram above), so
	// the rendered bar chart's leftmost and rightmost columns should be
	// full-height bars, and the middle should be entirely background.
	std::vector<uint8_t> data(2000);
	for (int i = 0; i < 1000; i++) data[i] = 50;
	for (int i = 1000; i < 2000; i++) data[i] = 200;
	Image<uint8_t, 1> img(data.data(), { 2000 });
	Histogram<1> hist(img, 256);

	std::vector<uint8_t> dstData(3 * 256 * 20);
	Image<uint8_t, 3> dst(dstData.data(), { 3, 256, 20 });
	histogram_image(hist, dst, (uint8_t)255, (uint8_t)0);

	bool leftBarFull = true, rightBarFull = true, middleEmpty = true;
	for (int y = 0; y < 20; y++)
	{
		if (dst(0, 0, y) != 255) leftBarFull = false;
		if (dst(0, 255, y) != 255) rightBarFull = false;
	}
	for (int x = 100; x < 156; x++)
		for (int y = 0; y < 20; y++)
			if (dst(0, x, y) != 0) middleEmpty = false;

	passfail << "histogram_image() on Histogram<1> is a real bar chart -- the low cluster's column is a full-height bar: " << (leftBarFull ? "Pass" : "Fail") << std::endl;
	passfail << "the high cluster's column is a full-height bar too (both clusters have equal counts, so both reach the max): " << (rightBarFull ? "Pass" : "Fail") << std::endl;
	passfail << "the empty buckets between the two clusters draw no bar at all: " << (middleEmpty ? "Pass" : "Fail") << std::endl;

	// Histogram<2>: a perfectly uniform joint distribution should render as
	// a perfectly uniform (every pixel == peakValue) heatmap.
	std::vector<uint8_t> chanAdata(16), chanBdata(16);
	for (int i = 0; i < 16; i++) { chanAdata[i] = i % 4; chanBdata[i] = i / 4; }
	Image<uint8_t, 1> chanA(chanAdata.data(), { 16 });
	Image<uint8_t, 1> chanB(chanBdata.data(), { 16 });
	Histogram<2> joint(std::array<int, 2>{4, 4}, std::array<const Image<uint8_t, 1>*, 2>{&chanA, &chanB});
	std::vector<uint8_t> jointDstData(3 * 4 * 4);
	Image<uint8_t, 3> jointDst(jointDstData.data(), { 3, 4, 4 });
	histogram_image(joint, jointDst, (uint8_t)255);
	bool allPeak = true;
	for (auto v : jointDstData) if (v != 255) allPeak = false;
	passfail << "histogram_image() on Histogram<2> is a real heatmap -- a uniform joint distribution renders as a uniform image: " << (allPeak ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}
