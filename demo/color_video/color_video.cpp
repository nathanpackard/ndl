#include <ndl/image.h>
#include <ndl/viewer/viewer.h>
#include <ndl/imageIO/video_io.h>
#include <iostream>
#include <filesystem>
#include <cstdint>
#include <string>
#include "../demoHelpers.h"

using namespace ndl;
namespace fs = std::filesystem;

// A real color video, loaded from an actual .mp4 file rather than
// synthesized -- the color-viewer counterpart to demo/nd_viewer's own tour
// of DIM generality. video_io.h's load_video_owned() (imageIO/video_io.h,
// opt-in like distance_transform.h -- #include it directly, it's not
// bundled into imageIO.h) returns an ordinary {3,width,height,frameCount}
// image: channel, the two spatial axes, and time, exactly the same
// {channel,...} layout every still image in this library already uses,
// just with one more (time) axis. Shown TWO ways, back to back, on the
// exact same data: first with the channel axis treated as just another
// generic higher-dimensional axis (grayscale, one row per channel --
// demo/nd_viewer's own 5D sphere treats its own channel axis this same
// way), then with NDLViewer.create()'s new options.colorAxis, which
// composites all 3 channel values at each spatial position into a true RGB
// pixel instead. Same underlying data, same viewer, same pairwise-slice
// grid -- the only difference is whether the viewer is TOLD which axis
// holds color.
int main()
{
    outputDir = NDL_COLOR_VIDEO_OUTPUT_DIR;
    fs::create_directories(outputDir);
    std::string dataDir = NDL_TEST_DATA_DIR;

    std::cout <<
        "This demo loads a real color video (not synthesized) and shows it two ways in\n"
        "viewer.h's N-D pairwise-slice viewer: first with its channel axis treated as\n"
        "just another generic higher-dimensional axis (grayscale, the same way\n"
        "demo/nd_viewer's own 5D example treats ITS channel axis), then with the\n"
        "viewer's new options.colorAxis, which composites the 3 channel values at each\n"
        "spatial position into true color instead. Output lands in:\n"
        "    build/demo/color_video/output\n";

    std::cout << "\n\n=== Loading a real video file ===\n";

    step("auto clip = image_io::load_video_owned(dataDir + \"/waves.mp4\", 400, 0, 10.0, &clipSpacing)",
        "load_video_owned() (imageIO/video_io.h) shells out to the system ffmpeg/ffprobe to\n"
        "             decode a real-world video file (see that header's own top comment for why this one\n"
        "             function isn't native the way everything else in ndl is: there's no small,\n"
        "             permissively-licensed H.264 decoder that can actually handle a real-world file like\n"
        "             this one, which is H.264 High profile/CABAC). The 400/0/10.0 arguments ask ffmpeg to\n"
        "             scale and decimate the source (1280x720 @ 25fps) down during decode, rather than ndl\n"
        "             ever holding the full-resolution clip in memory -- leaving targetHeight at 0 (rather\n"
        "             than having to already know and pass the matching height) asks\n"
        "             load_video_owned() to work out the height itself from the source's own probed aspect\n"
        "             ratio, since a caller loading an arbitrary file often doesn't know it up front.\n"
        "             Returns an ordinary {3,width,height,frameCount} image -- channel, the two spatial\n"
        "             axes, and time -- and, via the last argument, also fills in clipSpacing: an\n"
        "             ndl::VoxelSpacing<4> (viewer.h) load_video_owned() builds FROM WHAT IT ACTUALLY KNOWS\n"
        "             about its own 4 axes (channel gets \"channels\"; the two spatial axes get \"px\", since\n"
        "             ffmpeg's own decode is unscaled square pixels with no real-world size attached; time gets\n"
        "             a REAL physical unit, seconds, at 1/fps spacing -- the actual decoded rate, 10.0 here\n"
        "             since that's what was requested, or the source's own native rate via ffprobe had no\n"
        "             override been given) -- ready to hand straight to embedNDViewer() below, not something\n"
        "             this demo has to already know and re-derive by hand.");
    VoxelSpacing<4> clipSpacing;
    auto clip = image_io::load_video_owned(dataDir + "/waves.mp4", 400, 0, 10.0, &clipSpacing);
    showText("clip.extent()", "{3," + std::to_string(clip.extent()[1]) + "," + std::to_string(clip.extent()[2]) + "," + std::to_string(clip.extent()[3]) + "}");

    std::cout << "\n\n=== Same data, channel as just another axis (grayscale) ===\n";

    step("embedNDViewer(\"waves.mp4, channel as a generic axis\", clip, \"waves_grayscale.ndlv\", &clipSpacing)",
        "No colorAxis given, so every panel is ordinary grayscale -- the (0,3) channel-vs-time\n"
        "             panel is a real, if unusual, pairwise view this way: 3 discrete rows (one per color\n"
        "             channel) against the video's own timeline, exactly how demo/nd_viewer's 5D sphere\n"
        "             example treats its own channel axis. This is the SAME underlying data the color\n"
        "             version below shows -- only the viewer's own options differ. clipSpacing (above) means\n"
        "             the outer axis labels read \"[0] channels\", \"[1] px\", \"[2] px\", \"[3] s\" instead of\n"
        "             ndlviewer.js's own generic \"voxels\" default.");
    embedNDViewer("waves.mp4, channel as a generic axis (grayscale)", clip, "waves_grayscale.ndlv", &clipSpacing);

    std::cout << "\n\n=== Same data again, this time as true color ===\n";

    step("embedNDViewer(\"waves.mp4, true color\", clip, \"waves_color.ndlv\", &clipSpacing, 0, /*colorAxis*/ 0)",
        "colorAxis=0 tells the viewer axis 0 holds RGB channels: every panel whose own two axes\n"
        "             BOTH differ from axis 0 now reads all 3 channel values at each spatial position and\n"
        "             composites them into a real color pixel (ndlviewer.js's own extractSliceRGB()),\n"
        "             through the SAME shared window/level every grayscale panel already uses -- rather\n"
        "             than treating channel as just another axis to plot. A panel that DOES show axis 0 as\n"
        "             one of its own two axes (e.g. channel vs. time) is unaffected either way, since\n"
        "             there's nothing to composite when the channel axis itself is one of the two plotted\n"
        "             dimensions -- compare its own (0,3) panel here against the grayscale version above,\n"
        "             identical. colorAxis's own \"[0]\" outer label never appears here (axis 0 is never one of\n"
        "             displayAxes once it's the color axis -- see ndlviewer.js's own comment), but clipSpacing\n"
        "             still applies to axes 1-3.");
    embedNDViewer("waves.mp4, true color", clip, "waves_color.ndlv", &clipSpacing, 0, /*colorAxis*/ 0);

    std::cout <<
        "\n\nAll output written to: build/demo/color_video/output\n"
        "If you're viewing the generated tutorial page, compare the two embedded viewers above:\n"
        "same clip, same pairwise-slice grid, same shared window/level slider -- the first shows\n"
        "every panel as grayscale (channel treated as a generic axis, 3 separate rows in the\n"
        "channel-vs-time panel), the second composites channel 0/1/2 into real RGB wherever\n"
        "channel isn't one of the two plotted axes. Neither is special-cased in viewer.h itself\n"
        "(colorAxis lives entirely in ndlviewer.js, the browser-side half) -- the exported .ndlv\n"
        "binary is byte-for-byte identical either way, it's purely how the viewer chooses to\n"
        "read it.\n";

    return 0;
}
