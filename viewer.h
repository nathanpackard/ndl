#pragma once
#include <cassert>
#include <cstdint>
#include <array>
#include <ostream>
#include <string>
#include <type_traits>
#include "image.h"

// Primitives for viewing an Image<T,DIM> as a grid of synchronized 2D
// slices -- one per pair of axes, each fixed along every other axis at a
// shared N-dimensional cursor position, generalizing the classic
// axial/coronal/sagittal synchronized 3-plane medical-image viewer to
// arbitrary DIM. A sibling of convolution.h/morphology.h/visualize.h, not
// part of image.h's core Image object -- #include this directly if you use
// it.
//
// This header only ever produces *data* (a 2D view of an existing buffer,
// or a compact binary export of a whole volume) -- it deliberately has no
// rendering, no windowing UI, no interactivity, and no platform/GUI
// dependency of any kind. The actual interactive viewer (crosshair
// placement, drag-to-navigate, pairwise-view layout, Canvas2D rendering) is
// ndl/web/ndlviewer.js, a standalone JS file with no C++ dependency at
// runtime -- it only needs the binary format write_web_volume() produces
// to stay stable. Keeping the split this strict is deliberate: ndl stays
// header-only C++ with no GUI-toolkit dependency of any kind (in
// particular, no Qt), so "view this N-D image interactively" has to mean
// "export data a browser can render," not a native widget.
/// @ingroup viewer

namespace ndl
{
	namespace detail
	{
		// Recursive core of pairwise_slice(): walks CurAxis from DIM-1 down
		// to -1. At each step, an axis that isn't one of the two being kept
		// gets sliced out at its fixed cursor position; a kept axis is left
		// untouched and the recursion just moves to the next lower axis.
		//
		// This needs no runtime index bookkeeping despite Image::slice()
		// renumbering every dimension above the one removed: because axes
		// are processed strictly top-down and each removal happens
		// immediately (not batched), by induction every axis being
		// processed still sits at exactly its own original index in the
		// image *at the moment it's processed* -- nothing below the axis
		// currently being looked at has been touched yet (we haven't
		// reached it), and Image::slice() only ever shifts dimensions
		// *above* the one it removes. So `img.slice(CurAxis, cursor[CurAxis])`
		// is always correct as written, with no separate "current position"
		// tracking needed. The two kept axes end up in the result ordered by
		// their own original index (smaller -> dim 0, larger -> dim 1),
		// regardless of the order AxisA/AxisB were written in by the caller
		// -- see pairwise_slice()'s own doc comment.
		// DIM is threaded through explicitly at every call (never deduced)
		// so it's never simultaneously deduced from both img (Image<T,D>,
		// where the dimension count is an `int` template parameter) and
		// cursor (std::array<int,DIM>, whose size is a `std::size_t`
		// template parameter): GCC/Clang both reject deducing one
		// template parameter from two argument positions whose "kind"
		// (int vs. size_t) differs, even though the values necessarily
		// agree -- so only T and D (both deduced purely from img, a single
		// consistent source) are ever left to deduction here.
		template<int CurAxis, int AxisA, int AxisB, int DIM, class T, int D>
		Image<T, 2> pairwise_slice_recurse(const Image<T, D>& img, const std::array<int, DIM>& cursor)
		{
			if constexpr (CurAxis < 0)
			{
				static_assert(D == 2, "ndl::pairwise_slice(): internal error -- recursion reached CurAxis<0 without D==2. This should be unreachable given AxisA/AxisB's own range/distinctness static_asserts in pairwise_slice() itself.");
				return img;
			}
			else if constexpr (CurAxis == AxisA || CurAxis == AxisB)
			{
				return pairwise_slice_recurse<CurAxis - 1, AxisA, AxisB, DIM>(img, cursor);
			}
			else
			{
				return pairwise_slice_recurse<CurAxis - 1, AxisA, AxisB, DIM>(img.slice(CurAxis, cursor[CurAxis]), cursor);
			}
		}
	}

	// Extracts the 2D slice of img spanning axes AxisA/AxisB, with every
	// other axis fixed at cursor's value for it -- the core primitive
	// behind viewing an N-D image as a grid of pairwise-axis planes. A
	// genuine zero-copy Image<T,2> *view* over img's own memory (built
	// entirely from chained Image::slice() calls), matching Image's own
	// "never own memory" design -- exactly like view()/slice()/swap_axes()
	// themselves, just composed to remove DIM-2 axes in one call instead of
	// writing that chain out by hand.
	//
	// AxisA/AxisB are template parameters, not runtime arguments: which two
	// axes a given view shows is fixed at compile time (e.g. one call site
	// per panel of a pairwise-view grid), which is what makes the
	// recursive slice-chain above resolvable without any runtime
	// dimension-count dispatch. for_each_axis_pair() below drives every
	// $\binom{DIM}{2}$ pair of a compile-time-known DIM without hand-listing
	// them.
	//
	// Result axis order: dim 0 of the returned image is always
	// min(AxisA,AxisB)'s data, dim 1 is always max(AxisA,AxisB)'s --
	// regardless of which order AxisA/AxisB were written in at the call
	// site (pairwise_slice<2,0>(...) and pairwise_slice<0,2>(...) return
	// the same view). Use Image::swap_axes() on the result first if you
	// need the other order.
	/// Extracts the zero-copy 2D view of `img` spanning axes `AxisA`/`AxisB`, fixed elsewhere at `cursor`.
	/// @tparam AxisA One axis to keep (any order relative to AxisB -- see the result-order note above).
	/// @tparam AxisB The other axis to keep; must differ from AxisA.
	/// @tparam T     Element type (deduced from `img`).
	/// @tparam DIM   Source dimensionality (deduced from `img`).
	/// @tparam N     `cursor`'s own length (deduced from `cursor`; must equal DIM, checked at compile time -- kept as a separate template parameter from DIM rather than reusing it, since deducing one parameter from both an `Image<T,DIM>` argument and a `std::array<int,DIM>` argument at once isn't reliably possible: the two arguments' template parameters differ in kind (`int` vs. `std::size_t`) even when their values agree).
	/// @param  img    Source image.
	/// @param  cursor Fixed coordinate for every axis other than AxisA/AxisB (AxisA/AxisB's own entries are unused).
	/// @return A zero-copy Image<T,2> view over img's own memory.
	/// @ingroup viewer
	template<int AxisA, int AxisB, class T, int DIM, std::size_t N>
	Image<T, 2> pairwise_slice(const Image<T, DIM>& img, const std::array<int, N>& cursor)
	{
		static_assert(AxisA != AxisB, "ndl::pairwise_slice() requires two distinct axes");
		static_assert(AxisA >= 0 && AxisA < DIM && AxisB >= 0 && AxisB < DIM, "ndl::pairwise_slice() axes must be in [0,DIM)");
		static_assert(N == static_cast<std::size_t>(DIM), "ndl::pairwise_slice() requires cursor to have exactly DIM entries, one per axis of img");
		return detail::pairwise_slice_recurse<DIM - 1, AxisA, AxisB, DIM>(img, cursor);
	}

	namespace detail
	{
		template<int I, int J, int DIM, class Fn>
		void for_each_axis_pair_impl(Fn&& fn)
		{
			if constexpr (I >= DIM)
			{
				// done: I has walked past the last row
			}
			else if constexpr (J >= DIM)
			{
				for_each_axis_pair_impl<I + 1, I + 2, DIM>(std::forward<Fn>(fn));
			}
			else
			{
				fn(std::integral_constant<int, I>{}, std::integral_constant<int, J>{});
				for_each_axis_pair_impl<I, J + 1, DIM>(std::forward<Fn>(fn));
			}
		}
	}

	// Calls fn once for every pair (i,j), i<j, of a DIM-dimensional image's
	// axes -- i.e. every panel of a pairwise-slice grid -- without having
	// to hand-list $\binom{DIM}{2}$ pairs at each call site. Compile-time
	// unrolled (a fixed, small number of calls for any real DIM), so fn
	// receives i/j as std::integral_constant<int,...>, not plain ints: that
	// makes them usable as pairwise_slice<...>()'s own template arguments
	// inside fn, e.g.:
	//
	//   for_each_axis_pair<DIM>([&](auto i, auto j) {
	//       auto view = pairwise_slice<i(), j()>(img, cursor);
	//       ...
	//   });
	//
	/// Calls `fn(i, j)` once per axis pair (i<j) of a DIM-dimensional image, as compile-time `std::integral_constant<int,...>` values usable directly as template arguments (e.g. to pairwise_slice()).
	/// @tparam DIM Dimensionality to enumerate axis pairs for; must be >= 2.
	/// @tparam Fn  Callable as `fn(std::integral_constant<int,I>, std::integral_constant<int,J>)`.
	/// @param  fn  Called once per pair, in row-major (i, then j) order.
	/// @ingroup viewer
	template<int DIM, class Fn>
	void for_each_axis_pair(Fn&& fn)
	{
		static_assert(DIM >= 2, "ndl::for_each_axis_pair() needs at least 2 dimensions to form a pair");
		detail::for_each_axis_pair_impl<0, 1, DIM>(std::forward<Fn>(fn));
	}

	// Clamped linear rescale of src into dst: lo maps to 0, hi maps to 255,
	// linearly in between, with values outside [lo,hi] clamped rather than
	// wrapping -- the standard "window/level" prep step before displaying
	// an arbitrary-range image (float CT attenuation values, 16-bit
	// scientific imagery, ...) as an 8-bit grayscale texture. A degenerate
	// window (hi<=lo) maps every value to 0 rather than dividing by zero.
	// dst's value_type just needs to be assignable from the [0,255]
	// result -- uint8_t is the expected case, but nothing here requires it.
	/// Clamped linear rescale of `src` into `dst`: `lo`->0, `hi`->255, values outside `[lo,hi]` clamped.
	/// @tparam SrcImageT Source's minimal-interface image type; its value_type must be arithmetic.
	/// @tparam DstImageT Destination's minimal-interface image type (same DIM as SrcImageT).
	/// @param  src Source image.
	/// @param  dst Destination; must already exist with `src`'s own extent.
	/// @param  lo  Source value mapped to 0.
	/// @param  hi  Source value mapped to 255. A degenerate window (`hi<=lo`) maps every value to 0.
	/// @ingroup viewer
	template<class SrcImageT, class DstImageT>
	void normalize_to_u8(const SrcImageT& src, DstImageT& dst, double lo, double hi)
	{
		static_assert(std::is_arithmetic_v<typename SrcImageT::value_type>, "ndl::normalize_to_u8() requires an arithmetic source value_type -- not valid for e.g. std::complex<T>");
		assert(dst.extent() == src.extent());
		double range = hi - lo;
		for (const auto& coord : src.coordinates())
		{
			double normalized = 0.0;
			if (range > 0.0)
			{
				double v = static_cast<double>(src.at(coord));
				normalized = (v - lo) / range;
				normalized = normalized < 0.0 ? 0.0 : (normalized > 1.0 ? 1.0 : normalized);
			}
			dst.at(coord) = static_cast<typename DstImageT::value_type>(normalized * 255.0 + 0.5);
		}
	}

	namespace detail
	{
		// Dtype codes for the write_web_volume() binary format, one byte,
		// matched by ndlviewer.js's own parser -- see that file for the
		// JS-side counterpart. Only the types a JS typed array can
		// represent directly are covered (no std::complex, no bool --
		// those don't have a sane browser-side representation without a
		// conversion step this header deliberately leaves to the caller,
		// the same way image_io::save() leaves format-specific
		// conversions to the caller rather than guessing).
		template<class T> struct WebDTypeCode;
		template<> struct WebDTypeCode<uint8_t> { static constexpr uint8_t value = 0; };
		template<> struct WebDTypeCode<int8_t> { static constexpr uint8_t value = 1; };
		template<> struct WebDTypeCode<uint16_t> { static constexpr uint8_t value = 2; };
		template<> struct WebDTypeCode<int16_t> { static constexpr uint8_t value = 3; };
		template<> struct WebDTypeCode<uint32_t> { static constexpr uint8_t value = 4; };
		template<> struct WebDTypeCode<int32_t> { static constexpr uint8_t value = 5; };
		template<> struct WebDTypeCode<float> { static constexpr uint8_t value = 6; };
		template<> struct WebDTypeCode<double> { static constexpr uint8_t value = 7; };

		template<class T, class = void> struct HasWebDTypeCode : std::false_type {};
		template<class T> struct HasWebDTypeCode<T, std::void_t<decltype(WebDTypeCode<T>::value)>> : std::true_type {};
	}

	// Optional per-axis physical calibration for write_web_volume()/
	// embedNDViewer() -- deliberately NOT a member of Image<T,DIM> itself.
	// Image stays the minimal, cheap, non-owning core type this whole
	// library is built on (image.h's own top comment: construction,
	// view()/slice()/swap_axes(), arithmetic, reductions -- nothing else),
	// passed by value/reference through essentially every function here;
	// most images that exist (FFT magnitude, a label mask, a distance
	// transform, a difference image) have no physical unit at all, so
	// forcing DIM-many spacing values and unit strings onto every Image
	// ever constructed would be dead weight for the common case. Physical
	// calibration is instead an optional, external annotation supplied
	// only where it's actually meaningful -- passed alongside the Image to
	// just the code that cares (here, and equally NRRD/DICOM readers that
	// already parse spacing headers, e.g. imageIO/NRRD's own
	// element_spacing) -- the same "toolkit layered on top, not baked
	// into the core type" relationship convolution.h/morphology.h/
	// viewer.h itself already have to Image.
	//
	// spacing[k] is the physical size of one voxel along axis k (so a
	// volume's physical extent along axis k is spacing[k]*extent[k]);
	// unit[k] is a short label ("mm", "s", ...) shared by every value
	// expressed in spacing[k]'s own units. unit[k] left empty means axis k
	// has no physical calibration at all (spacing[k] is then ignored) --
	// different axes are free to mix calibrated and uncalibrated, or use
	// entirely different units from each other (e.g. 3 spatial axes in
	// "mm" alongside a time axis in "s", exactly demo/nd_viewer's own
	// space+time volume) -- see ndlviewer.js's own comment on
	// computePerAxisPixelSizes() for how the viewer resolves that mix into
	// one pixel size per axis.
	/// Optional per-axis physical calibration (voxel spacing + unit label) for write_web_volume()/embedNDViewer().
	/// @tparam DIM Dimensionality; must match the Image this is passed alongside.
	/// @ingroup viewer
	template<int DIM>
	struct VoxelSpacing
	{
		std::array<double, DIM> spacing{};      ///< Physical size of one voxel along each axis. Ignored for any axis whose `unit` entry is empty.
		std::array<std::string, DIM> unit{};    ///< Per-axis unit label (e.g. "mm", "m", "s"); empty means axis k is not physically calibrated.
	};

	// Writes img to os in a small, self-describing binary format designed
	// to be trivially parsed in a few lines of browser JS (DataView over
	// an ArrayBuffer) -- not a general interchange format the way NRRD/DICOM
	// are (imageIO.h's formats are all meant to round-trip through other
	// tools too); this one only has to satisfy ndlviewer.js, written
	// alongside it, so it stays deliberately minimal:
	//
	//   bytes 0-3   magic "NDLV"
	//   byte  4     format version (1, or 2 if `spacing` is non-null)
	//   byte  5     dtype code (detail::WebDTypeCode<T>::value)
	//   byte  6     DIM (number of axes)
	//   bytes 7..   DIM x uint32 extent, one per axis, little-endian
	//   -- version 2 only, immediately after the extents --
	//               DIM x float64 spacing, one per axis, little-endian
	//               DIM x [uint8 byte-length][unit bytes, ASCII] --
	//               length 0 means that axis has no unit
	//   remaining   raw element data, native byte order, in Image's own
	//               default packed layout (axis 0 fastest-varying --
	//               i.e. element (i0,i1,...) is at flat offset
	//               i0 + i1*extent[0] + i2*extent[0]*extent[1] + ...)
	//
	// version stays 1 (byte-for-byte identical to before VoxelSpacing
	// existed) whenever `spacing` is null -- the overwhelming majority of
	// callers, who have no physical calibration to offer, pay nothing for
	// this feature existing. Written assuming a little-endian host, which
	// every realistic target for this (a browser's own JS engine, and
	// every mainstream CPU architecture qvol/ndl actually run on) already
	// is -- not byte-swapped defensively, the same way the rest of ndl
	// doesn't defend against configurations that don't occur in practice.
	/// Writes `img` to `os` in ndlviewer.js's binary volume format (see this function's own comment for the exact byte layout).
	/// @tparam T       Element type; must be one of the types ndlviewer.js can read directly (uint8_t/int8_t/uint16_t/int16_t/uint32_t/int32_t/float/double) -- convert first (e.g. via normalize_to_u8()) if `img` holds something else.
	/// @tparam DIM     Dimensionality; must fit in a byte (DIM <= 255, never a real constraint in practice).
	/// @param  img     Source image.
	/// @param  os      Destination stream, opened in binary mode.
	/// @param  spacing Optional per-axis physical calibration; when non-null, written as a version-2 volume (see this function's own comment) so the viewer can show physical coordinates and size panels proportional to physical extent, not just voxel count.
	/// @ingroup viewer
	template<class T, int DIM>
	void write_web_volume(const Image<T, DIM>& img, std::ostream& os, const VoxelSpacing<DIM>* spacing = nullptr)
	{
		static_assert(detail::HasWebDTypeCode<T>::value, "ndl::write_web_volume() requires T to be one of uint8_t/int8_t/uint16_t/int16_t/uint32_t/int32_t/float/double -- convert first (e.g. via normalize_to_u8()) if your image holds something else, like std::complex or a wider/narrower type ndlviewer.js has no reader for");
		static_assert(DIM >= 1 && DIM <= 255, "ndl::write_web_volume() requires 1 <= DIM <= 255 (DIM is written as a single byte)");

		os.write("NDLV", 4);
		const uint8_t version = spacing ? 2 : 1;
		os.write(reinterpret_cast<const char*>(&version), 1);
		const uint8_t dtype = detail::WebDTypeCode<T>::value;
		os.write(reinterpret_cast<const char*>(&dtype), 1);
		const uint8_t dim = static_cast<uint8_t>(DIM);
		os.write(reinterpret_cast<const char*>(&dim), 1);
		for (int i = 0; i < DIM; i++)
		{
			const uint32_t extent = static_cast<uint32_t>(img.extent()[i]);
			os.write(reinterpret_cast<const char*>(&extent), sizeof(extent));
		}
		if (spacing)
		{
			for (int i = 0; i < DIM; i++)
			{
				const double sp = spacing->spacing[i];
				os.write(reinterpret_cast<const char*>(&sp), sizeof(sp));
			}
			for (int i = 0; i < DIM; i++)
			{
				const std::string& u = spacing->unit[i];
				const uint8_t len = static_cast<uint8_t>(u.size() > 255 ? 255 : u.size());
				os.write(reinterpret_cast<const char*>(&len), 1);
				os.write(u.data(), len);
			}
		}
		for (auto it = img.begin(); it != img.end(); ++it)
		{
			const T value = *it;
			os.write(reinterpret_cast<const char*>(&value), sizeof(T));
		}
	}
}
