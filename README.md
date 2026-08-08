# ndl
ndl is a multi-dimensional image processing library written in C++. ndl has the following features:
* multi-dimensional images: Images can be created of any data type with an arbitrary number of dimensions. Colors can be simply represented as an extra dimension
* smart-iterators: a single loop can iterate over a multi-dimensional image (no need for nested loops). The iterator keeps track of the n-dimensional coordinate for fast access to a local neighborhood within the image. This enables clean implementations of multi-dimensional neighborhood based filters.
* memory sharing: image data is provided at construction so that multiple image objects can reference the same memory. This is helpful for multi-view support as well as for embedded or gpu projects where different types of memory are used.
* multi-view: memory sharing allows images to be created fast without any data copies. For example, an image can be reflected, decimated, dimensions can be swapped, or a region of interest can be selected without any iteration through the memory. A new image class is quickly created that simply views and iterates through the same data differntly.
* lightweight: a templated header only library make for simple and easy integration with any c++ project.

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
ctest -LE smoke   # fast path: the 52 unit tests only (~1s)
ctest             # everything, including the 3 demo smoke tests (~15s)
ctest -L smoke    # just the demo smoke tests (crash/compile regressions, not correctness)
```

The unit tests are split across several GoogleTest-based executables in `unitTests/` (fetched automatically via CMake at configure time); the smoke tests just run each demo binary in `demo/` and check that it completes without crashing.
