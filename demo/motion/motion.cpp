#include <ndl/image.h>
#include <ndl/imageIO.h>
#include <ndl/convolution.h>
#include <ndl/optical_flow.h>
#include <ndl/feature_detection.h>
#include <ndl/visualize.h>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cstdint>
#include <cmath>
#include <complex>
#include <vector>
#include <array>
#include <string>
#include "../demoHelpers.h"

using namespace ndl;
namespace fs = std::filesystem;

// A step-by-step tour of ndl::lucas_kanade_flow() (optical_flow.h) and
// ndl::sift_flow() (feature_detection.h) -- two very different ways of
// answering the same question, "where did each pixel move between these two
// frames?" -- in the same spirit as every other demo in this repo: each
// step shows the code, explains it, and shows the result, first on small
// hand-checkable examples, then on a real photo pair whose results you
// check by *looking at the saved PNGs*.
//
// Lucas-Kanade is DENSE: every pixel gets its own independent flow
// estimate, straight from local image gradients, no matter how
// textureless the neighborhood is (though a textureless one gets an
// unreliable estimate -- see optical_flow.h's own comment on the
// aperture problem). sift_flow() is fundamentally SPARSE: it only ever
// has real information at the (comparatively few) locations it detected
// and matched a keypoint, and fills in everywhere else by interpolating
// between them -- so its dense output should look visibly coarser/
// blockier than Lucas-Kanade's, especially away from any actual match.
// Comparing the two side by side on the same frame pair is the point of
// Parts 5-6 below.
//
// The real photo pair is frame10.png/frame11.png from the Middlebury
// Optical Flow benchmark's "RubberWhale" sequence (Baker, Scharstein,
// Lewis, Roth, Black & Szeliski, "A Database and Evaluation Methodology
// for Optical Flow", IJCV 92(1):1-31, 2011 -- vision.middlebury.edu/flow/),
// a small camera pan across a still-life of toys against striped fabric --
// real, if subtle, inter-frame motion with plenty of texture for both
// algorithms to work with.
//
// step()/showArray()/showText()/saveForInspection()/outputDir come from
// demoHelpers.h, shared with every other demo.

int main()
{
    outputDir = NDL_MOTION_OUTPUT_DIR;
    fs::create_directories(outputDir);
    std::string dataDir = NDL_TEST_DATA_DIR;

    std::cout <<
        "This demo teaches ndl::lucas_kanade_flow() and ndl::sift_flow() the same way\n"
        "demo/convolution teaches convolve(): each step shows the code, explains it, and\n"
        "shows the result -- first on small numbers you can check by hand, then on a real\n"
        "photo pair whose results you check by *looking at the saved PNG*. Output PNGs\n"
        "land in:\n    " << outputDir << "\n";

    // ------------------------------------------------------------------
    // PART 1: gradient() mechanics, on a hand-checkable linear ramp
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 1: gradient() -- the building block both algorithms share ===\n";

    const int W1 = 8, H1 = 8;
    std::vector<double> rampData(W1 * H1);
    Image<double, 2> ramp(rampData.data(), { W1, H1 });
    for (int y = 0; y < H1; y++) for (int x = 0; x < W1; x++) ramp(x, y) = 2.0 * x + 3.0 * y;

    step("gradient(ramp, grad)   // ramp(x,y) = 2x + 3y",
        "gradient(src, dst) writes dst.slice(0,axis) = the central-difference partial derivative\n"
        "             of src along that axis, for every axis -- both algorithms below build on exactly\n"
        "             this. For the linear ramp 2x+3y, the true gradient is the constant (2,3) everywhere,\n"
        "             so every interior position here should read exactly that.");
    std::vector<double> gradData(2 * W1 * H1);
    Image<double, 3> grad(gradData.data(), { 2, W1, H1 });
    gradient(ramp, grad, BorderMode::Reflect);
    Image<double, 2> gx = grad.slice(0, 0), gy = grad.slice(0, 1);
    showArray("d/dx (should be 2.00 everywhere)", gx);
    showArray("d/dy (should be 3.00 everywhere)", gy);

    // ------------------------------------------------------------------
    // PART 2: lucas_kanade_flow() mechanics, on a known synthetic shift
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 2: lucas_kanade_flow() on a known synthetic shift ===\n";

    const int W2 = 60, H2 = 60;
    std::vector<double> synth0(W2 * H2), synth1(W2 * H2);
    Image<double, 2> s0(synth0.data(), { W2, H2 }), s1(synth1.data(), { W2, H2 });
    double trueShiftX = 1.5, trueShiftY = -0.8;
    for (int y = 0; y < H2; y++) for (int x = 0; x < W2; x++)
    {
        s0(x, y) = std::sin(x * 0.3) * std::cos(y * 0.3) * 100.0 + 128.0;
        s1(x, y) = std::sin((x - trueShiftX) * 0.3) * std::cos((y - trueShiftY) * 0.3) * 100.0 + 128.0;
    }

    step("lucas_kanade_flow(s0, s1, synthFlow, 7)   // s1 = s0 shifted by (1.5, -0.8)",
        "A textured (not flat -- flat regions hit the aperture problem, see optical_flow.h's own\n"
        "             comment) synthetic pattern, shifted by a known sub-pixel amount. Averaging the\n"
        "             recovered flow over the interior (away from the border, where the window can't see\n"
        "             a full neighborhood) should land close to the true (1.5, -0.8) shift.");
    std::vector<double> synthFlowData(2 * W2 * H2);
    Image<double, 3> synthFlow(synthFlowData.data(), { 2, W2, H2 });
    lucas_kanade_flow(s0, s1, synthFlow, 7, BorderMode::Reflect);
    Image<double, 2> sfx = synthFlow.slice(0, 0), sfy = synthFlow.slice(0, 1);
    double sumX = 0, sumY = 0; int count = 0;
    for (int y = 15; y < H2 - 15; y++) for (int x = 15; x < W2 - 15; x++) { sumX += sfx(x, y); sumY += sfy(x, y); count++; }
    showText("average recovered flow (interior)", "(" + std::to_string(sumX / count) + ", " + std::to_string(sumY / count) + ")   expected approximately (1.5, -0.8)");

    // ------------------------------------------------------------------
    // PART 3: Lucas-Kanade on a real photo pair
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 3: Lucas-Kanade on a real photo pair (RubberWhale) ===\n";

    OwnedImage<uint8_t, 3> frame0Raw = image_io::load_owned(dataDir + "/rubberwhale/frame10.png");
    OwnedImage<uint8_t, 3> frame1Raw = image_io::load_owned(dataDir + "/rubberwhale/frame11.png");
    step("image_io::load_owned(\"rubberwhale/frame10.png\"); ...frame11.png; downsample(..., 2)",
        "The Middlebury RubberWhale pair (see this file's own top comment for the citation) --\n"
        "             downsampled 2x purely to keep this demo's saved images (and runtime -- sift_flow()\n"
        "             below is by far the most expensive step in this whole demo) reasonable, the same\n"
        "             reason demo/convolution/demo/morphology downsample marbles.bmp.");
    OwnedImage<uint8_t, 3> frame0Color = downsample(frame0Raw, 2);
    OwnedImage<uint8_t, 3> frame1Color = downsample(frame1Raw, 2);
    saveForInspection("frame0", frame0Color, "01_frame0.png");
    saveForInspection("frame1", frame1Color, "02_frame1.png");

    OwnedImage<uint8_t, 3> f0grey3({ 1, frame0Color.extent()[1], frame0Color.extent()[2] });
    OwnedImage<uint8_t, 3> f1grey3({ 1, frame1Color.extent()[1], frame1Color.extent()[2] });
    frame0Color.mean(0, f0grey3);
    frame1Color.mean(0, f1grey3);
    Image<uint8_t, 2> f0grey = f0grey3.slice(0, 0), f1grey = f1grey3.slice(0, 0);
    std::array<int, 2> extent2D = { f0grey.extent()[0], f0grey.extent()[1] };

    step("lucas_kanade_flow(f0grey, f1grey, lkFlow, 9)",
        "Dense, per-pixel flow -- every one of this greyscale pair's own W*H positions gets an\n"
        "             independent estimate straight from the local gradients around it. A wider window\n"
        "             than Part 2's (radius 9 instead of 7) trades a little spatial precision for averaging\n"
        "             out more per-pixel noise -- this is real photo data, not a clean synthetic pattern.");
    OwnedImage<double, 3> lkFlow({ 2, extent2D[0], extent2D[1] });
    lucas_kanade_flow(f0grey, f1grey, lkFlow, 9, BorderMode::Reflect);

    step("flow_to_color(lkFlow, lkFlowColor, 255)",
        "hue = direction, brightness = magnitude (scaled to the field's own max). RubberWhale is a\n"
        "             small sideways camera pan across a scene with real depth -- the closer foreground toys\n"
        "             (bottom-left) and the nearer sweater (right) shift differently from the farther striped\n"
        "             backdrop during that pan, which is exactly motion PARALLAX, not noise: 03_lk_flow_color.png\n"
        "             should show a handful of distinct color regions -- one per rough depth layer -- rather\n"
        "             than one single uniform hue across the whole frame.");
    OwnedImage<uint8_t, 3> lkFlowColor({ 3, extent2D[0], extent2D[1] });
    flow_to_color(lkFlow, lkFlowColor, (uint8_t)255);
    saveForInspection("Lucas-Kanade flow, color-coded", lkFlowColor, "03_lk_flow_color.png");

    Image<double, 2> lkfx = lkFlow.slice(0, 0), lkfy = lkFlow.slice(0, 1);
    double lkMagSum = 0;
    for (const auto& c : lkfx.coordinates()) lkMagSum += std::sqrt(lkfx.at(c) * lkfx.at(c) + lkfy.at(c) * lkfy.at(c));
    showText("mean flow magnitude (Lucas-Kanade)", std::to_string(lkMagSum / lkfx.size()) + " px");

    // ------------------------------------------------------------------
    // PART 4: DoG keypoint detection mechanics, on a synthetic blob
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 4: detect_keypoints() -- the scale-space part that genuinely generalizes to any DIM ===\n";

    const int W4 = 100, H4 = 100;
    std::vector<double> blobData(W4 * H4, 0.0);
    Image<double, 2> blobImg(blobData.data(), { W4, H4 });
    double bx = 40, by = 60, bsigma = 3.0;
    for (int y = 0; y < H4; y++) for (int x = 0; x < W4; x++)
    {
        double dx = x - bx, dy = y - by;
        blobImg(x, y) = 200.0 * std::exp(-(dx * dx + dy * dy) / (2 * bsigma * bsigma));
    }
    step("detect_keypoints(blobImg)   // one Gaussian blob, sigma=3, at (40,60)",
        "A Difference-of-Gaussians scale-space search (feature_detection.h's own comment has the\n"
        "             full mechanics) finds local extrema across both space AND scale -- a single clean\n"
        "             blob should produce exactly one keypoint, right at the blob's own center, at close to\n"
        "             the blob's own scale.");
    auto blobKeypoints = detect_keypoints(blobImg);
    for (const auto& kp : blobKeypoints)
        showText("keypoint", "position=(" + std::to_string(kp.position[0]) + "," + std::to_string(kp.position[1]) + ")  scale=" + std::to_string(kp.scale) + "  response=" + std::to_string(kp.response) + "   (expected near (40,60))");

    // ------------------------------------------------------------------
    // PART 5: sift_flow() on the real photo pair
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 5: sift_flow() on the same real photo pair ===\n";

    step("detect_keypoints()+compute_descriptors()+match_descriptors(), shown separately for the visualization below",
        "sift_flow() below does all of this in one call; broken out here just so 05_sift_keypoints.png\n"
        "             (a small dot at every matched keypoint's own frame0 position) can be saved alongside\n"
        "             it, showing exactly WHERE this algorithm actually has information -- everywhere else\n"
        "             in 06_sift_flow_color.png is interpolated, not measured.");
    auto kp0 = detect_keypoints(f0grey);
    auto kp1 = detect_keypoints(f1grey);
    auto desc0 = compute_descriptors(f0grey, kp0);
    auto desc1 = compute_descriptors(f1grey, kp1);
    auto matches = match_descriptors(desc0, desc1);
    showText("keypoints in frame0 / frame1", std::to_string(kp0.size()) + " / " + std::to_string(kp1.size()));
    showText("matched pairs (nearest-neighbor + Lowe's ratio test)", std::to_string(matches.size()));

    OwnedImage<uint8_t, 3> keypointOverlay(frame0Color.view({}));
    for (const auto& m : matches)
    {
        const auto& pos = kp0[m.index0].position;
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++)
            {
                int x = pos[0] + dx, y = pos[1] + dy;
                if (x < 0 || x >= extent2D[0] || y < 0 || y >= extent2D[1]) continue;
                keypointOverlay(0, x, y) = 255; keypointOverlay(1, x, y) = 0; keypointOverlay(2, x, y) = 0;
            }
    }
    saveForInspection("frame0 with matched keypoints marked in red", keypointOverlay, "05_sift_keypoints.png");

    step("sift_flow(f0grey, f1grey, siftFlow)",
        "Propagates those sparse matched displacements to a dense per-pixel field via inverse-\n"
        "             distance-weighted interpolation (feature_detection.h's own comment has the details) --\n"
        "             06_sift_flow_color.png should be broadly similar in hue to 03_lk_flow_color.png\n"
        "             (both are measuring the same real motion) but visibly smoother/blockier, since it's\n"
        "             interpolated from " + std::to_string(matches.size()) + " points rather than measured at every pixel independently.");
    OwnedImage<double, 3> siftFlow({ 2, extent2D[0], extent2D[1] });
    sift_flow(f0grey, f1grey, siftFlow);

    step("flow_to_color(siftFlow, siftFlowColor, 255, magnitudeCap)   // magnitudeCap = 95th percentile magnitude",
        "flow_to_color()'s default \"scale brightness to the field's own true max magnitude\" is\n"
        "             exactly the failure mode windowed_heatmap() (visualize.h) exists to avoid: a single stray\n"
        "             match -- interpolated across a wide area by sift_flow()'s own inverse-distance weighting,\n"
        "             see above -- can produce one wildly large displacement that dwarfs every real one, crushing\n"
        "             the rest of the field to near-black. Capping brightness at the 95th percentile magnitude\n"
        "             instead (that one outlier simply saturates at full brightness) keeps the real motion\n"
        "             visible; percentile() is the same building block windowed_heatmap() itself uses.");
    Image<double, 2> siftfxForCap = siftFlow.slice(0, 0), siftfyForCap = siftFlow.slice(0, 1);
    std::vector<double> siftMagData(extent2D[0] * extent2D[1]);
    Image<double, 2> siftMag(siftMagData.data(), extent2D);
    for (const auto& c : siftfxForCap.coordinates())
        siftMag.at(c) = std::sqrt(std::pow(siftfxForCap.at(c), 2) + std::pow(siftfyForCap.at(c), 2));
    double siftMagCap = percentile(siftMag, 95.0);
    showText("95th-percentile flow magnitude (used as the color-coding cap)", std::to_string(siftMagCap) + " px");

    OwnedImage<uint8_t, 3> siftFlowColor({ 3, extent2D[0], extent2D[1] });
    flow_to_color(siftFlow, siftFlowColor, (uint8_t)255, siftMagCap);
    saveForInspection("SIFT-inspired flow, color-coded (95th-percentile-capped)", siftFlowColor, "06_sift_flow_color.png");

    // ------------------------------------------------------------------
    // PART 6: comparing the two, and the 2D complex representation
    // ------------------------------------------------------------------
    std::cout << "\n\n=== PART 6: comparing the two flow fields, and the 2D complex representation ===\n";

    step("average per-pixel Euclidean distance between lkFlow and siftFlow",
        "Both are estimating the same real underlying motion two completely independent ways --\n"
        "             they should agree reasonably well on average, even though sift_flow()'s own\n"
        "             smoothing/interpolation keeps them from matching exactly pixel-for-pixel.");
    Image<double, 2> siftfx = siftFlow.slice(0, 0), siftfy = siftFlow.slice(0, 1);
    double diffSum = 0;
    for (const auto& c : lkfx.coordinates())
    {
        double dx = lkfx.at(c) - siftfx.at(c), dy = lkfy.at(c) - siftfy.at(c);
        diffSum += std::sqrt(dx * dx + dy * dy);
    }
    showText("mean |lucas_kanade_flow - sift_flow|", std::to_string(diffSum / lkfx.size()) + " px");

    step("to_complex(lkFlow, lkFlowComplex); std::abs(...)/std::arg(...)",
        "The general {2,W,H} representation converted to Image<std::complex<double>,2> -- once there,\n"
        "             a flow vector's magnitude/direction are just std::abs()/std::arg() away, and rotating\n"
        "             every vector in the field by a fixed angle is a single complex multiply, not a\n"
        "             per-component trig rewrite. Shown here on the flow field's own strongest-motion pixel.");
    OwnedImage<std::complex<double>, 2> lkFlowComplex(extent2D);
    to_complex(lkFlow, lkFlowComplex);
    std::array<int, 2> strongest{ 0, 0 };
    double strongestMag = -1;
    for (const auto& c : lkFlowComplex.coordinates())
    {
        double m = std::abs(lkFlowComplex.at(c));
        if (m > strongestMag) { strongestMag = m; strongest = c; }
    }
    auto cval = lkFlowComplex.at(strongest);
    showText("strongest-motion pixel", "(" + std::to_string(strongest[0]) + "," + std::to_string(strongest[1]) + ")  magnitude=" + std::to_string(std::abs(cval)) + "px  angle=" + std::to_string(std::arg(cval) * 180.0 / M_PI) + " degrees");

    std::cout <<
        "\n\nAll outputs written to: " << outputDir << "\n"
        "01/02 are the two RubberWhale frames. 03 is Lucas-Kanade's dense flow, color-coded (hue=\n"
        "direction, brightness=magnitude). 05 marks every matched SIFT-inspired keypoint in red on\n"
        "frame0 -- compare its sparse dots against 06, the same color-coded visualization built by\n"
        "interpolating from just those points. 03 and 06 should broadly agree in hue (same real\n"
        "motion, two independent methods) while 06 looks visibly smoother/blockier -- exactly the\n"
        "dense-vs-sparse tradeoff this demo set out to show.\n";

    return 0;
}
