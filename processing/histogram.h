#pragma once
#include <cassert>
#include <cmath>
#include <vector>
#include <array>
#include <string>
#include <cstddef>
#include <ostream>
#include <iomanip>
#include <algorithm>
#include <type_traits>
#include "../image.h"
#include "visualize.h"

// The histogram toolkit: Histogram<VDIM>, histogram_equalize(), and
// histogram_image(), as free functions/a class built on any minimal-
// interface image type. A sibling of fft.h/matrix.h/convolution.h/
// morphology.h/distance_transform.h/summed_area_table.h/visualize.h, not
// part of image.h's core Image object -- #include this directly if you
// use it.

namespace ndl
{
	// Bins the joint distribution of VDIM co-occurring scalar values --
	// VDIM==1 is the ordinary single-channel histogram (what
	// otsu_threshold() in morphology.h builds), VDIM==2/3/... are joint
	// histograms (e.g. the joint distribution of two channels' values at
	// each pixel, for texture/co-occurrence analysis). Generalizes over
	// VDIM the same way Image<T,DIM> generalizes over spatial dimension
	// DIM -- and is in fact backed by one, storing its VDIM-dimensional
	// grid of bin counts as an OwnedImage<std::size_t,VDIM> rather than
	// hand-rolled storage.
	//
	// Each of the VDIM value axes has its own bin count and its own [lo,hi]
	// range; bucketOfAxis() maps a value on axis i into [0,bins[i]) by
	// linear interpolation, clamped at both ends (closed-inclusive at hi:
	// a value exactly at hi lands in the last bin, not one past it). An
	// axis whose lo==hi (every sample on it is the same single value) is
	// degenerate -- rather than being a special case callers need to avoid,
	// every sample on that axis simply lands in bucket 0 (nothing divides
	// by its zero range), and every other bucket on it stays empty.
	/// @ingroup histogram
	template<int VDIM>
	class Histogram
	{
	public:
		// The general entry point every other constructor below reduces
		// to: `coords` is walked once, and `valuesAt(coord)` must return
		// that position's VDIM joint values as a std::array<double,VDIM>.
		// Takes explicit lo/hi per axis (not auto-ranged) -- see the
		// auto-ranging convenience constructors below for the common case.
		/// @tparam CoordRange  Any range of std::array<int,VDIM>-shaped coordinates (e.g. what Image::coordinates() returns).
		/// @tparam ValuesAtFn  Callable as `valuesAt(coord) -> std::array<double,VDIM>`.
		/// @param  bins        Bin count per value axis.
		/// @param  lo          Lower edge per value axis (inclusive).
		/// @param  hi          Upper edge per value axis (inclusive).
		/// @param  coords      Coordinates to visit, once each.
		/// @param  valuesAt    Produces the VDIM joint values sampled at a given coordinate.
		template<class CoordRange, class ValuesAtFn>
		Histogram(std::array<int, VDIM> bins, std::array<double, VDIM> lo, std::array<double, VDIM> hi, const CoordRange& coords, ValuesAtFn&& valuesAt)
			: bins_(bins), lo_(lo), hi_(hi), counts_(bins)
		{
			counts_ = std::size_t(0);
			for (const auto& coord : coords)
			{
				std::array<double, VDIM> values = valuesAt(coord);
				counts_.at(bucketOf(values))++;
				total_++;
			}
		}

		// Auto-ranged convenience: VDIM co-registered images of the same
		// concrete type (e.g. two channels slice()'d from the same photo),
		// each axis' [lo,hi] taken from that axis' own source's [min,max].
		// All VDIM sources must share the same extent (checked via assert);
		// srcs[0]'s own coordinates() drives the shared walk.
		/// @tparam ImageT Any minimal-interface image type; every element of `srcs` must be this same concrete type.
		/// @param  bins   Bin count per value axis.
		/// @param  srcs   One source per value axis, each contributing that axis' values -- e.g. `{&redChannel, &greenChannel}` for a 2D joint histogram.
		template<class ImageT>
		Histogram(std::array<int, VDIM> bins, const std::array<const ImageT*, VDIM>& srcs)
			: Histogram(bins, axisRanges(srcs, true), axisRanges(srcs, false), srcs[0]->coordinates(),
				// The coordinate here is srcs[0]'s own (spatial) coordinate
				// type -- e.g. std::array<int,2> for a 2D photo's channels --
				// which has nothing to do with VDIM (the number of *value*
				// axes being jointly binned); a generic lambda parameter
				// deduces whatever that actually is instead of assuming it
				// matches VDIM.
				[&srcs](const auto& coord) {
					std::array<double, VDIM> values;
					for (int i = 0; i < VDIM; i++) values[i] = static_cast<double>(srcs[i]->at(coord));
					return values;
				})
		{
			for (int i = 1; i < VDIM; i++) assert(srcs[i]->extent() == srcs[0]->extent());
		}

		// VDIM==1 convenience: a single image, a plain int bin count --
		// otsu_threshold()'s own call shape.
		/// @tparam ImageT Any minimal-interface image type.
		/// @param  src    Source image.
		/// @param  bins   Bin count. Defaults to 256 (classic 8-bit-imagery resolution).
		template<class ImageT, int V = VDIM, class = std::enable_if_t<V == 1>>
		Histogram(const ImageT& src, int bins = 256)
			: Histogram(std::array<int, 1>{bins}, std::array<const ImageT*, 1>{&src})
		{
		}

		/// Count in a given VDIM-dimensional bin.
		std::size_t count(std::array<int, VDIM> bin) const { return counts_.at(bin); }
		/// VDIM==1 convenience overload of count(), taking a plain bin index.
		template<int V = VDIM, class = std::enable_if_t<V == 1>>
		std::size_t count(int bin) const { return counts_.at(std::array<int, 1>{bin}); }

		/// Maps `value` into [0,bins[axis]) for one value axis, clamped at both ends. Degenerate axes (lo[axis]==hi[axis]) always return 0.
		int bucketOfAxis(int axis, double value) const
		{
			double range = hi_[axis] - lo_[axis];
			if (range <= 0.0) return 0;
			int b = (int)((value - lo_[axis]) / range * bins_[axis]);
			return b < 0 ? 0 : (b >= bins_[axis] ? bins_[axis] - 1 : b);
		}
		/// Maps a VDIM-tuple of joint values into its bin, one bucketOfAxis() call per axis.
		std::array<int, VDIM> bucketOf(std::array<double, VDIM> values) const
		{
			std::array<int, VDIM> bin;
			for (int i = 0; i < VDIM; i++) bin[i] = bucketOfAxis(i, values[i]);
			return bin;
		}
		/// VDIM==1 convenience overload of bucketOf(), taking/returning a plain scalar.
		template<int V = VDIM, class = std::enable_if_t<V == 1>>
		int bucketOf(double value) const { return bucketOfAxis(0, value); }

		/// Bin count per value axis.
		const std::array<int, VDIM>& bins() const { return bins_; }
		/// Lower edge per value axis.
		const std::array<double, VDIM>& lo() const { return lo_; }
		/// Upper edge per value axis.
		const std::array<double, VDIM>& hi() const { return hi_; }
		/// Total number of samples counted (sum over every bin).
		std::size_t total() const { return total_; }
		/// Largest single bin count -- the normalization factor operator<< uses to scale bars/shading to the display.
		std::size_t maxCount() const
		{
			std::size_t m = 0;
			for (const auto& coord : counts_.coordinates()) m = std::max(m, counts_.at(coord));
			return m;
		}
		/// The raw VDIM-dimensional grid of bin counts -- a real, minimal-interface image in its own right (an OwnedImage<std::size_t,VDIM>), usable directly with bar_chart()/heatmap() (visualize.h) or anything else that just wants "a VDIM-dimensional array of numbers", not Histogram-specific. histogram_image() below is exactly this.
		const OwnedImage<std::size_t, VDIM>& counts() const { return counts_; }

	private:
		// Per-axis [lo,hi], auto-ranged from each srcs[i]'s own [min,max] --
		// scanned directly via extent()/at()/coordinates() rather than
		// relying on Image's own min()/max() members, so this works for any
		// minimal-interface source, same reasoning as otsu_threshold()'s own
		// range-scan in morphology.h.
		template<class ImageT>
		static std::array<double, VDIM> axisRanges(const std::array<const ImageT*, VDIM>& srcs, bool wantLo)
		{
			std::array<double, VDIM> result;
			for (int i = 0; i < VDIM; i++)
			{
				auto coords = srcs[i]->coordinates();
				assert(!coords.empty());
				double lo = static_cast<double>(srcs[i]->at(coords[0]));
				double hi = lo;
				for (const auto& c : coords)
				{
					double v = static_cast<double>(srcs[i]->at(c));
					if (v < lo) lo = v;
					if (v > hi) hi = v;
				}
				result[i] = wantLo ? lo : hi;
			}
			return result;
		}

		std::array<int, VDIM> bins_;
		std::array<double, VDIM> lo_, hi_;
		std::size_t total_ = 0;
		OwnedImage<std::size_t, VDIM> counts_;
	};

	// ASCII visualization: VDIM==1 prints one horizontal bar per bin;
	// VDIM==2 prints a grid of the bins' own raw counts, zero-padded to a
	// fixed width (however many digits the largest count needs) so every
	// column lines up, y=0 at the top; VDIM>=3 has no sane 2D terminal
	// rendering, so it prints summary statistics only. Both branches save
	// and restore the stream's own fill character, format flags, AND
	// precision (three separate pieces of std::ostream state -- fill()/
	// flags()/precision() are each their own independent member, so all
	// three need their own save-and-restore; precision() specifically is
	// NOT part of what flags() captures, unlike what its name might
	// suggest). Skipping any one of them would leave a caller's *next*
	// `os << someOtherNumber` zero-padded, fixed-precision, or -- easy to
	// miss, since std::setprecision() persists on the stream just as
	// durably as std::setfill()/std::fixed do -- rounded to 1 significant
	// digit by surprise.
	/// @ingroup histogram
	template<int VDIM>
	std::ostream& operator<<(std::ostream& os, const Histogram<VDIM>& h)
	{
		auto originalFill = os.fill();
		auto originalFlags = os.flags();
		auto originalPrecision = os.precision();

		if constexpr (VDIM == 1)
		{
			std::size_t maxC = h.maxCount();
			constexpr int barWidth = 50;
			double lo = h.lo()[0], hi = h.hi()[0];
			int bins = h.bins()[0];
			for (int b = 0; b < bins; b++)
			{
				std::size_t c = h.count(b);
				int barLen = maxC == 0 ? 0 : (int)((double)c / (double)maxC * barWidth);
				double loEdge = lo + (hi - lo) * b / bins;
				double hiEdge = lo + (hi - lo) * (b + 1) / bins;
				os << std::fixed << std::setprecision(1)
				   << std::setw(10) << loEdge << " - " << std::setw(10) << hiEdge << " | "
				   << std::string(barLen, '#') << " " << c << "\n";
			}
		}
		else if constexpr (VDIM == 2)
		{
			std::size_t maxC = h.maxCount();
			int width = 1;
			for (std::size_t m = maxC; m >= 10; m /= 10) width++;
			for (int y = 0; y < h.bins()[1]; y++)
			{
				for (int x = 0; x < h.bins()[0]; x++)
					os << (x ? " " : "") << std::setfill('0') << std::setw(width) << h.count({ x, y });
				os << "\n";
			}
		}
		else
		{
			os << VDIM << "-dimensional histogram: bins={";
			for (int i = 0; i < VDIM; i++) os << (i ? "," : "") << h.bins()[i];
			os << "}, total=" << h.total() << "\n";
		}

		os.fill(originalFill);
		os.flags(originalFlags);
		os.precision(originalPrecision);
		return os;
	}

	// Renders a Histogram<1> or Histogram<2> as a real image -- a bar chart
	// or a heatmap respectively -- rather than ASCII. Not its own drawing
	// code: hist.counts() (above) is already a real VDIM-dimensional
	// minimal-interface image (an OwnedImage<std::size_t,VDIM>), so this is
	// a thin dispatch straight to bar_chart()/heatmap() (visualize.h),
	// which have no idea a Histogram exists and never need to. Exists
	// because ASCII art doesn't survive every rendering path unscathed
	// (this file's own generated Doxygen tutorial page is a real example:
	// text printed directly via operator<< above, without an intervening
	// low-indent line, could end up flattened into a prose paragraph by
	// docs/generate_tutorial.py's parser, losing its alignment entirely
	// once rendered to HTML) -- an actual image sidesteps that whole
	// problem, for this and anything else that ever needs to visualize a
	// 1D/2D array of numbers.
	/// Renders a Histogram<1> as a bar chart, or a Histogram<2> as a heatmap, into dst.
	/// @tparam VDIM      1 or 2 -- no sane 2D image rendering exists for a higher-dimensional joint histogram.
	/// @tparam DstImageT Any minimal-interface image type whose last two axes are (width,height); its value_type must be arithmetic.
	/// @param  hist          Histogram to render.
	/// @param  dst           Destination; must already exist, sized for the chart/heatmap.
	/// @param  foregroundValue Pixel value at a bar's full height (VDIM==1) or the histogram's own peak bin (VDIM==2). Defaults to 1 -- pass e.g. 255 for a directly-viewable 8-bit image.
	/// @param  backgroundValue VDIM==1 only: pixel value where there's no bar. Defaults to 0.
	/// @ingroup histogram
	template<int VDIM, class DstImageT>
	void histogram_image(const Histogram<VDIM>& hist, DstImageT& dst, typename DstImageT::value_type foregroundValue = typename DstImageT::value_type(1), typename DstImageT::value_type backgroundValue = typename DstImageT::value_type(0))
	{
		static_assert(VDIM == 1 || VDIM == 2, "ndl::histogram_image() only supports VDIM 1 or 2 -- no sane 2D image rendering exists for a higher-dimensional joint histogram");
		if constexpr (VDIM == 1)
			bar_chart(hist.counts(), dst, foregroundValue, backgroundValue);
		else
			heatmap(hist.counts(), dst, foregroundValue);
	}

	// Classic CDF-based histogram equalization: remaps src's values so their
	// cumulative distribution is closer to uniform across [lo,hi] (Histogram's
	// own auto-ranged [min,max]) -- the standard contrast-stretching operation
	// for an image whose values cluster in a narrow sub-range instead of using
	// the full range. Built directly on Histogram<1>, both for the bucketing
	// and for its own degenerate-range handling (a uniform source is simply
	// copied through unchanged, same as trying to "spread out" a single value
	// would be meaningless).
	/// Histogram-equalizes src into dst: remaps values so their cumulative distribution is closer to uniform.
	/// @tparam SrcImageT Any minimal-interface image type; its value_type must be arithmetic.
	/// @tparam DstImageT Any minimal-interface image type (same DIM as SrcImageT); may differ from SrcImageT (e.g. a different concrete container).
	/// @param  src    Source image.
	/// @param  dst    Destination; must already exist with `src`'s own extent.
	/// @param  bins   Histogram resolution used for the remapping. Defaults to 256.
	/// @ingroup histogram
	template<class SrcImageT, class DstImageT>
	void histogram_equalize(const SrcImageT& src, DstImageT& dst, int bins = 256)
	{
		using SrcT = typename SrcImageT::value_type;
		using DstT = typename DstImageT::value_type;
		static_assert(std::is_arithmetic_v<SrcT>, "ndl::histogram_equalize() requires an arithmetic value_type -- not valid for e.g. std::complex<T>");
		assert(dst.extent() == src.extent());

		Histogram<1> hist(src, bins);
		double lo = hist.lo()[0], hi = hist.hi()[0];
		if (hi <= lo) // degenerate: a single value has nothing to spread out
		{
			for (const auto& coord : src.coordinates()) dst.at(coord) = static_cast<DstT>(src.at(coord));
			return;
		}

		std::vector<std::size_t> cdf(bins);
		std::size_t running = 0;
		for (int b = 0; b < bins; b++) { running += hist.count(b); cdf[b] = running; }
		std::size_t total = hist.total();

		for (const auto& coord : src.coordinates())
		{
			int b = hist.bucketOf(static_cast<double>(src.at(coord)));
			double frac = static_cast<double>(cdf[b]) / static_cast<double>(total);
			dst.at(coord) = static_cast<DstT>(lo + (hi - lo) * frac);
		}
	}
}
