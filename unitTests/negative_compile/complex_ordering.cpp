// Negative-compilation regression check (see unitTests/CMakeLists.txt):
// Image<T,DIM>::min() (and its ordering-dependent siblings -- max(),
// operator</<=/>/>=, otsu_threshold(), threshold(), convolve(),
// erode()/dilate()/percentile_filter()/median_filter(), logical_not(),
// mean()) require an arithmetic T, since they need a total order or a
// double conversion. std::complex has neither. This file exists purely to
// confirm that restriction stays in place; it is never meant to
// successfully compile.
#include <ndl/image.h>
#include <complex>
using namespace ndl;
int main()
{
    std::complex<double> data[4];
    Image<std::complex<double>, 1> img(data, { 4 });
    auto m = img.min();
    (void)m;
    return 0;
}
