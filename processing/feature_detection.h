#pragma once
#include <cassert>
#include <cmath>
#include <vector>
#include <array>
#include <limits>
#include <cstddef>
#include <type_traits>
#include "../image/border_mode.h"
#include "../image/detail.h"
#include "../image.h"
#include "convolution.h"
#include "matrix/core.h"
#include "matrix/decomposition.h"

// The feature detection/matching toolkit: detect_keypoints(),
// compute_descriptors(), match_descriptors(), and sift_flow() -- free
// functions over any minimal-interface image type. A sibling of
// fft.h/matrix.h/convolution.h/morphology.h/histogram.h/
// distance_transform.h/summed_area_table.h/visualize.h/optical_flow.h, not
// part of image.h's core Image object -- #include this directly if you use
// it.
//
// This is a SIFT-*inspired* pipeline, not literal SIFT, and that's a
// deliberate choice, not an oversight -- see compute_descriptors()' own
// comment for why. The scale-space extrema detection below IS a genuine,
// unambiguous N-dimensional generalization of the real published algorithm
// (it's exactly gaussian_blur() at increasing sigma, differenced, and
// searched for extrema over the full spatial+scale neighborhood -- nothing
// here is 2D-specific). The descriptor, in the next file section, is where
// this pipeline parts ways with literal SIFT.

namespace ndl
{
	// One detected scale-space extremum: where (pixel position, in the
	// source image's own DIM-dimensional coordinates), at what scale (the
	// sigma of the Gaussian-blurred level nearest the DoG layer it was found
	// in -- not sub-scale-refined, unlike published SIFT's own quadratic
	// interpolation step), and how strong (the DoG value's own magnitude at
	// that position -- higher means more contrast, and is what
	// contrastThreshold below filters on).
	/// A single scale-space extremum: position, detected scale, and response strength.
	/// @tparam DIM Spatial dimensionality (matches the source image's own DIM).
	/// @ingroup features
	template<int DIM>
	struct Keypoint
	{
		std::array<int, DIM> position;
		double scale;
		double response;
	};

	// Difference-of-Gaussians scale-space extrema detection, generalized to
	// any DIM: builds `numScales` increasingly-blurred copies of src spanning
	// one octave (sigma0 .. sigma0*2), differences adjacent levels into
	// `numScales-1` DoG images, then searches every INTERIOR DoG layer (one
	// that has a layer both above and below it, so a full neighborhood
	// exists) for positions that are a strict local max or min against every
	// one of their neighbors -- the full (3^DIM - 1) spatial neighbors at
	// the SAME layer, plus the 3^DIM spatial neighbors (including
	// straight-below/straight-above) at each ADJACENT layer, so
	// 3^(DIM+1) - 1 neighbors in total. This is precisely how published 3D
	// SIFT extensions (e.g. Cheung & Hamarneh, 2007) still do this part --
	// it's only the descriptor (see compute_descriptors() below) that
	// genuinely has no natural N-D form.
	//
	// The 3^DIM spatial neighbor OFFSETS themselves are enumerated by
	// reusing detail::kernelCenter()/kernelIncludedTaps()/kernelTapCoord()
	// (image/detail.h) -- the exact same machinery convolve()/erode()/
	// box_blur() already build their own kernel walks from, applied here to
	// a plain all-ones 3^DIM box "kernel" purely as a neighbor-offset list,
	// with BorderMode::Clamp resolving the lookup at the image's own spatial
	// border the same way every other kernel-walking function in this
	// library already does.
	//
	// Deliberately simplified relative to published SIFT in a few more
	// ways, alongside the single-octave restriction above: no sub-pixel/
	// sub-scale quadratic refinement of the extremum position (positions
	// are exact pixel/layer indices), and no low-contrast or edge-response
	// (Hessian-ratio) rejection beyond the one contrastThreshold cutoff on
	// |DoG value|. Good enough for a demo-scale keypoint set; a production
	// feature detector would add both.
	/// Difference-of-Gaussians scale-space extrema detection, generalized to any DIM.
	/// @tparam ImageT Any minimal-interface image type; its value_type must be arithmetic.
	/// @param  src               Source image.
	/// @param  numScales         Number of Gaussian-blurred levels spanning one octave (sigma0 to sigma0*2). Must be >= 4 (so at least one interior DoG layer exists to search). Defaults to 5.
	/// @param  sigma0             Smallest blur level's standard deviation. Defaults to 1.6 (the same starting point published SIFT uses).
	/// @param  contrastThreshold Minimum |DoG value| for an extremum to be kept -- rejects low-contrast, noise-dominated detections. Defaults to 5.0 (tuned for an 8-bit-range image; scale it for other ranges).
	/// @return Every detected extremum, in coordinate-scan order.
	/// @ingroup features
	template<class ImageT>
	auto detect_keypoints(const ImageT& src, int numScales = 5, double sigma0 = 1.6, double contrastThreshold = 5.0)
	{
		using T = typename ImageT::value_type;
		static_assert(std::is_arithmetic_v<T>, "ndl::detect_keypoints() requires a value_type convertible to double -- not valid for e.g. std::complex<T>");
		assert(numScales >= 4);
		assert(sigma0 > 0);

		auto extent = src.extent();
		constexpr int DIM = std::tuple_size<decltype(extent)>::value;
		using KeypointT = Keypoint<DIM>;
		std::vector<KeypointT> keypoints;

		OwnedImage<double, DIM> srcDbl(src);

		std::vector<OwnedImage<double, DIM>> blurred;
		blurred.reserve(numScales);
		std::vector<double> sigmas(numScales);
		for (int i = 0; i < numScales; i++)
		{
			sigmas[i] = sigma0 * std::pow(2.0, (double)i / (numScales - 3));
			OwnedImage<double, DIM> level(extent);
			gaussian_blur(srcDbl, level, sigmas[i], BorderMode::Reflect);
			blurred.push_back(std::move(level));
		}

		std::vector<OwnedImage<double, DIM>> dog;
		dog.reserve(numScales - 1);
		for (int i = 0; i < numScales - 1; i++)
		{
			OwnedImage<double, DIM> d(extent);
			auto a = blurred[i].begin();
			auto b = blurred[i + 1].begin();
			for (auto it = d.begin(); it != d.end(); ++it, ++a, ++b) *it = (*b) - (*a);
			dog.push_back(std::move(d));
		}

		// A plain all-ones 3^DIM box, used purely as a neighbor-offset
		// enumerator (see this function's own comment above) -- not a real
		// convolution kernel, nothing here is weighted by its values.
		std::array<int, DIM> neighExtent;
		neighExtent.fill(3);
		std::vector<double> neighKernelData(Image<double, DIM>::size(neighExtent), 1.0);
		Image<double, DIM> neighKernel(neighKernelData.data(), neighExtent);
		auto center = detail::kernelCenter(neighKernel);
		auto taps = detail::kernelIncludedTaps(neighKernel);

		for (int layer = 1; layer < numScales - 2; layer++)
		{
			for (const auto& coord : src.coordinates())
			{
				double val = dog[layer].at(coord);
				if (std::abs(val) < contrastThreshold) continue;

				bool isMax = true, isMin = true;
				for (int dLayer = -1; dLayer <= 1 && (isMax || isMin); dLayer++)
				{
					const auto& dogLayer = dog[layer + dLayer];
					for (const auto& tap : taps)
					{
						if (dLayer == 0 && tap == center) continue;
						double n = dogLayer.at(detail::kernelTapCoord(coord, tap, center, extent, BorderMode::Clamp));
						if (n >= val) isMax = false;
						if (n <= val) isMin = false;
						if (!isMax && !isMin) break;
					}
				}

				if (isMax || isMin)
					keypoints.push_back(KeypointT{ coord, sigmas[layer], std::abs(val) });
			}
		}

		return keypoints;
	}

	// A keypoint's descriptor: a flat vector of length numCellsPerAxis^DIM *
	// 2*DIM, unit-L2-normalized (the classic SIFT illumination-invariance
	// step -- brightness/contrast changes scale every gradient by roughly
	// the same factor, which normalizing away cancels out).
	/// A keypoint's local descriptor -- a flat, unit-normalized numCellsPerAxis^DIM*2*DIM-length vector. See compute_descriptors()' own comment for what it actually encodes and why.
	/// @ingroup features
	using Descriptor = std::vector<double>;

	// The part of this pipeline that is NOT literal SIFT (see this file's
	// own top comment) -- published SIFT's descriptor bins each grid cell's
	// gradients by ORIENTATION (an 8-way histogram of the 2D angle
	// atan2(gy,gx)), which has no natural analogue once there's more than
	// one angle needed to describe a direction (a 3D gradient direction
	// needs two angles, a 4D one needs three, and so on -- there's no single
	// canonical "N-D orientation histogram"). Rather than pick one arbitrary
	// generalization, this descriptor sidesteps the whole question: each
	// grid cell gets 2*DIM bins instead, one pair (positive, negative) per
	// spatial axis, accumulating |gradient component| directly rather than
	// a binned angle -- a well-defined "how much does the gradient here
	// point along +x, -x, +y, -y, ..." summary that means the same thing at
	// any DIM, Gaussian-weighted by distance from the keypoint (so pixels
	// near the patch edge contribute less, softening blockiness at the
	// patch boundary the same way published SIFT's own Gaussian window
	// does) and normalized to unit length at the end.
	//
	// The patch radius scales with the keypoint's own detected sigma
	// (patchRadiusInSigmas * kp.scale), which is what makes this
	// scale-invariant the same way SIFT is: a keypoint detected at a larger
	// scale gets a proportionally larger sampling patch, so the same
	// physical structure produces a comparable descriptor whether it was
	// detected small-and-close or large-and-far.
	//
	// ROTATION invariance -- unlike the descriptor grid above, THIS part
	// genuinely does generalize to any DIM, just not via an angle: a
	// rotation in N-D decomposes into simple rotations of 2-D planes (the
	// spectral theorem for orthogonal matrices), so there's no single
	// canonical "N-D angle" the way SIFT's 2D orientation is one number --
	// but there IS a canonical N-D generalization of "the dominant gradient
	// direction", and it doesn't need an angle at all: the eigenvectors of
	// the local structure tensor (the windowed, Gaussian-weighted sum of
	// gradient outer products over the patch -- the exact same tensor
	// optical_flow.h's lucas_kanade_flow() builds, just per-keypoint here
	// instead of dense-per-pixel). Rotate the whole patch by any Q in
	// SO(DIM) and every gradient in it rotates by Q too, so the structure
	// tensor becomes Q*T*Q^T -- and the eigenvectors of a conjugated
	// symmetric matrix are exactly Q times the original eigenvectors
	// (eigen_decomposition() is equivariant under orthogonal conjugation).
	// Re-expressing both the sampling grid's own position offsets AND each
	// pixel's gradient vector in that eigenbasis before binning therefore
	// cancels the original rotation out, the direct N-D analogue of
	// rotating SIFT's sampling grid to align with its one orientation
	// angle.
	//
	// Two real, unavoidable loose ends, both inherent to using an
	// eigenbasis rather than a single angle, not implementation bugs:
	//  - SIGN: eigenvectors are only defined up to sign (v and -v are both
	//    valid for the same eigenvalue) -- resolved here the standard way
	//    (as in point-cloud local-reference-frame descriptors like SHOT):
	//    flip each eigenvector so the patch's own gradient mass projects
	//    mostly positive along it. A patch whose gradients are exactly
	//    balanced in both directions along some axis (rare, but possible)
	//    has no real answer here -- same as SIFT's own orientation
	//    histogram having multiple equally-tall peaks at a symmetric
	//    keypoint.
	//  - DEGENERACY: if two or more eigenvalues are equal (e.g. perfectly
	//    isotropic local structure -- a corner with 4-fold symmetry, say),
	//    the eigenvectors spanning that degenerate subspace aren't uniquely
	//    determined at all (any orthonormal basis of the subspace is
	//    equally valid), so the frame becomes numerically unstable exactly
	//    where it matters least (an isotropic patch has no real "preferred
	//    direction" to align to in the first place).
	// A REFLECTION (det = -1) is also possible after independent
	// sign-flips of DIM eigenvectors -- corrected by flipping the smallest-
	// eigenvalue (least informative) axis back, so the frame is always a
	// true rotation.
	/// Computes each keypoint's local descriptor, oriented via the local structure tensor's eigenbasis -- see this function's own comment for exactly what it encodes, how the orientation frame works, and why it isn't literal SIFT.
	/// @tparam ImageT Any minimal-interface image type; its value_type must be arithmetic.
	/// @tparam DIM    Spatial dimensionality; must match src's own DIM.
	/// @param  src               Source image the keypoints were detected in.
	/// @param  keypoints         Keypoints to describe (from detect_keypoints()).
	/// @param  numCellsPerAxis   Grid resolution per axis (numCellsPerAxis^DIM cells total, each with 2*DIM bins). Defaults to 4 (matching published SIFT's own 4x4 grid, for DIM==2).
	/// @param  patchRadiusInSigmas Sampling patch radius, in multiples of the keypoint's own detected scale. Defaults to 6.
	/// @return One descriptor per keypoint, same order as `keypoints`.
	/// @ingroup features
	template<class ImageT, int DIM>
	std::vector<Descriptor> compute_descriptors(const ImageT& src, const std::vector<Keypoint<DIM>>& keypoints, int numCellsPerAxis = 4, double patchRadiusInSigmas = 6.0)
	{
		using T = typename ImageT::value_type;
		static_assert(std::is_arithmetic_v<T>, "ndl::compute_descriptors() requires a value_type convertible to double -- not valid for e.g. std::complex<T>");
		assert(numCellsPerAxis > 0);

		auto extent = src.extent();
		constexpr int ImgDIM = std::tuple_size<decltype(extent)>::value;
		static_assert(ImgDIM == DIM, "ndl::compute_descriptors() requires src's own spatial dimensionality to match the keypoints' DIM");

		OwnedImage<double, DIM> srcDbl(src);
		std::array<int, DIM + 1> gradExtent;
		gradExtent[0] = DIM;
		for (int d = 0; d < DIM; d++) gradExtent[d + 1] = extent[d];
		OwnedImage<double, DIM + 1> grad(gradExtent);
		gradient(srcDbl, grad, BorderMode::Reflect);

		std::vector<Image<double, DIM>> gradAxis;
		gradAxis.reserve(DIM);
		for (int a = 0; a < DIM; a++) gradAxis.push_back(grad.slice(0, a));

		int numCells = 1;
		for (int d = 0; d < DIM; d++) numCells *= numCellsPerAxis;
		std::size_t descLen = (std::size_t)numCells * 2 * DIM;

		std::vector<Descriptor> descriptors;
		descriptors.reserve(keypoints.size());

		for (const auto& kp : keypoints)
		{
			double patchRadius = patchRadiusInSigmas * kp.scale;
			double gaussSigma = patchRadius * 0.5;
			int r = (int)std::ceil(patchRadius);

			// Walks every in-bounds patch pixel within the Gaussian-weighted
			// sampling radius, same membership test each of the three passes
			// below needs -- shared here so it's only written once.
			auto forEachPatchPixel = [&](auto&& fn) {
				std::array<int, DIM> offset;
				offset.fill(-r);
				while (true)
				{
					std::array<int, DIM> pixCoord;
					bool inBounds = true;
					double distSq = 0;
					for (int d = 0; d < DIM; d++)
					{
						pixCoord[d] = kp.position[d] + offset[d];
						if (pixCoord[d] < 0 || pixCoord[d] >= extent[d]) inBounds = false;
						distSq += (double)offset[d] * offset[d];
					}
					if (inBounds && distSq <= patchRadius * patchRadius)
					{
						double weight = std::exp(-distSq / (2 * gaussSigma * gaussSigma));
						fn(offset, pixCoord, weight);
					}

					int d = 0;
					for (; d < DIM; d++)
					{
						offset[d]++;
						if (offset[d] <= r) break;
						offset[d] = -r;
					}
					if (d == DIM) break;
				}
			};

			// Pass 1: the local structure tensor -- the same "windowed sum
			// of gradient outer products" optical_flow.h builds densely,
			// here just over one keypoint's own patch.
			Matrix<double, DIM> structureTensor;
			structureTensor.set_zero();
			forEachPatchPixel([&](const std::array<int, DIM>&, const std::array<int, DIM>& pixCoord, double weight)
			{
				std::array<double, DIM> g;
				for (int a = 0; a < DIM; a++) g[a] = gradAxis[a].at(pixCoord);
				for (int i = 0; i < DIM; i++)
					for (int j = 0; j < DIM; j++)
						structureTensor(i, j) += weight * g[i] * g[j];
			});

			std::array<double, DIM> eigenvalues;
			Matrix<double, DIM> frame;
			eigen_decomposition(structureTensor, eigenvalues, frame);

			// Pass 2: sign-disambiguate each eigenvector (see this
			// function's own comment) by checking which way the patch's own
			// gradient mass actually projects along it.
			std::array<double, DIM> axisProjection{};
			forEachPatchPixel([&](const std::array<int, DIM>&, const std::array<int, DIM>& pixCoord, double weight)
			{
				for (int k = 0; k < DIM; k++)
				{
					double proj = 0;
					for (int a = 0; a < DIM; a++) proj += frame(a, k) * gradAxis[a].at(pixCoord);
					axisProjection[k] += weight * proj;
				}
			});
			for (int k = 0; k < DIM; k++)
				if (axisProjection[k] < 0)
					for (int a = 0; a < DIM; a++) frame(a, k) = -frame(a, k);

			// A reflection (det -1) can result from an odd number of the
			// independent per-axis sign flips above -- fixed by flipping the
			// least-informative (smallest-eigenvalue) axis back, so `frame`
			// is always a true rotation.
			if (determinant(frame) < 0)
				for (int a = 0; a < DIM; a++) frame(a, DIM - 1) = -frame(a, DIM - 1);

			// Pass 3: bin into the descriptor, with both the sampling
			// position and the gradient vector re-expressed in `frame`
			// first -- this is what actually cancels out any true rotation
			// of the underlying image (see this function's own comment).
			Descriptor desc(descLen, 0.0);
			forEachPatchPixel([&](const std::array<int, DIM>& offset, const std::array<int, DIM>& pixCoord, double weight)
			{
				std::array<double, DIM> rotatedOffset{}, rotatedGrad{};
				for (int k = 0; k < DIM; k++)
				{
					double posAcc = 0, gradAcc = 0;
					for (int a = 0; a < DIM; a++)
					{
						posAcc += frame(a, k) * offset[a];
						gradAcc += frame(a, k) * gradAxis[a].at(pixCoord);
					}
					rotatedOffset[k] = posAcc;
					rotatedGrad[k] = gradAcc;
				}

				int flatCell = 0, mult = 1;
				for (int d = 0; d < DIM; d++)
				{
					double norm = (rotatedOffset[d] / patchRadius + 1.0) / 2.0; // [0,1] across the patch
					int c = (int)(norm * numCellsPerAxis);
					if (c < 0) c = 0;
					if (c >= numCellsPerAxis) c = numCellsPerAxis - 1;
					flatCell += c * mult;
					mult *= numCellsPerAxis;
				}
				for (int a = 0; a < DIM; a++)
				{
					double g = rotatedGrad[a];
					std::size_t bin = (std::size_t)flatCell * (2 * DIM) + 2 * a + (g >= 0 ? 0 : 1);
					desc[bin] += std::abs(g) * weight;
				}
			});

			double norm = 0;
			for (double v : desc) norm += v * v;
			norm = std::sqrt(norm);
			if (norm > 1e-12) for (double& v : desc) v /= norm;

			descriptors.push_back(std::move(desc));
		}
		return descriptors;
	}

	// One matched keypoint pair between two descriptor sets, by index into
	// each set's own keypoints/descriptors.
	/// A single matched keypoint pair, by index into each set's own keypoints/descriptors.
	/// @ingroup features
	struct DescriptorMatch
	{
		int index0;
		int index1;
		double distance;
	};

	// Brute-force nearest-neighbor descriptor matching with Lowe's ratio
	// test: a match is kept only when its nearest neighbor is convincingly
	// closer than its SECOND-nearest (best/secondBest < ratioThreshold),
	// which rejects ambiguous matches (e.g. in a repetitive/textureless
	// region where many descriptors look nearly identical) far more
	// effectively than any fixed absolute-distance cutoff could.
	/// Matches descriptors0 against descriptors1 via nearest-neighbor + Lowe's ratio test.
	/// @param descriptors0   Descriptors from the first image.
	/// @param descriptors1   Descriptors from the second image; must be the same length as each entry in descriptors0.
	/// @param ratioThreshold Maximum allowed (nearest distance / second-nearest distance). Lower is stricter. Defaults to 0.8 (Lowe's own published value).
	/// @return One DescriptorMatch per surviving descriptors0 entry.
	/// @ingroup features
	inline std::vector<DescriptorMatch> match_descriptors(const std::vector<Descriptor>& descriptors0, const std::vector<Descriptor>& descriptors1, double ratioThreshold = 0.8)
	{
		std::vector<DescriptorMatch> matches;
		for (std::size_t i = 0; i < descriptors0.size(); i++)
		{
			double best = std::numeric_limits<double>::max();
			double second = std::numeric_limits<double>::max();
			int bestJ = -1;
			for (std::size_t j = 0; j < descriptors1.size(); j++)
			{
				double distSq = 0;
				for (std::size_t k = 0; k < descriptors0[i].size(); k++)
				{
					double diff = descriptors0[i][k] - descriptors1[j][k];
					distSq += diff * diff;
				}
				double dist = std::sqrt(distSq);
				if (dist < best) { second = best; best = dist; bestJ = (int)j; }
				else if (dist < second) second = dist;
			}
			if (bestJ >= 0 && (second == 0.0 || best / second < ratioThreshold))
				matches.push_back(DescriptorMatch{ (int)i, bestJ, best });
		}
		return matches;
	}

	// Orchestrates the full pipeline -- detect_keypoints() on each frame,
	// compute_descriptors(), match_descriptors() -- into a dense per-pixel
	// displacement field, the same {DIM, ...extent} shape
	// lucas_kanade_flow() (optical_flow.h) produces, so the two are directly
	// comparable/interchangeable in anything built on top of either.
	// Matched keypoints are inherently SPARSE, though (only detected at
	// blob-like, high-contrast locations, unlike Lucas-Kanade's genuinely
	// per-pixel estimate) -- every pixel's displacement here is filled in by
	// inverse-distance-weighted interpolation (Shepard's method, power 2)
	// from the sparse matched displacements, a standard, well-defined
	// any-DIM scattered-data interpolation. A pixel exactly at a matched
	// keypoint gets that match's own displacement exactly; everywhere else
	// is a distance-weighted blend of every match, so the field is smooth
	// but progressively less trustworthy far from any actual match --
	// expect a visibly blockier, coarser field than lucas_kanade_flow()'s,
	// especially wherever matches are sparse.
	/// Sparse SIFT-inspired feature matching, propagated to a dense per-pixel displacement field via inverse-distance-weighted interpolation.
	/// @tparam SrcImageT Any minimal-interface image type; its value_type must be arithmetic.
	/// @tparam DstImageT Any minimal-interface image type with exactly one more axis than SrcImageT (a leading component axis of size DIM); may differ from SrcImageT otherwise.
	/// @param  frame0    Earlier frame.
	/// @param  frame1    Later frame; same extent as frame0.
	/// @param  flowOut   Destination; must already exist, extent {DIM, ...frame0's own extent}.
	/// @param  numScales, sigma0, contrastThreshold  Forwarded to detect_keypoints() on each frame.
	/// @param  numCellsPerAxis Forwarded to compute_descriptors().
	/// @param  ratioThreshold  Forwarded to match_descriptors().
	/// @return The number of matches the dense field was interpolated from -- 0 means flowOut was filled with all zeros (nothing to interpolate from).
	/// @ingroup features
	template<class SrcImageT, class DstImageT>
	std::size_t sift_flow(const SrcImageT& frame0, const SrcImageT& frame1, DstImageT& flowOut,
		int numScales = 5, double sigma0 = 1.6, double contrastThreshold = 5.0,
		int numCellsPerAxis = 4, double ratioThreshold = 0.8)
	{
		assert(frame1.extent() == frame0.extent());
		auto extent = frame0.extent();
		constexpr int DIM = std::tuple_size<decltype(extent)>::value;
		auto flowExtent = flowOut.extent();
		constexpr int FlowDIM = std::tuple_size<decltype(flowExtent)>::value;
		static_assert(FlowDIM == DIM + 1, "ndl::sift_flow() requires flowOut to have exactly one more axis than the frames (a leading component axis of size DIM)");
		assert(flowExtent[0] == DIM);
		for (int d = 0; d < DIM; d++) assert(flowExtent[d + 1] == extent[d]);

		auto kp0 = detect_keypoints(frame0, numScales, sigma0, contrastThreshold);
		auto kp1 = detect_keypoints(frame1, numScales, sigma0, contrastThreshold);
		auto desc0 = compute_descriptors(frame0, kp0, numCellsPerAxis);
		auto desc1 = compute_descriptors(frame1, kp1, numCellsPerAxis);
		auto matches = match_descriptors(desc0, desc1, ratioThreshold);

		std::vector<std::array<double, DIM>> samplePos, sampleDisp;
		samplePos.reserve(matches.size());
		sampleDisp.reserve(matches.size());
		for (const auto& m : matches)
		{
			std::array<double, DIM> pos{}, disp{};
			for (int d = 0; d < DIM; d++)
			{
				pos[d] = kp0[m.index0].position[d];
				disp[d] = kp1[m.index1].position[d] - kp0[m.index0].position[d];
			}
			samplePos.push_back(pos);
			sampleDisp.push_back(disp);
		}

		for (const auto& coord : frame0.coordinates())
		{
			std::array<double, DIM> result{};
			if (!samplePos.empty())
			{
				double weightTotal = 0;
				bool exact = false;
				for (std::size_t s = 0; s < samplePos.size() && !exact; s++)
				{
					double distSq = 0;
					for (int d = 0; d < DIM; d++) { double diff = coord[d] - samplePos[s][d]; distSq += diff * diff; }
					if (distSq < 1e-9) { result = sampleDisp[s]; exact = true; break; }
					double w = 1.0 / distSq;
					weightTotal += w;
					for (int d = 0; d < DIM; d++) result[d] += w * sampleDisp[s][d];
				}
				if (!exact && weightTotal > 0)
					for (int d = 0; d < DIM; d++) result[d] /= weightTotal;
			}
			for (int d = 0; d < DIM; d++)
				flowOut.slice(0, d).at(coord) = result[d];
		}

		return matches.size();
	}
}
