#include <ndl/image.h>
#include <ndl/processing/convolution.h>
#include <ndl/imageIO.h>
#include <ndl/processing/fft.h>
#include <ndl/mathHelpers.h>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <cstdint>
#include <complex>
#include <chrono>
#include <vector>
#include <array>
#include <string>
#include "../demoHelpers.h"

using namespace ndl;
using namespace ndl::fft;
namespace fs = std::filesystem;

// A step-by-step tour of ndl::convolve() (plus ndl::gaussian_blur(), which is
// built on it) -- free functions, convolution.h, not Image members: see
// image/core.h's own comment for why -- in the same spirit as demo/multiview's tour of view()/slice():
// each step shows the code, explains it, and shows the result. The two demos
// differ in one respect, though -- multiview's arrays were small enough to
// print in full and check by eye; a real photo is not. So Part 1 below
// establishes the mechanics on a small hand-checkable grid (exactly like
// multiview does), and every step after that applies the same operations to
// a real photo, saving the result as a PNG for *visual* inspection instead of
// printing pixel values, alongside min/max/mean (via Image's own reductions)
// so the numbers and the picture can be checked against each other.
//
// step()/showArray()/showText()/saveForInspection()/outputDir come from
// demoHelpers.h, shared with every other demo.

// Spells out, tap by tap, exactly what a 3x3 all-1s sumKernel sums at
// (cx,cy) under the given border mode -- built from the literal same
// _clamp()/_wrap()/_reflect() primitives convolve() itself uses
// (mathHelpers.h), so this is a real trace of what convolve() computes,
// not a hand-simplified stand-in for it. Exists so a reader can check a
// convolve() result by hand without having to re-derive how each border
// mode remaps an out-of-bounds tap themselves.
std::string sumKernelBreakdown(const Image<int, 2>& src, int cx, int cy, BorderMode border)
{
    int W = src.extent()[0], H = src.extent()[1];
    std::ostringstream oss;
    int total = 0;
    for (int dy = -1; dy <= 1; dy++)
    {
        if (dy != -1) oss << " + ";
        for (int dx = -1; dx <= 1; dx++)
        {
            int x = cx + dx, y = cy + dy;
            switch (border)
            {
                case BorderMode::Clamp:   x = _clamp(W, x);   y = _clamp(H, y);   break;
                case BorderMode::Wrap:    x = _wrap(W, x);    y = _wrap(H, y);    break;
                case BorderMode::Reflect: x = _reflect(W, x); y = _reflect(H, y); break;
            }
            int v = src(x, y);
            total += v;
            oss << (dx != -1 ? "+" : "") << v;
        }
    }
    oss << " = " << total;
    return oss.str();
}

// Applies a (non-negative-weight) kernel to a color image one channel at a
// time, via per_channel(), so colors don't bleed into each other. Safe to
// write straight to a uint8_t output only because every kernel used with
// this helper below has non-negative weights summing to <= 1, which
// guarantees the result never leaves [0,255].
void convolveColor(const Image<uint8_t, 3>& src, const Image<double, 2>& kernel, Image<uint8_t, 3>& dst, BorderMode border)
{
    per_channel(src, dst, 0, [&](const auto& s, auto& d) { ndl::convolve(s, d, kernel, border); });
}
void gaussianBlurColor(const Image<uint8_t, 3>& src, double sigma, Image<uint8_t, 3>& dst, BorderMode border)
{
    per_channel(src, dst, 0, [&](const auto& s, auto& d) { ndl::gaussian_blur(s, d, sigma, border); });
}
// Same idea as convolveColor(), but for kernels that can produce negative or
// out-of-range results (sharpen, emboss): convolves through a double
// intermediate per channel, then clamps via ndl::to_displayable()
// (convolution.h) instead of trusting convolve()'s own uint8_t output path.
void convolveColorSafe(const Image<uint8_t, 3>& src, const Image<double, 2>& kernel, Image<uint8_t, 3>& dst, BorderMode border, double bias = 0.0)
{
    per_channel(src, dst, 0, [&](const auto& s, auto& d)
    {
        OwnedImage<double, 2> srcDbl(s); // deep copy, converts uint8_t -> double
        auto outDbl = OwnedImage<double, 2>::like(srcDbl);
        ndl::convolve(srcDbl, outDbl, kernel, border);
        ndl::to_displayable(outDbl, d, bias);
    });
}

// The frequency-domain equivalent of convolveColor() above: correlates a
// color image with `kernel` by taking each channel to the frequency domain,
// multiplying by the kernel's (once-computed, shared across channels)
// spectrum, and transforming back -- see the big comment on
// FFT.FFTMatchesSpatialConvolution in unitTests/fft_tests.cpp for exactly
// which DFT identity this is and why it's conj() (cross-correlation) rather
// than the textbook convolution theorem. Always circular (BorderMode::Wrap
// is the only border mode that means anything here -- the DFT treats every
// axis as periodic whether asked to or not). Works for any src/kernel
// extent, via fft.h's FFTBluestein for whichever axes aren't a power of
// two -- though a power-of-two extent is still the fastest case internally.
void fftCorrelateColor(const Image<uint8_t, 3>& src, const Image<double, 2>& kernel, Image<uint8_t, 3>& dst)
{
    int W = src.extent()[1], H = src.extent()[2];

    // The kernel's spectrum doesn't depend on the image at all, so it's
    // computed once and reused for every channel below rather than
    // recomputed per channel.
    std::array<int, 2> center{ kernel.extent()[0] / 2, kernel.extent()[1] / 2 };
    OwnedImage<std::complex<double>, 2> kernelPadded({ W, H });
    kernelPadded = std::complex<double>(0, 0);
    for (const auto& kCoord : kernel.coordinates())
    {
        std::array<int, 2> at;
        for (int d = 0; d < 2; d++)
        {
            int delta = kCoord[d] - center[d];
            int m = kernelPadded.extent()[d];
            at[d] = ((delta % m) + m) % m; // wraps the kernel's own center onto index 0
        }
        kernelPadded.at(at) = std::complex<double>(kernel.at(kCoord), 0);
    }
    auto kernelFreq = OwnedImage<std::complex<double>, 2>::like(kernelPadded);
    fftn<double, 2>(kernelPadded, kernelFreq);

    per_channel(src, dst, 0, [&](const auto& s, auto& d)
    {
        OwnedImage<double, 2> srcDbl(s); // deep copy, converts uint8_t -> double

        auto imgFreq = OwnedImage<std::complex<double>, 2>::like(kernelFreq);
        fftn<double, 2>(srcDbl, imgFreq);

        auto product = OwnedImage<std::complex<double>, 2>::like(kernelFreq);
        for (const auto& coord : product.coordinates())
            product.at(coord) = std::conj(kernelFreq.at(coord)) * imgFreq.at(coord);

        auto back = OwnedImage<double, 2>::like(srcDbl);
        ifftn<double, 2>(product, back);

        for (const auto& coord : back.coordinates())
        {
            double v = back.at(coord);
            d.at(coord) = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
        }
    });
}

int main()
{
    outputDir = NDL_CONVOLUTION_OUTPUT_DIR;
    fs::create_directories(outputDir);
    std::string dataDir = NDL_TEST_DATA_DIR;

    std::cout <<
        "This demo teaches ndl::convolve() (and ndl::gaussian_blur(), which is built on it)\n"
        "the same way demo/multiview teaches view()/slice(): each step shows the code,\n"
        "explains it, and shows the result -- first on small numbers you can check by\n"
        "hand, then on a real photo whose results you check by *looking at the saved\n"
        "PNG*. Output PNGs land in:\n    build/demo/convolution/output\n";

    // ------------------------------------------------------------------
    // PART 1: the mechanics, on numbers you can check by hand
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 1: how convolve() works ===\n";

    OwnedImage<int, 2> grid({ 5, 5 });
    { int i = 0; for (auto it = grid.begin(); it != grid.end(); ++it) *it = ++i; }

    step("Image<int,2> grid(data, {5,5});  ... fill 1..25",
        "A plain 5x5 grid, x fastest. convolve() computes one output value per input position\n"
        "             from a small neighborhood around it, so this grid is the shared input for every\n"
        "             step in Part 1.");
    showArray("grid", grid);

    OwnedImage<int, 2> identityKernel({ 3, 3 }, { 0,0,0, 0,1,0, 0,0,0 });
    OwnedImage<int, 2> out1({ 5, 5 });

    step("ndl::convolve(grid, out1, identityKernel)",
        "convolve(src, dst, kernel) centers `kernel` on every position of `src` in turn, multiplies\n"
        "             each kernel weight by the input value under it, and sums: dst(coord) = sum over\n"
        "             kernel taps of kernel(k) * src(coord + k - center). A kernel that's 1 at its own\n"
        "             center and 0 everywhere else reproduces exactly the value already there -- convolving\n"
        "             with it is a no-op, the simplest possible check that the machinery above does what it\n"
        "             says.");
    showArray("identityKernel", identityKernel);
    ndl::convolve(grid, out1, identityKernel);
    showArray("out1 (should equal grid)", out1);

    OwnedImage<int, 2> sumKernel({ 3, 3 });
    sumKernel = 1;
    OwnedImage<int, 2> out2({ 5, 5 });

    step("ndl::convolve(grid, out2, sumKernel)   // sumKernel is 3x3, all 1s",
        "Every weight is 1, so each output value is just the SUM of the 3x3 neighborhood around it.\n"
        "             For an INTERIOR position like (2,2), every tap is a real, in-bounds grid element:\n"
        "             reading straight off the grid above (row y=1 gives x=1..3 -> 7,8,9; row y=2 ->\n"
        "             12,13,14; row y=3 -> 17,18,19), out2(2,2) = 7+8+9 + 12+13+14 + 17+18+19 = 117.\n"
        "             For a CORNER like (0,0), some of the 3x3's taps (x,y in {-1,0,1}x{-1,0,1}) fall\n"
        "             outside the grid entirely -- this call passes no BorderMode, so it uses convolve()'s\n"
        "             default, BorderMode::Clamp, which substitutes the nearest in-bounds pixel for any\n"
        "             out-of-bounds one (so both x=-1 and y=-1 become 0). The breakdown printed below\n"
        "             spells out exactly which 9 grid values that produces and what they sum to -- read it\n"
        "             alongside out2(0,0) to see the same 27 land in both places.");
    showArray("sumKernel", sumKernel);
    ndl::convolve(grid, out2, sumKernel);
    showArray("out2", out2);
    showText("out2(2,2)", std::to_string(out2(2, 2)) + "  (expected 117, interior -- see explanation)");
    showText("out2(0,0) breakdown (default border = Clamp)", sumKernelBreakdown(grid, 0, 0, BorderMode::Clamp));
    showText("out2(0,0)", std::to_string(out2(0, 0)) + "  (should match the breakdown above)");

    step("ndl::convolve(grid, out2, sumKernel, BorderMode::Clamp / Wrap / Reflect)",
        "Redoing that same corner, out2(0,0), under all three border modes -- the breakdowns below are\n"
        "             built from the exact same _clamp()/_wrap()/_reflect() functions convolve() itself\n"
        "             calls (mathHelpers.h), so they're a real trace of the computation, not a paraphrase\n"
        "             of it. Clamp repeats the nearest in-bounds pixel for an out-of-bounds tap (x=-1 -> 0,\n"
        "             y=-1 -> 0). Wrap treats the 5-wide/5-tall grid as periodic, so x=-1/y=-1 wrap to the\n"
        "             OPPOSITE edge (index 4), pulling in row/column 4's much larger values instead --\n"
        "             that's why Wrap's total (99) is so much bigger than Clamp's (27). Reflect mirrors a\n"
        "             tap back into the grid using -x-1 (so x=-1 -> 0 too, same as Clamp) -- for THIS\n"
        "             specific case (a radius-1 kernel, so the only out-of-bounds offset that ever occurs\n"
        "             is -1) Clamp and Reflect are mathematically guaranteed to agree, which is why both\n"
        "             read 27 below; they'd diverge for a wider kernel whose taps reach 2+ steps out of\n"
        "             bounds (there, Reflect mirrors past the edge instead of repeating it).");
    showText("out2(0,0) breakdown with Clamp", sumKernelBreakdown(grid, 0, 0, BorderMode::Clamp));
    showText("out2(0,0) breakdown with Wrap", sumKernelBreakdown(grid, 0, 0, BorderMode::Wrap));
    showText("out2(0,0) breakdown with Reflect", sumKernelBreakdown(grid, 0, 0, BorderMode::Reflect));
    ndl::convolve(grid, out2, sumKernel, BorderMode::Clamp);
    int clampCorner = out2(0, 0);
    ndl::convolve(grid, out2, sumKernel, BorderMode::Wrap);
    int wrapCorner = out2(0, 0);
    ndl::convolve(grid, out2, sumKernel, BorderMode::Reflect);
    int reflectCorner = out2(0, 0);
    showText("out2(0,0) with Clamp", std::to_string(clampCorner) + "  (should match the Clamp breakdown above)");
    showText("out2(0,0) with Wrap", std::to_string(wrapCorner) + "  (should match the Wrap breakdown above)");
    showText("out2(0,0) with Reflect", std::to_string(reflectCorner) + "  (should match the Reflect breakdown above)");

    // ------------------------------------------------------------------
    // PART 2: the same idea, at photographic scale -- box blur
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 2: a real photo, box blur ===\n";

    OwnedImage<uint8_t, 3> photo = image_io::load_owned(dataDir + "/ng_bwgirl_crop.jpg");

    step("image_io::load_owned(\"ng_bwgirl_crop.jpg\")",
        "A real photo: extent {channel, x, y}, channel-interleaved, loaded exactly like every other\n"
        "             image_io-supported format. Saved right back out unmodified first, so you have an\n"
        "             unblurred reference to compare every later step against.");
    saveForInspection("photo", photo, "01_original.png");

    OwnedImage<double, 2> boxKernel({ 3, 3 });
    boxKernel = 1.0 / 9.0;
    OwnedImage<uint8_t, 3> boxBlurred(photo.extent());

    step("convolveColor(photo, boxKernel, boxBlurred, BorderMode::Clamp)   // boxKernel is 3x3, all 1/9",
        "The exact same sumKernel idea from Part 1, just normalized (weights sum to 1 instead of 9)\n"
        "             so brightness is preserved instead of tripled, applied one color channel at a time --\n"
        "             convolveColor() slices off red/green/blue and convolves each separately, so colors\n"
        "             don't bleed into each other. A small, mild blur.");
    convolveColor(photo, boxKernel, boxBlurred, BorderMode::Clamp);
    saveForInspection("box-blurred photo", boxBlurred, "02_box_blur.png");

    // ------------------------------------------------------------------
    // PART 3: Gaussian blur -- a library feature built on convolve()
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 3: gaussian_blur() ===\n";

    step("ndl::gaussian_blur(impulse, response, 1.0)   // impulse is all 0 except one center pixel",
        "Convolution has a classic way to *see* a kernel: feed it an image that's all zero except one\n"
        "             pixel, and the output at every position is just that kernel's own weight there\n"
        "             (multiplying a single value by each weight and summing changes nothing else).\n"
        "             ndl::gaussian_blur(src, dst, sigma) builds a Gaussian-weighted kernel sized by sigma\n"
        "             (radius = ceil(3*sigma), so sigma=1.0 gives a 7x7 kernel here) and calls convolve()\n"
        "             with it -- so blurring a single bright dot traces out that kernel's actual shape,\n"
        "             printed below as numbers.");
    OwnedImage<double, 2> impulse({ 11, 11 });
    impulse = 0.0;
    impulse(5, 5) = 1000.0;
    OwnedImage<double, 2> response({ 11, 11 });
    ndl::gaussian_blur(impulse, response, 1.0);
    showArray("response (the Gaussian kernel's own shape, scaled by 1000)", response);

    OwnedImage<uint8_t, 3> gauss1(photo.extent());
    step("gaussianBlurColor(photo, 2.0, gauss1, BorderMode::Clamp)",
        "The same gaussian_blur() call, on the real photo, per channel. sigma=2.0 gives a wider,\n"
        "             softer blur than Part 2's 3x3 box -- compare 02_box_blur.png and\n"
        "             03_gaussian_sigma2.png side by side.");
    gaussianBlurColor(photo, 2.0, gauss1, BorderMode::Clamp);
    saveForInspection("gaussian-blurred (sigma=2.0)", gauss1, "03_gaussian_sigma2.png");

    OwnedImage<uint8_t, 3> gauss2(photo.extent());
    step("gaussianBlurColor(photo, 6.0, gauss2, BorderMode::Clamp)",
        "A much larger sigma -- the radius grows with it too (ceil(3*6.0)=18, a 37x37 kernel), so\n"
        "             this is a heavy blur, the kind used to approximate depth-of-field or to build an\n"
        "             image pyramid.");
    gaussianBlurColor(photo, 6.0, gauss2, BorderMode::Clamp);
    saveForInspection("gaussian-blurred (sigma=6.0)", gauss2, "04_gaussian_sigma6.png");

    // ------------------------------------------------------------------
    // PART 4: edge detection -- Sobel, an arbitrary kernel pair
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 4: edge detection (Sobel) ===\n";

    // A different source image on purpose: ng_bwgirl_crop.jpg is a tight crop of an
    // old, heavily grained photo, and Sobel is a derivative -- it amplifies that per-
    // pixel grain into what looks like solid static rather than clean edges (real
    // behavior, not a bug; it's *why* production edge detectors like Canny blur
    // first). marbles.bmp has cleaner per-pixel contrast, so its edges show clearly
    // without needing that extra step, which keeps this part focused on convolve()
    // and Sobel rather than on noise removal.
    OwnedImage<uint8_t, 3> marblesRaw = image_io::load_owned(dataDir + "/marbles.bmp");
    // marbles.bmp is a large photo (1419x1001); ndl::downsample() (convolution.h)
    // is the same "blur first, then keep every Nth pixel" pattern most image
    // libraries use for a resize -- gaussian_blur() so the pixels a plain
    // strided view() would otherwise skip get averaged in rather than
    // aliased into noise, then keeping every Nth position along every axis
    // except channelAxis. Keeps every image this Part and Part 6 below saves
    // (and the docs generated from them) a reasonable size, without changing
    // anything about how convolve()/Sobel/fftn() themselves work.
    OwnedImage<uint8_t, 3> marbles = downsample(marblesRaw, 2);

    step("image_io::load_owned(\"marbles.bmp\"); ndl::downsample(marblesRaw, 2)",
        "The start image for this Part and the frequency-domain Part 6 below -- saved on its own,\n"
        "             same as photo was in Part 2, so 06_sobel_edges.png further down has an unmodified\n"
        "             'before' to be compared against instead of only showing the 'after'. Downsampled 2x\n"
        "             from the raw file (see ndl::downsample(), convolution.h) purely to keep this demo's\n"
        "             saved images (and the docs generated from them) a reasonable size -- unrelated to\n"
        "             convolve() or Sobel themselves.");
    saveForInspection("marbles", marbles, "05_marbles_original.png");

    OwnedImage<uint8_t, 3> grey3({ 1, marbles.extent()[1], marbles.extent()[2] });
    step("marbles.mean(0, grey3)   // per-axis reduction over the channel axis",
        "Sobel below looks for brightness change, not color, so marbles.bmp is reduced to greyscale\n"
        "             first -- reusing Image::mean(axis, output) from the reductions work rather than\n"
        "             writing a separate greyscale routine: averaging over axis 0 (channel) collapses\n"
        "             R,G,B down to one value per pixel, written into a same-rank output with extent 1\n"
        "             along that axis.");
    marbles.mean(0, grey3);
    Image<uint8_t, 2> greyU8 = grey3.slice(0, 0); // drop the now-size-1 channel axis, back to plain 2D
    showText("greyU8", "extent {" + std::to_string(greyU8.extent()[0]) + ", " + std::to_string(greyU8.extent()[1]) + "}, single channel");

    OwnedImage<double, 2> grey(greyU8); // deep copy, converts uint8_t -> double for Sobel's signed math

    OwnedImage<double, 2> sobelX({ 3, 3 }, { -1,0,1,  -2,0,2,  -1,0,1 });
    OwnedImage<double, 2> sobelY({ 3, 3 }, { -1,-2,-1,  0,0,0,  1,2,1 });

    step("ndl::convolve(grey, gx, sobelX, BorderMode::Reflect); ndl::convolve(grey, gy, sobelY, BorderMode::Reflect)",
        "Two more 3x3 kernels -- proof convolve() takes any weights, not just symmetric blur-style\n"
        "             ones. sobelX responds to horizontal brightness change (vertical edges), sobelY to\n"
        "             vertical change (horizontal edges); a flat region gives 0 from both, which is why\n"
        "             this runs on double-precision grey rather than uint8_t -- the results are signed.");
    showArray("sobelX", sobelX);
    showArray("sobelY", sobelY);

    auto gx = OwnedImage<double, 2>::like(grey), gy = OwnedImage<double, 2>::like(grey);
    ndl::convolve(grey, gx, sobelX, BorderMode::Reflect);
    ndl::convolve(grey, gy, sobelY, BorderMode::Reflect);

    step("gx.multiply(gx, gx2); gy.multiply(gy, gy2); gx2.add(gy2, magSq); then sqrt() elementwise",
        "Gradient magnitude is sqrt(gx^2 + gy^2) -- built here from the non-mutating arithmetic\n"
        "             methods (multiply/add) added alongside convolve(), rather than a hand-written loop,\n"
        "             then a sqrt+clamp pass (ndl::to_displayable()) converts back to a displayable 8-bit\n"
        "             image, saved as a single-channel (greyscale) PNG.");
    auto gx2 = OwnedImage<double, 2>::like(grey), gy2 = OwnedImage<double, 2>::like(grey), magSq = OwnedImage<double, 2>::like(grey);
    gx.multiply(gx, gx2);
    gy.multiply(gy, gy2);
    gx2.add(gy2, magSq);

    auto magnitude = OwnedImage<double, 2>::like(grey);
    {
        auto sqIt = magSq.begin();
        for (auto it = magnitude.begin(); it != magnitude.end(); ++it, ++sqIt) *it = std::sqrt(*sqIt);
    }

    OwnedImage<uint8_t, 3> edgeImage({ 1, grey.extent()[0], grey.extent()[1] });
    Image<uint8_t, 2> edgeChannel = edgeImage.slice(0, 0);
    ndl::to_displayable(magnitude, edgeChannel);
    saveForInspection("Sobel edge magnitude", edgeImage, "06_sobel_edges.png");

    // ------------------------------------------------------------------
    // PART 5: arbitrary kernels -- sharpen and emboss
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 5: arbitrary kernels ===\n";

    OwnedImage<double, 2> sharpenKernel({ 3, 3 }, { 0,-1,0,  -1,5,-1,  0,-1,0 });
    OwnedImage<uint8_t, 3> sharpened(photo.extent());

    step("convolveColorSafe(photo, sharpenKernel, sharpened, BorderMode::Reflect)",
        "convolve() places no restriction on kernel weights -- here center=5, neighbors=-1, weights\n"
        "             still summing to 1 like the blur kernels, but the negative neighbors subtract off\n"
        "             nearby brightness, which *increases* local contrast at edges instead of smoothing it\n"
        "             out. Sharpening can genuinely overshoot below 0 or above 255 at strong edges, which\n"
        "             is exactly the case convolveColorSafe() (double intermediate + explicit clamp) exists\n"
        "             for, instead of convolve()'s own uint8_t output path.");
    showArray("sharpenKernel", sharpenKernel);
    convolveColorSafe(photo, sharpenKernel, sharpened, BorderMode::Reflect);
    saveForInspection("sharpened photo", sharpened, "07_sharpen.png");

    OwnedImage<double, 2> embossKernel({ 3, 3 }, { -2,-1,0,  -1,1,1,  0,1,2 });
    OwnedImage<uint8_t, 3> embossed(photo.extent());

    step("convolveColorSafe(photo, embossKernel, embossed, BorderMode::Reflect, 128.0)",
        "An asymmetric kernel: it responds to change along one diagonal and is flat along the other,\n"
        "             so flat regions of the photo collapse toward 0 (black) rather than toward their own\n"
        "             brightness. The usual fix -- applied here via ndl::to_displayable()'s bias argument --\n"
        "             is to add 128 back so 'no change' lands on mid-grey instead of black.");
    showArray("embossKernel", embossKernel);
    convolveColorSafe(photo, embossKernel, embossed, BorderMode::Reflect, 128.0);
    saveForInspection("embossed photo", embossed, "08_emboss.png");

    // ------------------------------------------------------------------
    // PART 6: the same operations again, via the frequency domain
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 6: the frequency domain ===\n";

    step("Image<uint8_t,3> crop = marbles.view({0,290,186}, {2,439,297});   // 150x112, all 3 channels -- deliberately NOT a power of two",
        "fftn()/ifftn() handle ANY extent, via Bluestein's algorithm (the chirp-z transform; see\n"
        "             FFTBluestein in fft.h) for whichever axes aren't already a power of two. This crop is\n"
        "             smaller than the full marbles image for two much more mundane reasons: keeping this demo's\n"
        "             pixel-by-pixel comparison and the timing benchmark below fast, and keeping the log-magnitude\n"
        "             spectrum image a comfortable size to look at. Its exact size, 150x112, is deliberately NOT a\n"
        "             power of two in either dimension (128x128 would have been) -- specifically so the fftn()\n"
        "             calls below exercise the arbitrary-size Bluestein path rather than the direct power-of-two\n"
        "             one. marbles rather than photo on purpose here, same reason Part 4 switched to it for\n"
        "             Sobel: photo's real film-grain noise spreads energy across every frequency roughly\n"
        "             evenly, which swamps the log-magnitude spectrum below into near-uniform static rather\n"
        "             than showing readable structure -- marbles' cleaner per-pixel contrast doesn't have that\n"
        "             problem. Every step below works on this crop.");
    Image<uint8_t, 3> crop = marbles.view({ 0, 290, 186 }, { 2, 439, 297 });
    saveForInspection("crop", crop, "09_crop.png");

    OwnedImage<uint8_t, 3> greyCrop3({ 1, crop.extent()[1], crop.extent()[2] });
    crop.mean(0, greyCrop3);
    Image<uint8_t, 2> greyCropU8 = greyCrop3.slice(0, 0);
    OwnedImage<double, 2> greyCropDbl(greyCropU8); // deep copy, uint8_t -> double

    step("fftn<double,2>(greyCropDbl, freq)   // greyscale, so there's one spectrum to look at, not three",
        "The magnitude of each complex output value says how much of that frequency is present in the\n"
        "             crop -- low frequencies (slow brightness changes, like the overall lighting) near the\n"
        "             corners of the raw output, high frequencies (sharp edges, fine texture) further out.\n"
        "             Displaying that directly is unreadable: fftshift() below recenters it (a numpy.fft\n"
        "             convention -- DC in the middle, not the corner) and a log(1+magnitude) scale tames\n"
        "             its enormous dynamic range (the DC term alone is the sum of every one of the crop's\n"
        "             16800 pixels) so faint high-frequency detail doesn't just disappear next to it.");
    OwnedImage<std::complex<double>, 2> greyFreq(greyCropU8.extent());
    fftn<double, 2>(greyCropDbl, greyFreq);

    auto logMag = OwnedImage<double, 2>::like(greyCropU8);
    {
        auto fIt = greyFreq.begin();
        for (auto it = logMag.begin(); it != logMag.end(); ++it, ++fIt) *it = std::log(1.0 + std::abs(*fIt));
    }
    auto magShifted = OwnedImage<double, 2>::like(greyCropU8);
    fftshift(logMag, magShifted);

    double maxMag = magShifted.max();
    OwnedImage<uint8_t, 3> spectrumImage({ 1, greyCropU8.extent()[0], greyCropU8.extent()[1] });
    Image<uint8_t, 2> spectrumChannel = spectrumImage.slice(0, 0);
    {
        auto mIt = magShifted.begin();
        for (auto it = spectrumChannel.begin(); it != spectrumChannel.end(); ++it, ++mIt) *it = (uint8_t)((*mIt / maxMag) * 255.0);
    }
    saveForInspection("log-magnitude spectrum (fftshifted)", spectrumImage, "10_spectrum.png");

    // A 5x5 box kernel, a bit stronger than Part 2's 3x3 one -- big enough
    // (25 taps, averaging over a 5x5 neighborhood) that the blur is obvious
    // at a glance rather than something you have to squint at two images to
    // spot, and it lines up with the "bigger kernel" case in the timing
    // benchmark below, which reuses this exact kernel rather than defining
    // its own.
    OwnedImage<double, 2> box5Kernel({ 5, 5 });
    box5Kernel = 1.0 / (5.0 * 5.0);

    step({
            "fftCorrelateColor(crop, box5Kernel, fftBoxBlurred)   // 5x5 box kernel -- a bit stronger than the 3x3 one from Part 2",
            "// fftCorrelateColor() itself does exactly this, per color channel:",
            "fftn<double,2>(kernelPadded, kernelFreq);          // kernel -> its own spectrum (computed once, shared by every channel)",
            "fftn<double,2>(channelAsDouble, imgFreq);          // this channel -> its own spectrum",
            "product = conj(kernelFreq) * imgFreq;              // per-frequency multiply -- this IS the convolution",
            "ifftn<double,2>(product, blurredChannel);          // spectrum -> back to pixels"
        },
        "The crop shown again just above is this comparison's starting point. fftCorrelateColor() takes\n"
        "             each channel to the frequency domain, multiplies by the kernel's spectrum (conjugated --\n"
        "             convolve() computes a correlation, not a textbook convolution; see the comment on\n"
        "             fftCorrelateColor() in this file, or testFFTMatchesSpatialConvolution in unitTests.cpp,\n"
        "             for the exact identity and why), and transforms back -- the 5 lines above are that\n"
        "             function's real body, not a paraphrase of it. That's a real, independent computation of\n"
        "             the *same* answer Part 2's convolveColor(crop, box5Kernel, ..., BorderMode::Wrap) would\n"
        "             give -- computed below for direct comparison rather than taken on faith.");
    saveForInspection("crop (this comparison's starting point, shown again for convenience)", crop, "09_crop.png");
    OwnedImage<uint8_t, 3> fftBoxBlurred(crop.extent());
    fftCorrelateColor(crop, box5Kernel, fftBoxBlurred);
    saveForInspection("FFT-domain box blur", fftBoxBlurred, "11_fft_box_blur.png");

    OwnedImage<uint8_t, 3> spatialBoxBlurred(crop.extent());
    convolveColor(crop, box5Kernel, spatialBoxBlurred, BorderMode::Wrap);
    saveForInspection("spatial-domain box blur (BorderMode::Wrap, for a fair comparison)", spatialBoxBlurred, "12_spatial_box_blur_wrap.png");

    auto diff = countMismatches(fftBoxBlurred, spatialBoxBlurred);
    showText("largest per-pixel difference between the two", std::to_string(diff.maxDiff) + " (out of 0-255) -- 11 and 12 should look identical");

    step("timing: spatial convolve() vs FFT correlation, at two very different kernel sizes",
        "convolve()'s cost scales with image size TIMES kernel size (every output pixel visits every\n"
        "             kernel tap); fftn()'s cost scales with image size alone (kernel size only changes how\n"
        "             the kernel gets *built*, not the transform cost) -- so which one wins depends entirely\n"
        "             on the kernel. A 3x3 kernel is 9 taps; the 5x5 one already used above for the visual\n"
        "             comparison is 25, ~2.8x more spatial work for the exact same image, while the FFT side\n"
        "             barely changes (same 7 image-sized transforms either way -- 1 for the kernel, 2 per\n"
        "             channel). Averaged over a few repetitions below.");
    OwnedImage<uint8_t, 3> timingOut(crop.extent());
    const int reps = 3;

    auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; r++) convolveColor(crop, boxKernel, timingOut, BorderMode::Wrap);
    auto t1 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; r++) fftCorrelateColor(crop, boxKernel, timingOut);
    auto t2 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; r++) convolveColor(crop, box5Kernel, timingOut, BorderMode::Wrap);
    auto t3 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; r++) fftCorrelateColor(crop, box5Kernel, timingOut);
    auto t4 = std::chrono::steady_clock::now();

    auto ms = [reps](std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count() / reps;
    };
    showText("3x3 kernel  -- spatial convolve()", std::to_string(ms(t0, t1)) + " ms/call");
    showText("3x3 kernel  -- FFT correlation", std::to_string(ms(t1, t2)) + " ms/call");
    showText("5x5 kernel -- spatial convolve()", std::to_string(ms(t2, t3)) + " ms/call");
    showText("5x5 kernel -- FFT correlation", std::to_string(ms(t3, t4)) + " ms/call");

    std::cout <<
        "\n\nAll outputs written to: build/demo/convolution/output\n"
        "Open 01_original.png alongside the rest to compare by eye: 02/03/04 should look\n"
        "progressively softer. 06 should show bright edges on a dark background -- compare it to 05,\n"
        "marbles' own unmodified original. 07 should look crisper than the original, and 08 should\n"
        "look like a grey relief carving. 10 is what the 150x112 crop (09) looks like in the frequency\n"
        "domain -- a bright center fading outward, with any strong directional texture in the crop\n"
        "showing up as streaks through it. 11 and 12 should be visually indistinguishable -- a 5-pixel\n"
        "box blur, computed two independent ways (frequency domain and spatial domain), that agree\n"
        "to within a pixel or two of rounding.\n";

    return 0;
}
