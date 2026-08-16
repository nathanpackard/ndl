// The actual function bodies for minimp4.h (vendored verbatim in this same
// directory, CC0/public domain -- see minimp4.h's own top comment), in its
// own dedicated translation unit. See imageIO/video_io.h's own top comment
// for exactly why this can't just be a `#define MINIMP4_IMPLEMENTATION`
// inside that header directly: minih264e.h needs the exact same treatment,
// and the two libraries' internal helpers collide by name (and, worse,
// minih264e.h's implementation leaks ~130 unscoped preprocessor macros)
// if their implementations ever land in the same translation unit --
// keeping each in its own .cpp file sidesteps that entirely, since neither
// TU's internal names or macros are visible outside itself.
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"
