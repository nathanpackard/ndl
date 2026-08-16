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
- \subpage nd_viewer_tutorial -- pairwise_slice()/for_each_axis_pair() (viewer.h): viewing a genuinely 4D (space + time) volume as a synchronized grid of every pairwise-axis 2D plane, with the actual interactive WebGL viewer (ndlviewer.js) embedded live in the page.
- \subpage color_video_tutorial -- load_video_owned() (imageIO/video_io.h): loading a real .mp4 file and viewing it two ways in the same pairwise-slice viewer -- channel as a generic higher-dimensional axis (grayscale) vs. true RGB compositing via the viewer's new options.colorAxis.

Apps {#apps}
====

Unlike the tutorials above, an app (`apps/`) is a live client/server program -- it keeps
running until stopped, so there's no "captured output" to embed into a static page the way
a demo's is. Each app page below still documents the full source and its own explanation
(see `docs/generate_app_doc.py`), just with build/run instructions in place of an embedded
result.

- \subpage live_video_stream_app -- ring_buffer.h/viewport.h/net/websocket_server.h/net/json.h: streaming a local video file (or, eventually, any real-time sensor) into a browser over a hand-rolled WebSocket, where the CLIENT's current view (crop region, resolution, window/level) drives what the server actually renders -- the same "client's view is a request" model Google Earth/Neuroglancer use, applied to a live feed.
