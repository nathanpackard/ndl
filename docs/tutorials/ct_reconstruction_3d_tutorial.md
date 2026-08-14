# 3D Cone-Beam CT Reconstruction Tutorial {#ct_reconstruction_3d_tutorial}

This demo is the 3D, cone/fan-beam sibling of demo/ct_reconstruction (2D parallel-beam) -- same forward_project()/back_project() (projection.h), a genuinely perspective geometry this time. It's considerably slower (128^3 volume, ~740k cone-beam rays per pass) -- expect roughly 1-2 minutes end to end, mostly PART 5's reconstruction loop. Output PNGs land in: /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output

## PART 1: cone-beam geometry -- a single voxel, checked by hand

### Step 1
```cpp
forward_project(singleVoxel, sino1, geometry1)   // one voxel at the cone's own center of rotation
```

Unlike an off-center voxel, a voxel exactly on the source-to-detector axis lands at the SAME detector pixel (the detector's own center) at every view angle regardless of the geometry being a true perspective (cone-beam) projection -- the one case simple enough to check without a calculator, and a real test of the (detW/2,detH/2) recentering math (see buildConeBeamGeometry()'s own comment).

```text
view 0 peak detector pixel (expect (4,4)):   (4,4)  value=0.987654
view 1 peak detector pixel (expect (4,4)):   (4,4)  value=1.000000
view 2 peak detector pixel (expect (4,4)):   (4,4)  value=1.000000
view 3 peak detector pixel (expect (4,4)):   (4,4)  value=1.000000
```

## PART 2: the 3D Shepp-Logan phantom (128^3)

### Step 2
```cpp
sheppLoganValue3D(x,y,z)   // ten ellipsoids, additive density
```

The classic Shepp-Logan phantom, extended to a solid (Kak & Slaney) -- an idealized 3D head volume built from ten overlapping ellipsoids. Three orthogonal central slices are saved below (axial/coronal/sagittal) since a whole volume can't be viewed as one PNG.

[![ct_reconstruction_3d_tutorial_01_phantom_axial.png](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_01_phantom_axial.png)](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_01_phantom_axial.png)

```text
phantom, axial slice (z=64): /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output/01_phantom_axial.png
    extent = {128, 128}   min=0  max=255  mean=30.4158
```

[![ct_reconstruction_3d_tutorial_02_phantom_coronal.png](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_02_phantom_coronal.png)](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_02_phantom_coronal.png)

```text
phantom, coronal slice (y=64): /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output/02_phantom_coronal.png
    extent = {128, 128}   min=0  max=255  mean=25.6238
```

[![ct_reconstruction_3d_tutorial_03_phantom_sagittal.png](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_03_phantom_sagittal.png)](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_03_phantom_sagittal.png)

```text
phantom, sagittal slice (x=64): /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output/03_phantom_sagittal.png
    extent = {128, 128}   min=0  max=255  mean=40.6919
```

## PART 3: forward_project() the phantom through a 64x64-detector cone-beam geometry

### Step 3
```cpp
forward_project(phantom, sinogram, geometry)   // 180 cone-beam views over 360 degrees
```

Each of the 180 saved views is a full 64x64 cone-beam projection image (not a single 1D row, unlike the 2D demo's sinogram) -- 04_projection_view0.png shows one representative view; 05_sinogram_slice.png shows the classic sinogram layout (view angle x detector row) for just the detector's own central row, across all 180 views.

[![ct_reconstruction_3d_tutorial_04_projection_view0.png](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_04_projection_view0.png)](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_04_projection_view0.png)

```text
cone-beam projection, view 0: /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output/04_projection_view0.png
    extent = {64, 64}   min=0  max=255  mean=29.7607
```

[![ct_reconstruction_3d_tutorial_05_sinogram_slice.png](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_05_sinogram_slice.png)](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_05_sinogram_slice.png)

```text
sinogram slice (central detector row, all views): /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output/05_sinogram_slice.png
    extent = {180, 64}   min=0  max=255  mean=71.91
```

## PART 4: cone-beam anti-aliasing is spatially VARIABLE, unlike parallel-beam's uniform case

### Step 4
```cpp
forward_project(phantom, sinoAA, smallGeometry, Linear{}, /*autoAA=*/true)   // 3 views only, for a quick before/after
```

For parallel-beam projection (demo/ct_reconstruction), magnification is constant everywhere, so the correct anti-aliasing footprint is a single fixed size per view. Cone-beam is a true perspective projection: a voxel's footprint on the detector shrinks approaching the detector and grows approaching the source, so forward_project()'s automatic anti-aliasing (projection.h's own top comment) genuinely differs from the unfiltered result here -- just 3 views for a quick, cheap before/after comparison; PART 5 below runs the real reconstruction with autoAA on for all 180.

```text
mean |AA-enabled - AA-disabled| over these 3 views:   0.207334 (nonzero confirms AA is genuinely filtering, not a no-op)
```

## PART 5: reconstructing the phantom from ONLY the 64x64-detector sinogram

### Step 5
```cpp
recon += lambda * back_project(sinogram - forward_project(recon), autoAA=true); clamp to [0, max_density_bound(sinogram)]
```

The same algorithm demo/ct_reconstruction uses in 2D, plus two things that particular demo doesn't need. First: back_project() is forward_project()'s exact adjoint regardless of DIM, beam geometry, OR whether autoAA is on (the dot-product test in unitTests/projection_tests.cpp checks all of this directly, to ~1e-15 relative error even with AA enabled and the data touching the volume's own boundary -- projection.h's own top comment has the matched-filter construction that makes this possible). Second: this 64x64-detector/180-view setup has fewer measurements (180*64*64 ~= 737k) than volume unknowns (128^3 ~= 2.1M) -- an underdetermined system, where PLAIN (unconstrained, unfiltered) Landweber exhibits classic SEMI-CONVERGENCE: RMSE against the ground truth drops for the first ~10 iterations, then starts RISING again as later iterations fit increasingly noisy/artifact-prone null-space directions instead of real structure. Clamping each update to [0, max_density_bound(sinogram)] (projection.h's own comment has the derivation -- the lower bound is physical non-negativity, the upper bound is read directly off the sinogram) fixes the semi-convergence outright: box-constrained ("projected") gradient descent doesn't wander into those noisy directions in the first place, RMSE keeps falling monotonically for at least 25 iterations. autoAA=true goes further: since this geometry's detector really is coarser than the volume (PART 4 just confirmed autoAA changes the forward projection here, not a no-op), letting forward_project() account for that instead of aliasing it away measurably lowers the RMSE this reaches at every iteration -- not just a display fix, a better-conditioned problem for the iteration to actually solve. Both parallelized over views (std::execution::par) is what keeps 25 iterations of a 128^3 cone-beam volume, with AA, inside this demo's own 1-2 minute budget.

```text
iteration 0:   RMSE vs. ground truth = 0.163685
iteration 1:   RMSE vs. ground truth = 0.152973
iteration 2:   RMSE vs. ground truth = 0.148445
iteration 3:   RMSE vs. ground truth = 0.145588
iteration 4:   RMSE vs. ground truth = 0.143413
iteration 5:   RMSE vs. ground truth = 0.141635
iteration 6:   RMSE vs. ground truth = 0.140142
iteration 7:   RMSE vs. ground truth = 0.138874
iteration 8:   RMSE vs. ground truth = 0.137786
iteration 9:   RMSE vs. ground truth = 0.136844
iteration 10:   RMSE vs. ground truth = 0.136026
iteration 11:   RMSE vs. ground truth = 0.135312
iteration 12:   RMSE vs. ground truth = 0.134687
iteration 13:   RMSE vs. ground truth = 0.134139
iteration 14:   RMSE vs. ground truth = 0.133658
iteration 15:   RMSE vs. ground truth = 0.133237
iteration 16:   RMSE vs. ground truth = 0.132869
iteration 17:   RMSE vs. ground truth = 0.132547
iteration 18:   RMSE vs. ground truth = 0.132266
iteration 19:   RMSE vs. ground truth = 0.132023
iteration 20:   RMSE vs. ground truth = 0.131814
iteration 21:   RMSE vs. ground truth = 0.131635
iteration 22:   RMSE vs. ground truth = 0.131484
iteration 23:   RMSE vs. ground truth = 0.131359
iteration 24:   RMSE vs. ground truth = 0.131257
```

[![ct_reconstruction_3d_tutorial_06_reconstruction_axial.png](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_06_reconstruction_axial.png)](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_06_reconstruction_axial.png)

```text
reconstruction, axial slice (z=64): /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output/06_reconstruction_axial.png
    extent = {128, 128}   min=0  max=255  mean=31.1061
```

[![ct_reconstruction_3d_tutorial_07_reconstruction_coronal.png](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_07_reconstruction_coronal.png)](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_07_reconstruction_coronal.png)

```text
reconstruction, coronal slice (y=64): /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output/07_reconstruction_coronal.png
    extent = {128, 128}   min=0  max=255  mean=21.1902
```

[![ct_reconstruction_3d_tutorial_08_reconstruction_sagittal.png](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_08_reconstruction_sagittal.png)](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_08_reconstruction_sagittal.png)

```text
reconstruction, sagittal slice (x=64): /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output/08_reconstruction_sagittal.png
    extent = {128, 128}   min=0  max=255  mean=32.3024
```

All outputs written to: /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output 01-03 are the ground-truth phantom's three central slices; 04-05 show the cone-beam sinogram (what a real cone-beam CT scanner's detector would actually measure); 06-08 are the reconstruction's matching slices, recovered from ONLY that sinogram (with autoAA on and the max_density_bound() constraint applied -- PART 5's own comment covers both). Compare 06-08 against 01-03 -- the background is clean (max_density_bound()'s whole point: no bright stray-pixel overshoot), the overall oval shape and largest internal structure are recognizable, and the ring/grid pattern within the object outline is visibly SOFTER than it would be with AA off (RMSE is measurably lower too, at every iteration -- PART 5's own comment), but it isn't gone. That's expected, not a bug: AA stops the iteration from aliasing detail the sinogram can't support into sharp noise, but it doesn't add information -- the system is still underdetermined (737k detector measurements for 2.1M volume unknowns), and max_density_bound() is only ever tight for background voxels, not interior ones (projection.h's own comment on it has the reasoning), so neither constraint touches the remaining interior ambiguity. demo/ct_reconstruction's 2D/ 60-iteration result looks cleaner throughout because that problem isn't underdetermined at all (145 detector pixels x 90 views vs. only 10000 pixel unknowns) -- closing the rest of this gap here would take more views, a higher-resolution detector, or real regularization (e.g. total-variation), not just more of what's already implemented.

