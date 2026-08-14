# 3D Cone-Beam CT Reconstruction Tutorial {#ct_reconstruction_3d_tutorial}

This demo is the 3D, cone/fan-beam sibling of demo/ct_reconstruction (2D parallel-beam) -- same forward_project()/back_project() (projection.h), a genuinely perspective geometry this time. It's considerably slower than a 2D demo (64^3 volume, ~740k cone-beam rays per pass) -- expect well under a minute end to end, mostly PART 5's reconstruction loop. Output PNGs land in: /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output

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

## PART 2: the 3D Shepp-Logan phantom (64^3)

### Step 2
```cpp
sheppLoganValue3D(x,y,z)   // ten ellipsoids, additive density
```

The classic Shepp-Logan phantom, extended to a solid (Kak & Slaney) -- an idealized 3D head volume built from ten overlapping ellipsoids, evaluated directly onto a 64^3 grid (see PART 3's comment for why 64^3, not the finer 128^3 an earlier version of this demo used). Three orthogonal central slices are saved below (axial/coronal/sagittal) since a whole volume can't be viewed as one PNG.

[![ct_reconstruction_3d_tutorial_01_phantom_axial.png](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_01_phantom_axial.png)](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_01_phantom_axial.png)

```text
phantom, axial slice (z=64): /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output/01_phantom_axial.png
    extent = {64, 64}   min=0  max=255  mean=30.5547
```

[![ct_reconstruction_3d_tutorial_02_phantom_coronal.png](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_02_phantom_coronal.png)](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_02_phantom_coronal.png)

```text
phantom, coronal slice (y=64): /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output/02_phantom_coronal.png
    extent = {64, 64}   min=0  max=255  mean=24.7119
```

[![ct_reconstruction_3d_tutorial_03_phantom_sagittal.png](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_03_phantom_sagittal.png)](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_03_phantom_sagittal.png)

```text
phantom, sagittal slice (x=64): /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output/03_phantom_sagittal.png
    extent = {64, 64}   min=0  max=255  mean=40.2671
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
    extent = {64, 64}   min=0  max=255  mean=30.5503
```

[![ct_reconstruction_3d_tutorial_05_sinogram_slice.png](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_05_sinogram_slice.png)](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_05_sinogram_slice.png)

```text
sinogram slice (central detector row, all views): /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output/05_sinogram_slice.png
    extent = {180, 64}   min=0  max=255  mean=70.7186
```

## PART 4: cone-beam anti-aliasing is spatially VARIABLE, unlike parallel-beam's uniform case

### Step 4
```cpp
forward_project(phantom, sinoAA, smallGeometry, Linear{}, /*autoAA=*/true)   // 3 views only, for a quick before/after
```

For parallel-beam projection (demo/ct_reconstruction), magnification is constant everywhere, so the correct anti-aliasing footprint is a single fixed size per view. Cone-beam is a true perspective projection: a voxel's footprint on the detector shrinks approaching the detector and grows approaching the source, so forward_project()'s automatic anti-aliasing (projection.h's own top comment) genuinely differs from the unfiltered result here -- just 3 views for a quick, cheap before/after comparison; PART 5 below runs the real reconstruction with autoAA on for all 180.

```text
mean |AA-enabled - AA-disabled| over these 3 views:   0.131187 (nonzero confirms AA is genuinely filtering, not a no-op)
```

## PART 5: reconstructing the phantom from ONLY the 64x64-detector sinogram

### Step 5
```cpp
recon += lambda * back_project(sinogram - forward_project(recon), autoAA=true); clamp to [0, max_density_bound(sinogram)]
```

The same algorithm demo/ct_reconstruction uses in 2D, plus the same three things covered in that demo's own comment. First: back_project() is forward_project()'s exact adjoint regardless of DIM, beam geometry, OR whether autoAA is on (the dot-product test in unitTests/projection_tests.cpp checks all of this directly, to ~1e-15 relative error even with AA enabled and the data touching the volume's own boundary -- projection.h's own top comment has the matched-filter construction that makes this possible). Second: PART 3's own comment covers why this demo now reconstructs onto a 64^3 grid rather than the finer 128^3 an earlier version used -- with 64^3, this 64x64-detector/180-view setup has MORE measurements (180*64*64 ~= 737k) than volume unknowns (64^3 ~= 262k), an overdetermined, well-posed system, unlike the earlier 128^3 version where plain (unconstrained, unfiltered) Landweber exhibited classic SEMI-CONVERGENCE (RMSE dropping for a while, then rising again as later iterations fit increasingly noisy/artifact-prone null-space directions). Clamping each update to [0, max_density_bound(sinogram)] (projection.h's own comment has the derivation -- the lower bound is physical non-negativity, the upper bound is read directly off the sinogram) is kept regardless, both because it's still a real (if smaller) help here and because it's what keeps the background artifact-free (see the final summary below). Third: autoAA=true evaluates a genuine PER-RAY-SAMPLE footprint (projection.h's own top comment, and unitTests/projection_tests.cpp's AAHalfWidthVariesWithDepthAlongRay, which checks it against the closed-form pinhole-camera magnification formula directly) -- an earlier version evaluated it once per view at the volume's own center, which is measurably wrong for true perspective geometry (the real footprint varies ~1.5x between the near-source and near-detector sides of this exact volume) and was silently over-filtering one side while under-filtering the other. Per-sample evaluation needs a matrix inversion at every ray sample, so projection.h uses a fast direct (adjugate/cofactor, not SVD) inverse specifically to keep it practical; that cost plus forward_project()/back_project() both parallelizing over views (std::execution::par) is what keeps 25 iterations of this now considerably smaller 64^3 volume comfortably fast.

```text
iteration 0:   RMSE vs. ground truth = 0.141711
iteration 1:   RMSE vs. ground truth = 0.130883
iteration 2:   RMSE vs. ground truth = 0.125346
iteration 3:   RMSE vs. ground truth = 0.121384
iteration 4:   RMSE vs. ground truth = 0.118372
iteration 5:   RMSE vs. ground truth = 0.115972
iteration 6:   RMSE vs. ground truth = 0.114014
iteration 7:   RMSE vs. ground truth = 0.112388
iteration 8:   RMSE vs. ground truth = 0.111022
iteration 9:   RMSE vs. ground truth = 0.109863
iteration 10:   RMSE vs. ground truth = 0.108879
iteration 11:   RMSE vs. ground truth = 0.108033
iteration 12:   RMSE vs. ground truth = 0.107299
iteration 13:   RMSE vs. ground truth = 0.106660
iteration 14:   RMSE vs. ground truth = 0.106103
iteration 15:   RMSE vs. ground truth = 0.105621
iteration 16:   RMSE vs. ground truth = 0.105203
iteration 17:   RMSE vs. ground truth = 0.104840
iteration 18:   RMSE vs. ground truth = 0.104525
iteration 19:   RMSE vs. ground truth = 0.104254
iteration 20:   RMSE vs. ground truth = 0.104019
iteration 21:   RMSE vs. ground truth = 0.103820
iteration 22:   RMSE vs. ground truth = 0.103651
iteration 23:   RMSE vs. ground truth = 0.103511
iteration 24:   RMSE vs. ground truth = 0.103395
```

[![ct_reconstruction_3d_tutorial_06_reconstruction_axial.png](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_06_reconstruction_axial.png)](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_06_reconstruction_axial.png)

```text
reconstruction, axial slice (z=64): /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output/06_reconstruction_axial.png
    extent = {64, 64}   min=0  max=255  mean=30.3142
```

[![ct_reconstruction_3d_tutorial_07_reconstruction_coronal.png](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_07_reconstruction_coronal.png)](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_07_reconstruction_coronal.png)

```text
reconstruction, coronal slice (y=64): /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output/07_reconstruction_coronal.png
    extent = {64, 64}   min=0  max=255  mean=25.5408
```

[![ct_reconstruction_3d_tutorial_08_reconstruction_sagittal.png](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_08_reconstruction_sagittal.png)](images/ct_reconstruction_3d_tutorial/ct_reconstruction_3d_tutorial_08_reconstruction_sagittal.png)

```text
reconstruction, sagittal slice (x=64): /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output/08_reconstruction_sagittal.png
    extent = {64, 64}   min=0  max=255  mean=38.2383
```

All outputs written to: /home/nathanpackard/git/ndl/build/demo/ct_reconstruction_3d/output 01-03 are the ground-truth phantom's three central slices; 04-05 show the cone-beam sinogram (what a real cone-beam CT scanner's detector would actually measure); 06-08 are the reconstruction's matching slices, recovered from ONLY that sinogram (with a genuine per-ray-sample autoAA and the max_density_bound() constraint applied -- PART 5's own comment covers both). Compare 06-08 against 01-03 -- the background is clean (max_density_bound()'s whole point: no bright stray-pixel overshoot) and the overall shape, proportions, and internal structure are recognizable at this grid's own resolution, without the persistent ring/grid null-space pattern an earlier, 128^3 version of this same demo had. That difference is the point of PART 3's resolution choice, not a coincidence: reconstructing onto a 64^3 grid instead of 128^3 (with the same 64x64/180-view detector) turns this from an underdetermined system (737k measurements for 2.1M unknowns, where multiple different volumes fit the same sinogram equally well, and the residual ambiguity shows up as structured aliasing) into an overdetermined one (737k measurements for only 262k unknowns) -- AA and max_density_bound() were only ever mitigating that underlying mismatch, not fixing it. The real tradeoff: this reconstruction is honestly coarser than the 128^3 phantom it's compared against here -- but that's the resolution this 64x64 detector genuinely supports (each detector pixel's own footprint back in the volume is ~1.75 voxel-widths at this grid), so 64^3 isn't losing real detail, just no longer pretending to reconstruct finer than the measurements can support. Recovering 128^3-level detail for real would take a higher-resolution detector (more, or smaller, detector pixels) or more views, not a finer reconstruction grid alone.

