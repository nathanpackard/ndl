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

#include "image.h"
#include "convolution.h"
#include "net/json.h"

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
	}

	// The one v1 op. params (a net::JsonValue object):
	//   axisI, axisJ    (required) -- which two of source's own axes this slice varies over.
	//   colorAxis       (optional) -- if given, reads 3 values per position (channels 0,1,2 of this
	//                   axis) and composites true RGB, the same convention ndlviewer.js's own
	//                   colorAxis option uses.
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

		int channels = colorAxis >= 0 ? 3 : 1;
		std::vector<uint8_t> cropPixels((std::size_t)cropW * cropH * channels);
		std::size_t p = 0;
		for (int y = 0; y < cropH; y++)
			for (int x = 0; x < cropW; x++)
			{
				std::array<int, DIM> coord = base;
				coord[axisI] = cropMinI + x;
				coord[axisJ] = cropMinJ + y;
				if (colorAxis >= 0)
				{
					for (int ch = 0; ch < 3; ch++)
					{
						coord[colorAxis] = ch;
						double v = (double)source.at(coord);
						cropPixels[p++] = range > 0 ? (uint8_t)clampInt((int)std::lround(((v - windowMin) / range) * 255), 0, 255) : 0;
					}
				}
				else
				{
					double v = (double)source.at(coord);
					cropPixels[p++] = range > 0 ? (uint8_t)clampInt((int)std::lround(((v - windowMin) / range) * 255), 0, 255) : 0;
				}
			}

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
