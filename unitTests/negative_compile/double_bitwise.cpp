// Negative-compilation regression check (see unitTests/CMakeLists.txt):
// Image<T,DIM>::operator%=/&=/|=/^= require an integral T -- modulus and
// bitwise ops aren't defined for floating-point types. This file exists
// purely to confirm that restriction stays in place; it is never meant to
// successfully compile.
#include <ndl/image.h>
using namespace ndl;
int main()
{
    double dataA[4] = {};
    double dataB[4] = {};
    Image<double, 1> a(dataA, { 4 });
    Image<double, 1> b(dataB, { 4 });
    a %= b;
    return 0;
}
