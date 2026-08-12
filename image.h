#pragma once

// Umbrella header for ndl's core Image object. The implementation lives in
// image/, split by concern so each piece is easier to navigate on its own:
//   image/border_mode.h  - BorderMode enum + Image forward declaration
//   image/detail.h        - shared, non-public helpers (is_image_like,
//                            coordinate generation, kernel-tap walking --
//                            the latter used only by convolution.h/
//                            morphology.h, not by anything in this file)
//   image/core.h           - the Image<T,DIM> class itself: construction,
//                            view()/slice()/swap_axes()/mirror(), the
//                            arithmetic/comparison operators, and the
//                            whole-image/per-axis reductions. That's the
//                            whole class -- no convolution or morphology
//                            here; see below.
//   image/owned.h          - OwnedImage<T,DIM>, the owning subclass of Image
//   image/packed_bit.h     - PackedBitImage<DIM>, a compact bit-per-pixel
//                            image type sharing a minimal structural
//                            interface (extent()/at()/coordinates()) with
//                            Image, without deriving from it
//   image/kernels.h        - make_box_kernel()/make_cross_kernel() (kernel
//                            shapes) and per_channel() -- generic building
//                            blocks any toolkit's kernels/per-channel
//                            helpers can use, not specific to one
//   image/print.h          - operator<< for Image, and the free
//                            operator<(scalar, Image) comparison
//
// Not included here: convolution.h (convolve()/gaussian_blur()) and
// morphology.h (erode()/dilate()/median_filter()/percentile_filter()/
// threshold()/otsu_threshold()). Those are toolkits built on top of Image,
// not part of it -- the same relationship fft.h's fftn()/ifftn() have to
// Image. #include whichever of the three you actually use; #include
// <ndl/image.h> alone gets you the core object only.
//
// Include order matters for the files below: core.h needs border_mode.h/
// detail.h; owned.h/kernels.h/print.h need Image's full definition, so they
// come after core.h. Each file also includes exactly what it directly
// needs, so including any of them individually works too.
#include "image/border_mode.h"
#include "image/detail.h"
#include "image/core.h"
#include "image/owned.h"
#include "image/packed_bit.h"
#include "image/kernels.h"
#include "image/print.h"
