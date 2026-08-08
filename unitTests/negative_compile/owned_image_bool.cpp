// Negative-compilation regression check (see unitTests/CMakeLists.txt):
// OwnedImage<bool,DIM> is deliberately unbuildable -- std::vector<bool> is
// bit-packed and has no .data() for Image to alias (see the static_assert
// in image/owned.h). This file exists purely to confirm that restriction
// stays in place; it is never meant to successfully compile.
#include <ndl/image.h>
using namespace ndl;
int main()
{
    OwnedImage<bool, 2> x({ 5, 5 });
    return 0;
}
