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
- \subpage histogram_tutorial -- Histogram<VDIM> and histogram_equalize() (histogram.h), joint histograms, and the Histogram-backed otsu_threshold().
- \subpage distance_transform_tutorial -- distance_transform()/distance_transform_squared() (distance_transform.h), and pairing it with invert() for distance-to-foreground.
- \subpage summed_area_table_tutorial -- summed_area_table()/rectangle_sum() (summed_area_table.h), and O(1) box filtering timed against convolve().
- \subpage motion_tutorial -- lucas_kanade_flow() (optical_flow.h) and sift_flow() (feature_detection.h): dense vs. sparse-then-interpolated displacement estimation, and flow_to_color() visualization, on a real Middlebury benchmark frame pair.
- \subpage ct_reconstruction_tutorial -- forward_project()/back_project() (projection.h), ray-marched CT-style projection built to be an exact adjoint pair, and an iterative Shepp-Logan phantom reconstruction built on nothing else.
- \subpage ct_reconstruction_3d_tutorial -- the 3D, cone/fan-beam sibling: a genuinely perspective ProjectionMatrix<Real,3> (matrix/projection.h), a 128^3 3D Shepp-Logan phantom, spatially-variable automatic anti-aliasing, and reconstruction through a 64x64 detector.
