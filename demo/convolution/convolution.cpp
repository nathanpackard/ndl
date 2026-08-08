#include <ndl/image.h>
#include <ndl/imageIO.h>
#include <ndl/fft.h>
#include <iostream>
#include <filesystem>
#include <cmath>
#include <cstdint>
#include <complex>
#include <chrono>
#include <vector>
#include <array>
#include <string>

using namespace ndl;
using namespace ndl::fft;
namespace fs = std::filesystem;

// A step-by-step tour of Image::convolve() (plus gaussian_blur(), which is
// built on it), in the same spirit as demo/multiview's tour of view()/slice():
// each step shows the code, explains it, and shows the result. The two demos
// differ in one respect, though -- multiview's arrays were small enough to
// print in full and check by eye; a real photo is not. So Part 1 below
// establishes the mechanics on a small hand-checkable grid (exactly like
// multiview does), and every step after that applies the same operations to
// a real photo, saving the result as a PNG for *visual* inspection instead of
// printing pixel values, alongside min/max/mean (via Image's own reductions)
// so the numbers and the picture can be checked against each other.

int stepNumber = 0;

void step(const std::string& code, const std::string& explanation)
{
    std::cout << "\n[" << ++stepNumber << "] code:    " << code << "\n";
    std::cout << "    explain: " << explanation << "\n";
}

template<class ImageT>
void showArray(const std::string& label, const ImageT& img) { std::cout << "    " << label << ":\n" << img; }
void showText(const std::string& label, const std::string& text) { std::cout << "    " << label << ":   " << text << "\n"; }

std::string outputDir;

// Saves img as a PNG under the demo's output folder and prints where it
// went plus min/max/mean, so the numbers and the picture can be checked
// against each other rather than eyeballing the picture alone.
template<class T, int DIM>
void saveForInspection(const std::string& label, const Image<T, DIM>& img, const std::string& filename)
{
    std::string path = outputDir + "/" + filename;
    image_io::save(img, path);
    std::cout << "    " << label << ": " << path << "\n";
    std::cout << "        extent = {";
    for (int i = 0; i < DIM; i++) std::cout << (i ? ", " : "") << img.extent()[i];
    std::cout << "}   min=" << (int)img.min() << "  max=" << (int)img.max() << "  mean=" << img.mean() << "\n";
}

// Converts a double-precision image to a displayable uint8_t one, clamping
// to [0,255] and optionally re-centering by `bias` first. Needed whenever a
// kernel's weights aren't all non-negative (sharpen, emboss below): convolve()
// accumulates in double internally but casts straight to T on the way out,
// and casting a negative double straight to uint8_t is undefined behavior,
// not a wraparound -- so those kernels convolve into a double buffer and
// come through here instead of writing directly to a uint8_t output.
template<int DIM>
void toDisplayable(const Image<double, DIM>& src, Image<uint8_t, DIM>& dst, double bias = 0.0)
{
    auto srcIt = src.begin();
    for (auto it = dst.begin(); it != dst.end(); ++it, ++srcIt)
    {
        double v = *srcIt + bias;
        *it = (uint8_t)(v < 0.0 ? 0.0 : (v > 255.0 ? 255.0 : v));
    }
}

// Applies a (non-negative-weight) kernel to a color image one channel at a
// time, so colors don't bleed into each other -- slice() shares memory with
// src/dst rather than copying, and convolve()/gaussian_blur() don't care how
// many dimensions they're given, so this is just a loop, not special-cased
// library code. Safe to write straight to a uint8_t output only because
// every kernel used with this helper below has non-negative weights summing
// to <= 1, which guarantees the result never leaves [0,255].
void convolveColor(const Image<uint8_t, 3>& src, const Image<double, 2>& kernel, Image<uint8_t, 3>& dst, BorderMode border)
{
    for (int c = 0; c < src.extent()[0]; c++)
    {
        Image<uint8_t, 2> srcChannel = src.slice(0, c);
        Image<uint8_t, 2> dstChannel = dst.slice(0, c);
        srcChannel.convolve(kernel, dstChannel, border);
    }
}
void gaussianBlurColor(const Image<uint8_t, 3>& src, double sigma, Image<uint8_t, 3>& dst, BorderMode border)
{
    for (int c = 0; c < src.extent()[0]; c++)
    {
        Image<uint8_t, 2> srcChannel = src.slice(0, c);
        Image<uint8_t, 2> dstChannel = dst.slice(0, c);
        srcChannel.gaussian_blur(sigma, dstChannel, border);
    }
}
// Same idea, but for kernels that can produce negative or out-of-range
// results (sharpen, emboss): convolves through a double intermediate per
// channel, then clamps via toDisplayable() instead of trusting convolve()'s
// own uint8_t output path.
void convolveColorSafe(const Image<uint8_t, 3>& src, const Image<double, 2>& kernel, Image<uint8_t, 3>& dst, BorderMode border, double bias = 0.0)
{
    for (int c = 0; c < src.extent()[0]; c++)
    {
        Image<uint8_t, 2> srcChannel = src.slice(0, c);

        std::vector<double> srcDblData(srcChannel.size());
        Image<double, 2> srcDbl(srcDblData.data(), srcChannel); // deep copy, converts uint8_t -> double

        std::vector<double> outDblData(srcChannel.size());
        Image<double, 2> outDbl(outDblData.data(), srcChannel.extent());
        srcDbl.convolve(kernel, outDbl, border);

        Image<uint8_t, 2> dstChannel = dst.slice(0, c);
        toDisplayable(outDbl, dstChannel, bias);
    }
}

// Moves the zero-frequency (DC) component from index 0 of each axis to the
// middle of that axis (index extent/2), wrapping the rest around it -- the
// standard rearrangement (numpy calls it fftshift) for *displaying* a
// spectrum, since fftn()'s raw output has DC in the corner with frequency
// increasing outward in a way that wraps at the edge, which reads as
// meaningless noise until it's centered.
template<class T, int DIM>
void fftshift(const Image<T, DIM>& src, Image<T, DIM>& dst)
{
    for (const auto& coord : src.coordinates())
    {
        std::array<int, DIM> shifted;
        for (int d = 0; d < DIM; d++) shifted[d] = (coord[d] + src.extent()[d] / 2) % src.extent()[d];
        dst.at(shifted) = src.at(coord);
    }
}

// The frequency-domain equivalent of convolveColor() above: correlates a
// color image with `kernel` by taking each channel to the frequency domain,
// multiplying by the kernel's (once-computed, shared across channels)
// spectrum, and transforming back -- see the big comment on
// testFFTMatchesSpatialConvolution in unitTests.cpp for exactly which DFT
// identity this is and why it's conj() (cross-correlation) rather than the
// textbook convolution theorem. Always circular (BorderMode::Wrap is the
// only border mode that means anything here -- the DFT treats every axis as
// periodic whether asked to or not), and only meaningful when width and
// height are both powers of two, same as fftn() itself.
void fftCorrelateColor(const Image<uint8_t, 3>& src, const Image<double, 2>& kernel, Image<uint8_t, 3>& dst)
{
    int W = src.extent()[1], H = src.extent()[2];

    // The kernel's spectrum doesn't depend on the image at all, so it's
    // computed once and reused for every channel below rather than
    // recomputed per channel.
    std::array<int, 2> center{ kernel.extent()[0] / 2, kernel.extent()[1] / 2 };
    std::vector<std::complex<double>> kernelPaddedData((size_t)W * H);
    Image<std::complex<double>, 2> kernelPadded(kernelPaddedData.data(), { W, H });
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
    std::vector<std::complex<double>> kernelFreqData((size_t)W * H);
    Image<std::complex<double>, 2> kernelFreq(kernelFreqData.data(), { W, H });
    fftn<double, 2>(kernelPadded, kernelFreq);

    for (int c = 0; c < src.extent()[0]; c++)
    {
        Image<uint8_t, 2> srcChannel = src.slice(0, c);

        std::vector<double> srcDblData(srcChannel.size());
        Image<double, 2> srcDbl(srcDblData.data(), srcChannel); // deep copy, converts uint8_t -> double

        std::vector<std::complex<double>> imgFreqData((size_t)W * H), productData((size_t)W * H);
        Image<std::complex<double>, 2> imgFreq(imgFreqData.data(), { W, H });
        Image<std::complex<double>, 2> product(productData.data(), { W, H });
        fftn<double, 2>(srcDbl, imgFreq);

        for (const auto& coord : product.coordinates())
            product.at(coord) = std::conj(kernelFreq.at(coord)) * imgFreq.at(coord);

        std::vector<double> backData((size_t)W * H);
        Image<double, 2> back(backData.data(), { W, H });
        ifftn<double, 2>(product, back);

        Image<uint8_t, 2> dstChannel = dst.slice(0, c);
        for (const auto& coord : back.coordinates())
        {
            double v = back.at(coord);
            dstChannel.at(coord) = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
        }
    }
}

int main()
{
    outputDir = NDL_CONVOLUTION_OUTPUT_DIR;
    fs::create_directories(outputDir);
    std::string dataDir = NDL_TEST_DATA_DIR;

    std::cout <<
        "This demo teaches Image::convolve() (and gaussian_blur(), which is built on it)\n"
        "the same way demo/multiview teaches view()/slice(): each step shows the code,\n"
        "explains it, and shows the result -- first on small numbers you can check by\n"
        "hand, then on a real photo whose results you check by *looking at the saved\n"
        "PNG*. Output PNGs land in:\n    " << outputDir << "\n";

    // ------------------------------------------------------------------
    // PART 1: the mechanics, on numbers you can check by hand
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 1: how convolve() works ===\n";

    std::vector<int> gridData(25);
    Image<int, 2> grid(gridData.data(), { 5, 5 });
    { int i = 0; for (auto it = grid.begin(); it != grid.end(); ++it) *it = ++i; }

    step("Image<int,2> grid(data, {5,5});  ... fill 1..25",
        "A plain 5x5 grid, x fastest. convolve() computes one output value per input position\n"
        "             from a small neighborhood around it, so this grid is the shared input for every\n"
        "             step in Part 1.");
    showArray("grid", grid);

    std::vector<int> identityKernelData = { 0,0,0, 0,1,0, 0,0,0 };
    Image<int, 2> identityKernel(identityKernelData.data(), { 3, 3 });
    std::vector<int> out1Data(25);
    Image<int, 2> out1(out1Data.data(), { 5, 5 });

    step("grid.convolve(identityKernel, out1)",
        "convolve(kernel, output) centers `kernel` on every position of *this in turn, multiplies\n"
        "             each kernel weight by the input value under it, and sums: output(coord) = sum over\n"
        "             kernel taps of kernel(k) * input(coord + k - center). A kernel that's 1 at its own\n"
        "             center and 0 everywhere else reproduces exactly the value already there -- convolving\n"
        "             with it is a no-op, the simplest possible check that the machinery above does what it\n"
        "             says.");
    showArray("identityKernel", identityKernel);
    grid.convolve(identityKernel, out1);
    showArray("out1 (should equal grid)", out1);

    std::vector<int> sumKernelData(9, 1);
    Image<int, 2> sumKernel(sumKernelData.data(), { 3, 3 });
    std::vector<int> out2Data(25);
    Image<int, 2> out2(out2Data.data(), { 5, 5 });

    step("grid.convolve(sumKernel, out2)   // sumKernel is 3x3, all 1s",
        "Every weight is 1, so each output value is just the SUM of the 3x3 neighborhood around it:\n"
        "             out2(2,2) should be the sum of grid's 8 neighbors plus its own center --\n"
        "             7+8+9 + 12+13+14 + 17+18+19 = 117.");
    showArray("sumKernel", sumKernel);
    grid.convolve(sumKernel, out2);
    showArray("out2", out2);
    showText("out2(2,2)", std::to_string(out2(2, 2)) + "  (expected 117)");

    step("grid.convolve(sumKernel, out2, BorderMode::Clamp / Wrap / Reflect)",
        "At the edges and corners, some kernel taps fall outside the 5x5 grid -- `border` decides\n"
        "             what stands in for them. Clamp repeats the nearest edge pixel, Wrap treats the grid\n"
        "             as periodic (wraps to the opposite edge), Reflect mirrors back into the grid. All\n"
        "             three are computed below for the same corner, out2(0,0), so you can see they\n"
        "             genuinely produce different numbers -- the border argument isn't cosmetic.");
    grid.convolve(sumKernel, out2, BorderMode::Clamp);
    int clampCorner = out2(0, 0);
    grid.convolve(sumKernel, out2, BorderMode::Wrap);
    int wrapCorner = out2(0, 0);
    grid.convolve(sumKernel, out2, BorderMode::Reflect);
    int reflectCorner = out2(0, 0);
    showText("out2(0,0) with Clamp", std::to_string(clampCorner));
    showText("out2(0,0) with Wrap", std::to_string(wrapCorner));
    showText("out2(0,0) with Reflect", std::to_string(reflectCorner));

    // ------------------------------------------------------------------
    // PART 2: the same idea, at photographic scale -- box blur
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 2: a real photo, box blur ===\n";

    std::array<int, 3> photoExtent;
    std::vector<uint8_t> photoData = image_io::load(dataDir + "/ng_bwgirl_crop.jpg", photoExtent);
    Image<uint8_t, 3> photo(photoData.data(), photoExtent);

    step("image_io::load(\"ng_bwgirl_crop.jpg\", extent)",
        "A real photo: extent {channel, x, y}, channel-interleaved, loaded exactly like every other\n"
        "             image_io-supported format. Saved right back out unmodified first, so you have an\n"
        "             unblurred reference to compare every later step against.");
    saveForInspection("photo", photo, "01_original.png");

    std::vector<double> boxKernelData(9, 1.0 / 9.0);
    Image<double, 2> boxKernel(boxKernelData.data(), { 3, 3 });
    std::vector<uint8_t> boxBlurredData(photo.size());
    Image<uint8_t, 3> boxBlurred(boxBlurredData.data(), photoExtent);

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

    step("impulse.gaussian_blur(1.0, response)   // impulse is all 0 except one center pixel",
        "Convolution has a classic way to *see* a kernel: feed it an image that's all zero except one\n"
        "             pixel, and the output at every position is just that kernel's own weight there\n"
        "             (multiplying a single value by each weight and summing changes nothing else).\n"
        "             Image::gaussian_blur(sigma, output) builds a Gaussian-weighted kernel sized by sigma\n"
        "             (radius = ceil(3*sigma), so sigma=1.0 gives a 7x7 kernel here) and calls convolve()\n"
        "             with it -- so blurring a single bright dot traces out that kernel's actual shape,\n"
        "             printed below as numbers.");
    std::vector<double> impulseData(121, 0.0);
    Image<double, 2> impulse(impulseData.data(), { 11, 11 });
    impulse(5, 5) = 1000.0;
    std::vector<double> responseData(121);
    Image<double, 2> response(responseData.data(), { 11, 11 });
    impulse.gaussian_blur(1.0, response);
    showArray("response (the Gaussian kernel's own shape, scaled by 1000)", response);

    std::vector<uint8_t> gauss1Data(photo.size());
    Image<uint8_t, 3> gauss1(gauss1Data.data(), photoExtent);
    step("gaussianBlurColor(photo, 2.0, gauss1, BorderMode::Clamp)",
        "The same gaussian_blur() call, on the real photo, per channel. sigma=2.0 gives a wider,\n"
        "             softer blur than Part 2's 3x3 box -- compare 02_box_blur.png and\n"
        "             03_gaussian_sigma2.png side by side.");
    gaussianBlurColor(photo, 2.0, gauss1, BorderMode::Clamp);
    saveForInspection("gaussian-blurred (sigma=2.0)", gauss1, "03_gaussian_sigma2.png");

    std::vector<uint8_t> gauss2Data(photo.size());
    Image<uint8_t, 3> gauss2(gauss2Data.data(), photoExtent);
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
    std::array<int, 3> marblesExtent;
    std::vector<uint8_t> marblesData = image_io::load(dataDir + "/marbles.bmp", marblesExtent);
    Image<uint8_t, 3> marbles(marblesData.data(), marblesExtent);

    std::vector<uint8_t> grey3Data((size_t)marbles.extent()[1] * marbles.extent()[2]);
    Image<uint8_t, 3> grey3(grey3Data.data(), { 1, marbles.extent()[1], marbles.extent()[2] });
    step("marbles.mean(0, grey3)   // per-axis reduction over the channel axis",
        "Sobel below looks for brightness change, not color, so marbles.bmp is reduced to greyscale\n"
        "             first -- reusing Image::mean(axis, output) from the reductions work rather than\n"
        "             writing a separate greyscale routine: averaging over axis 0 (channel) collapses\n"
        "             R,G,B down to one value per pixel, written into a same-rank output with extent 1\n"
        "             along that axis.");
    marbles.mean(0, grey3);
    Image<uint8_t, 2> greyU8 = grey3.slice(0, 0); // drop the now-size-1 channel axis, back to plain 2D
    showText("greyU8", "extent {" + std::to_string(greyU8.extent()[0]) + ", " + std::to_string(greyU8.extent()[1]) + "}, single channel");

    std::vector<double> greyDblData(greyU8.size());
    Image<double, 2> grey(greyDblData.data(), greyU8); // deep copy, converts uint8_t -> double for Sobel's signed math

    std::vector<double> sobelXData = { -1,0,1,  -2,0,2,  -1,0,1 };
    std::vector<double> sobelYData = { -1,-2,-1,  0,0,0,  1,2,1 };
    Image<double, 2> sobelX(sobelXData.data(), { 3, 3 });
    Image<double, 2> sobelY(sobelYData.data(), { 3, 3 });

    step("grey.convolve(sobelX, gx, BorderMode::Reflect); grey.convolve(sobelY, gy, BorderMode::Reflect)",
        "Two more 3x3 kernels -- proof convolve() takes any weights, not just symmetric blur-style\n"
        "             ones. sobelX responds to horizontal brightness change (vertical edges), sobelY to\n"
        "             vertical change (horizontal edges); a flat region gives 0 from both, which is why\n"
        "             this runs on double-precision grey rather than uint8_t -- the results are signed.");
    showArray("sobelX", sobelX);
    showArray("sobelY", sobelY);

    std::vector<double> gxData(grey.size()), gyData(grey.size());
    Image<double, 2> gx(gxData.data(), grey.extent()), gy(gyData.data(), grey.extent());
    grey.convolve(sobelX, gx, BorderMode::Reflect);
    grey.convolve(sobelY, gy, BorderMode::Reflect);

    step("gx.multiply(gx, gx2); gy.multiply(gy, gy2); gx2.add(gy2, magSq); then sqrt() elementwise",
        "Gradient magnitude is sqrt(gx^2 + gy^2) -- built here from the non-mutating arithmetic\n"
        "             methods (multiply/add) added alongside convolve(), rather than a hand-written loop,\n"
        "             then a sqrt+clamp pass (toDisplayable) converts back to a displayable 8-bit image,\n"
        "             saved as a single-channel (greyscale) PNG.");
    std::vector<double> gx2Data(grey.size()), gy2Data(grey.size()), magSqData(grey.size());
    Image<double, 2> gx2(gx2Data.data(), grey.extent()), gy2(gy2Data.data(), grey.extent()), magSq(magSqData.data(), grey.extent());
    gx.multiply(gx, gx2);
    gy.multiply(gy, gy2);
    gx2.add(gy2, magSq);

    std::vector<double> magData(grey.size());
    Image<double, 2> magnitude(magData.data(), grey.extent());
    {
        auto sqIt = magSq.begin();
        for (auto it = magnitude.begin(); it != magnitude.end(); ++it, ++sqIt) *it = std::sqrt(*sqIt);
    }

    std::vector<uint8_t> edgeData(grey.size());
    Image<uint8_t, 3> edgeImage(edgeData.data(), { 1, grey.extent()[0], grey.extent()[1] });
    Image<uint8_t, 2> edgeChannel = edgeImage.slice(0, 0);
    toDisplayable(magnitude, edgeChannel);
    saveForInspection("Sobel edge magnitude", edgeImage, "05_sobel_edges.png");

    // ------------------------------------------------------------------
    // PART 5: arbitrary kernels -- sharpen and emboss
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 5: arbitrary kernels ===\n";

    std::vector<double> sharpenData = { 0,-1,0,  -1,5,-1,  0,-1,0 };
    Image<double, 2> sharpenKernel(sharpenData.data(), { 3, 3 });
    std::vector<uint8_t> sharpenedData(photo.size());
    Image<uint8_t, 3> sharpened(sharpenedData.data(), photoExtent);

    step("convolveColorSafe(photo, sharpenKernel, sharpened, BorderMode::Reflect)",
        "convolve() places no restriction on kernel weights -- here center=5, neighbors=-1, weights\n"
        "             still summing to 1 like the blur kernels, but the negative neighbors subtract off\n"
        "             nearby brightness, which *increases* local contrast at edges instead of smoothing it\n"
        "             out. Sharpening can genuinely overshoot below 0 or above 255 at strong edges, which\n"
        "             is exactly the case convolveColorSafe() (double intermediate + explicit clamp) exists\n"
        "             for, instead of convolve()'s own uint8_t output path.");
    showArray("sharpenKernel", sharpenKernel);
    convolveColorSafe(photo, sharpenKernel, sharpened, BorderMode::Reflect);
    saveForInspection("sharpened photo", sharpened, "06_sharpen.png");

    std::vector<double> embossData = { -2,-1,0,  -1,1,1,  0,1,2 };
    Image<double, 2> embossKernel(embossData.data(), { 3, 3 });
    std::vector<uint8_t> embossedData(photo.size());
    Image<uint8_t, 3> embossed(embossedData.data(), photoExtent);

    step("convolveColorSafe(photo, embossKernel, embossed, BorderMode::Reflect, 128.0)",
        "An asymmetric kernel: it responds to change along one diagonal and is flat along the other,\n"
        "             so flat regions of the photo collapse toward 0 (black) rather than toward their own\n"
        "             brightness. The usual fix -- applied here via toDisplayable()'s bias argument -- is\n"
        "             to add 128 back so 'no change' lands on mid-grey instead of black.");
    showArray("embossKernel", embossKernel);
    convolveColorSafe(photo, embossKernel, embossed, BorderMode::Reflect, 128.0);
    saveForInspection("embossed photo", embossed, "07_emboss.png");

    // ------------------------------------------------------------------
    // PART 6: the same operations again, via the frequency domain
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 6: the frequency domain ===\n";

    step("Image<uint8_t,3> crop = marbles.view({0,581,372}, {2,836,627});   // 256x256, all 3 channels",
        "fftn() needs every dimension's extent to be a power of two (it's asserted -- see fft.h), and\n"
        "             neither photo nor marbles is one in either dimension. view() crops out a 256x256\n"
        "             region instead of touching the whole image -- the same view() from demo/multiview,\n"
        "             just now feeding fftn() rather than being printed. marbles rather than photo on\n"
        "             purpose here, same reason Part 4 switched to it for Sobel: photo's real film-grain\n"
        "             noise spreads energy across every frequency roughly evenly, which swamps the log-\n"
        "             magnitude spectrum below into near-uniform static rather than showing readable\n"
        "             structure -- marbles' cleaner per-pixel contrast doesn't have that problem. Every\n"
        "             step below works on this crop.");
    Image<uint8_t, 3> crop = marbles.view({ 0, 581, 372 }, { 2, 836, 627 });
    saveForInspection("crop", crop, "08_crop.png");

    std::vector<uint8_t> greyCrop3Data((size_t)crop.extent()[1] * crop.extent()[2]);
    Image<uint8_t, 3> greyCrop3(greyCrop3Data.data(), { 1, crop.extent()[1], crop.extent()[2] });
    crop.mean(0, greyCrop3);
    Image<uint8_t, 2> greyCropU8 = greyCrop3.slice(0, 0);
    std::vector<double> greyCropDblData(greyCropU8.size());
    Image<double, 2> greyCropDbl(greyCropDblData.data(), greyCropU8); // deep copy, uint8_t -> double

    step("fftn<double,2>(greyCropDbl, freq)   // greyscale, so there's one spectrum to look at, not three",
        "The magnitude of each complex output value says how much of that frequency is present in the\n"
        "             crop -- low frequencies (slow brightness changes, like the overall lighting) near the\n"
        "             corners of the raw output, high frequencies (sharp edges, fine texture) further out.\n"
        "             Displaying that directly is unreadable: fftshift() below recenters it (a numpy.fft\n"
        "             convention -- DC in the middle, not the corner) and a log(1+magnitude) scale tames\n"
        "             its enormous dynamic range (the DC term alone is the sum of all 65536 pixels) so\n"
        "             faint high-frequency detail doesn't just disappear next to it.");
    std::vector<std::complex<double>> greyFreqData((size_t)crop.extent()[1] * crop.extent()[2]);
    Image<std::complex<double>, 2> greyFreq(greyFreqData.data(), greyCropU8.extent());
    fftn<double, 2>(greyCropDbl, greyFreq);

    std::vector<double> logMagData(greyCropU8.size());
    Image<double, 2> logMag(logMagData.data(), greyCropU8.extent());
    {
        auto fIt = greyFreq.begin();
        for (auto it = logMag.begin(); it != logMag.end(); ++it, ++fIt) *it = std::log(1.0 + std::abs(*fIt));
    }
    std::vector<double> magShiftedData(greyCropU8.size());
    Image<double, 2> magShifted(magShiftedData.data(), greyCropU8.extent());
    fftshift(logMag, magShifted);

    double maxMag = magShifted.max();
    std::vector<uint8_t> spectrumData(greyCropU8.size());
    Image<uint8_t, 3> spectrumImage(spectrumData.data(), { 1, greyCropU8.extent()[0], greyCropU8.extent()[1] });
    Image<uint8_t, 2> spectrumChannel = spectrumImage.slice(0, 0);
    {
        auto mIt = magShifted.begin();
        for (auto it = spectrumChannel.begin(); it != spectrumChannel.end(); ++it, ++mIt) *it = (uint8_t)((*mIt / maxMag) * 255.0);
    }
    saveForInspection("log-magnitude spectrum (fftshifted)", spectrumImage, "09_spectrum.png");

    step("fftCorrelateColor(crop, boxKernel, fftBoxBlurred)   // same 3x3 boxKernel from Part 2",
        "fftCorrelateColor() takes each channel to the frequency domain, multiplies by the kernel's\n"
        "             spectrum (conjugated -- convolve() computes a correlation, not a textbook convolution;\n"
        "             see the comment on fftCorrelateColor() in this file, or\n"
        "             testFFTMatchesSpatialConvolution in unitTests.cpp, for the exact identity and why),\n"
        "             and transforms back. That's a real, independent computation of the *same* answer\n"
        "             Part 2's convolveColor(crop, boxKernel, ..., BorderMode::Wrap) would give -- computed\n"
        "             below for direct comparison rather than taken on faith.");
    std::vector<uint8_t> fftBoxBlurredData(crop.size());
    Image<uint8_t, 3> fftBoxBlurred(fftBoxBlurredData.data(), crop.extent());
    fftCorrelateColor(crop, boxKernel, fftBoxBlurred);
    saveForInspection("FFT-domain box blur", fftBoxBlurred, "10_fft_box_blur.png");

    std::vector<uint8_t> spatialBoxBlurredData(crop.size());
    Image<uint8_t, 3> spatialBoxBlurred(spatialBoxBlurredData.data(), crop.extent());
    convolveColor(crop, boxKernel, spatialBoxBlurred, BorderMode::Wrap);
    saveForInspection("spatial-domain box blur (BorderMode::Wrap, for a fair comparison)", spatialBoxBlurred, "11_spatial_box_blur_wrap.png");

    int maxPixelDiff = 0;
    for (const auto& coord : crop.coordinates())
        maxPixelDiff = std::max(maxPixelDiff, std::abs((int)fftBoxBlurred.at(coord) - (int)spatialBoxBlurred.at(coord)));
    showText("largest per-pixel difference between the two", std::to_string(maxPixelDiff) + " (out of 0-255) -- 10 and 11 should look identical");

    step("timing: spatial convolve() vs FFT correlation, at two very different kernel sizes",
        "convolve()'s cost scales with image size TIMES kernel size (every output pixel visits every\n"
        "             kernel tap); fftn()'s cost scales with image size alone (kernel size only changes how\n"
        "             the kernel gets *built*, not the transform cost) -- so which one wins depends entirely\n"
        "             on the kernel. A 3x3 kernel is 9 taps; a 51x51 one is 2601, ~300x more spatial work for\n"
        "             the exact same image, while the FFT side barely changes (same 7 image-sized transforms\n"
        "             either way -- 1 for the kernel, 2 per channel). Averaged over a few repetitions below.");
    std::vector<double> box51Data((size_t)51 * 51, 1.0 / (51.0 * 51.0));
    Image<double, 2> box51Kernel(box51Data.data(), { 51, 51 });
    std::vector<uint8_t> timingOutData(crop.size());
    Image<uint8_t, 3> timingOut(timingOutData.data(), crop.extent());
    const int reps = 3;

    auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; r++) convolveColor(crop, boxKernel, timingOut, BorderMode::Wrap);
    auto t1 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; r++) fftCorrelateColor(crop, boxKernel, timingOut);
    auto t2 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; r++) convolveColor(crop, box51Kernel, timingOut, BorderMode::Wrap);
    auto t3 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; r++) fftCorrelateColor(crop, box51Kernel, timingOut);
    auto t4 = std::chrono::steady_clock::now();

    auto ms = [reps](std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count() / reps;
    };
    showText("3x3 kernel  -- spatial convolve()", std::to_string(ms(t0, t1)) + " ms/call");
    showText("3x3 kernel  -- FFT correlation", std::to_string(ms(t1, t2)) + " ms/call");
    showText("51x51 kernel -- spatial convolve()", std::to_string(ms(t2, t3)) + " ms/call");
    showText("51x51 kernel -- FFT correlation", std::to_string(ms(t3, t4)) + " ms/call");

    std::cout <<
        "\n\nAll outputs written to: " << outputDir << "\n"
        "Open 01_original.png alongside the rest to compare by eye: 02/03/04 should look\n"
        "progressively softer, 05 should show bright edges on a dark background, 06 should look\n"
        "crisper than the original, and 07 should look like a grey relief carving. 09 is what the\n"
        "256x256 crop (08) looks like in the frequency domain -- a bright center fading outward, with\n"
        "any strong directional texture in the crop showing up as streaks through it. 10 and 11 should\n"
        "be visually indistinguishable -- two independently computed answers to the same question, one\n"
        "from the spatial domain and one from the frequency domain.\n";

    return 0;
}
