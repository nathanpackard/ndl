#pragma once
#include <string>
#include <vector>
#include <array>
#include <map>
#include <functional>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <cstdint>
#include <execution>
#include <numeric>

#include "../image.h"
#include "../processing/convolution.h"
#include "../processing/interpolation.h"
#include "../processing/matrix/core.h"
#include "../processing/channels.h"
#include "../net/json.h"

// The "generic viewport interface" the live-streaming redesign is built
// around: instead of a server blasting whole frames and letting the
// browser crop what it already has, the CLIENT's current view (which
// axes, what crop region, what resolution, what window/level) is a
// request, and the server renders exactly that. A sibling of
// ring_buffer.h/net/websocket_server.h, not part of image.h's core --
// #include this directly if you use it.
//
// Three pieces, deliberately independent of any networking:
//   - renderSlice(): one "op" -- crop + resample + window a 2D slice out
//     of ANY minimal-interface source (extent()/at()/coordinates()), the
//     direct C++ counterpart to ndlviewer.js's own extractSliceU8()/
//     extractSliceRGB(), just extended with an explicit crop region and
//     output resolution.
//   - RendererRegistry<SourceT>: op-name -> render-function dispatch.
//     renderSlice() is the only op registered for v1, but adding another
//     ndl visualization mode later is "write one more function with this
//     same signature and registerOp() it," not touching the wire
//     protocol or anything else that already exists.
//   - Viewport: one independently addressable render context -- its own
//     params (what to render) and pending output (the last rendered
//     result not yet consumed), each "latest wins" on update rather than
//     queued. This class has no idea a WebSocket (or anything else)
//     exists; something else (an app's own render loop, a WebSocket
//     server's onMessage callback) drives it and does something with a
//     taken pendingOutput.
namespace ndl
{
	/// One renderSlice() (or any other registered op's) result -- caller-owned pixels, ready to send/display.
	/// @ingroup viewer
	struct RenderedFrame
	{
		int width = 0;
		int height = 0;
		int channels = 1;   ///< 1 = grayscale, 3 = RGB (a colorAxis was given).
		/// width*height*channels bytes, channel-fastest, then x, then y (row-major) -- windowed to [0,255].
		std::vector<uint8_t> pixels;
	};

	namespace detail_viewport
	{
		inline int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
		inline double clampD(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

		// matrix3FromColumnMajor() used to live here -- promoted to
		// Matrix<Real,N>::from_column_major() (processing/matrix/core.h)
		// once it became clear "build a matrix from a column-major flat
		// array" (the layout OpenGL/WebGL/web/ndlviewer.js's own uniform-
		// matrix convention uses, vs. Matrix's own row-major storage) is a
		// general construction concern belonging on Matrix itself, not a
		// one-off adapter local to this header -- see that method's own
		// comment for the full rationale (still the exact same mapping,
		// still confirmed by RenderVolumeRotationChangesWhatTheRaySees's
		// own rotation-sensitivity test passing unchanged).

		// channelReductionOutputCount()/reduceChannels() used to live here
		// (this was their only caller) -- promoted to processing/channels.h
		// once it became clear the reduction logic itself has nothing to do
		// with rendering or viewports specifically; #include'd above,
		// resolved via ordinary enclosing-namespace lookup (both now live
		// in ndl::, same as everything renderSlice()/renderVolume() below
		// still calls unqualified).
	}

	// The one v1 op. params (a net::JsonValue object):
	//   axisI, axisJ    (required) -- which two of source's own axes this slice varies over.
	//   colorAxis       (optional) -- if given, reads every value along this axis at each position and
	//                   reduces them via channelReduction below; ndlviewer.js's own colorAxis
	//                   convention (default "rgb") composites true RGB from exactly 3 values.
	//   channelReduction (optional, default "rgb") -- how colorAxis's own values collapse into the
	//                   output: "rgb" (needs exactly 3 values, passthrough), "magnitude" (sqrt of sum
	//                   of squares, any count, grayscale -- the natural way to view e.g. a 2-channel
	//                   real+imag axis), "phase" (atan2(values[1],values[0]), needs exactly 2,
	//                   grayscale), or "sum"/"mean"/"max" (any count, grayscale). Ignored when
	//                   colorAxis isn't given. See processing/channels.h's own reduceChannels() comment.
	//   cropMin/cropMax (optional, each a 2-element array [alongAxisI, alongAxisJ]) -- the region to
	//                   extract, in source coordinates; defaults to the axis's own full extent.
	//                   Clamped to the source's own CURRENT extent (which can shrink/grow for a live
	//                   ring-buffer source between calls).
	//   outputWidth/outputHeight (optional) -- see the resampling policy below.
	//   windowMin/windowMax (optional, default 0/255) -- the same shared intensity window every flat
	//                   panel in ndlviewer.js already uses.
	//   fixed           (optional, an object mapping a STRING axis index to an integer value) -- every
	//                   axis that's neither axisI, axisJ, nor colorAxis must have a value from here (or
	//                   defaults to 0); e.g. for a live ring-buffer source, its own ring/time axis
	//                   belongs here, resolved to a concrete index by WHATEVER OWNS the source (it
	//                   knows what "latest" means for its own data; renderSlice() itself only ever sees
	//                   concrete integers, deliberately -- see this header's own top comment on why it
	//                   stays source-agnostic).
	//
	// Resampling policy, deliberately simple: NEVER upsamples (a request
	// for a bigger output than the crop's own native pixel count just
	// returns the crop at its native size -- the client scales up for
	// display via its own canvas, free, no extra bytes on the wire).
	// When the requested output is smaller, downsamples via the already-
	// existing ndl::downsample() (convolution.h) at the nearest integer
	// factor -- its own blur-then-decimate is the numerically correct way
	// to shrink an image, not just a shortcut. The RESULT's actual
	// dimensions (not necessarily an exact match, since downsample()'s
	// factor is integer-only) are what's reported back in the returned
	// RenderedFrame -- callers display whatever size comes back rather
	// than the server forcing an exact match.
	/// Direct C++ port of ndlviewer.js's extractSliceU8()/extractSliceRGB(), extended with a crop region
	/// and output resolution. Works over any minimal-interface source (extent()/at()/coordinates()).
	/// @tparam SourceT Any minimal-interface image-like type.
	/// @throws std::invalid_argument or ndl::net::JsonValue's own exceptions on malformed/out-of-range params.
	/// @ingroup viewer
	template<class SourceT>
	RenderedFrame renderSlice(const SourceT& source, const net::JsonValue& params)
	{
		using namespace detail_viewport;
		auto extent = source.extent();
		constexpr int DIM = (int)std::tuple_size<decltype(extent)>::value;

		int axisI = params["axisI"].asInt();
		int axisJ = params["axisJ"].asInt();
		if (axisI < 0 || axisI >= DIM || axisJ < 0 || axisJ >= DIM || axisI == axisJ)
			throw std::invalid_argument("renderSlice(): axisI/axisJ out of range or equal");
		int colorAxis = (params.has("colorAxis") && !params["colorAxis"].isNull()) ? params["colorAxis"].asInt() : -1;

		int cropMinI = 0, cropMinJ = 0, cropMaxI = extent[axisI], cropMaxJ = extent[axisJ];
		if (params.has("cropMin")) { const auto& a = params["cropMin"].asArray(); cropMinI = a[0].asInt(); cropMinJ = a[1].asInt(); }
		if (params.has("cropMax")) { const auto& a = params["cropMax"].asArray(); cropMaxI = a[0].asInt(); cropMaxJ = a[1].asInt(); }
		cropMinI = clampInt(cropMinI, 0, extent[axisI]); cropMaxI = clampInt(cropMaxI, 0, extent[axisI]);
		cropMinJ = clampInt(cropMinJ, 0, extent[axisJ]); cropMaxJ = clampInt(cropMaxJ, 0, extent[axisJ]);
		int cropW = cropMaxI - cropMinI, cropH = cropMaxJ - cropMinJ;
		if (cropW <= 0 || cropH <= 0) throw std::invalid_argument("renderSlice(): empty crop region");

		double windowMin = params.numberOr("windowMin", 0.0);
		double windowMax = params.numberOr("windowMax", 255.0);
		double range = windowMax - windowMin;

		std::array<int, DIM> base{};
		if (params.has("fixed"))
			for (const auto& kv : params["fixed"].asObject())
			{
				int axis = std::stoi(kv.first);
				if (axis >= 0 && axis < DIM) base[axis] = clampInt(kv.second.asInt(), 0, extent[axis] - 1);
			}

		std::string channelReduction = params.has("channelReduction") ? params["channelReduction"].asString() : "rgb";
		int channels = colorAxis >= 0 ? channelReductionOutputCount(channelReduction) : 1;
		std::vector<uint8_t> cropPixels((std::size_t)cropW * cropH * channels);

		// A bad channelReduction (wrong channel count for "rgb"/"phase", or
		// an unrecognized mode name) has to be caught HERE, sequentially,
		// rather than left to the first reduceChannels() call inside the
		// parallel loop below to throw it: an exception escaping a
		// std::execution::par callable can't propagate to the caller at
		// all (the standard mandates std::terminate() instead -- the same
		// reasoning fftn() documents for its own per-axis validation).
		// Reuses reduceChannels() itself (on a throwaway buffer) rather
		// than re-deriving its own validation rules here a second time --
		// the real per-pixel calls below are guaranteed not to throw once
		// this one hasn't.
		if (colorAxis >= 0)
		{
			std::vector<double> validationBuf((std::size_t)extent[colorAxis], 0.0);
			double validationOut[3];
			reduceChannels(validationBuf.data(), (int)validationBuf.size(), channelReduction, validationOut);
		}

		// One task per crop ROW, run in parallel -- every row reads a
		// disjoint set of source coordinates and writes a disjoint slice
		// of cropPixels, the same "independent chunks, no merge step"
		// shape as renderVolume()'s own per-row parallelization above (see
		// that loop's own comment for the full reasoning). channelValues/
		// reduced move to one buffer PER ROW TASK (cropH allocations
		// total) rather than one shared buffer reused across the whole
		// crop, for the same reason.
		std::vector<int> rowIndices(cropH);
		std::iota(rowIndices.begin(), rowIndices.end(), 0);
		std::for_each(std::execution::par, rowIndices.begin(), rowIndices.end(), [&](int y)
		{
			std::vector<double> channelValues(colorAxis >= 0 ? (std::size_t)extent[colorAxis] : 0);
			double reduced[3];
			std::size_t p = (std::size_t)y * cropW * channels;
			for (int x = 0; x < cropW; x++)
			{
				std::array<int, DIM> coord = base;
				coord[axisI] = cropMinI + x;
				coord[axisJ] = cropMinJ + y;
				if (colorAxis >= 0)
				{
					for (int ch = 0; ch < (int)channelValues.size(); ch++)
					{
						coord[colorAxis] = ch;
						channelValues[ch] = (double)source.at(coord);
					}
					int outN = reduceChannels(channelValues.data(), (int)channelValues.size(), channelReduction, reduced);
					for (int ch = 0; ch < outN; ch++)
						cropPixels[p++] = range > 0 ? (uint8_t)clampInt((int)std::lround(((reduced[ch] - windowMin) / range) * 255), 0, 255) : 0;
				}
				else
				{
					double v = (double)source.at(coord);
					cropPixels[p++] = range > 0 ? (uint8_t)clampInt((int)std::lround(((v - windowMin) / range) * 255), 0, 255) : 0;
				}
			}
		});

		int outputWidth = params.has("outputWidth") ? params["outputWidth"].asInt() : cropW;
		int outputHeight = params.has("outputHeight") ? params["outputHeight"].asInt() : cropH;
		if (outputWidth <= 0) outputWidth = cropW;
		if (outputHeight <= 0) outputHeight = cropH;

		RenderedFrame result;
		if (outputWidth >= cropW && outputHeight >= cropH)
		{
			result.width = cropW; result.height = cropH; result.channels = channels; result.pixels = std::move(cropPixels);
			return result;
		}

		int factorW = std::max(1, cropW / outputWidth);
		int factorH = std::max(1, cropH / outputHeight);
		int factor = std::max(1, std::min(factorW, factorH));
		if (factor <= 1)
		{
			result.width = cropW; result.height = cropH; result.channels = channels; result.pixels = std::move(cropPixels);
			return result;
		}

		// Wraps the crop's own raw bytes as a minimal-interface {channel,x,y}
		// image so ndl::downsample() (convolution.h) can operate on it
		// directly -- channelAxis=0 matches the channel-fastest layout
		// cropPixels was already packed in above, and keeps downsample()
		// from blurring/decimating the channel axis itself.
		Image<uint8_t, 3> cropImg(cropPixels.data(), { channels, cropW, cropH });
		OwnedImage<uint8_t, 3> down = downsample(cropImg, factor, /*channelAxis*/ 0);
		result.width = down.extent()[1];
		result.height = down.extent()[2];
		result.channels = channels;
		result.pixels.assign(down.begin(), down.end());
		return result;
	}

	// The native, server-side counterpart to web/ndlviewer.js's own WebGL
	// volume shaders (VOLUME_FRAGMENT_SRC_GRAYSCALE/_COLOR) -- an
	// orthographic ray-march through a 3-axis sub-block of ANY minimal-
	// interface source, with the exact same Beer-Lambert absorption
	// compositing, so the two stay visually comparable even though one
	// runs in GLSL on the GPU and this one runs in portable C++ on the
	// CPU (see this project's own comparison-verification step). Not a
	// port for its own sake: this is what lets a LIVE (streaming) source
	// get a volume-rendered view at all -- the browser's own WebGL panel
	// only ever works against a fully-downloaded static blob, and nothing
	// about a live ring buffer changes that; the static case keeps using
	// its existing, fast, GPU-accelerated path unmodified (see viewer.h's
	// own top comment on the client framework for that scope boundary).
	//
	// Reuses ndl::sample() (processing/interpolation.h) with the default
	// Linear interpolator for trilinear sampling at each ray step --
	// genuinely N-linear (see that header's own comment on Linear), not
	// reimplemented here, the same "reuse an existing toolkit function"
	// pattern renderSlice() already follows with ndl::downsample().
	//
	// params (a net::JsonValue object):
	//   axisA, axisB, axisC (required) -- the 3 axes forming the sub-volume, must be distinct.
	//   rotation (optional, a 9-number flat array) -- a column-major 3x3 matrix, the SAME
	//             convention web/ndlviewer.js's own mat3Multiply()/rotationMatrix() use (see
	//             Matrix<Real,N>::from_column_major()'s own comment, processing/matrix/core.h)
	//             -- defaults to identity.
	//   alphaScale (optional, default 0.5) -- matches VOLUME_FRAGMENT_SRC's own uAlphaScale and
	//             DEFAULT_ALPHA_SCALE.
	//   colorAxis (optional) -- same convention as renderSlice(): reads every value along this axis
	//             at each sample position and reduces them via channelReduction below. Under "rgb"
	//             (the default), that means composited true color, with density derived from the
	//             composited color via the same BT.601-ish luminance weights (0.299/0.587/0.114)
	//             VOLUME_FRAGMENT_SRC_COLOR uses; every other mode reduces to a single grayscale
	//             density value instead, same as renderSlice()'s own non-color path.
	//   channelReduction (optional, default "rgb") -- same modes/meaning as renderSlice()'s own
	//             channelReduction (see that function's own comment); ignored when colorAxis isn't given.
	//   windowMin/windowMax (optional, default 0/255) -- same shared intensity window renderSlice() uses.
	//   outputWidth/outputHeight (optional, default 256 each) -- rendered directly at this
	//             resolution (no separate crop/downsample step the way renderSlice() has: ray-
	//             marching already samples at whatever density is asked for).
	//   fixed     (optional) -- same convention as renderSlice(): a {axis: value} map for any axis
	//             beyond axisA/axisB/axisC/colorAxis.
	//
	// STEPS is deliberately smaller than the shader's own 220: that constant amortizes over
	// thousands of GPU cores in parallel, while this runs one ray at a time on the CPU -- 96 is a
	// tuned balance (enough steps that a solid object still reads as solid, not banded) between
	// visual fidelity and wall-clock cost for a single render call.
	/// Direct C++ port of web/ndlviewer.js's own WebGL volume ray-marcher (VOLUME_FRAGMENT_SRC_*),
	/// computed in portable C++ instead of GLSL -- see this function's own comment for the full
	/// rationale and the exact param shape.
	/// @tparam SourceT Any minimal-interface source whose value_type is arithmetic.
	/// @throws std::invalid_argument or ndl::net::JsonValue's own exceptions on malformed/out-of-range params.
	/// @ingroup viewer
	template<class SourceT>
	RenderedFrame renderVolume(const SourceT& source, const net::JsonValue& params)
	{
		using namespace detail_viewport;
		auto extent = source.extent();
		constexpr int DIM = (int)std::tuple_size<decltype(extent)>::value;

		int axisA = params["axisA"].asInt();
		int axisB = params["axisB"].asInt();
		int axisC = params["axisC"].asInt();
		if (axisA < 0 || axisA >= DIM || axisB < 0 || axisB >= DIM || axisC < 0 || axisC >= DIM
			|| axisA == axisB || axisA == axisC || axisB == axisC)
			throw std::invalid_argument("renderVolume(): axisA/axisB/axisC out of range or not distinct");
		int colorAxis = (params.has("colorAxis") && !params["colorAxis"].isNull()) ? params["colorAxis"].asInt() : -1;

		// "rotation" (optional, a 9-number flat column-major array -- see
		// this function's own comment) defaults to the identity's own flat
		// values via net::JsonValue::numbersOr() (net/json.h), then reads
		// as a real Matrix<double,3> via from_column_major() (processing/
		// matrix/core.h) -- see that method's own comment for the layout
		// mapping.
		std::array<double, 9> identityFlat = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
		Matrix<double, 3> R = Matrix<double, 3>::from_column_major(params.numbersOr("rotation", identityFlat).data());

		double alphaScale = params.numberOr("alphaScale", 0.5);
		double windowMin = params.numberOr("windowMin", 0.0);
		double windowMax = params.numberOr("windowMax", 255.0);
		double range = windowMax - windowMin;

		int outputWidth = params.has("outputWidth") ? params["outputWidth"].asInt() : 256;
		int outputHeight = params.has("outputHeight") ? params["outputHeight"].asInt() : 256;
		if (outputWidth <= 0) outputWidth = 256;
		if (outputHeight <= 0) outputHeight = 256;

		std::array<int, DIM> base{};
		if (params.has("fixed"))
			for (const auto& kv : params["fixed"].asObject())
			{
				int axis = std::stoi(kv.first);
				if (axis >= 0 && axis < DIM) base[axis] = clampInt(kv.second.asInt(), 0, extent[axis] - 1);
			}

		// Given a fractional position along axisA/axisB/axisC (in raw voxel-
		// index units, NOT normalized texCoord -- the *2*0.5+0.5 texCoord
		// dance in the shader/below is purely about mapping the ray-march's
		// own [-1,1] local space to [0,1]; this lambda's own job is the
		// SEPARATE texCoord->voxel-index conversion, `texCoord*extent-0.5`,
		// the standard GL-texel-center convention), samples the source at
		// that position with every other axis held at `base` (and colorAxis,
		// if given, forced to `forceColorCh`).
		auto sampleAt = [&](double posA, double posB, double posC, int forceColorCh) -> double
		{
			std::array<double, DIM> position;
			for (int k = 0; k < DIM; k++) position[k] = (double)base[k];
			position[axisA] = posA; position[axisB] = posB; position[axisC] = posC;
			if (colorAxis >= 0) position[colorAxis] = (double)forceColorCh;
			return sample(source, position);
		};

		std::string channelReduction = params.has("channelReduction") ? params["channelReduction"].asString() : "rgb";
		int channels = colorAxis >= 0 ? channelReductionOutputCount(channelReduction) : 1;
		RenderedFrame result;
		result.width = outputWidth; result.height = outputHeight; result.channels = channels;
		result.pixels.assign((std::size_t)outputWidth * outputHeight * channels, 0);

		// Same reasoning as renderSlice()'s own pre-loop validation call
		// just above: a bad channelReduction has to throw HERE, before the
		// parallel per-row loop below, not from inside it.
		if (colorAxis >= 0)
		{
			std::vector<double> validationBuf((std::size_t)extent[colorAxis], 0.0);
			double validationOut[3];
			reduceChannels(validationBuf.data(), (int)validationBuf.size(), channelReduction, validationOut);
		}

		const int STEPS = 96;
		const double ALPHA_DENSITY = 12.0;
		double tStep = 2.0 * std::sqrt(3.0) / STEPS;
		std::array<double, 3> rayDir = R * std::array<double, 3>{ 0.0, 0.0, 1.0 };
		double rdLen = std::sqrt(rayDir[0] * rayDir[0] + rayDir[1] * rayDir[1] + rayDir[2] * rayDir[2]);
		if (rdLen > 0) { rayDir[0] /= rdLen; rayDir[1] /= rdLen; rayDir[2] /= rdLen; }

		// One task per output ROW, run in parallel: every ray is
		// independent of every other ray (each only ever reads `source`/
		// `R`/`base` and writes its own disjoint slice of result.pixels),
		// the same "chunks are already independent, no merge step"
		// reasoning fftn()'s own per-fiber std::execution::par usage
		// documents -- rows rather than individual pixels purely to keep
		// the parallel task count modest (outputHeight, not
		// outputWidth*outputHeight) without losing any real parallelism
		// (a single ray's own up-to-96-step march is already far more
		// work than the scheduling overhead of one task). channelValues/
		// reducedRaw move from a single buffer shared (and reused, not
		// reallocated) across the whole image to one PER ROW TASK instead
		// -- still only outputHeight allocations total, cheap next to the
		// per-pixel ray-march cost each row goes on to do, but now safe
		// for concurrent rows to mutate their own copy independently.
		// Every validation that could throw (axisA/B/C, etc.) already
		// happened above, sequentially, before this loop starts --
		// required, not just tidy: an exception escaping a
		// std::execution::par callable can't propagate to the caller at
		// all (the standard mandates std::terminate() instead), the exact
		// reason fftn() validates its own per-axis extent before entering
		// its own parallel for_each rather than inside it.
		std::vector<int> rowIndices(outputHeight);
		std::iota(rowIndices.begin(), rowIndices.end(), 0);
		std::for_each(std::execution::par, rowIndices.begin(), rowIndices.end(), [&](int py)
		{
			std::vector<double> channelValues(colorAxis >= 0 ? (std::size_t)extent[colorAxis] : 0);
			double reducedRaw[3];

			// vsY: -1 at screen-top, +1 at screen-bottom -- the SAME
			// "vScreen" convention (already flipped relative to raw clip
			// space) web/ndlviewer.js's own navigateFromEvent()/
			// scrollVolumeDepth() compute directly from mouse position, so a
			// `rotation` matrix that works correctly there means the same
			// thing here.
			double vsY = ((py + 0.5) / outputHeight) * 2.0 - 1.0;
			for (int px = 0; px < outputWidth; px++)
			{
				std::size_t p = (std::size_t)py * outputWidth + px;
				double vsX = ((px + 0.5) / outputWidth) * 2.0 - 1.0;
				std::array<double, 3> rayOrigin = R * std::array<double, 3>{ vsX, vsY, 0.0 };
				std::array<double, 3> pos = {
					rayOrigin[0] - rayDir[0] * std::sqrt(3.0),
					rayOrigin[1] - rayDir[1] * std::sqrt(3.0),
					rayOrigin[2] - rayDir[2] * std::sqrt(3.0)
				};
				double accumR = 0, accumG = 0, accumB = 0, accumA = 0;
				for (int i = 0; i < STEPS; i++)
				{
					double tcx = pos[0] * 0.5 + 0.5, tcy = pos[1] * 0.5 + 0.5, tcz = pos[2] * 0.5 + 0.5;
					if (tcx >= 0.0 && tcx <= 1.0 && tcy >= 0.0 && tcy <= 1.0 && tcz >= 0.0 && tcz <= 1.0)
					{
						double voxA = tcx * extent[axisA] - 0.5, voxB = tcy * extent[axisB] - 0.5, voxC = tcz * extent[axisC] - 0.5;
						double r, g, b, v;
						if (colorAxis >= 0)
						{
							for (int ch = 0; ch < (int)channelValues.size(); ch++) channelValues[ch] = sampleAt(voxA, voxB, voxC, ch);
							int outN = reduceChannels(channelValues.data(), (int)channelValues.size(), channelReduction, reducedRaw);
							if (outN == 3)
							{
								r = range > 0 ? clampD((reducedRaw[0] - windowMin) / range, 0, 1) : 0;
								g = range > 0 ? clampD((reducedRaw[1] - windowMin) / range, 0, 1) : 0;
								b = range > 0 ? clampD((reducedRaw[2] - windowMin) / range, 0, 1) : 0;
								v = 0.299 * r + 0.587 * g + 0.114 * b; // same BT.601-ish luminance weights VOLUME_FRAGMENT_SRC_COLOR uses to derive density from color
							}
							else
							{
								v = range > 0 ? clampD((reducedRaw[0] - windowMin) / range, 0, 1) : 0;
								r = g = b = v;
							}
						}
						else
						{
							v = range > 0 ? clampD((sampleAt(voxA, voxB, voxC, 0) - windowMin) / range, 0, 1) : 0;
							r = g = b = v;
						}
						double alpha = 1.0 - std::exp(-v * alphaScale * ALPHA_DENSITY * tStep);
						accumR += (1.0 - accumA) * alpha * r;
						accumG += (1.0 - accumA) * alpha * g;
						accumB += (1.0 - accumA) * alpha * b;
						accumA += (1.0 - accumA) * alpha;
						if (accumA > 0.98) break;
					}
					pos[0] += rayDir[0] * tStep; pos[1] += rayDir[1] * tStep; pos[2] += rayDir[2] * tStep;
				}
				if (channels == 3)
				{
					result.pixels[p * 3 + 0] = (uint8_t)clampInt((int)std::lround(accumR * 255), 0, 255);
					result.pixels[p * 3 + 1] = (uint8_t)clampInt((int)std::lround(accumG * 255), 0, 255);
					result.pixels[p * 3 + 2] = (uint8_t)clampInt((int)std::lround(accumB * 255), 0, 255);
				}
				else
				{
					result.pixels[p] = (uint8_t)clampInt((int)std::lround(accumR * 255), 0, 255);
				}
			}
		});
		return result;
	}

	// The native-value-hover counterpart to renderSlice()/renderVolume():
	// those two produce WINDOWED, quantized-to-[0,255] display pixels --
	// exactly what's needed to draw something on screen, but useless for
	// answering "what is this value, really" (the same thing the static
	// (`.ndlv`-upload) viewer's own hover readout already shows correctly
	// today, reading straight out of its local volume.data rather than
	// any rendered pixel). A live client has no local copy of the data to
	// read from, so it needs to ask the server directly -- this is that
	// ask, deliberately as small and separate a capability as possible:
	// no windowing, no channel reduction, no resampling, just "the raw
	// value at this exact coordinate," clamped into range rather than
	// throwing (a client's own last-known crop/pan state can be
	// momentarily stale relative to a live-resizing source, e.g. right as
	// a ring buffer's own extent changes between frames -- clamping is
	// the same forgiving-of-transient-staleness choice renderSlice()'s
	// own crop-bounds clamping already makes, not a new policy).
	/// Raw, unwindowed value at an exact full-dimensional coordinate -- see this function's own comment.
	/// @tparam SourceT Any minimal-interface source.
	/// @tparam DIM Deduced from `coord`.
	/// @ingroup viewer
	template<class SourceT, std::size_t DIM>
	double queryValue(const SourceT& source, const std::array<int, DIM>& coord)
	{
		using namespace detail_viewport;
		auto extent = source.extent();
		constexpr int SDIM = (int)std::tuple_size<decltype(extent)>::value;
		static_assert(SDIM == (int)DIM, "queryValue(): coord must have exactly source's own dimension");
		std::array<int, SDIM> clamped;
		for (int k = 0; k < SDIM; k++) clamped[k] = clampInt(coord[k], 0, extent[k] - 1);
		return (double)source.at(clamped);
	}

	/// Same as the std::array overload above, but reading the coordinate out of a `net::JsonValue`'s own
	/// `"coord"` array field -- the direct counterpart to the `queryValue` wire message
	/// (`{"type":"queryValue","id":"...","coord":[...]}`), so an app's own onMessage dispatch can call
	/// this with the parsed message object directly.
	/// @tparam SourceT Any minimal-interface source.
	/// @ingroup viewer
	template<class SourceT>
	double queryValue(const SourceT& source, const net::JsonValue& message)
	{
		auto extent = source.extent();
		constexpr int DIM = (int)std::tuple_size<decltype(extent)>::value;
		const auto& coordArr = message["coord"].asArray();
		std::array<int, DIM> coord{};
		for (int k = 0; k < DIM && k < (int)coordArr.size(); k++) coord[k] = coordArr[k].asInt();
		return queryValue(source, coord);
	}

	/// Op-name -> render-function dispatch, templated on the one concrete source type a given
	/// registry/server instance renders from. See this header's own top comment for the rationale.
	/// @tparam SourceT Any minimal-interface image-like type.
	/// @ingroup viewer
	template<class SourceT>
	class RendererRegistry
	{
	public:
		using RenderFn = std::function<RenderedFrame(const SourceT&, const net::JsonValue&)>;

		/// Registers (or replaces) the render function for `name`.
		void registerOp(std::string name, RenderFn fn) { renderers_[std::move(name)] = std::move(fn); }
		/// True if an op named `name` is registered.
		bool has(const std::string& name) const { return renderers_.count(name) != 0; }

		/// @throws std::invalid_argument if `op` isn't registered.
		RenderedFrame render(const std::string& op, const SourceT& source, const net::JsonValue& params) const
		{
			auto it = renderers_.find(op);
			if (it == renderers_.end()) throw std::invalid_argument("RendererRegistry: unknown op \"" + op + "\"");
			return it->second(source, params);
		}

	private:
		std::map<std::string, RenderFn> renderers_;
	};

	/// One independently addressable server-side render context -- its own render params and its own
	/// pending (not-yet-consumed) rendered output, each "latest wins" on update rather than queued (see
	/// this header's own top comment). Transport-agnostic: something else drives update()/render and
	/// does something with a taken pendingOutput.
	/// @ingroup viewer
	class Viewport
	{
	public:
		Viewport(std::string id, std::string op) : id_(std::move(id)), op_(std::move(op)) {}

		const std::string& id() const { return id_; }
		const std::string& op() const { return op_; }

		/// Replaces the current render params outright -- NOT a merge; the caller (the code parsing an
		/// updateViewport message) is responsible for merging onto the previous params first if that's
		/// the desired semantics. "Latest wins": a param set here before a previous one was ever read
		/// back via params() is simply gone, never queued.
		void setParams(net::JsonValue params)
		{
			std::lock_guard<std::mutex> lock(paramsMutex_);
			params_ = std::move(params);
		}
		net::JsonValue params() const
		{
			std::lock_guard<std::mutex> lock(paramsMutex_);
			return params_;
		}

		/// "Latest wins": overwrites whatever rendered output hadn't been sent yet -- the actual
		/// mechanism behind this project's "keep newest, drop stale" policy for outgoing frames.
		void setPendingOutput(RenderedFrame frame)
		{
			std::lock_guard<std::mutex> lock(outputMutex_);
			pendingOutput_ = std::move(frame);
			hasPendingOutput_ = true;
		}
		/// Takes (and clears) the pending output, if there is one.
		/// @return true if `out` was filled in; false if nothing has changed since the last take.
		bool takePendingOutput(RenderedFrame& out)
		{
			std::lock_guard<std::mutex> lock(outputMutex_);
			if (!hasPendingOutput_) return false;
			out = std::move(pendingOutput_);
			hasPendingOutput_ = false;
			return true;
		}

	private:
		std::string id_;
		std::string op_;
		mutable std::mutex paramsMutex_;
		net::JsonValue params_;
		std::mutex outputMutex_;
		RenderedFrame pendingOutput_;
		bool hasPendingOutput_ = false;
	};
}
