#pragma once
#include <cassert>
#include <cmath>
#include <array>
#include <vector>
#include <algorithm>
#include <type_traits>
#include "image.h"

// The visualization toolkit: bar_chart()/heatmap(), as free functions over
// any minimal-interface image type. A sibling of fft.h/matrix.h/
// convolution.h/morphology.h/histogram.h/distance_transform.h/
// summed_area_table.h, not part of image.h's core Image object -- #include
// this directly if you use it.
//
// Exists specifically so nothing in this library needs its own one-off
// ASCII-art rendering: histogram.h's Histogram<1>/Histogram<2> render
// themselves via histogram_image(), which is a thin wrapper around
// bar_chart()/heatmap() below rather than separate drawing code -- and
// anything else that ever needs to visualize a 1D or 2D array of numbers
// (not just a Histogram) can reach for the exact same two functions.

namespace ndl
{
	// Renders any 1D minimal-interface numeric array as a vertical bar
	// chart: one bar per element, its height scaled to the array's own max
	// value (an all-<=0 array draws no bars at all, not a divide-by-zero).
	// dst must already exist, sized for the chart's own pixel dimensions --
	// its extent's *last two* axes are read as (width, height) and every
	// leading axis as a channel, so this works for a single-channel
	// Image<T,2> as well as a channel-interleaved Image<T,3>: every channel
	// gets the same bar/background value at a given (x,y), so the chart
	// reads as neutral grey/white rather than tinted on a multi-channel
	// destination.
	/// Renders src as a vertical bar chart into dst.
	/// @tparam SrcImageT Any minimal-interface 1D image type; its value_type must be arithmetic.
	/// @tparam DstImageT Any minimal-interface image type whose last two axes are (width,height); its value_type must be arithmetic.
	/// @param  src           Source values, one bar each.
	/// @param  dst           Destination; must already exist, sized for the chart.
	/// @param  barValue      Pixel value written for a bar. Defaults to 1 (the same generic on/off convention threshold() defaults to) -- pass e.g. 255 for a directly-viewable 8-bit image.
	/// @param  backgroundValue Pixel value written where there's no bar. Defaults to 0.
	/// @ingroup visualize
	template<class SrcImageT, class DstImageT>
	void bar_chart(const SrcImageT& src, DstImageT& dst, typename DstImageT::value_type barValue = typename DstImageT::value_type(1), typename DstImageT::value_type backgroundValue = typename DstImageT::value_type(0))
	{
		using SrcT = typename SrcImageT::value_type;
		using DstT = typename DstImageT::value_type;
		static_assert(std::is_arithmetic_v<SrcT>, "ndl::bar_chart() requires an arithmetic source value_type -- not valid for e.g. std::complex<T>");
		static_assert(std::is_arithmetic_v<DstT>, "ndl::bar_chart() requires an arithmetic destination value_type -- not valid for e.g. std::complex<T>");

		int n = src.extent()[0];
		assert(n > 0);

		auto dstExtent = dst.extent();
		constexpr int DstDIM = std::tuple_size<decltype(dstExtent)>::value;
		static_assert(DstDIM >= 2, "ndl::bar_chart() needs a destination with at least 2 axes (width, height)");
		int W = dstExtent[DstDIM - 2], H = dstExtent[DstDIM - 1];
		assert(W > 0 && H > 0);

		double maxVal = 0;
		for (const auto& c : src.coordinates()) maxVal = std::max(maxVal, (double)src.at(c));

		// Each column's own bar height, computed once up front rather than
		// per destination pixel (which would otherwise re-derive "which
		// bin does column x belong to" once per channel too). Bin i's
		// column span is computed from i and i+1 (not i and a fixed bar
		// width added on) so n bars always exactly tile the full width W,
		// with no rounding gaps or overlaps between them.
		std::vector<int> barHeightAtX(W, 0);
		if (maxVal > 0)
		{
			for (int i = 0; i < n; i++)
			{
				double v = (double)src.at({ i }) / maxVal;
				if (v <= 0) continue;
				int barHeight = (int)(v * H);
				int x0 = (int)((long long)i * W / n);
				int x1 = (int)((long long)(i + 1) * W / n) - 1;
				if (x1 < x0) x1 = x0;
				for (int x = x0; x <= x1 && x < W; x++) barHeightAtX[x] = barHeight;
			}
		}

		// A single pass over dst's own coordinates (every channel, every
		// pixel) rather than only ever addressing channel 0: `at()` needs
		// a full DstDIM-length coordinate, and leaving every leading
		// (channel) component at its default 0 would draw the bars into
		// just one channel of a multi-channel image instead of all of them.
		for (const auto& coord : dst.coordinates())
		{
			int x = coord[DstDIM - 2], y = coord[DstDIM - 1];
			dst.at(coord) = (y >= H - barHeightAtX[x]) ? barValue : backgroundValue;
		}
	}

	// Renders any 2D minimal-interface numeric array as a greyscale
	// intensity image: one pixel per element, scaled to the array's own
	// max value -- the 2D counterpart to bar_chart() above, same "scale to
	// max, write into a caller-sized output" shape. dst's last two axes
	// are read as (width, height), same convention bar_chart() uses; every
	// leading axis is a channel, written identically so the image reads as
	// neutral greyscale rather than tinted on a multi-channel destination.
	/// Renders src as a greyscale heatmap into dst.
	/// @tparam SrcImageT Any minimal-interface 2D image type; its value_type must be arithmetic.
	/// @tparam DstImageT Any minimal-interface image type whose last two axes are (width,height), matching src's own; its value_type must be arithmetic.
	/// @param  src           Source values.
	/// @param  dst           Destination; must already exist, sized so its last two axes match src's own extent.
	/// @param  peakValue     Pixel value written for src's own maximum value (everything else scales linearly toward 0). Defaults to 1 -- pass e.g. 255 for a directly-viewable 8-bit image.
	/// @ingroup visualize
	template<class SrcImageT, class DstImageT>
	void heatmap(const SrcImageT& src, DstImageT& dst, typename DstImageT::value_type peakValue = typename DstImageT::value_type(1))
	{
		using SrcT = typename SrcImageT::value_type;
		using DstT = typename DstImageT::value_type;
		static_assert(std::is_arithmetic_v<SrcT>, "ndl::heatmap() requires an arithmetic source value_type -- not valid for e.g. std::complex<T>");
		static_assert(std::is_arithmetic_v<DstT>, "ndl::heatmap() requires an arithmetic destination value_type -- not valid for e.g. std::complex<T>");

		auto dstExtent = dst.extent();
		constexpr int DstDIM = std::tuple_size<decltype(dstExtent)>::value;
		static_assert(DstDIM >= 2, "ndl::heatmap() needs a destination with at least 2 axes (width, height)");
		assert(dstExtent[DstDIM - 2] == src.extent()[0] && dstExtent[DstDIM - 1] == src.extent()[1]);

		double maxVal = 0;
		for (const auto& c : src.coordinates()) maxVal = std::max(maxVal, (double)src.at(c));

		// Same reasoning as bar_chart() above: walk dst's own coordinates
		// (every channel, every pixel), re-deriving the (x,y) source
		// lookup each time, rather than only ever writing channel 0.
		for (const auto& coord : dst.coordinates())
		{
			std::array<int, 2> srcCoord{ coord[DstDIM - 2], coord[DstDIM - 1] };
			double v = maxVal <= 0 ? 0.0 : (double)src.at(srcCoord) / maxVal;
			dst.at(coord) = (DstT)(v * (double)peakValue);
		}
	}

	namespace detail
	{
		// h,s,v each in [0,1] -> r,g,b each in [0,1]. The standard sector-based
		// HSV->RGB conversion (identical in every graphics text/library);
		// lives here rather than in mathHelpers.h since flow_to_color() below
		// is its only caller.
		inline void hsvToRgb(double h, double s, double v, double& r, double& g, double& b)
		{
			double i = std::floor(h * 6.0);
			double f = h * 6.0 - i;
			double p = v * (1.0 - s);
			double q = v * (1.0 - f * s);
			double t = v * (1.0 - (1.0 - f) * s);
			switch (((int)i % 6 + 6) % 6)
			{
				case 0: r = v; g = t; b = p; break;
				case 1: r = q; g = v; b = p; break;
				case 2: r = p; g = v; b = t; break;
				case 3: r = p; g = q; b = v; break;
				case 4: r = t; g = p; b = v; break;
				default: r = v; g = p; b = q; break;
			}
		}
	}

	// Visualizes a 2D flow field (the {2,W,H} representation optical_flow.h's
	// lucas_kanade_flow()/feature_detection.h's sift_flow() both produce) as
	// a color image: hue encodes direction (a full color wheel over the
	// angle atan2(dy,dx)), brightness encodes magnitude (scaled to the
	// field's own max, same "scale to max" convention as heatmap()) -- the
	// standard way to make a 2D vector field readable at a glance in a
	// single image, the same convention the Middlebury optical-flow
	// benchmark's own reference visualizations use.
	//
	// 2D-only, unlike bar_chart()/heatmap() above: hue is a single angle,
	// which only means "direction" for a 2-component vector -- there's no
	// analogous single-angle encoding for a 3+ component displacement (the
	// same reason optical_flow.h's to_complex()/from_complex() are 2D-only
	// too). For any other DIM, visualize each component separately instead
	// -- flow.slice(0,axis) is an ordinary DIM-dimensional numeric array,
	// so heatmap() already renders it directly, no flow-specific code
	// needed for that case.
	/// Visualizes a 2D flow field as a color wheel image: hue = direction, brightness = magnitude (scaled to the field's own max).
	/// @tparam SrcImageT Any minimal-interface image type with a leading component axis of size 2 (e.g. lucas_kanade_flow()'s own output).
	/// @tparam DstImageT Any minimal-interface image type with a leading channel axis of size 3 (R,G,B), matching src's own spatial extent.
	/// @param  flow      Source flow field, extent {2, W, H}.
	/// @param  dst       Destination; must already exist, extent {3, W, H}.
	/// @param  peakValue Pixel value at full brightness (the field's own max magnitude). Defaults to 1 -- pass e.g. 255 for a directly-viewable 8-bit image.
	/// @ingroup visualize
	template<class SrcImageT, class DstImageT>
	void flow_to_color(const SrcImageT& flow, DstImageT& dst, typename DstImageT::value_type peakValue = typename DstImageT::value_type(1))
	{
		using DstT = typename DstImageT::value_type;
		static_assert(std::is_arithmetic_v<DstT>, "ndl::flow_to_color() requires an arithmetic destination value_type -- not valid for e.g. std::complex<T>");

		auto flowExtent = flow.extent();
		assert(flowExtent[0] == 2);
		auto dstExtent = dst.extent();
		assert(dstExtent[0] == 3 && dstExtent[1] == flowExtent[1] && dstExtent[2] == flowExtent[2]);

		auto dx = flow.slice(0, 0);
		auto dy = flow.slice(0, 1);

		double maxMag = 0;
		for (const auto& coord : dx.coordinates())
		{
			double vx = dx.at(coord), vy = dy.at(coord);
			maxMag = std::max(maxMag, std::sqrt(vx * vx + vy * vy));
		}

		for (const auto& coord : dx.coordinates())
		{
			double vx = dx.at(coord), vy = dy.at(coord);
			double mag = std::sqrt(vx * vx + vy * vy);
			double hue = (std::atan2(vy, vx) + M_PI) / (2.0 * M_PI); // [0,1)
			double value = maxMag > 0 ? mag / maxMag : 0.0;

			double r, g, b;
			detail::hsvToRgb(hue, 1.0, value, r, g, b);

			dst.at(std::array<int, 3>{0, coord[0], coord[1]}) = static_cast<DstT>(r * (double)peakValue);
			dst.at(std::array<int, 3>{1, coord[0], coord[1]}) = static_cast<DstT>(g * (double)peakValue);
			dst.at(std::array<int, 3>{2, coord[0], coord[1]}) = static_cast<DstT>(b * (double)peakValue);
		}
	}
}
