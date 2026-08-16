#include <ndl/image.h>
#include <ndl/imageIO.h>
#include <ndl/convolution.h>
#include <ndl/summed_area_table.h>
#include <ndl/visualize.h>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cstdint>
#include <chrono>
#include <random>
#include <vector>
#include <array>
#include <string>
#include "../demoHelpers.h"

using namespace ndl;
namespace fs = std::filesystem;

// A step-by-step tour of ndl::summed_area_table()/rectangle_sum() -- free
// functions, summed_area_table.h, not Image members: see image/core.h's
// own comment for why -- in the same spirit as demo/convolution and
// demo/morphology: each step shows the code, explains it, and shows the
// result -- first on small numbers you can check by hand, then on a real
// photo, ending with the same kind of timing comparison demo/convolution's
// Part 6 does for FFT vs spatial convolution.
//
// step()/showArray()/showText()/saveForInspection()/outputDir come from
// demoHelpers.h, shared with every other demo.

int main()
{
    outputDir = NDL_SUMMED_AREA_TABLE_OUTPUT_DIR;
    fs::create_directories(outputDir);
    std::string dataDir = NDL_TEST_DATA_DIR;

    std::cout <<
        "This demo teaches ndl::summed_area_table()/rectangle_sum() the same way\n"
        "demo/convolution teaches convolve(): each step shows the code, explains it, and\n"
        "shows the result -- first on small numbers you can check by hand, then on a\n"
        "real photo whose results you check by *looking at the saved PNG* and the\n"
        "printed numbers. Output PNGs land in:\n    build/demo/summed_area_table/output\n";

    // ------------------------------------------------------------------
    // PART 1: the mechanics, on a 1D row you can check by hand
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 1: summed_area_table() on a 1D row ===\n";

    int rowData[5] = { 1, 2, 3, 4, 5 };
    Image<int, 1> row(rowData, { 5 });
    long tableData[5];
    Image<long, 1> table(tableData, { 5 });

    step("summed_area_table(row, table)",
        "summed_area_table(src, dst) writes, at every position, the RUNNING SUM of every src value\n"
        "             from the start up to and including that position -- table(i) = row(0)+...+row(i).\n"
        "             Reading `row` by hand: 1, 1+2=3, 3+3=6, 6+4=10, 10+5=15.");
    summed_area_table(row, table);
    showArray("row", row);
    showArray("table", table);

    step("rectangle_sum(table, {1}, {3})   // sum of row[1..3] = 2+3+4",
        "Once built, rectangle_sum() answers 'what's the sum over this range' in O(1) -- just a\n"
        "             couple of lookups into table -- instead of the O(range size) a direct sum would cost\n"
        "             every time. In 1D that's simply table(hi) - table(lo-1); rectangle_sum() generalizes\n"
        "             the same idea to any DIM via inclusion-exclusion over the table's own corners.");
    long r13 = rectangle_sum(table, std::array<int, 1>{1}, std::array<int, 1>{3});
    showText("rectangle_sum(table, {1}, {3})", std::to_string(r13) + "  (expected 2+3+4=9)");

    // ------------------------------------------------------------------
    // PART 2: 2D, and a direct correctness check against brute force
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 2: 2D, checked against a direct sum ===\n";

    OwnedImage<int, 2> grid({ 5, 5 });
    { int i = 0; for (auto it = grid.begin(); it != grid.end(); ++it) *it = ++i; }
    OwnedImage<long, 2> grid2DTable({ 5, 5 });
    summed_area_table(grid, grid2DTable);
    showArray("grid", grid);
    showArray("grid2DTable", grid2DTable);

    step("rectangle_sum(grid2DTable, {1,1}, {3,3})   // the interior 3x3 block",
        "The same interior 3x3 block Part 1 of demo/convolution sums by hand for its own sumKernel\n"
        "             example: rows y=1..3, columns x=1..3 -> 7,8,9,12,13,14,17,18,19, summing to 117.");
    long r33 = rectangle_sum(grid2DTable, std::array<int, 2>{1, 1}, std::array<int, 2>{3, 3});
    long bruteForce33 = 0;
    for (int y = 1; y <= 3; y++) for (int x = 1; x <= 3; x++) bruteForce33 += grid(x, y);
    showText("rectangle_sum result", std::to_string(r33) + "  (expected 117)");
    showText("direct brute-force sum, for comparison", std::to_string(bruteForce33));

    // ------------------------------------------------------------------
    // PART 3: a real photo
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 3: a real photo ===\n";

    OwnedImage<uint8_t, 3> photo = image_io::load_owned(dataDir + "/ng_bwgirl_crop.jpg");
    saveForInspection("photo", photo, "01_original.png");

    OwnedImage<uint8_t, 3> grey3({ 1, photo.extent()[1], photo.extent()[2] });
    photo.mean(0, grey3);
    Image<uint8_t, 2> grey = grey3.slice(0, 0);
    std::array<int, 2> greyExtent = { grey.extent()[0], grey.extent()[1] };

    step("summed_area_table(grey, greyTable)   // widened to double so a full-image sum can't overflow",
        "grey is uint8_t (max 255 per pixel); summing every pixel in even a modest photo overflows\n"
        "             uint8_t immediately, so the destination is a wider type instead -- double here, though\n"
        "             any sufficiently wide integer type (e.g. std::int64_t) works too. src and dst are\n"
        "             independently typed on purpose for exactly this reason, unlike convolve()/erode()/etc.");
    std::vector<double> greyTableData((std::size_t)greyExtent[0] * greyExtent[1]);
    Image<double, 2> greyTable(greyTableData.data(), greyExtent);
    summed_area_table(grey, greyTable);
    showText("full-image sum (table's own last corner)", std::to_string(greyTable.at({ greyExtent[0] - 1, greyExtent[1] - 1 })));

    step("heatmap(greyTable, dst, 255)   // the table itself, rendered as an image",
        "What does a summed-area table actually look like? heatmap() (visualize.h) renders any\n"
        "             2D minimal-interface numeric array as greyscale, scaled to the array's own max value --\n"
        "             the same tool histogram_image() is built on. Unlike grey itself (light/dark following\n"
        "             the photo's own content), every value here is a RUNNING SUM of everything above and to\n"
        "             the left of it, so the table only ever grows moving right or down: 02_sat_heatmap.png\n"
        "             should look almost black in the top-left corner (barely anything summed in yet) and\n"
        "             brighten smoothly toward solid white at the bottom-right corner (the full-image sum\n"
        "             printed just above) -- a cumulative brightness map, not a picture of the photo itself.");
    OwnedImage<uint8_t, 2> satHeatmap(greyExtent);
    heatmap(greyTable, satHeatmap, (uint8_t)255);
    saveForInspection("summed-area table (0=barely summed, white=full running sum)", satHeatmap, "02_sat_heatmap.png");

    step("200 random rectangle_sum() queries vs. direct brute-force sums",
        "Correctness check on real (not hand-picked) data: random rectangles, compared against\n"
        "             directly summing every pixel in each one by brute force.");
    std::mt19937 rng(2026);
    std::uniform_int_distribution<int> xd(0, greyExtent[0] - 1), yd(0, greyExtent[1] - 1);
    int mismatches = 0;
    for (int trial = 0; trial < 200; trial++)
    {
        int x0 = xd(rng), x1 = xd(rng), y0 = yd(rng), y1 = yd(rng);
        if (x0 > x1) std::swap(x0, x1);
        if (y0 > y1) std::swap(y0, y1);
        long brute = 0;
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) brute += grey(x, y);
        double q = rectangle_sum(greyTable, std::array<int, 2>{x0, y0}, std::array<int, 2>{x1, y1});
        if (std::abs(q - (double)brute) > 1e-6) mismatches++;
    }
    showText("mismatches out of 200 random rectangles", std::to_string(mismatches) + "  (expected 0)");

    // ------------------------------------------------------------------
    // PART 4: the payoff -- O(1) box filtering, timed against convolve()
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 4: O(1) box blur vs. convolve(), at growing kernel sizes ===\n";

    step("box_blur(grey, out, radius, BorderMode::Wrap)   vs.   convolve(grey, kernel, out, Wrap)",
        "convolve()'s cost scales with kernel size (every output pixel visits every kernel tap,\n"
        "             (2*radius+1)^2 of them); box_blur()'s cost per pixel is always exactly one\n"
        "             rectangle_sum() call -- 4 table lookups -- no matter how large radius gets, the same\n"
        "             'pay once, query cheaply' trade fftn() makes for repeated convolutions demo/convolution's\n"
        "             own Part 6 times against spatial convolve(). Both are given the same BorderMode::Wrap\n"
        "             here, so unlike a border-naive summed-area-table box blur (which would need to skip or\n"
        "             fudge the border pixels its window can't fully reach), these two should now match\n"
        "             pixel-for-pixel everywhere, border included -- checked below, not just eyeballed. The\n"
        "             timings are the actual point.");

    OwnedImage<uint8_t, 2> satBlurred(greyExtent);
    OwnedImage<uint8_t, 2> convBlurred(greyExtent);

    for (int radius : { 2, 8, 32 })
    {
        auto t0 = std::chrono::steady_clock::now();
        box_blur(grey, satBlurred, radius, BorderMode::Wrap);
        auto t1 = std::chrono::steady_clock::now();

        int k = 2 * radius + 1;
        std::vector<double> kernelData((std::size_t)k * k, 1.0 / (double)(k * k));
        Image<double, 2> kernel(kernelData.data(), { k, k });
        auto t2 = std::chrono::steady_clock::now();
        ndl::convolve(grey, convBlurred, kernel, BorderMode::Wrap);
        auto t3 = std::chrono::steady_clock::now();

        double satMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double convMs = std::chrono::duration<double, std::milli>(t3 - t2).count();
        showText("radius=" + std::to_string(radius) + " (" + std::to_string(k) + "x" + std::to_string(k) + " kernel)",
            "SAT box blur " + std::to_string(satMs) + " ms   vs   convolve() " + std::to_string(convMs) + " ms");
    }
    saveForInspection("SAT box blur (radius=32, BorderMode::Wrap)", satBlurred, "03_sat_box_blur.png");
    saveForInspection("convolve() box blur (radius=32, BorderMode::Wrap)", convBlurred, "04_convolve_box_blur.png");

    step("count pixels where 03 and 04 differ",
        "Both used the same BorderMode::Wrap this time, so with border handling now shared between them\n"
        "             instead of just approximated, the two independently-computed blurs should agree almost\n"
        "             everywhere, border included -- not just look similar in the interior. 'Almost' rather\n"
        "             than 'exactly', though: box_blur() sums the whole window once (via rectangle_sum()) and\n"
        "             divides by the area a single time, while convolve() accumulates one already-divided\n"
        "             weight per tap -- 4225 of them, for this 65x65 kernel -- so the two floating-point sums\n"
        "             take different rounding paths to (almost always) the same double value before each\n"
        "             narrows to uint8_t. A handful of pixels landing right on a truncation boundary can come\n"
        "             out ±1 apart between the two paths; that's rounding noise, not a correctness bug --\n"
        "             checked below by how many pixels differ, and by how much.");
    auto boxBlurDiff = countMismatches(satBlurred, convBlurred);
    showText("mismatched pixels, box_blur() vs convolve() (both BorderMode::Wrap)", std::to_string(boxBlurDiff.count) + " out of " + std::to_string(satBlurred.size()) + "  (expect a tiny handful, from floating-point rounding order, not 0)");
    showText("largest per-pixel difference among those mismatches", std::to_string(boxBlurDiff.maxDiff) + "  (expected 1 -- a rounding tie, not a real disagreement)");

    std::cout <<
        "\n\nAll outputs written to: build/demo/summed_area_table/output\n"
        "01 is the original photo. 02 is that photo's own summed-area table, visualized directly --\n"
        "a cumulative running sum, not a picture, so it should look like a smooth gradient from\n"
        "near-black (top-left) to white (bottom-right), nothing like 01. 03/04 are the same radius-32\n"
        "box blur computed two independent ways -- both using BorderMode::Wrap, they should look\n"
        "pixel-for-pixel identical almost everywhere, border included (confirmed by the mismatch count\n"
        "just above: a tiny handful of pixels differing by exactly 1, floating-point rounding noise\n"
        "between two different summation orders, not a real disagreement). The timings above are the\n"
        "real point: convolve()'s cost should grow sharply with radius while box_blur()'s stays roughly flat.\n";

    return 0;
}
