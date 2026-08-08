#pragma once

namespace ndl
{
	template<class T, int DIM> class Image; // forward declaration, needed by detail::is_image_like in detail.h

	// Border handling for Image::convolve() -- reuses the same _clamp/_wrap/_reflect
	// primitives the iterator's clamp()/wrap()/reflect() accessors use for the same purpose.
	enum class BorderMode { Clamp, Wrap, Reflect };
}
