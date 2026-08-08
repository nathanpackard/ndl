#pragma once
#include <vector>
#include <array>
#include <cassert>
#include <algorithm>
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
	template<class T, int DIM>
	class OwnedImage : private detail::OwnedImageStorage<T>, public Image<T, DIM>
	{
		using Storage = detail::OwnedImageStorage<T>;
	public:
		explicit OwnedImage(std::array<int, DIM> extent)
			: Storage(Image<T, DIM>::size(extent)), Image<T, DIM>(Storage::data.data(), extent)
		{ }

		// For the common case of a small kernel/grid with specific literal
		// values (e.g. a Sobel kernel), which a plain extent-only
		// OwnedImage can't express any more directly than the
		// std::vector-then-Image pair it's meant to replace -- this makes
		// it a single line instead: OwnedImage<double,2> sobelX({3,3},
		// {-1,0,1, -2,0,2, -1,0,1});
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
		static OwnedImage like(const Image<U, DIM>& source) { return OwnedImage(source.extent()); }
	};
}
