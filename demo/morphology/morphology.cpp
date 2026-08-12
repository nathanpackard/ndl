#include <ndl/image.h>
#include <ndl/morphology.h>
#include <ndl/convolution.h>
#include <ndl/imageIO.h>
#include <iostream>
#include <filesystem>
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <random>
#include "../demoHelpers.h"

using namespace ndl;
namespace fs = std::filesystem;

// A step-by-step tour of ndl::erode()/ndl::dilate()/ndl::median_filter()/
// ndl::percentile_filter() -- free functions, morphology.h, not Image
// members: see image/core.h's own comment for why -- in the same spirit as
// demo/convolution: each step shows the code, explains it, and shows the
// result -- first on a small hand-checkable grid, then on real images,
// saved as PNGs for visual inspection alongside the numbers.
//
// step()/showArray()/showText()/saveForInspection()/outputDir come from
// demoHelpers.h, shared with every other demo.

// Per-channel helpers, same reasoning as demo/convolution's convolveColor():
// erode()/dilate()/median_filter() run one color channel at a time so colors
// don't bleed into each other -- per_channel() (from image.h) does the
// slice()-and-loop mechanics once, shared with demo/convolution, so each of
// these is just its underlying Image method plumbed through. Unlike
// convolve() with an arbitrary (possibly negative-weight) kernel, none of
// these three can produce a value outside the input's own range --
// min/max/percentile always return one of the actual input values -- so
// there's no clamping/double-precision-intermediate concern here the way
// sharpen/emboss needed in demo/convolution.
void erodeColor(const Image<uint8_t, 3>& src, const Image<double, 2>& kernel, Image<uint8_t, 3>& dst, BorderMode border)
{
    per_channel(src, dst, 0, [&](const auto& s, auto& d) { ndl::erode(s, d, kernel, border); });
}
void dilateColor(const Image<uint8_t, 3>& src, const Image<double, 2>& kernel, Image<uint8_t, 3>& dst, BorderMode border)
{
    per_channel(src, dst, 0, [&](const auto& s, auto& d) { ndl::dilate(s, d, kernel, border); });
}
void medianColor(const Image<uint8_t, 3>& src, const Image<double, 2>& kernel, Image<uint8_t, 3>& dst, BorderMode border)
{
    per_channel(src, dst, 0, [&](const auto& s, auto& d) { ndl::median_filter(s, d, kernel, border); });
}
void percentileColor(const Image<uint8_t, 3>& src, const Image<double, 2>& kernel, Image<uint8_t, 3>& dst, double percentile, BorderMode border)
{
    per_channel(src, dst, 0, [&](const auto& s, auto& d) { ndl::percentile_filter(s, d, kernel, percentile, border); });
}

// Corrupts a fixed fraction of pixels to pure black or pure white --
// classic salt-and-pepper noise, the specific kind of corruption a median
// filter is the standard tool for (as opposed to Gaussian sensor noise,
// which a mean/gaussian blur handles better). Deterministic (fixed seed) so
// the demo produces the same picture every run.
void addSaltAndPepperNoise(const Image<uint8_t, 3>& src, Image<uint8_t, 3>& dst, double fraction)
{
    dst = src;
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> chance(0.0, 1.0);
    std::uniform_int_distribution<int> blackOrWhite(0, 1);
    int W = src.extent()[1], H = src.extent()[2];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (chance(rng) < fraction)
            {
                uint8_t v = blackOrWhite(rng) ? 255 : 0;
                for (int c = 0; c < src.extent()[0]; c++) dst(c, x, y) = v;
            }
}

int main()
{
    outputDir = NDL_MORPHOLOGY_OUTPUT_DIR;
    fs::create_directories(outputDir);
    std::string dataDir = NDL_TEST_DATA_DIR;

    std::cout <<
        "This demo teaches ndl::erode()/ndl::dilate()/ndl::median_filter()/ndl::percentile_filter()\n"
        "the same way demo/convolution teaches convolve(): each step shows the code, explains\n"
        "it, and shows the result -- first on small numbers you can check by hand, then on\n"
        "real images whose results you check by *looking at the saved PNG*. Output PNGs\n"
        "land in:\n    " << outputDir << "\n";

    // ------------------------------------------------------------------
    // PART 1: the mechanics, on numbers you can check by hand
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 1: how erode()/dilate()/median_filter() work ===\n";

    OwnedImage<int, 2> grid({ 5, 5 });
    { int i = 0; for (auto it = grid.begin(); it != grid.end(); ++it) *it = ++i; }

    step("Image<int,2> grid(data, {5,5});  ... fill 1..25",
        "The same 5x5 grid demo/convolution's Part 1 used. erode()/dilate()/median_filter()\n"
        "             all compute one output value per input position from a neighborhood around\n"
        "             it, same as convolve() -- they just combine the neighborhood differently.");
    showArray("grid", grid);

    OwnedImage<double, 2> box3({ 3, 3 });
    make_box_kernel(box3);
    OwnedImage<double, 2> cross3({ 3, 3 });
    make_cross_kernel(cross3);

    step("make_box_kernel(box3); make_cross_kernel(cross3);   // both 3x3",
        "Two structuring-element shapes, built once and reused below for both morphology and\n"
        "             (later) convolve(): make_box_kernel() marks every tap 1 (the full 3x3\n"
        "             neighborhood); make_cross_kernel() marks only the center and the 4 taps that\n"
        "             vary along exactly one axis -- a plus sign. Both are ordinary kernel Images:\n"
        "             nonzero = included, same convention convolve() already uses.");
    showArray("box3", box3);
    showArray("cross3", cross3);

    OwnedImage<int, 2> out1({ 5, 5 });

    step("ndl::erode(grid, out1, box3)",
        "erode() replaces each value with the MINIMUM of its neighborhood -- out1(2,2) should\n"
        "             be the smallest of the 3x3 block around grid's center (7,8,9,12,13,14,17,18,19):\n"
        "             7. Bright regions shrink, dark regions grow -- the classic morphological reading.");
    ndl::erode(grid, out1, box3);
    showArray("out1 (box erode)", out1);
    showText("out1(2,2)", std::to_string(out1(2, 2)) + "  (expected 7)");

    ndl::dilate(grid, out1, box3);
    step("ndl::dilate(grid, out1, box3)",
        "dilate() is the mirror image: the MAXIMUM of the neighborhood. out1(2,2) should be 19,\n"
        "             the largest of that same 3x3 block.");
    showArray("out1 (box dilate)", out1);
    showText("out1(2,2)", std::to_string(out1(2, 2)) + "  (expected 19)");

    ndl::median_filter(grid, out1, box3);
    step("ndl::median_filter(grid, out1, box3)   // == ndl::percentile_filter(grid, out1, box3, 50.0)",
        "The middle value of the sorted 3x3 neighborhood (1,7,8,9,12,13,14,17,18,19 minus the\n"
        "             one that isn't included -- 9 values, so the 5th) -- 13, the same value the\n"
        "             center already held here, since this grid has no outliers. median_filter() only\n"
        "             gets interesting on noisy data, which Part 3 below gets to.");
    showArray("out1 (median)", out1);
    showText("out1(2,2)", std::to_string(out1(2, 2)) + "  (expected 13)");

    step("ndl::erode(grid, out1, cross3) vs ndl::erode(grid, out1, box3)   // shape changes the answer",
        "Swapping the box for the cross changes which 5 values (not 9) are considered: only\n"
        "             8,12,13,14,18 (center plus its 4-neighbors) -- so out1(2,2) is still checkable\n"
        "             by hand, but from a smaller, differently-shaped set.");
    ndl::erode(grid, out1, cross3);
    showText("out1(2,2) with cross3", std::to_string(out1(2, 2)) + "  (expected 8, min of 8,12,13,14,18)");
    ndl::dilate(grid, out1, cross3);
    showText("out1(2,2) with cross3", std::to_string(out1(2, 2)) + "  (expected 18, max of 8,12,13,14,18)");

    // ------------------------------------------------------------------
    // PART 2: box vs cross, made visible -- dilating a single point
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 2: box vs cross, made visible ===\n";

    OwnedImage<int, 2> dot({ 11, 11 });
    dot(5, 5) = 1;

    OwnedImage<double, 2> box5({ 5, 5 });
    make_box_kernel(box5);
    OwnedImage<double, 2> cross5({ 5, 5 });
    make_cross_kernel(cross5);

    step("ndl::dilate(dot, boxGrown, box5) vs ndl::dilate(dot, crossGrown, cross5)   // radius-2 kernels",
        "Dilating a single 1-pixel dot with a structuring element traces out that element's own\n"
        "             shape exactly (same idea as demo/convolution's impulse-response step for\n"
        "             gaussian_blur() -- the dilation of one point by a shape is just that shape,\n"
        "             translated to the point): a box grows the dot into a solid 5x5 SQUARE, a cross\n"
        "             grows it into a thin 5-long PLUS SIGN, not a filled diamond -- only the 4 axis\n"
        "             arms turn on, the diagonal-adjacent cells stay 0, printed below side by side.");
    OwnedImage<int, 2> boxGrown({ 11, 11 });
    ndl::dilate(dot, boxGrown, box5);
    OwnedImage<int, 2> crossGrown({ 11, 11 });
    ndl::dilate(dot, crossGrown, cross5);
    showArray("boxGrown (square)", boxGrown);
    showArray("crossGrown (plus sign)", crossGrown);

    // ------------------------------------------------------------------
    // PART 3: erode/dilate on a real image
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 3: erode/dilate on a real photo ===\n";

    OwnedImage<uint8_t, 3> marblesRaw = image_io::load_owned(dataDir + "/marbles.bmp");
    // marbles.bmp is a large photo (1419x1001); ndl::downsample() (convolution.h)
    // keeps every image this Part saves (and the docs generated from them)
    // a reasonable size, without changing anything about how erode()/
    // dilate() themselves work.
    OwnedImage<uint8_t, 3> marbles = downsample(marblesRaw, 2);
    saveForInspection("marbles", marbles, "01_original.png");

    OwnedImage<uint8_t, 3> eroded(marbles.extent());
    step("erodeColor(marbles, box5, eroded, BorderMode::Clamp)",
        "erode() on a real photo shrinks bright regions and thickens dark ones -- the bright\n"
        "             highlights on each marble should visibly shrink, and the dark gaps between\n"
        "             marbles should visibly thicken.");
    erodeColor(marbles, box5, eroded, BorderMode::Clamp);
    saveForInspection("eroded", eroded, "02_eroded.png");

    OwnedImage<uint8_t, 3> dilated(marbles.extent());
    step("dilateColor(marbles, box5, dilated, BorderMode::Clamp)",
        "The mirror image: bright regions grow, dark regions shrink -- compare 02_eroded.png\n"
        "             and 03_dilated.png against 01_original.png side by side.");
    dilateColor(marbles, box5, dilated, BorderMode::Clamp);
    saveForInspection("dilated", dilated, "03_dilated.png");

    // ------------------------------------------------------------------
    // PART 4: median_filter vs gaussian_blur, on salt-and-pepper noise
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 4: median_filter vs gaussian_blur on noisy data ===\n";

    Image<uint8_t, 3> crop = marbles.view({ 0, 226, 122 }, { 2, 481, 377 }); // 256x256, all 3 channels
    saveForInspection("crop", crop, "04_crop.png");

    OwnedImage<uint8_t, 3> noisy(crop.extent());
    step("addSaltAndPepperNoise(crop, noisy, 0.05)   // 5% of pixels forced to pure black or white",
        "Salt-and-pepper noise -- scattered pixels forced to 0 or 255, nothing in between -- is\n"
        "             the specific corruption median_filter() (unlike gaussian_blur()) is good at\n"
        "             removing, because its output is always one of the surviving nearby pixel values,\n"
        "             never an average that a single extreme outlier can drag toward itself.");
    addSaltAndPepperNoise(crop, noisy, 0.05);
    saveForInspection("noisy", noisy, "05_noisy.png");

    OwnedImage<uint8_t, 3> medianCleaned(crop.extent());
    step("medianColor(noisy, box3, medianCleaned, BorderMode::Clamp)",
        "Each noisy pixel is very likely to be outvoted by its (uncorrupted) neighbors' median,\n"
        "             so it gets replaced outright -- edges and texture should come back looking sharp,\n"
        "             not blurry, because every output pixel is a real pixel value from somewhere\n"
        "             nearby, not a blend.");
    medianColor(noisy, box3, medianCleaned, BorderMode::Clamp);
    saveForInspection("median-cleaned", medianCleaned, "06_median_cleaned.png");

    OwnedImage<uint8_t, 3> gaussianCleaned(crop.extent());
    step("gaussianBlurColor-style: ndl::gaussian_blur(noisy.slice(0,c), ..., 1.5, BorderMode::Clamp) per channel",
        "For comparison: the same noisy image run through demo/convolution's Part 3 tool instead.\n"
        "             A gaussian blur averages every pixel with its neighbors, including the 0s and\n"
        "             255s -- so instead of removing the noise it smears each corrupted pixel into a\n"
        "             soft grey/white smudge over its neighborhood, and blurs real edges at the same\n"
        "             time. Compare 06_median_cleaned.png (sharp) against 07_gaussian_cleaned.png\n"
        "             (smudged) directly.");
    per_channel(noisy, gaussianCleaned, 0, [](const auto& s, auto& d) { ndl::gaussian_blur(s, d, 1.5, BorderMode::Clamp); });
    saveForInspection("gaussian-cleaned", gaussianCleaned, "07_gaussian_cleaned.png");

    long medianDiff = 0, gaussianDiff = 0;
    for (const auto& coord : crop.coordinates())
    {
        medianDiff += std::abs((int)medianCleaned.at(coord) - (int)crop.at(coord));
        gaussianDiff += std::abs((int)gaussianCleaned.at(coord) - (int)crop.at(coord));
    }
    showText("total |cleaned - original crop| difference, median", std::to_string(medianDiff));
    showText("total |cleaned - original crop| difference, gaussian", std::to_string(gaussianDiff) + "  (lower is closer to the noise-free original)");

    // ------------------------------------------------------------------
    // PART 5: percentile_filter -- the erode/median/dilate continuum
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 5: percentile_filter() ===\n";

    step("percentileColor(noisy, box3, out, p, BorderMode::Clamp)   for p in {10, 50, 90}",
        "percentile_filter() generalizes all three: percentile 0 is erode() (the minimum),\n"
        "             100 is dilate() (the maximum), 50 is median_filter(). In between is a genuine\n"
        "             'soft' erode/dilate -- p=10 shrinks bright regions like erode but is more\n"
        "             resistant to a single stray dark noise pixel, since it takes the 10th-ranked\n"
        "             value of the neighborhood rather than the strict minimum.");
    OwnedImage<uint8_t, 3> p10(crop.extent()), p50(crop.extent()), p90(crop.extent());
    percentileColor(noisy, box3, p10, 10.0, BorderMode::Clamp);
    percentileColor(noisy, box3, p50, 50.0, BorderMode::Clamp);
    percentileColor(noisy, box3, p90, 90.0, BorderMode::Clamp);
    saveForInspection("percentile 10 (soft erode)", p10, "08_percentile_10.png");
    saveForInspection("percentile 50 (== median_filter)", p50, "09_percentile_50.png");
    saveForInspection("percentile 90 (soft dilate)", p90, "10_percentile_90.png");

    // ------------------------------------------------------------------
    // PART 6: opening and closing -- composing erode/dilate, no new code needed
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 6: opening and closing ===\n";

    step("erodeColor(noisy, ...) then dilateColor(..., box3, opened, ...)   -- \"opening\"",
        "Two of the operations above, composed: erode then dilate (\"opening\" in the standard\n"
        "             morphology vocabulary) removes small bright specks -- salt noise, mostly --\n"
        "             while letting large bright regions shrink and then grow back to roughly their\n"
        "             original size. No new library code, just two calls in sequence.");
    OwnedImage<uint8_t, 3> openStage(crop.extent()), opened(crop.extent());
    erodeColor(noisy, box3, openStage, BorderMode::Clamp);
    dilateColor(openStage, box3, opened, BorderMode::Clamp);
    saveForInspection("opened (erode then dilate)", opened, "11_opened.png");

    step("dilateColor(noisy, ...) then erodeColor(..., box3, closed, ...)   -- \"closing\"",
        "The other order: dilate then erode (\"closing\") instead fills small dark specks --\n"
        "             pepper noise -- while similarly restoring large regions close to their original\n"
        "             size.");
    OwnedImage<uint8_t, 3> closeStage(crop.extent()), closed(crop.extent());
    dilateColor(noisy, box3, closeStage, BorderMode::Clamp);
    erodeColor(closeStage, box3, closed, BorderMode::Clamp);
    saveForInspection("closed (dilate then erode)", closed, "12_closed.png");

    // ------------------------------------------------------------------
    // PART 7: Otsu thresholding -- noisy vs. denoised
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 7: Otsu thresholding ===\n";

    OwnedImage<uint8_t, 3> greyClean3({ 1, crop.extent()[1], crop.extent()[2] });
    OwnedImage<uint8_t, 3> greyNoisy3({ 1, crop.extent()[1], crop.extent()[2] });
    step("crop.mean(0, greyClean3); noisy.mean(0, greyNoisy3);   // per-axis reduction over the channel axis",
        "otsu_threshold()/threshold() work on one scalar per pixel, so both the clean crop and its\n"
        "             salt-and-pepper-corrupted copy (both already built in Part 4) are reduced to\n"
        "             greyscale first -- same reduction demo/convolution's Sobel step used.");
    crop.mean(0, greyClean3);
    noisy.mean(0, greyNoisy3);
    Image<uint8_t, 2> greyClean = greyClean3.slice(0, 0);
    Image<uint8_t, 2> greyNoisy = greyNoisy3.slice(0, 0);

    step("uint8_t t = ndl::otsu_threshold(greyNoisy); ndl::threshold(greyNoisy, binaryNoisy, t, 255, 0);",
        "otsu_threshold() still finds *a* split on the noisy greyscale, but the salt-and-pepper\n"
        "             corruption pushes individual pixels across that split regardless of what's actually\n"
        "             underneath them, so the binary result should be full of isolated stray black/white\n"
        "             speckles rather than clean regions -- the same corruption Part 4 introduced, now\n"
        "             breaking a *different* operation than the blur it was shown against there.");
    uint8_t tNoisy = ndl::otsu_threshold(greyNoisy);
    showText("otsu_threshold() on the noisy greyscale", std::to_string((int)tNoisy));
    OwnedImage<uint8_t, 3> binaryNoisy({ 1, greyNoisy.extent()[0], greyNoisy.extent()[1] });
    Image<uint8_t, 2> binaryNoisyChannel = binaryNoisy.slice(0, 0);
    ndl::threshold(greyNoisy, binaryNoisyChannel, tNoisy, (uint8_t)255, (uint8_t)0);
    saveForInspection("binary (thresholded directly, noisy)", binaryNoisy, "13_binary_noisy.png");

    step("ndl::median_filter(greyNoisy, greyDenoised, box3, Clamp);   // then ndl::otsu_threshold()",
        "Denoising first (median_filter(), Part 4's own tool for this exact corruption) removes\n"
        "             most of the salt-and-pepper noise before Otsu ever sees it, so the resulting threshold\n"
        "             should split the image into far cleaner, more connected regions -- one pass of the\n"
        "             same 3x3 kernel Part 4 used is enough here, since salt-and-pepper noise (unlike, say,\n"
        "             heavy film grain) is exactly what median_filter() is good at removing outright. Compare\n"
        "             13_binary_noisy.png against 15_binary_denoised.png directly, and both against\n"
        "             14_binary_clean.png -- the Otsu result on the never-corrupted crop, the ground truth\n"
        "             both are approximating.");
    OwnedImage<uint8_t, 2> greyDenoised(greyNoisy.extent());
    // greyNoisy is an Image<uint8_t,2> (a slice()'d view), greyDenoised an
    // OwnedImage<uint8_t,2> (its own concrete type) -- median_filter()'s
    // independent SrcImageT/DstImageT template parameters deduce each side
    // on its own, so no explicit template argument is needed even though
    // the two sides are different concrete types.
    ndl::median_filter(greyNoisy, greyDenoised, box3, BorderMode::Clamp);

    uint8_t tClean = ndl::otsu_threshold(greyClean);
    OwnedImage<uint8_t, 3> binaryClean({ 1, greyClean.extent()[0], greyClean.extent()[1] });
    Image<uint8_t, 2> binaryCleanChannel = binaryClean.slice(0, 0);
    ndl::threshold(greyClean, binaryCleanChannel, tClean, (uint8_t)255, (uint8_t)0);
    saveForInspection("binary (clean crop, ground truth)", binaryClean, "14_binary_clean.png");

    uint8_t tDenoised = ndl::otsu_threshold(greyDenoised);
    showText("otsu_threshold(): clean / noisy / denoised", std::to_string((int)tClean) + " / " + std::to_string((int)tNoisy) + " / " + std::to_string((int)tDenoised));
    OwnedImage<uint8_t, 3> binaryDenoised({ 1, greyNoisy.extent()[0], greyNoisy.extent()[1] });
    Image<uint8_t, 2> binaryDenoisedChannel = binaryDenoised.slice(0, 0);
    ndl::threshold(greyDenoised, binaryDenoisedChannel, tDenoised, (uint8_t)255, (uint8_t)0);
    saveForInspection("binary (denoised first)", binaryDenoised, "15_binary_denoised.png");

    // Quantify the visual difference against the ground truth: how many
    // pixels does each binary result disagree with the clean crop's own
    // threshold on? A much higher mismatch count for the noisy version is a
    // direct, numeric measure of how much closer denoise-then-threshold gets
    // to the truth than threshold-directly.
    showText("pixels disagreeing with the clean-crop ground truth, noisy threshold", std::to_string(countMismatches(binaryNoisyChannel, binaryCleanChannel).count));
    showText("pixels disagreeing with the clean-crop ground truth, denoised threshold", std::to_string(countMismatches(binaryDenoisedChannel, binaryCleanChannel).count) + "  (lower is closer to the truth)");

    // ------------------------------------------------------------------
    // PART 8: binary morphology, tying it together
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 8: binary morphology ===\n";

    step("ndl::erode(binaryDenoisedChannel, erodedBinary, box3, Clamp); ndl::dilate(binaryDenoisedChannel, dilatedBinary, box3, Clamp);",
        "The same erode()/dilate() from Parts 1 and 3, now on a genuinely binary (0/255) image\n"
        "             instead of continuous pixel data: erode() shrinks the white regions and thickens the\n"
        "             black ones, dilate() does the opposite -- the classic \"binary morphology\" operations\n"
        "             from any image processing textbook, built from the exact same two functions this demo\n"
        "             already used on real-valued images.");
    OwnedImage<uint8_t, 3> erodedBinaryImg({ 1, greyNoisy.extent()[0], greyNoisy.extent()[1] });
    OwnedImage<uint8_t, 3> dilatedBinaryImg({ 1, greyNoisy.extent()[0], greyNoisy.extent()[1] });
    Image<uint8_t, 2> erodedBinaryChannel = erodedBinaryImg.slice(0, 0);
    Image<uint8_t, 2> dilatedBinaryChannel = dilatedBinaryImg.slice(0, 0);
    ndl::erode(binaryDenoisedChannel, erodedBinaryChannel, box3, BorderMode::Clamp);
    ndl::dilate(binaryDenoisedChannel, dilatedBinaryChannel, box3, BorderMode::Clamp);
    saveForInspection("binary eroded", erodedBinaryImg, "16_binary_eroded.png");
    saveForInspection("binary dilated", dilatedBinaryImg, "17_binary_dilated.png");

    step("opening/closing on the already-denoised binary image",
        "One more opening/closing pass (Part 6's idea again, applied here to the ALREADY-denoised\n"
        "             binary result) mops up whatever small speckles median_filter() didn't fully catch --\n"
        "             a standard real-world pipeline: denoise, threshold, then clean up the binary result\n"
        "             with morphology.");
    OwnedImage<uint8_t, 3> openStage2Img({ 1, greyNoisy.extent()[0], greyNoisy.extent()[1] });
    OwnedImage<uint8_t, 3> opened2Img({ 1, greyNoisy.extent()[0], greyNoisy.extent()[1] });
    Image<uint8_t, 2> openStage2Channel = openStage2Img.slice(0, 0);
    Image<uint8_t, 2> opened2Channel = opened2Img.slice(0, 0);
    ndl::erode(binaryDenoisedChannel, openStage2Channel, box3, BorderMode::Clamp);
    ndl::dilate(openStage2Channel, opened2Channel, box3, BorderMode::Clamp);
    saveForInspection("binary opened", opened2Img, "18_binary_opened.png");

    OwnedImage<uint8_t, 3> closeStage2Img({ 1, greyNoisy.extent()[0], greyNoisy.extent()[1] });
    OwnedImage<uint8_t, 3> closed2Img({ 1, greyNoisy.extent()[0], greyNoisy.extent()[1] });
    Image<uint8_t, 2> closeStage2Channel = closeStage2Img.slice(0, 0);
    Image<uint8_t, 2> closed2Channel = closed2Img.slice(0, 0);
    ndl::dilate(binaryDenoisedChannel, closeStage2Channel, box3, BorderMode::Clamp);
    ndl::erode(closeStage2Channel, closed2Channel, box3, BorderMode::Clamp);
    saveForInspection("binary closed", closed2Img, "19_binary_closed.png");

    // ------------------------------------------------------------------
    // PART 9: PackedBitImage -- the same processing, compact storage
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 9: PackedBitImage ===\n";

    step("PackedBitImage<2> packedDenoised(greyDenoised.extent()); ndl::threshold(greyDenoised, packedDenoised, tDenoised);",
        "erode()/dilate()/median_filter()/threshold() are each implemented exactly once, as free functions\n"
        "             (ndl::erode() etc., in morphology.h) written against any type exposing extent()/at()/coordinates() --\n"
        "             Image satisfied that from the start, and PackedBitImage (1 bit of real storage per pixel,\n"
        "             instead of a whole byte) is built to satisfy the same contract, so the exact same calls\n"
        "             below run against it completely unmodified, no PackedBitImage-specific code needed.");
    PackedBitImage<2> packedDenoised(greyDenoised.extent());
    ndl::threshold(greyDenoised, packedDenoised, tDenoised);

    step("ndl::erode(packedDenoised, packedEroded, box3); ndl::dilate(packedDenoised, packedDilated, box3);",
        "The same box3 kernel and BorderMode::Clamp already used on binaryDenoisedChannel in Part 8 above --\n"
        "             erosion/dilation of a bit image reduce to AND/OR of the neighborhood, the standard definition\n"
        "             of binary morphology. Checked below bit-for-bit against Part 8's own erodedBinaryChannel/\n"
        "             dilatedBinaryChannel: two independent storage representations (a byte per pixel vs. a bit\n"
        "             per pixel), the same shared algorithm, and they should agree on every single pixel.");
    PackedBitImage<2> packedEroded(greyDenoised.extent());
    PackedBitImage<2> packedDilated(greyDenoised.extent());
    ndl::erode(packedDenoised, packedEroded, box3, BorderMode::Clamp);
    ndl::dilate(packedDenoised, packedDilated, box3, BorderMode::Clamp);

    long erodeMismatch = 0, dilateMismatch = 0;
    for (const auto& coord : erodedBinaryChannel.coordinates())
    {
        if (bool(packedEroded.at(coord)) != (erodedBinaryChannel.at(coord) != 0)) erodeMismatch++;
        if (bool(packedDilated.at(coord)) != (dilatedBinaryChannel.at(coord) != 0)) dilateMismatch++;
    }
    showText("PackedBitImage erode() vs the byte-per-pixel Image result, mismatched pixels", std::to_string(erodeMismatch) + "  (expected 0)");
    showText("PackedBitImage dilate() vs the byte-per-pixel Image result, mismatched pixels", std::to_string(dilateMismatch) + "  (expected 0)");

    std::size_t byteImageBytes = greyDenoised.size();                       // 1 byte per pixel
    std::size_t packedImageBytes = (greyDenoised.size() + 63) / 64 * 8;     // 1 bit per pixel, rounded up to whole 64-bit words
    showText("memory: Image<uint8_t,2> mask", std::to_string(byteImageBytes) + " bytes");
    showText("memory: PackedBitImage", std::to_string(packedImageBytes) + " bytes (" + std::to_string(byteImageBytes / (double)packedImageBytes) + "x smaller)");

    std::cout <<
        "\n\nAll outputs written to: " << outputDir << "\n"
        "Open 01_original.png alongside 02/03 to see bright regions shrink/grow. Open\n"
        "05_noisy.png alongside 06/07 to see median_filter() remove salt-and-pepper noise\n"
        "cleanly where gaussian_blur() only smears it. 08/09/10 should look like a smooth\n"
        "progression from soft-erode to median to soft-dilate. 11/12 should look close to\n"
        "05_noisy.png but with most of the speckled noise gone. 13 (thresholded directly) should\n"
        "look visibly speckled compared to 15 (denoised first) -- open both alongside 14, the\n"
        "ground-truth threshold of the never-corrupted crop. 16-19 show binary\n"
        "erode/dilate/opening/closing starting from 15, the cleaner of the two thresholded results.\n"
        "Part 9 has no new PNGs -- it reruns 16/17's erode/dilate through PackedBitImage instead and\n"
        "confirms the result matches exactly, at a fraction of the memory.\n";

    return 0;
}
