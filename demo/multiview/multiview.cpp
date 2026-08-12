#include <ndl/image.h>
#include <iostream>
using namespace ndl;

// A step-by-step tour of Image::slice() and Image::view() (plus mirror() and
// swap_axes(), which build on the same ideas). Each step below prints four
// things: the exact code that runs, an explanation of what it does, the INPUT
// array it operates on, and the OUTPUT array it produces -- so you can read
// this file's *output* top to bottom and learn the API without needing to
// also read the source, and without ever having to guess what a step's input
// was.
//
// Every operation shown here -- printing (operator<<), slice(), view(),
// mirror(), swap_axes() -- is written against Image<T,DIM> for arbitrary DIM,
// not anything specific to 4 dimensions; DIM==2 (an ordinary image) and
// DIM==3 (add a channel or a depth axis) are the common cases, and nothing
// below is DIM-specific to those either. This demo picks DIM==4 specifically
// to demonstrate what happens PAST the trivial cases, where a 2D grid alone
// stops being enough to show what's going on (see the printing step below)
// and "which dimension did I just slice/view relative to" stops being
// obvious by inspection -- if 2D/3D examples still left you wondering how
// this all generalizes, this is that answer, made concrete.
//
// Everything operates on one 4D image: 2 frames of a 2-channel, 4-wide/3-tall
// image (extent {x=4, y=3, channel=2, frame=2}, 48 elements), filled 1..48 in
// memory order (x fastest, then y, then channel, then frame). That makes each
// (channel, frame) combination a contiguous, easy-to-recognize block of 12
// numbers, and every block -- printed as a 2D grid -- is the same pattern:
//
//    1  2  3  4
//    5  6  7  8
//    9 10 11 12
//
// which is frame 0, channel 0. (Frame 0/channel 1 is 13..24, frame 1/channel 0
// is 25..36, frame 1/channel 1 is 37..48.) Every result below can be checked by
// eye against that grid.
//
// The full 4D array (ndImage) is 48 elements -- printed once, in full, up
// front. Steps that take it as input don't reprint all 48 numbers every time;
// they say so explicitly instead and point back at that first printout.

int stepNumber = 0;

void step(const std::string& code, const std::string& explanation)
{
    std::cout << "\n[" << ++stepNumber << "] code:    " << code << "\n";
    std::cout << "    explain: " << explanation << "\n";
}

// input/output printers: one pair that prints an actual (small) Image, one
// pair that states in words what the input/output is (for the full
// 48-element 4D array, a single scalar, or an exception, where printing the
// whole thing isn't the clearest way to show it), and one pair that does
// both -- a short note plus the actual array -- for e.g. "here's the extent,
// and here's what it holds". Most steps have exactly one input: and one
// output: line; a step showing the same underlying memory through more than
// one view (e.g. proving a write is visible through objects other than the
// one written through) calls outputNoted() more than once instead.
template<class ImageT>
void input(const ImageT& img) { std::cout << "    input:\n" << img; }
void inputText(const std::string& text) { std::cout << "    input:   " << text << "\n"; }
template<class ImageT>
void inputNoted(const std::string& note, const ImageT& img) { std::cout << "    input:   " << note << "\n" << img; }

template<class ImageT>
void output(const ImageT& img) { std::cout << "    output:\n" << img; }
void outputText(const std::string& text) { std::cout << "    output:  " << text << "\n"; }
template<class ImageT>
void outputNoted(const std::string& note, const ImageT& img) { std::cout << "    output:  " << note << "\n" << img; }

// Not every step's input/output is itself worth (or able to be) printed as an
// array -- e.g. element access's real input is the full 48-element ndImage,
// already printed in full above. context() prints a *different*, smaller
// array purely as a visual aid for locating the result; it's deliberately
// not labeled input: or output:, since it's neither.
template<class ImageT>
void context(const std::string& note, const ImageT& img) { std::cout << "    context: " << note << "\n" << img; }

int main()
{
    const int width = 4, height = 3, channels = 2, frames = 2;
    std::vector<int> imageData(width * height * channels * frames);
    Image<int, 4> ndImage(imageData.data(), {width, height, channels, frames});

    int i = 0;
    for (auto it = ndImage.begin(); it != ndImage.end(); ++it)
        *it = ++i;

    // Deliberately not a step() call -- this comes before step-numbering
    // starts, so later references like "step 1" unambiguously mean the
    // first *transformation* shown, not this initial "here's the whole
    // array" printout. Indentation still matches step()'s own "    code:"/
    // "    explain:" convention (4 spaces, 13-space-aligned continuation
    // lines) for visual consistency with every step below it.
    std::cout << "=== printing an N-dimensional image (N=4 here) ===\n";
    std::cout << "code:    std::cout << ndImage\n";
    std::cout << "    explain: operator<< is written for Image<T,DIM> at any DIM, not specifically DIM==4: for DIM<=2\n"
                  "             it's just rows and columns, and it recurses from the OUTERMOST dimension inward for\n"
                  "             however many more there are -- here it loops frame (dim3), and for each frame value\n"
                  "             loops channel (dim2), and for each of those loops y (dim1) -- down to x (dim0), which\n"
                  "             is what actually becomes one printed line (a comma-separated row). That's a lot of\n"
                  "             nesting to track from blank lines alone once DIM > 3, so every 2D (y,x) block is\n"
                  "             preceded by an explicit \"[dim2=.., dim3=..]\" header naming every higher dimension's\n"
                  "             current index -- one more \"dimN=..\" entry for however many dimensions DIM actually\n"
                  "             has, blank lines still separate blocks too, but the header stays unambiguous regardless\n"
                  "             of DIM, which is exactly the case 4D is here to demonstrate.\n";
    inputText("ndImage itself -- this is the setup, not a transformation of anything.");
    output(ndImage);
    std::cout << "(the [dim2=1, dim3=1] block above is the one used in step 5 below)\n";

    step("ndImage.slice(3, 0)",
         "slice(dim, index) fixes dimension `dim` to a single index and removes it from the result --\n"
         "             dropping the dimension count by one, like ndImage[:,:,:,0] would in numpy. It shares the same\n"
         "             underlying memory as ndImage (no copy, O(1)), the same way view() does. Slicing away dim3\n"
         "             (frame, the LAST dimension) leaves dims 0,1,2 (x,y,channel) numbered exactly as before --\n"
         "             that only changes when you slice away a dimension something else is numbered relative to\n"
         "             (see step 3).");
    inputText("the original 4D ndImage, extent {4,3,2,2} -- printed in full above.");
    Image<int, 3> frame0 = ndImage.slice(3, 0);
    outputNoted("extent = {" + std::to_string(frame0.extent()[0]) + "," + std::to_string(frame0.extent()[1]) + "," + std::to_string(frame0.extent()[2]) +
                "}  (still x,y,channel -- dim3 is just gone)", frame0);

    step("frame0.slice(2, 0)",
         "Slicing again, this time on the 3D result of step 1: dim2 of frame0 is channel (frame0's dims are\n"
         "             still x=0,y=1,channel=2, per step 1), so this fixes channel=0 and drops down to a plain 2D\n"
         "             (x,y) grid -- our reference grid for the rest of this file. slice() calls chain naturally\n"
         "             since each one just returns another Image, one dimension smaller, still sharing memory.");
    inputNoted("frame0, the output of step 1 (repeated here for reference):", frame0);
    Image<int, 2> referenceGrid = frame0.slice(2, 0);
    output(referenceGrid);

    step("ndImage.slice(0, 0)",
         "To make the renumbering rule concrete: this slices away dim0 (x) instead of a trailing dimension.\n"
         "             Every dimension ABOVE the one you slice shifts down by one in the result; dimensions below it\n"
         "             (there are none here, since we sliced dim0) are unaffected. So y (was dim1) becomes dim0,\n"
         "             channel (was dim2) becomes dim1, and frame (was dim3) becomes dim2. Notice the values below are\n"
         "             a totally different arrangement of the same 12 numbers per block: rows are now y (3 values),\n"
         "             not x (4 values) -- because dim0 itself changed meaning, from x to y.");
    inputText("the original 4D ndImage, extent {4,3,2,2} -- printed in full above.");
    Image<int, 3> noX = ndImage.slice(0, 0);
    outputNoted("extent = {" + std::to_string(noX.extent()[0]) + "," + std::to_string(noX.extent()[1]) + "," + std::to_string(noX.extent()[2]) +
                "}  (y,channel,frame -- each renumbered down by one)", noX);

    step("ndImage(2, 1, 0, 0)",
         "Element access: one integer per dimension (x, y, channel, frame). Reads a single value -- unlike\n"
         "             slice()/view(), there's no array in the output, just the one number at that coordinate.");
    inputText("the original 4D ndImage, extent {4,3,2,2} -- printed in full above. (referenceGrid below is NOT\n"
              "             the input -- ndImage(2,1,0,0) reads directly from ndImage. It's shown only to locate the value.)");
    outputText(std::to_string(ndImage(2, 1, 0, 0)) + "  (a single scalar, not an array -- row y=1 above is 5,6,7,8, and x=2 is 7)");

    step("ndImage.slice(3, 1).slice(2, 1)",
         "Both slice indices this time are 1: drop frame (=1) then channel (=1), landing on the frame1/channel1\n"
         "             2D grid -- expected values 37..48, matching the [dim2=1, dim3=1] block from the full print above.");
    inputText("the original 4D ndImage, extent {4,3,2,2} -- printed in full above.");
    output(ndImage.slice(3, 1).slice(2, 1));

    step("referenceGrid.view({1}, {2})",
         "view()'s start/end are both INCLUSIVE indices into the current image. This keeps only\n"
         "             columns (x) 1 and 2 of the reference grid -- 2 of its original 4 columns.");
    input(referenceGrid);
    output(referenceGrid.view({1}, {2}));

    step("referenceGrid.view({-2}, {-1})",
         "Negative indices count from the last element (Python-slice style): -1 is the last column,\n"
         "             -2 the second-to-last. This keeps the last two columns of the reference grid.");
    input(referenceGrid);
    output(referenceGrid.view({-2}, {-1}));

    step("referenceGrid.view({}, {}, {-1,1})",
         "A negative STEP mirrors that dimension (full range: {} defaults to the whole dimension).\n"
         "             This reverses the column order of the reference grid: 4,3,2,1 per row instead of 1,2,3,4.");
    input(referenceGrid);
    output(referenceGrid.view({}, {}, {-1, 1}));

    step("referenceGrid.mirror(0)",
         "mirror(dim) is a named shortcut for exactly the previous line: view() with a full-range,\n"
         "             negative step on just that one dimension. Same result as step 8.");
    input(referenceGrid);
    output(referenceGrid.mirror(0));

    step("referenceGrid.mirror(0).mirror(0)",
         "Mirroring an already-mirrored view returns to the original -- this only holds because each\n"
         "             mirror composes off the PREVIOUS view's own resolved memory address, not off a running\n"
         "             offset measured from referenceGrid. (A flattened/accumulated offset would double-apply\n"
         "             the first mirror's correction and land somewhere else entirely.)");
    input(referenceGrid.mirror(0));
    output(referenceGrid.mirror(0).mirror(0));

    step("referenceGrid.view({}, {}, {2,1})",
         "A step MAGNITUDE > 1 decimates: keeps every 2nd element starting at the (default) start,\n"
         "             so only columns 0 and 2 of the reference grid survive.");
    input(referenceGrid);
    output(referenceGrid.view({}, {}, {2, 1}));

    step("referenceGrid.view({}, {}, {-2,1})",
         "Decimate AND mirror together: a negative step decimates by walking backward from the END\n"
         "             instead of forward from start, so this picks columns 3 and 1 (not 0 and 2, like step 11's positive\n"
         "             decimation) -- a mirrored decimation is a DIFFERENT set of columns, not the same set reordered,\n"
         "             whenever the step magnitude is more than 1.");
    input(referenceGrid);
    output(referenceGrid.view({}, {}, {-2, 1}));

    step("ndImage.view({0,1,1,0}, {-1,2,1,0}, {-1,1,1,1})",
         "view() configures every dimension independently in a single call: mirror x (dim0), keep\n"
         "             rows 1-2 of y (dim1), channel 1 only (dim2), frame 0 only (dim3). No slicing or chaining needed.");
    inputText("the original 4D ndImage, extent {4,3,2,2} -- printed in full above.");
    Image<int, 4> combined = ndImage.view({0, 1, 1, 0}, {-1, 2, 1, 0}, {-1, 1, 1, 1});
    outputNoted("extent = {" + std::to_string(combined.extent()[0]) + "," + std::to_string(combined.extent()[1]) + "," +
                std::to_string(combined.extent()[2]) + "," + std::to_string(combined.extent()[3]) + "}", combined);

    step("ndImage.swap_axes(0, 1)",
         "swap_axes() transposes two dimensions. The full swapped result is still 4D (extent {3,4,2,2}); it's\n"
         "             sliced down to frame0/channel0 below, for a direct visual comparison to the reference grid --\n"
         "             rows and columns are swapped.");
    inputText("the original 4D ndImage, extent {4,3,2,2} -- printed in full above.");
    Image<int, 4> swapped = ndImage.swap_axes(0, 1);
    outputNoted("extent = {" + std::to_string(swapped.extent()[0]) + "," + std::to_string(swapped.extent()[1]) + "," +
                std::to_string(swapped.extent()[2]) + "," + std::to_string(swapped.extent()[3]) +
                "}  (x/y extents traded places); sliced to frame0/channel0 for display:", swapped.slice(3, 0).slice(2, 0));

    step("referenceGrid.view({0},{0})(0,0) = -1;",
         "Neither slice() nor view() ever copies -- both share the original memory. Writing through\n"
         "             any chain of them writes into ndImage's own backing array. (start and end both 0 here,\n"
         "             so this view is a single column -- remember start/end are inclusive, view({0},{1}) would be two.)\n"
         "             To make the sharing concrete, below is the SAME underlying data shown three ways after the\n"
         "             write: the narrow column view actually written through, referenceGrid (the object it was\n"
         "             carved from), and a completely SEPARATE slice pulled fresh from ndImage afterward -- built\n"
         "             from scratch, sharing no C++ object with the other two. All three show the -1, because\n"
         "             there was only ever one array underneath any of them.");
    inputNoted("column 0 of referenceGrid, i.e. referenceGrid.view({0},{0}), before the write:", referenceGrid.view({0}, {0}));

    referenceGrid.view({0}, {0})(0, 0) = -1;

    outputNoted("1) the same column view, re-evaluated after the write:", referenceGrid.view({0}, {0}));
    outputNoted("2) referenceGrid itself (the view was carved from this) -- note only element (0,0) changed:", referenceGrid);
    outputNoted("3) ndImage.slice(3,0).slice(2,0), built fresh from ndImage just now -- a brand new object, same memory:",
        ndImage.slice(3, 0).slice(2, 0));

    step("try { ndImage.view({0,0,0,3}, {-1,-1,-1,0}); } catch (const std::out_of_range& e) { ... }",
         "There is no wraparound: an invalid range (here, frame start=3 is out of bounds for an\n"
         "             extent-2 dimension) throws std::out_of_range instead of silently reading/writing past the buffer.");
    inputText("the original 4D ndImage, extent {4,3,2,2} -- printed in full above (with an out-of-range frame start).");
    try {
        ndImage.view({0, 0, 0, 3}, {-1, -1, -1, 0});
        outputText("(no exception -- this would be a bug)");
    } catch (const std::out_of_range& e) {
        outputText(std::string("threw -- ") + e.what());
    }

    return 0;
}
