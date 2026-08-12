#include <ndl/image.h>
#include <ndl/imageIO.h>
#include <ndl/morphology.h>
#include <ndl/convolution.h>
#include <ndl/distance_transform.h>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cstdint>
#include <cmath>
#include <vector>
#include <array>
#include <string>

using namespace ndl;
namespace fs = std::filesystem;

// A step-by-step tour of ndl::distance_transform()/distance_transform_squared()
// -- free functions, distance_transform.h, not Image members: see
// image/core.h's own comment for why -- in the same spirit as
// demo/convolution and demo/morphology: each step shows the code, explains
// it, and shows the result -- first on small numbers you can check by
// hand, then on a real photo whose results you check by *looking at the
// saved PNG*.

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

// Normalizes a floating-point distance field to a viewable 8-bit greyscale
// image: 0 (nearest a background pixel) is black, the field's own max is
// white. Demo/display infrastructure only, not itself a step being taught
// here -- distance_transform() itself already produced the real distances.
void distanceToGreyscale(const Image<double, 2>& dist, Image<uint8_t, 3>& out)
{
    double maxD = dist.max();
    auto channel = out.slice(0, 0);
    auto it = channel.begin();
    for (auto dIt = dist.begin(); dIt != dist.end(); ++dIt, ++it)
        *it = (uint8_t)(maxD <= 0 ? 0 : (*dIt / maxD) * 255.0);
    // Duplicate the single channel into the other two so the saved PNG is
    // plain greyscale (R==G==B) rather than looking tinted.
    auto ch1 = out.slice(0, 1), ch2 = out.slice(0, 2);
    ch1 = channel;
    ch2 = channel;
}

int main()
{
    outputDir = NDL_DISTANCE_TRANSFORM_OUTPUT_DIR;
    fs::create_directories(outputDir);
    std::string dataDir = NDL_TEST_DATA_DIR;

    std::cout <<
        "This demo teaches ndl::distance_transform()/distance_transform_squared() the\n"
        "same way demo/convolution teaches convolve(): each step shows the code, explains\n"
        "it, and shows the result -- first on small numbers you can check by hand, then\n"
        "on a real photo whose results you check by *looking at the saved PNG*. Output\n"
        "PNGs land in:\n    " << outputDir << "\n";

    // ------------------------------------------------------------------
    // PART 1: the mechanics, on a 1D row you can check by hand
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 1: distance_transform() on a 1D row ===\n";

    bool rowData[10] = { false, true, true, true, false, true, true, false, true, true };
    Image<bool, 1> row(rowData, { 10 });

    step("distance_transform(row, dist)",
        "distance_transform(src, dst) writes, at every position, the (true Euclidean) distance to\n"
        "             the nearest position where src is false/zero -- the same convention OpenCV's own\n"
        "             distanceTransform() uses: a false/background pixel is trivially distance 0, and\n"
        "             distance grows the deeper into a run of true/foreground pixels you go. Reading `row`\n"
        "             by hand: positions 0,4,7 are false (distance 0); position 1 is 1 step from position 0;\n"
        "             position 2 is 2 steps from either false neighbor; and so on.");
    double distData[10];
    Image<double, 1> dist(distData, { 10 });
    distance_transform(row, dist);
    showArray("row (1=foreground)", row);
    showArray("dist", dist);

    step("distance_transform_squared(row, distSq)",
        "The squared version skips the final sqrt() -- exact, and often what you actually want if\n"
        "             you're only ever going to compare distances against each other or against a squared\n"
        "             threshold, since sqrt is both extra work and (for non-perfect-square results) where\n"
        "             floating-point rounding first enters the picture. distance_transform() itself is just\n"
        "             this plus one elementwise sqrt pass.");
    double distSqData[10];
    Image<double, 1> distSq(distSqData, { 10 });
    distance_transform_squared(row, distSq);
    showArray("distSq", distSq);

    // ------------------------------------------------------------------
    // PART 2: a 2D grid, and the invert() trick
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 2: a 2D grid, and distance-to-foreground via invert() ===\n";

    bool gridData[25] = {
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,1,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
    };
    Image<bool, 2> grid(gridData, { 5, 5 });

    step("distance_transform(grid, gridDist)   // one foreground point at the center",
        "A single foreground point at (2,2), everything else background: the distance transform's\n"
        "             own convention (distance to nearest BACKGROUND pixel) means every position except\n"
        "             (2,2) itself reads 0 here, since (2,2)'s immediate neighbors are already background --\n"
        "             the single foreground point is 1 step from a background pixel in every direction, so\n"
        "             gridDist(2,2) should read exactly 1.");
    double gridDistData[25];
    Image<double, 2> gridDist(gridDistData, { 5, 5 });
    distance_transform(grid, gridDist);
    showArray("grid", grid);
    showArray("gridDist", gridDist);

    step("ndl::invert(grid, invertedGrid); distance_transform(invertedGrid, distToForeground);",
        "Want distance to the nearest FOREGROUND pixel instead? distance_transform.h doesn't need its\n"
        "             own separate flag for that -- invert() (morphology.h) flips the source first, so\n"
        "             'nearest background pixel of the inverted image' becomes 'nearest foreground pixel of\n"
        "             the original'. Now every position reads its Chebyshev-flavored distance to (2,2) --\n"
        "             the corners (distance^2 = 2^2+2^2 = 8, i.e. distance ~2.83) should read the largest\n"
        "             values, and (2,2) itself reads exactly 0.");
    bool invertedGridData[25];
    Image<bool, 2> invertedGrid(invertedGridData, { 5, 5 });
    ndl::invert(grid, invertedGrid);
    double distToForegroundData[25];
    Image<double, 2> distToForeground(distToForegroundData, { 5, 5 });
    distance_transform(invertedGrid, distToForeground);
    showArray("distToForeground", distToForeground);

    // ------------------------------------------------------------------
    // PART 3: a real photo -- threshold, then transform
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 3: a real photo ===\n";

    std::array<int, 3> photoExtent;
    std::vector<uint8_t> photoData = image_io::load(dataDir + "/ng_bwgirl_crop.jpg", photoExtent);
    Image<uint8_t, 3> photo(photoData.data(), photoExtent);
    saveForInspection("photo", photo, "01_original.png");

    OwnedImage<uint8_t, 3> grey3({ 1, photo.extent()[1], photo.extent()[2] });
    photo.mean(0, grey3);
    Image<uint8_t, 2> grey = grey3.slice(0, 0);

    step("gaussian_blur(grey, greySmooth, 1.5, BorderMode::Clamp)",
        "grey is a real scanned photo, so it carries real film-grain noise -- fine, speckled variation\n"
        "             baked into every pixel, the Gaussian-sensor-noise case demo/morphology's Part 4\n"
        "             contrasts against salt-and-pepper (median_filter() territory): a gaussian_blur() is the\n"
        "             right tool for grain like this, not median_filter(), since the corruption is already a\n"
        "             small continuous jitter rather than isolated 0/255 outliers. Skipping this step and\n"
        "             thresholding grey directly would carry that per-pixel jitter straight through\n"
        "             otsu_threshold()'s single global cutoff, speckling the binary mask -- and, further\n"
        "             downstream, distance_transform() reads every one of those stray flipped pixels as its\n"
        "             own tiny island of foreground/background, fracturing what should be one smooth distance\n"
        "             field into a field full of tiny local maxima.");
    OwnedImage<uint8_t, 2> greySmooth(grey.extent());
    // Explicit ImageT: grey is an Image<uint8_t,2> (a slice()'d view),
    // greySmooth an OwnedImage<uint8_t,2> (its own concrete type) -- same
    // deduction wrinkle demo/morphology's median_filter() call below hits,
    // resolved the same way.
    gaussian_blur<Image<uint8_t, 2>>(grey, greySmooth, 1.5, BorderMode::Clamp);

    step("uint8_t t = otsu_threshold(greySmooth); threshold(greySmooth, maskU8, t, 1, 0);",
        "distance_transform() needs a binary-ish source, so the smoothed greyscale photo is thresholded --\n"
        "             otsu_threshold() (morphology.h, Histogram<1>-backed, see demo/histogram) picks the\n"
        "             cutoff automatically, the same way demo/morphology's Part 7 does.");
    uint8_t t = otsu_threshold(greySmooth);
    OwnedImage<uint8_t, 2> maskU8(greySmooth.extent());
    threshold(greySmooth, maskU8, t, (uint8_t)1, (uint8_t)0);

    OwnedImage<uint8_t, 3> maskImg({ 1, grey.extent()[0], grey.extent()[1] });
    Image<uint8_t, 2> maskImgChannel = maskImg.slice(0, 0);
    threshold(greySmooth, maskImgChannel, t, (uint8_t)255, (uint8_t)0);
    saveForInspection("binary mask (smoothed, then otsu-thresholded)", maskImg, "02_mask.png");

    step("distance_transform(maskU8, dt)   // distance to the nearest background (0) pixel",
        "distance_transform()'s source doesn't need to literally be bool -- any arithmetic value_type\n"
        "             works, nonzero read as foreground (the same convention convolve()'s kernel taps and\n"
        "             threshold()'s own onValue/offValue already use), so maskU8's 0/1 values feed straight\n"
        "             in with no conversion. Every background (0) pixel is trivially 0, and foreground (1)\n"
        "             pixels grow brighter the deeper inside a bright blob they are. Saved with 0 mapped to\n"
        "             black and the field's own maximum mapped to white, so 03_distance.png should look like\n"
        "             a soft, glowing version of 02_mask.png's white regions.");
    std::vector<double> photoDistData(grey.extent()[0] * grey.extent()[1]);
    Image<double, 2> photoDist(photoDistData.data(), grey.extent());
    distance_transform(maskU8, photoDist);
    showText("max distance found", std::to_string(photoDist.max()));

    OwnedImage<uint8_t, 3> distImg({ 3, grey.extent()[0], grey.extent()[1] });
    distanceToGreyscale(photoDist, distImg);
    saveForInspection("distance field (0=black, max=white)", distImg, "03_distance.png");

    std::cout <<
        "\n\nAll outputs written to: " << outputDir << "\n"
        "01 is the original photo. 02 is its (smoothed first, then) Otsu-thresholded binary mask --\n"
        "smoothing away the photo's own film grain before thresholding is what keeps this mask free of\n"
        "the stray single-pixel speckling that grain would otherwise punch through a raw threshold. 03 is\n"
        "the distance transform of that mask, visualized as greyscale -- it should look like a soft glow\n"
        "filling the interior of 02's bright regions, brightest at each region's own 'deepest' point (its\n"
        "approximate medial axis) and fading smoothly to black at every edge, not fractured by grain-sized\n"
        "local maxima.\n";

    return 0;
}
