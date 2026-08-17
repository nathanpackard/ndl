// Negative-compilation regression check (see unitTests/CMakeLists.txt):
// pairwise_slice<AxisA,AxisB>() requires two distinct axes -- passing the
// same axis twice can't produce a meaningful 2D view (there'd be no second
// axis left to show), so it's rejected via static_assert rather than
// producing a degenerate 1-wide result. This file exists purely to confirm
// that restriction stays in place; it is never meant to successfully
// compile.
#include <ndl/viewer/viewer.h>
using namespace ndl;
int main()
{
	float data[24];
	Image<float, 3> img(data, { 2, 3, 4 });
	std::array<int, 3> cursor = { 0, 0, 0 };
	auto view = pairwise_slice<1, 1>(img, cursor);
	(void)view;
	return 0;
}
