// Copyright (C) 2009   Nathan Packard   <nathanpackard@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as 
// published by the Free Software Foundation; either version 3 of the 
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public 
// License along with this program; if not, see 
// <http://www.gnu.org/licenses/>.

#ifndef _NDL
#define _NDL

#include <graph.h>
#include <image.h>

#endif

/*
b=copy(a) //do exact copy
b=copy(a,true) //do exact shared copy

//ASSIGNMENT
a = b
if a is empty, a=image with selection of b
if a has selection, copy b to selection of a

//OTHER
when mapping from one selection to another, do interpolation
*/


/*! 
\mainpage NDL
\section intro_sec Introduction
NDL, or N-Dimensional Library, is an ND graphics 
processing library (for images and meshes) relesased under the LGPLv3. 
It was created by Nathan Packard at UC Davis as a tool for his pHd 
work on computed tomography systems. The aim of this project is to 
collect succinct, and fast image / mesh processing routines that can 
be generalized to N Dimensions and generalized in any other way possible. 
Other graphics libraries write different code for accomplishing the same 
problem in 2D or 3D. These problems can be implemented for the general 
case while preserving effency by using the Standard Template Library to 
specify the dimensionality of the images / meshes at compile time. 

\section highlights_sec Highlighted Features

ndl::Image
-# ND Fast Fourier Transfrom
-# ND Distance Transform
-# ND Linear Transforms [rotate, translate, scale] using Nearest Neighbor, (Bi,Tri,..)Linear, or (Bi,Tri,..)Cubic interpolation
-# ND Projection [Ortho Projection, Perspective Projection (not done), Back Projection (not done)]
-# ND Clustering [Connected Components, K-means]
-# ND Linear Filters [blur,edge,sobeledge,unsharp]
-# ND Non-Linear / Morpheological Filters [anisotropic smoothing (not done),median,erode (min), dialate (max), opening, closing, tophat, bottomhat]
-# ND Statistical Functions [mean,median,stdev,skewness,kurtosis,min,max,percentile,histogram]

ndl::Mesh
-# an ND point cloud to ND delauny mesh generator is near completion
-# still under development

\section install_sec Getting Started
NDL is a header based library. This means that all you have to do is #include <ndl.h>
and you can begin using it. There are however plugins for image I/O and image viewing which may require
certain libraries. These plugins are disabled by default but can be enabled by including macros that are listed on the 
documentation for the given plugin. See ndl::ImageTypePlugin for a list of image I/O plugins. See 
ndl::ViewPlugin for a list of image viewing plugins. Mesh file plugins will eventually be supported, but this
part of NDL requires more development.

NDL is primarily implemented in two classes: ndl::Image and ndl::Mesh, they use a 
few supporting classes for plugins (ndl::ImageTypePlugin, ndl::ViewPlugin), linear algebra (Vector, Matrix), 
and shape primitives (ndl::Shape,ndl::Simplex). Demo image and mesh programs
are included with NDL and they provide a good starting point for new users.

\section todo_sec Developer ToDo List
-# REMOVE MATRIX AND VECTOR VARS FROM IMAGE.H
-# add exception messages (partularly about not enough memory)
-# generalize filtering mechanisim to size N
-# reduce memory requrements for operations by subdividing the problem
-# figure out projection stuff in imagedemo
-# Look at FLTK (fast light toolkit) to make a gui app with graphicslib
-# implement recon (with generic forward and back projection methods)
-# look at enable or disable of openMP option
-# look at adding cuda or glsl or cg support??
-# add meshing support
-# add texture support (google GLCM for info)
*/
