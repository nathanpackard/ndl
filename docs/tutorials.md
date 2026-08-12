Tutorials {#tutorials}
=========

Each tutorial below is generated automatically from a real demo program's actual
output (see `docs/generate_tutorial.py` and the `docs` CMake target) -- the exact
walkthrough you'd see running the demo yourself, with the code, the rationale
behind it, and (where applicable) the saved result images all inline. They're
regenerated every time the docs are built, so they can never drift from what the
library actually does.

- \subpage multiview_tutorial -- Image::view()/slice()/mirror()/swap_axes(): building zero-copy views of the same memory.
- \subpage convolution_tutorial -- convolve()/gaussian_blur() (convolution.h), arbitrary kernels, border handling, and the FFT-domain equivalent.
- \subpage morphology_tutorial -- erode()/dilate()/median_filter()/percentile_filter() (morphology.h), thresholding, and PackedBitImage.
