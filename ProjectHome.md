NDL, or N-Dimensional Library, is an ND image processing library released under the LGPL v3. It was created by Nathan Packard at UC Davis as a tool for his PhD work on computed tomography systems. The aim of this project is to collect succinct and fast image processing routines that can be generalized to N Dimensions and generalized in any other way possible. Other graphics libraries write different code for accomplishing the same problem in 1D, 2D or 3D. These problems can be implemented for the general case while preserving efficiency by using the Standard Template Library to specify the dimensionality of the images at compile time.

The library is header based. This means that all you have to do is #include <ndl.h> and you can begin using it. There are however plugins for image I/O and image viewing which require certain libraries. NDL was designed to be cross platform, but as of now has only been tested with Microsoft Windows using Microsoft compilers. Both 32 bit and 64 bit (x64) compilation has been tested successfully.

<a href='Hidden comment: 
wxwidgets, openmp, libjpeg, EasyBMP, bctlib, analyzelib, dcmtk). These libraries must be linked to your project and you must #define a constant to enable these libraries in your code (NDL_USE_WXWIDGETS, NDL_USE_OMP, NDL_USE_JPEG, NDL_USE_BMP, NDL_USE_BCTLIB, NDL_USE_ANALYZELIB, NDL_USE_DCMTK).
'></a>

Some Highlights from the library include:
  * ND Fast Fourier Transfrom
  * ND Distance Transform
  * ND Linear Transforms (rotate, translate, scale) using Nearest Neighbor, (Bi,Tri,..)Linear, or (Bi,Tri,..)Cubic interpolation
  * ND Projection (Ortho Projection, Perspective Projection, Back Projection)
  * ND Clustering (Connected Components, K-means)
  * ND Linear Filters (blur,edge,sobeledge,unsharp)
  * ND Non-Linear / Morpheological Filters (median,erode (min), dialate (max), opening, closing, tophat, bottomhat)
  * ND Statistical Functions (mean, median, stdev, skewness, kurtosis, min, max, percentile, histogram)
  * ND Denoising (Total Variation Denoising, anisotropic smoothing (not done yet))
  * ND Viewing (using the gui toolkit wxwidgets, viewing is done by launching a separate process that allows your code to be running while the viewer remains responsive, updating in real time)

NDL also comes with several software applications that (aside from being useful in and of themselves) demonstrate some of the functionality of the library. These applications are:
  * ndlview (generic viewer for ND images supporting raw, jpeg, bmp, DICOM, analyze, and bct)
  * ndlrecon (a cone beam computed tomography image reconstruction program)
  * ndlroc (an application designed for doing observer performance studies on medical images via roc analysis)