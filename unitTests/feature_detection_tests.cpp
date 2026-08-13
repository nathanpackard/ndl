#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <cmath>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/feature_detection.h>

#include "testHelpers.h"

using namespace ndl;

TEST(FeatureDetection, DetectsKnownBlob) {
	std::stringstream passfail;
	std::cout << std::endl << "DOG BLOB DETECTION" << std::endl;

	// A single bright Gaussian blob on an otherwise flat background --
	// the classic DoG sanity check: a real blob detector should find
	// exactly one strong extremum, right at the blob's own center.
	const int W = 100, H = 100;
	std::vector<double> data(W * H, 0.0);
	Image<double, 2> img(data.data(), { W, H });
	double bx = 40, by = 60, bsigma = 3.0;
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
		{
			double dx = x - bx, dy = y - by;
			img(x, y) = 200.0 * std::exp(-(dx * dx + dy * dy) / (2 * bsigma * bsigma));
		}

	auto keypoints = detect_keypoints(img, 5, 1.6, 5.0);
	passfail << "at least one keypoint detected: " << (!keypoints.empty() ? "Pass" : "Fail") << std::endl;

	bool foundNearBlob = false;
	for (const auto& kp : keypoints)
	{
		double d = std::sqrt(std::pow(kp.position[0] - bx, 2) + std::pow(kp.position[1] - by, 2));
		if (d < 5.0) foundNearBlob = true;
	}
	passfail << "a keypoint was found within 5px of the blob's true center: " << (foundNearBlob ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(FeatureDetection, EmptyFlatImageHasNoKeypoints) {
	std::stringstream passfail;
	std::cout << std::endl << "DOG ON A FLAT IMAGE" << std::endl;

	// A perfectly flat image has zero DoG response everywhere -- nothing
	// should pass the contrast threshold, confirming the detector doesn't
	// hallucinate keypoints from numerical noise on featureless input.
	const int W = 40, H = 40;
	std::vector<double> data(W * H, 128.0);
	Image<double, 2> img(data.data(), { W, H });

	auto keypoints = detect_keypoints(img, 5, 1.6, 5.0);
	passfail << "a flat image produces no keypoints: " << (keypoints.empty() ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(FeatureDetection, DescriptorOrientationIsRotationInvariant) {
	std::stringstream passfail;
	std::cout << std::endl << "DESCRIPTOR ORIENTATION INVARIANCE" << std::endl;

	// An elongated (anisotropic) Gaussian blob -- real directional
	// structure, analytically rotated by `theta` about its own center
	// (coordinates pre-rotated before evaluating the Gaussian, so there's
	// no pixel-resampling/interpolation artifact to confound the test).
	auto ellipticalBlob = [](double x, double y, double cx, double cy, double theta, double sigmaU, double sigmaV) {
		double dx = x - cx, dy = y - cy;
		double c = std::cos(-theta), s = std::sin(-theta);
		double u = c * dx - s * dy;
		double v = s * dx + c * dy;
		return 200.0 * std::exp(-(u * u / (2 * sigmaU * sigmaU) + v * v / (2 * sigmaV * sigmaV)));
	};

	const int W = 100, H = 100;
	double cx = 50, cy = 50, sigmaU = 6.0, sigmaV = 2.5; // elongated along U

	std::vector<double> dataA(W * H), dataB(W * H);
	Image<double, 2> imgA(dataA.data(), { W, H });
	Image<double, 2> imgB(dataB.data(), { W, H });

	double thetaB = 0.7; // ~40 degrees
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
		{
			imgA(x, y) = ellipticalBlob(x, y, cx, cy, 0.0, sigmaU, sigmaV);
			imgB(x, y) = ellipticalBlob(x, y, cx, cy, thetaB, sigmaU, sigmaV);
		}

	auto kpA = detect_keypoints(imgA, 5, 1.6, 5.0);
	auto kpB = detect_keypoints(imgB, 5, 1.6, 5.0);
	passfail << "keypoints found in both the original and rotated blob: " << (!kpA.empty() && !kpB.empty() ? "Pass" : "Fail") << std::endl;

	// Find the keypoint nearest the blob's own center in each image.
	auto nearest = [](const std::vector<Keypoint<2>>& kps, double px, double py) {
		std::size_t best = 0; double bestD = 1e18;
		for (std::size_t i = 0; i < kps.size(); i++)
		{
			double d = std::pow(kps[i].position[0] - px, 2) + std::pow(kps[i].position[1] - py, 2);
			if (d < bestD) { bestD = d; best = i; }
		}
		return kps[best];
	};
	std::vector<Keypoint<2>> kpsA1 = { nearest(kpA, cx, cy) }, kpsB1 = { nearest(kpB, cx, cy) };
	auto descA = compute_descriptors(imgA, kpsA1);
	auto descB = compute_descriptors(imgB, kpsB1);

	double distSq = 0;
	for (std::size_t i = 0; i < descA[0].size(); i++) { double diff = descA[0][i] - descB[0][i]; distSq += diff * diff; }
	double distRotated = std::sqrt(distSq);

	// Contrast: a keypoint with a genuinely DIFFERENT shape (a much
	// rounder blob -- a different structure-tensor eigenvalue ratio, not
	// just the same shape at a different rotation) should be much less
	// alike, confirming the small rotated-self distance above reflects
	// real orientation invariance rather than descriptors being
	// uniformly close together regardless of content.
	std::vector<double> dataC(W * H);
	Image<double, 2> imgC(dataC.data(), { W, H });
	double cx2 = 30, cy2 = 65;
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
			imgC(x, y) = ellipticalBlob(x, y, cx2, cy2, 0.0, 4.5, 4.5);
	auto kpC = detect_keypoints(imgC, 5, 1.6, 5.0);
	passfail << "keypoints found in the contrast (differently-shaped) blob: " << (!kpC.empty() ? "Pass" : "Fail") << std::endl;
	std::vector<Keypoint<2>> kpsC1 = { nearest(kpC, cx2, cy2) };
	auto descC = compute_descriptors(imgC, kpsC1);

	double distSqDiff = 0;
	for (std::size_t i = 0; i < descA[0].size(); i++) { double diff = descA[0][i] - descC[0][i]; distSqDiff += diff * diff; }
	double distDifferentShape = std::sqrt(distSqDiff);

	passfail << "rotated-self descriptor distance (" << distRotated << ") is smaller than different-shape descriptor distance (" << distDifferentShape << "): " << (distRotated < distDifferentShape ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(FeatureDetection, SiftFlowRecoversKnownShift) {
	std::stringstream passfail;
	std::cout << std::endl << "SIFT_FLOW KNOWN-SHIFT RECOVERY" << std::endl;

	// Several distinct Gaussian blobs (good keypoint-rich content, unlike
	// a single blob) shifted by a known translation between frame0/frame1.
	const int W = 120, H = 120;
	std::vector<std::array<double, 3>> blobs = { // {x, y, sigma}
		{30, 30, 3}, {80, 25, 4}, {50, 80, 3.5}, {90, 90, 2.5}, {20, 90, 3}
	};
	auto render = [&](std::vector<double>& data, double shiftX, double shiftY) {
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++)
			{
				double v = 20.0;
				for (const auto& b : blobs)
				{
					double dx = x - shiftX - b[0], dy = y - shiftY - b[1];
					v += 180.0 * std::exp(-(dx * dx + dy * dy) / (2 * b[2] * b[2]));
				}
				data[y * W + x] = v;
			}
	};

	std::vector<double> data0(W * H), data1(W * H);
	render(data0, 0, 0);
	double shiftX = 3.0, shiftY = 2.0;
	render(data1, shiftX, shiftY);
	Image<double, 2> frame0(data0.data(), { W, H });
	Image<double, 2> frame1(data1.data(), { W, H });

	std::vector<double> flowData(2 * W * H);
	Image<double, 3> flow(flowData.data(), { 2, W, H });
	std::size_t numMatches = sift_flow(frame0, frame1, flow);
	passfail << "at least one match was found: " << (numMatches > 0 ? "Pass" : "Fail") << std::endl;

	Image<double, 2> fx = flow.slice(0, 0), fy = flow.slice(0, 1);
	int goodCount = 0;
	for (const auto& b : blobs)
	{
		int x = (int)b[0], y = (int)b[1];
		if (std::abs(fx(x, y) - shiftX) < 1.0 && std::abs(fy(x, y) - shiftY) < 1.0) goodCount++;
	}
	passfail << "recovered flow matches the known shift at every blob location within 1px: " << (goodCount == (int)blobs.size() ? "Pass" : "Fail") << " (" << goodCount << "/" << blobs.size() << ")" << std::endl;

	reportPassFail(passfail);
}
