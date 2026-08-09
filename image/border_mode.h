#pragma once

namespace ndl
{
	template<class T, int DIM> class Image; // forward declaration, needed by detail::is_image_like in detail.h

	/// How convolve()/erode()/dilate()/median_filter()/percentile_filter() handle a
	/// neighborhood offset that falls outside the image: Clamp to the nearest edge
	/// pixel, Wrap around to the opposite edge, or Reflect back into the image.
	/// @ingroup morphology_filtering
	// Border handling for Image::convolve() -- reuses the same _clamp/_wrap/_reflect
	// primitives the iterator's clamp()/wrap()/reflect() accessors use for the same purpose.
	enum class BorderMode
	{
		Clamp,   ///< Out-of-bounds reads return the nearest in-bounds element (edge repeats).
		Wrap,    ///< Out-of-bounds reads wrap around to the opposite edge (as if the image tiled).
		Reflect  ///< Out-of-bounds reads mirror back into the image across the edge.
	};
}
