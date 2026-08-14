#pragma once
#include <vector>
#include <array>
#include <cassert>
#include <algorithm>
#include <type_traits>
#include "core.h"

namespace ndl
{
	namespace detail
	{
		// Holds nothing but the buffer, and exposes it as `data` (a plain
		// member, not inherited vector methods) -- deliberately NOT a
		// std::vector itself as OwnedImage's base, because private
		// inheritance only hides *accessibility*, not the *name*: an
		// ambiguous-lookup error between std::vector<T>::begin()/at() and
		// Image<T,DIM>::begin()/at() would still fire on every unqualified
		// call, regardless of which base is private. Wrapping the vector in
		// a one-member struct means this base contributes exactly one name
		// (`data`) to lookup, so nothing collides with Image's interface.
		template<class T>
		struct OwnedImageStorage
		{
			std::vector<T> data;
			explicit OwnedImageStorage(std::size_t n) : data(n) { }
		};
	}

	// Owns its own backing storage, unlike Image (which never allocates and
	// always operates on caller-supplied memory) -- a convenience for the
	// very common "I just need a fresh output buffer of this shape" case, so
	// call sites don't have to spell out their own std::vector<T> +
	// Image<T,DIM> pair by hand every time. Everything else about it *is* an
	// Image: it inherits the full public interface (erode(), view(), at(),
	// begin()/end(), ...), so an OwnedImage can be used anywhere an
	// Image<T,DIM> is expected.
	//
	// Move-only, not copyable: Image's data_/root_data_ are raw pointers
	// into the owned buffer, resolved once at construction, so copying the
	// buffer (which allocates a *new* one) would leave those pointers aimed
	// at the old, about-to-be-destroyed allocation. Moving is safe -- a
	// std::vector's move transfers ownership of its existing heap allocation
	// without relocating it, so the inherited pointers stay valid across a
	// move even though they were never told about it.
	//
	// Move-CONSTRUCTIBLE but not move-ASSIGNABLE: assignment is a different
	// story from construction. Image's own operator= writes element values
	// into memory that already exists (extent_/stride_/etc. are const, so an
	// existing Image can never be rebound to a different buffer or shape) --
	// exactly the semantics a shared "view" needs, but incompatible with
	// what a real move-assignment would require (fully replacing *this's
	// identity with another object's). Move-construction doesn't have this
	// problem, since it builds a brand new object rather than reassigning an
	// existing one, so it's supported normally.
	//
	// Privately inheriting from OwnedImageStorage<T> (rather than holding it
	// as a plain member) is the mechanism that makes any of this safe to
	// begin with: base classes finish constructing in declaration order,
	// before any of the derived class's own members do -- so
	// OwnedImageStorage<T> (listed first) is fully built, and its data
	// pointer already stable, by the time Image<T,DIM>'s own constructor
	// (listed second) runs and captures that pointer. A member-initializer-
	// list ordering trick can't substitute for this: a std::vector *member*
	// would still be uninitialized when a base class constructor needed it,
	// regardless of what order it's written in the initializer list -- base
	// subobjects always finish first.
	/// A move-only Image<T,DIM> subclass that allocates and owns its own backing
	/// storage, instead of requiring a pre-existing buffer. Not valid for T=bool --
	/// use PackedBitImage<DIM> for a compact owned boolean image instead.
	///
	/// Behaves exactly like an Image<T,DIM> in every other respect -- the full
	/// interface (view()/slice()/convolve()/at()/...) is inherited, unchanged.
	/// The only differences are lifetime-related: OwnedImage allocates its own
	/// buffer at construction and frees it at destruction, and is move-only
	/// (move-constructible but not move- or copy-assignable; see the file
	/// comment above the class for why).
	///
	/// @tparam T   Element type; same restrictions as Image<T,DIM>'s own,
	///             plus T=bool is rejected entirely (std::vector<bool> is
	///             bit-packed and has no .data() for Image to alias).
	/// @tparam DIM Number of dimensions (>= 1).
	/// @ingroup core_image
	template<class T, int DIM>
	class OwnedImage : private detail::OwnedImageStorage<T>, public Image<T, DIM>
	{
		// OwnedImageStorage<T>'s std::vector<T> data member is the whole
		// reason this class works at all (see the comment above) -- except
		// for T=bool, where std::vector<bool> is the infamous bit-packed
		// specialization that has no .data() to capture in the first place.
		// That already fails to compile (Storage::data.data() below simply
		// doesn't exist for T=bool), just from deep inside the constructor
		// initializer list; this puts the actual reason, and the fix, right
		// at the point of use instead.
		static_assert(!std::is_same_v<T, bool>, "OwnedImage<bool,DIM> doesn't work -- std::vector<bool> is bit-packed and has no .data() for Image to alias. Use PackedBitImage<DIM> instead for a compact owned boolean image.");
		using Storage = detail::OwnedImageStorage<T>;
	public:
		/// Allocates a fresh, zero-initialized buffer of the given shape.
		/// @param extent Shape: element count along each dimension.
		explicit OwnedImage(std::array<int, DIM> extent)
			: Storage(Image<T, DIM>::size(extent)), Image<T, DIM>(Storage::data.data(), extent)
		{ }

		// For the common case of a small kernel/grid with specific literal
		// values (e.g. a Sobel kernel), which a plain extent-only
		// OwnedImage can't express any more directly than the
		// std::vector-then-Image pair it's meant to replace -- this makes
		// it a single line instead: OwnedImage<double,2> sobelX({3,3},
		// {-1,0,1, -2,0,2, -1,0,1});
		/// Allocates a fresh buffer of the given shape, filled from `values` (row-major order).
		/// @param extent Shape: element count along each dimension.
		/// @param values Initial contents, in row-major (dimension 0 fastest) order; must have exactly `size(extent)` elements.
		OwnedImage(std::array<int, DIM> extent, std::initializer_list<T> values)
			: Storage(Image<T, DIM>::size(extent)), Image<T, DIM>(Storage::data.data(), extent)
		{
			assert(values.size() == Storage::data.size());
			std::copy(values.begin(), values.end(), Storage::data.begin());
		}

		// Deep copy of `source`, converting element type if needed -- the
		// owned equivalent of Image's own T*+source constructor, minus the
		// caller having to allocate a buffer for it first.
		template<class U>
		/// Deep-copies `source` into a freshly allocated buffer, converting element type if needed.
		/// @tparam U      source's element type (converted to T element-by-element).
		/// @param  source Image to copy from; this OwnedImage is allocated with its extent.
		explicit OwnedImage(const Image<U, DIM>& source)
			: Storage(source.size()), Image<T, DIM>(Storage::data.data(), source)
		{ }

		// Declaring any operator= here hides ALL of Image's operator=
		// overloads (ordinary C++ name hiding, not specific to this class) --
		// this brings the scalar-broadcast and cross-type ones back into
		// scope, while the two explicit overloads below still correctly win
		// for the exact OwnedImage-to-OwnedImage case (a non-template exact
		// match beats a using-declared template).
		using Image<T, DIM>::operator=;
		OwnedImage(const OwnedImage&) = delete;
		OwnedImage& operator=(const OwnedImage&) = delete;
		OwnedImage(OwnedImage&&) = default;
		OwnedImage& operator=(OwnedImage&&) = delete;

		// A fresh, uninitialized buffer with the same extent as `source` --
		// the owned equivalent of numpy's empty_like().
		template<class U>
		/// A fresh, uninitialized buffer with the same extent as `source` (numpy's empty_like()).
		/// @tparam U      source's element type (need not match T).
		/// @param  source Image whose extent (not contents) is copied.
		/// @return A new OwnedImage<T,DIM> with `source`'s shape and unspecified initial contents.
		static OwnedImage like(const Image<U, DIM>& source) { return OwnedImage(source.extent()); }
	};

	// Free-function convenience wrappers around Image<T,DIM>::sum(axis,...)/
	// min/max/mean(axis,...): those members need a caller-provided output
	// already allocated with the reduced (keepdims) extent, which means
	// hand-computing that extent and declaring a buffer for it at every
	// call site (confirmed repeated at multiple sites in
	// unitTests/image_arithmetic_tests.cpp) -- the same "I just need a
	// fresh buffer" ergonomics gap OwnedImage::like() already closes for
	// the whole-image case. These do the same for the per-axis case:
	// compute the reduced extent, allocate an OwnedImage<T,DIM> for it,
	// call the existing member, return the result.
	/// Per-axis sum, as a freshly-allocated OwnedImage<T,DIM> (numpy's keepdims=True convention) -- see Image<T,DIM>::sum(axis,output) for the underlying reduction.
	/// @param src  Source image.
	/// @param axis Axis to reduce.
	/// @return A new OwnedImage<T,DIM>, src's own extent with `axis` set to 1.
	template<class T, int DIM>
	OwnedImage<T, DIM> sum(const Image<T, DIM>& src, int axis)
	{
		assert(axis >= 0 && axis < DIM);
		auto extent = src.extent();
		extent[axis] = 1;
		OwnedImage<T, DIM> result(extent);
		src.sum(axis, result);
		return result;
	}
	/// Per-axis minimum, as a freshly-allocated OwnedImage<T,DIM> (numpy's keepdims=True convention) -- see Image<T,DIM>::min(axis,output) for the underlying reduction.
	/// @param src  Source image.
	/// @param axis Axis to reduce.
	/// @return A new OwnedImage<T,DIM>, src's own extent with `axis` set to 1.
	template<class T, int DIM>
	OwnedImage<T, DIM> min(const Image<T, DIM>& src, int axis)
	{
		assert(axis >= 0 && axis < DIM);
		auto extent = src.extent();
		extent[axis] = 1;
		OwnedImage<T, DIM> result(extent);
		src.min(axis, result);
		return result;
	}
	/// Per-axis maximum, as a freshly-allocated OwnedImage<T,DIM> (numpy's keepdims=True convention) -- see Image<T,DIM>::max(axis,output) for the underlying reduction.
	/// @param src  Source image.
	/// @param axis Axis to reduce.
	/// @return A new OwnedImage<T,DIM>, src's own extent with `axis` set to 1.
	template<class T, int DIM>
	OwnedImage<T, DIM> max(const Image<T, DIM>& src, int axis)
	{
		assert(axis >= 0 && axis < DIM);
		auto extent = src.extent();
		extent[axis] = 1;
		OwnedImage<T, DIM> result(extent);
		src.max(axis, result);
		return result;
	}
	/// Per-axis mean, as a freshly-allocated OwnedImage<T,DIM> (numpy's keepdims=True convention) -- see Image<T,DIM>::mean(axis,output) for the underlying reduction (stays in T throughout, unlike whole-image mean()).
	/// @param src  Source image.
	/// @param axis Axis to reduce.
	/// @return A new OwnedImage<T,DIM>, src's own extent with `axis` set to 1.
	template<class T, int DIM>
	OwnedImage<T, DIM> mean(const Image<T, DIM>& src, int axis)
	{
		assert(axis >= 0 && axis < DIM);
		auto extent = src.extent();
		extent[axis] = 1;
		OwnedImage<T, DIM> result(extent);
		src.mean(axis, result);
		return result;
	}
}
