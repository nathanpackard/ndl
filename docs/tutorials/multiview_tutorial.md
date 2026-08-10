# Multiview Tutorial {#multiview_tutorial}

## printing a 4D image

```cpp
std::cout << ndImage
```

operator<< recurses from the OUTERMOST dimension inward: it loops frame (dim3), and for each frame value loops channel (dim2), and for each of those loops y (dim1) -- down to x (dim0), which is what actually becomes one printed line (a comma-separated row). That's a lot of nesting to track from blank lines alone, so every 2D (y,x) block is preceded by an explicit "[dim2=.., dim3=..]" header naming every higher dimension's current index -- blank lines still separate blocks too, but the header is unambiguous at any number of dimensions, where counting blank lines stops being practical past 3D.

```text
    input:   ndImage itself -- this is the setup, not a transformation of anything.
    output:
[dim2=0, dim3=0]
 1.00,  2.00,  3.00,  4.00, 
 5.00,  6.00,  7.00,  8.00, 
 9.00, 10.00, 11.00, 12.00, 
```

```text
[dim2=1, dim3=0]
13.00, 14.00, 15.00, 16.00, 
17.00, 18.00, 19.00, 20.00, 
21.00, 22.00, 23.00, 24.00, 
```

```text
[dim2=0, dim3=1]
25.00, 26.00, 27.00, 28.00, 
29.00, 30.00, 31.00, 32.00, 
33.00, 34.00, 35.00, 36.00, 
```

```text
[dim2=1, dim3=1]
37.00, 38.00, 39.00, 40.00, 
41.00, 42.00, 43.00, 44.00, 
45.00, 46.00, 47.00, 48.00, 
```

```text
(the [dim2=1, dim3=1] block above is the one used in step 5 below)
```

### Step 1
```cpp
ndImage.slice(3, 0)
```

slice(dim, index) fixes dimension `dim` to a single index and removes it from the result -- dropping the dimension count by one, like ndImage[:,:,:,0] would in numpy. It shares the same underlying memory as ndImage (no copy, O(1)), the same way view() does. Slicing away dim3 (frame, the LAST dimension) leaves dims 0,1,2 (x,y,channel) numbered exactly as before -- that only changes when you slice away a dimension something else is numbered relative to (see step 3).

```text
    input:   the original 4D ndImage, extent {4,3,2,2} -- printed in full above.
    output:  extent = {4,3,2}  (still x,y,channel -- dim3 is just gone)
[dim2=0]
 1.00,  2.00,  3.00,  4.00, 
 5.00,  6.00,  7.00,  8.00, 
 9.00, 10.00, 11.00, 12.00, 
```

```text
[dim2=1]
13.00, 14.00, 15.00, 16.00, 
17.00, 18.00, 19.00, 20.00, 
21.00, 22.00, 23.00, 24.00, 
```

### Step 2
```cpp
frame0.slice(2, 0)
```

Slicing again, this time on the 3D result of step 1: dim2 of frame0 is channel (frame0's dims are still x=0,y=1,channel=2, per step 1), so this fixes channel=0 and drops down to a plain 2D (x,y) grid -- our reference grid for the rest of this file. slice() calls chain naturally since each one just returns another Image, one dimension smaller, still sharing memory.

```text
    input:   frame0, the output of step 1 (repeated here for reference):
[dim2=0]
 1.00,  2.00,  3.00,  4.00, 
 5.00,  6.00,  7.00,  8.00, 
 9.00, 10.00, 11.00, 12.00, 
```

```text
[dim2=1]
13.00, 14.00, 15.00, 16.00, 
17.00, 18.00, 19.00, 20.00, 
21.00, 22.00, 23.00, 24.00, 
```

```text
   output:
1.00,  2.00,  3.00,  4.00, 
5.00,  6.00,  7.00,  8.00, 
9.00, 10.00, 11.00, 12.00, 
```

### Step 3
```cpp
ndImage.slice(0, 0)
```

To make the renumbering rule concrete: this slices away dim0 (x) instead of a trailing dimension. Every dimension ABOVE the one you slice shifts down by one in the result; dimensions below it (there are none here, since we sliced dim0) are unaffected. So y (was dim1) becomes dim0, channel (was dim2) becomes dim1, and frame (was dim3) becomes dim2. Notice the values below are a totally different arrangement of the same 12 numbers per block: rows are now y (3 values), not x (4 values) -- because dim0 itself changed meaning, from x to y.

```text
    input:   the original 4D ndImage, extent {4,3,2,2} -- printed in full above.
    output:  extent = {3,2,2}  (y,channel,frame -- each renumbered down by one)
[dim2=0]
 1.00,  5.00,  9.00, 
13.00, 17.00, 21.00, 
```

```text
[dim2=1]
25.00, 29.00, 33.00, 
37.00, 41.00, 45.00, 
```

### Step 4
```cpp
ndImage(2, 1, 0, 0)
```

Element access: one integer per dimension (x, y, channel, frame). Reads a single value -- unlike slice()/view(), there's no array in the output, just the one number at that coordinate.

```text
input:   the original 4D ndImage, extent {4,3,2,2} -- printed in full above. (referenceGrid below is NOT
         the input -- ndImage(2,1,0,0) reads directly from ndImage. It's shown only to locate the value.)
output:  7  (a single scalar, not an array -- row y=1 above is 5,6,7,8, and x=2 is 7)
```

### Step 5
```cpp
ndImage.slice(3, 1).slice(2, 1)
```

Both slice indices this time are 1: drop frame (=1) then channel (=1), landing on the frame1/channel1 2D grid -- expected values 37..48, matching the [dim2=1, dim3=1] block from the full print above.

```text
    input:   the original 4D ndImage, extent {4,3,2,2} -- printed in full above.
    output:
37.00, 38.00, 39.00, 40.00, 
41.00, 42.00, 43.00, 44.00, 
45.00, 46.00, 47.00, 48.00, 
```

### Step 6
```cpp
referenceGrid.view({1}, {2})
```

view()'s start/end are both INCLUSIVE indices into the current image. This keeps only columns (x) 1 and 2 of the reference grid -- 2 of its original 4 columns.

```text
   input:
1.00,  2.00,  3.00,  4.00, 
5.00,  6.00,  7.00,  8.00, 
9.00, 10.00, 11.00, 12.00, 
```

```text
    output:
 2.00,  3.00, 
 6.00,  7.00, 
10.00, 11.00, 
```

### Step 7
```cpp
referenceGrid.view({-2}, {-1})
```

Negative indices count from the last element (Python-slice style): -1 is the last column, -2 the second-to-last. This keeps the last two columns of the reference grid.

```text
   input:
1.00,  2.00,  3.00,  4.00, 
5.00,  6.00,  7.00,  8.00, 
9.00, 10.00, 11.00, 12.00, 
```

```text
    output:
 3.00,  4.00, 
 7.00,  8.00, 
11.00, 12.00, 
```

### Step 8
```cpp
referenceGrid.view({}, {}, {-1,1})
```

A negative STEP mirrors that dimension (full range: {} defaults to the whole dimension). This reverses the column order of the reference grid: 4,3,2,1 per row instead of 1,2,3,4.

```text
   input:
1.00,  2.00,  3.00,  4.00, 
5.00,  6.00,  7.00,  8.00, 
9.00, 10.00, 11.00, 12.00, 
```

```text
    output:
 4.00,  3.00,  2.00,  1.00, 
 8.00,  7.00,  6.00,  5.00, 
12.00, 11.00, 10.00,  9.00, 
```

### Step 9
```cpp
referenceGrid.mirror(0)
```

mirror(dim) is a named shortcut for exactly the previous line: view() with a full-range, negative step on just that one dimension. Same result as step 8.

```text
   input:
1.00,  2.00,  3.00,  4.00, 
5.00,  6.00,  7.00,  8.00, 
9.00, 10.00, 11.00, 12.00, 
```

```text
    output:
 4.00,  3.00,  2.00,  1.00, 
 8.00,  7.00,  6.00,  5.00, 
12.00, 11.00, 10.00,  9.00, 
```

### Step 10
```cpp
referenceGrid.mirror(0).mirror(0)
```

Mirroring an already-mirrored view returns to the original -- this only holds because each mirror composes off the PREVIOUS view's own resolved memory address, not off a running offset measured from referenceGrid. (A flattened/accumulated offset would double-apply the first mirror's correction and land somewhere else entirely.)

```text
    input:
 4.00,  3.00,  2.00,  1.00, 
 8.00,  7.00,  6.00,  5.00, 
12.00, 11.00, 10.00,  9.00, 
```

```text
   output:
1.00,  2.00,  3.00,  4.00, 
5.00,  6.00,  7.00,  8.00, 
9.00, 10.00, 11.00, 12.00, 
```

### Step 11
```cpp
referenceGrid.view({}, {}, {2,1})
```

A step MAGNITUDE > 1 decimates: keeps every 2nd element starting at the (default) start, so only columns 0 and 2 of the reference grid survive.

```text
   input:
1.00,  2.00,  3.00,  4.00, 
5.00,  6.00,  7.00,  8.00, 
9.00, 10.00, 11.00, 12.00, 
```

```text
   output:
1.00,  3.00, 
5.00,  7.00, 
9.00, 11.00, 
```

### Step 12
```cpp
referenceGrid.view({}, {}, {-2,1})
```

Decimate AND mirror together: a negative step decimates by walking backward from the END instead of forward from start, so this picks columns 3 and 1 (not 0 and 2, like step 11's positive decimation) -- a mirrored decimation is a DIFFERENT set of columns, not the same set reordered, whenever the step magnitude is more than 1.

```text
   input:
1.00,  2.00,  3.00,  4.00, 
5.00,  6.00,  7.00,  8.00, 
9.00, 10.00, 11.00, 12.00, 
```

```text
    output:
 4.00,  2.00, 
 8.00,  6.00, 
12.00, 10.00, 
```

### Step 13
```cpp
ndImage.view({0,1,1,0}, {-1,2,1,0}, {-1,1,1,1})
```

view() configures every dimension independently in a single call: mirror x (dim0), keep rows 1-2 of y (dim1), channel 1 only (dim2), frame 0 only (dim3). No slicing or chaining needed.

```text
    input:   the original 4D ndImage, extent {4,3,2,2} -- printed in full above.
    output:  extent = {4,2,1,1}
[dim2=0, dim3=0]
20.00, 19.00, 18.00, 17.00, 
24.00, 23.00, 22.00, 21.00, 
```

### Step 14
```cpp
ndImage.swap_axes(0, 1)
```

swap_axes() transposes two dimensions. The full swapped result is still 4D (extent {3,4,2,2}); it's sliced down to frame0/channel0 below, for a direct visual comparison to the reference grid -- rows and columns are swapped.

```text
   input:   the original 4D ndImage, extent {4,3,2,2} -- printed in full above.
   output:  extent = {3,4,2,2}  (x/y extents traded places); sliced to frame0/channel0 for display:
1.00,  5.00,  9.00, 
2.00,  6.00, 10.00, 
3.00,  7.00, 11.00, 
4.00,  8.00, 12.00, 
```

### Step 15
```cpp
referenceGrid.view({0},{0})(0,0) = -1;
```

Neither slice() nor view() ever copies -- both share the original memory. Writing through any chain of them writes into ndImage's own backing array. (start and end both 0 here, so this view is a single column -- remember start/end are inclusive, view({0},{1}) would be two.) To make the sharing concrete, below is the SAME underlying data shown three ways after the write: the narrow column view actually written through, referenceGrid (the object it was carved from), and a completely SEPARATE slice pulled fresh from ndImage afterward -- built from scratch, sharing no C++ object with the other two. All three show the -1, because there was only ever one array underneath any of them.

```text
    input:   column 0 of referenceGrid, i.e. referenceGrid.view({0},{0}), before the write:
1.00, 
5.00, 
9.00, 
```

```text
    output:  1) the same column view, re-evaluated after the write:
-1.00, 
 5.00, 
 9.00, 
```

```text
    output:  2) referenceGrid itself (the view was carved from this) -- note only element (0,0) changed:
-1.00,  2.00,  3.00,  4.00, 
 5.00,  6.00,  7.00,  8.00, 
 9.00, 10.00, 11.00, 12.00, 
```

```text
    output:  3) ndImage.slice(3,0).slice(2,0), built fresh from ndImage just now -- a brand new object, same memory:
-1.00,  2.00,  3.00,  4.00, 
 5.00,  6.00,  7.00,  8.00, 
 9.00, 10.00, 11.00, 12.00, 
```

### Step 16
```cpp
try { ndImage.view({0,0,0,3}, {-1,-1,-1,0}); } catch (const std::out_of_range& e) { ... }
```

There is no wraparound: an invalid range (here, frame start=3 is out of bounds for an extent-2 dimension) throws std::out_of_range instead of silently reading/writing past the buffer.

input:   the original 4D ndImage, extent {4,3,2,2} -- printed in full above (with an out-of-range frame start). output:  threw -- Image::view: [start=3, end=0] is not a valid range for dimension 3 (extent 2); views may not wrap around

