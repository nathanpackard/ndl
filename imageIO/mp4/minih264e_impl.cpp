// The actual function bodies for minih264e.h (vendored, CC0/public
// domain -- see minih264e.h's own top comment), in its own dedicated
// translation unit -- see minimp4_impl.cpp's own comment for why (name AND
// macro collisions if this ever shared a TU with minimp4.h's own
// implementation) and imageIO/video_io.h's own top comment for the bigger
// picture. minih264e.h's implementation has been patched in place (each
// site marked with a comment) to add explicit casts at every place its
// original C source relied on implicit void*-to-typed-pointer conversions
// -- valid C, not valid C++ -- so this compiles as ordinary C++ like any
// other ndl translation unit, no separate C compiler needed.
#define MINIH264_IMPLEMENTATION
#include "minih264e.h"
