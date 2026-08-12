# ndl

ndl is a header-only C++17 library for N-dimensional image processing. An `Image<T,DIM>` works with any element type (`uint8_t`, `float`, `bool`, `std::complex<double>`, ...) and any number of dimensions -- a color image is just a 3D image with a channel axis, a video is a 4D image with a time axis, and the same code (iteration, filtering, arithmetic) works unchanged across all of them, with no per-dimension special-casing anywhere in user code.

## Why it's built this way

**Views are the primitive, not copies.** `Image` never owns memory -- it's a lightweight `{pointer, extent, stride}` triple over a buffer you already have. `view()`, `slice()`, `swap_axes()`, and `mirror()` each return a *new* `Image` describing the *same* memory differently -- a region of interest, a decimated or mirrored range, a transposed pair of axes, a single color channel -- all in O(1), no matter how large the image is. Write through a view and the original buffer changes; that's the point. `OwnedImage<T,DIM>` is the same interface for the common "give me a fresh output buffer" case, when you don't have existing memory to view.

**One algorithm, several storage backends.** `erode()`, `dilate()`, `median_filter()`, `percentile_filter()`, and `threshold()` aren't `Image` member functions with a `PackedBitImage` reimplementation bolted on the side -- they're free functions written once against a minimal structural interface (`extent()`/`at()`/`coordinates()`), so the exact same code runs against a normal `Image<uint8_t,DIM>` and against `PackedBitImage<DIM>` (a genuinely bit-packed boolean image, 1 bit of real storage per pixel instead of a whole byte) with no duplication and no special-casing. `pack()`/`unpack()` convert between the two when you want the compact representation.

**Wrong usage fails to compile, not silently.** Calling `min()`/`max()`/`convolve()`/`otsu_threshold()` (or any of the ordering-dependent operations) on an `Image<std::complex<double>,DIM>` is rejected at compile time with a message naming the actual reason (`std::complex` has no total order), not a five-frame-deep template error. The same goes for bitwise/modulus operators on floating-point images, and for `OwnedImage<bool,DIM>` (which would silently break on `std::vector<bool>`'s bit-packing) -- it's rejected with a message pointing at `PackedBitImage` instead. These restrictions are exercised by dedicated negative-compilation regression tests, not just left as a comment.

**A real toolbox, not just a container.** Beyond views and arithmetic: separable convolution with any kernel and border handling (clamp/wrap/reflect); Gaussian blur; morphology (erosion, dilation, opening, closing, box and cross structuring elements); order-statistic filtering (median and arbitrary percentile); manual and automatic (Otsu) thresholding; an N-dimensional FFT that works at *any* size, not just powers of two (via Bluestein's algorithm, transparently, only when needed -- power-of-two dimensions still take the fast direct path); fixed-size matrix/vector math for transforms; and image I/O for PNG, BMP, JPEG, DICOM, NRRD, and raw formats.

**Lightweight and dependency-free.** Header-only -- `#include <ndl/...>` and go. TBB is the only optional dependency (for multi-threaded FFT fiber transforms); everything works, just single-threaded, without it.

## Building and Testing

The library itself is header-only (just `#include <ndl/...>`), but the unit tests and demos are built via CMake.

Prerequisites: CMake 3.16+, a C++17 compiler. TBB is optional -- if found, `fft.h`'s `fftn()`/`ifftn()` run multi-threaded; otherwise they still work, just single-threaded.

```
cmake -B build
cmake --build build
```

Running tests (from the repo root, or add `--test-dir build` from elsewhere):

```
cd build
ctest -LE smoke   # fast path: the 61 unit tests only (~1s)
ctest             # everything, including the 3 demo smoke tests (~15s)
ctest -L smoke    # just the demo smoke tests (crash/compile regressions, not correctness)
```

The unit tests are split across several GoogleTest-based executables in `unitTests/` (fetched automatically via CMake at configure time); the smoke tests just run each demo binary in `demo/` and check that it completes without crashing. `unitTests/negative_compile/` holds a few deliberately-uncompilable regression checks (see `unitTests/CMakeLists.txt`) verifying the library's compile-time type restrictions stay in place.

For local dev/test iteration with every `assert()` in the codebase active (e.g. `Image::at()`'s bounds check) but without a plain Debug build's full `-O0` cost, build with `-DCMAKE_BUILD_TYPE=Checked` -- optimized like `RelWithDebInfo`, but (unlike every stock CMake build type except `Debug`) deliberately doesn't define `NDEBUG`.

## Generating Documentation

API documentation is generated with [Doxygen](https://www.doxygen.nl/) (graphviz is optional, for class/collaboration diagrams):

```
cmake -B build -DNDL_BUILD_DOCS=ON
cmake --build build --target docs
```

Output lands in `build/docs/html/index.html`. `NDL_BUILD_DOCS` is off by default -- it's not part of the normal build/test loop, and the target is silently skipped (with a configure-time message) if Doxygen isn't installed.

The same docs are published automatically on every push to `master` (see `.github/workflows/docs.yml`) at **https://nathanpackard.github.io/ndl/**.

For browsing on github.com directly without leaving the file tree, `docs/tutorials/` holds a plain-markdown snapshot of the same generated tutorial pages (images included) -- regenerate it with `docs/update_tutorial_snapshots.sh` after a change worth re-publishing; it's committed, not auto-generated, so it only updates when that script is run and the result committed.
