#pragma once

// Umbrella header -- #include <ndl/image.h> keeps working exactly as
// before; the implementation itself lives in image/, split by concern so
// each piece is easier to navigate on its own:
//   image/border_mode.h  - BorderMode enum + Image forward declaration
//   image/detail.h        - shared, non-public helpers (is_image_like,
//                            coordinate generation, kernel-tap walking)
//   image/algorithms.h    - erode()/dilate()/median_filter()/
//                            percentile_filter()/threshold(), as free
//                            functions over any minimal-interface image type
//   image/core.h           - the Image<T,DIM> class itself
//   image/owned.h          - OwnedImage<T,DIM>, the owning subclass of Image
//   image/packed_bit.h     - PackedBitImage<DIM>, a compact bit-per-pixel
//                            image type sharing algorithms.h's minimal
//                            interface without deriving from Image
//   image/kernels.h        - make_box_kernel()/make_cross_kernel()/
//                            per_channel()
//   image/print.h          - operator<< for Image, and the free
//                            operator<(scalar, Image) comparison
// Include order matters (algorithms.h before core.h, since Image's members
// forward to the free functions; owned.h/kernels.h/print.h after core.h,
// since they need Image's full definition) -- each file also includes
// exactly what it directly needs, so including any of them individually
// works too.
#include "image/border_mode.h"
#include "image/detail.h"
#include "image/algorithms.h"
#include "image/core.h"
#include "image/owned.h"
#include "image/packed_bit.h"
#include "image/kernels.h"
#include "image/print.h"
