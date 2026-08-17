#include <ndl/image.h>
#include <ndl/imageIO.h>
#include <ndl/processing/histogram.h>
#include <ndl/processing/morphology.h>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include "../demoHelpers.h"

using namespace ndl;
namespace fs = std::filesystem;

// A step-by-step tour of ndl::Histogram<VDIM> and ndl::histogram_equalize()
// -- free functions/a class, histogram.h, not Image members: see
// image/core.h's own comment for why -- in the same spirit as
// demo/convolution and demo/morphology: each step shows the code, explains
// it, and shows the result -- first on small numbers you can check by
// hand, then on a real photo whose results you check by *looking at the
// saved PNGs*, including the histograms themselves: histogram_image()
// (built on visualize.h's bar_chart()/heatmap()) renders a Histogram as a
// real image rather than ASCII, which is what this demo saves and shows
// throughout, past the one small hand-checkable example in Part 1 where
// printing it as text is still perfectly readable.
//
// step()/showArray()/showText()/saveForInspection()/outputDir come from
// demoHelpers.h, shared with every other demo.

int main()
{
    outputDir = NDL_HISTOGRAM_OUTPUT_DIR;
    fs::create_directories(outputDir);
    std::string dataDir = NDL_TEST_DATA_DIR;

    std::cout <<
        "This demo teaches ndl::Histogram<VDIM> and ndl::histogram_equalize() the same\n"
        "way demo/convolution teaches convolve(): each step shows the code, explains\n"
        "it, and shows the result -- first on small numbers you can check by hand,\n"
        "then on a real photo whose results you check by *looking at the saved PNGs*,\n"
        "the histograms themselves included (histogram_image(), a real bar chart or\n"
        "heatmap, not ASCII). Output PNGs land in:\n    build/demo/histogram/output\n";

    // ------------------------------------------------------------------
    // PART 1: the mechanics, on numbers you can check by hand
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 1: how Histogram<1> works ===\n";

    std::vector<uint8_t> smallData = { 1, 1, 1, 2, 2, 5, 5, 5, 5, 9 };
    Image<uint8_t, 1> small(smallData.data(), { 10 });

    step("Histogram<1> hist(small, 4)   // 10 values, range [1,9], 4 bins",
        "Histogram<VDIM> bins the joint distribution of VDIM co-occurring scalar values --\n"
        "             VDIM==1 is the ordinary single-channel histogram used here. lo()/hi() auto-range\n"
        "             from small's own [min,max] (1 and 9), split into 4 equal-width buckets: [1,3),\n"
        "             [3,5), [5,7), [7,9] (the last bucket closed-inclusive at hi, same convention\n"
        "             otsu_threshold() below relies on). Reading off `small`: three 1s and two 2s land\n"
        "             in bucket 0 (5 total), zero values land in bucket 1, four 5s land in bucket 2, and\n"
        "             the single 9 lands in bucket 3 (closed-inclusive at hi, not one past it).");
    Histogram<1> hist(small, 4);
    showText("total()", std::to_string(hist.total()) + "  (expected 10)");
    showText("lo() / hi()", std::to_string(hist.lo()[0]) + " / " + std::to_string(hist.hi()[0]));
    showText("count(0), count(1), count(2), count(3)",
        std::to_string(hist.count(0)) + ", " + std::to_string(hist.count(1)) + ", " +
        std::to_string(hist.count(2)) + ", " + std::to_string(hist.count(3)) + "  (expected 5, 0, 4, 1)");

    step("showArray(\"hist\", hist)   // operator<< prints one ASCII bar per bin",
        "Every Histogram<1> can be printed directly, the same way Image itself can (image/print.h) --\n"
        "             here it's a horizontal bar per bin instead of a grid of values, scaled so the tallest\n"
        "             bin's bar spans a fixed width. Only used here, on this small a histogram (4 bins) --\n"
        "             see histogram_image() just below for how the rest of this demo shows one at real-photo\n"
        "             scale instead.");
    showArray("hist", hist);

    step("histogram_image(hist, dst, 255, 0)",
        "The same Histogram<1>, rendered as an actual image instead of text: bar_chart() (visualize.h)\n"
        "             draws one vertical bar per bin, scaled to the tallest bin, and histogram_image() is a\n"
        "             thin dispatch straight to it -- hist.counts() is already a real minimal-interface image\n"
        "             (an OwnedImage<std::size_t,1>), so there's no Histogram-specific drawing code at all.\n"
        "             00_small_hist.png should show 4 bars: short, none, tall, and a short one, matching\n"
        "             count(0..3) = 5, 0, 4, 1 above.");
    OwnedImage<uint8_t, 3> smallHistImg({ 3, 80, 40 });
    histogram_image(hist, smallHistImg, (uint8_t)255, (uint8_t)0);
    saveForInspection("small histogram, as an image", smallHistImg, "00_small_hist.png");

    step("uint8_t t = otsu_threshold(small)   // otsu_threshold() is Histogram<1>-backed now",
        "morphology.h's otsu_threshold() builds exactly this Histogram<1> internally (see its own\n"
        "             comment) instead of a hand-rolled bin array -- same algorithm, same answer, just\n"
        "             sharing this class instead of duplicating its own copy of the bucketing logic.");
    uint8_t t = otsu_threshold(small);
    showText("otsu_threshold(small)", std::to_string((int)t));

    // ------------------------------------------------------------------
    // PART 2: a real photo, greyscale histogram
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 2: a real photo's histogram ===\n";

    OwnedImage<uint8_t, 3> photo = image_io::load_owned(dataDir + "/ng_bwgirl_crop.jpg");
    saveForInspection("photo", photo, "01_original.png");

    OwnedImage<uint8_t, 3> grey3({ 1, photo.extent()[1], photo.extent()[2] });
    photo.mean(0, grey3);
    Image<uint8_t, 2> grey = grey3.slice(0, 0);

    step("Histogram<1> photoHist(grey);   // default 256 bins -- no reason to coarsen it now that it's an image, not ASCII",
        "The same Histogram<1>, now over a real photo's greyscale values, at the default 256-bin\n"
        "             resolution -- unlike an ASCII bar chart (one line per bin), an image has no reason to\n"
        "             coarsen the resolution just to keep it readable.");
    Histogram<1> photoHist(grey);
    OwnedImage<uint8_t, 3> photoHistImg({ 3, 256, 120 });
    histogram_image(photoHist, photoHistImg, (uint8_t)255, (uint8_t)0);
    saveForInspection("photo histogram", photoHistImg, "02_photo_histogram.png");

    // ------------------------------------------------------------------
    // PART 3: histogram equalization
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 3: histogram_equalize() ===\n";

    step("histogram_equalize(grey, equalized)",
        "Remaps grey's values so their cumulative distribution is closer to uniform across its own\n"
        "             [min,max] range -- the classic contrast-stretching operation for a photo whose values\n"
        "             cluster in a narrow sub-range instead of using the full range. Compare\n"
        "             03_grey_original.png (the unequalized greyscale photo) against 04_equalized.png, and\n"
        "             the two histogram images below: the equalized one should look visibly flatter/more\n"
        "             spread out across its bins than the original's, which likely has a few tall spikes.");
    saveForInspection("grey (unequalized)", grey, "03_grey_original.png");

    std::vector<uint8_t> equalizedData(grey.extent()[0] * grey.extent()[1]);
    Image<uint8_t, 2> equalized(equalizedData.data(), grey.extent());
    histogram_equalize(grey, equalized);
    saveForInspection("equalized", equalized, "04_equalized.png");

    OwnedImage<uint8_t, 3> equalizedHistImg({ 3, 256, 120 });
    histogram_image(Histogram<1>(equalized), equalizedHistImg, (uint8_t)255, (uint8_t)0);
    saveForInspection("equalized histogram", equalizedHistImg, "05_equalized_histogram.png");

    // ------------------------------------------------------------------
    // PART 4: joint histograms -- generalizing over VDIM
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 4: joint histograms (Histogram<2>) ===\n";

    step("Histogram<2> joint({64,64}, {&redChannel, &greenChannel});   histogram_image(joint, dst, 255)",
        "Histogram<VDIM> generalizes over VDIM the same way Image<T,DIM> generalizes over spatial\n"
        "             dimension DIM: VDIM is the number of *value* axes being jointly binned, independent\n"
        "             of the source images' own spatial dimensionality. Here VDIM=2 bins the joint\n"
        "             distribution of the red and green channels' values at each pixel -- a classic\n"
        "             co-occurrence/texture-analysis tool. histogram_image() dispatches to heatmap()\n"
        "             (visualize.h) instead of bar_chart() for VDIM==2 -- same idea, same underlying\n"
        "             'scale to max, write into dst' shape, just one image pixel per bin instead of one bar.\n"
        "             A natural photo's red/green channels are strongly correlated, so\n"
        "             06_joint_histogram.png's brightest pixels should cluster near the diagonal.");
    Image<uint8_t, 2> redChannel = photo.slice(0, 0);
    Image<uint8_t, 2> greenChannel = photo.slice(0, 1);
    Histogram<2> joint(std::array<int, 2>{64, 64}, std::array<const Image<uint8_t, 2>*, 2>{&redChannel, &greenChannel});
    showText("joint.total()", std::to_string(joint.total()) + "  (expected: one per pixel)");
    OwnedImage<uint8_t, 3> jointImg({ 3, 64, 64 });
    histogram_image(joint, jointImg, (uint8_t)255);
    saveForInspection("joint red/green histogram", jointImg, "06_joint_histogram.png");

    std::cout <<
        "\n\nAll outputs written to: build/demo/histogram/output\n"
        "00 is a tiny hand-checkable bar chart -- 4 bars matching count(0..3) = 5,0,4,1 from Part 1.\n"
        "01 is the original photo. 02 is its greyscale histogram. 03/04 are the greyscale photo\n"
        "before/after histogram_equalize(), and 05 is the equalized histogram -- it should look visibly\n"
        "flatter than 02. 06 is the joint red/green distribution across every pixel as a heatmap --\n"
        "brighter pixels mark (red,green) value combinations that occur more often in this photo, and\n"
        "should cluster near the diagonal for a natural photo (red and green tend to rise and fall\n"
        "together).\n";

    return 0;
}
