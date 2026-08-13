#pragma once
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

// Umbrella header for ndl's Matrix<Real,N>, mirroring how image.h is an
// umbrella over image/ -- split by concern so each piece is easier to
// navigate on its own:
//   matrix/core.h           - the Matrix<Real,N> class itself: construction,
//                             element access, arithmetic, transpose. That's
//                             the whole class -- no determinant, inverse,
//                             eigendecomposition, or transform-matrix
//                             builders here; see below. Same split
//                             image/core.h has for Image<T,DIM> (no
//                             convolution or morphology there either).
//   matrix/decomposition.h  - determinant()/inverse()/invert(),
//                             eigen_decomposition() (Jacobi rotation, for
//                             symmetric matrices), and SVD<Real,N> -- free
//                             functions (and one class that genuinely needs
//                             to hold decomposition state) built on top of
//                             Matrix, the same "core class + toolkit" split
//                             convolution.h/morphology.h/etc. have relative
//                             to Image.
//   matrix/transform.h      - make_scale_matrix()/make_translate_matrix()/
//                             make_rotate_matrix()/make_shear_matrix()/
//                             make_projection_matrix()/
//                             make_ortho_projection_matrix()/
//                             transform_point() -- free functions building/
//                             applying homogeneous-coordinate transforms.
//
// #include this file for everything at once; #include one of the pieces
// above directly if you only need that part (e.g. optical_flow.h only ever
// needs matrix/core.h's Matrix itself, not the decomposition or transform
// toolkits).

#include "matrix/core.h"
#include "matrix/decomposition.h"
#include "matrix/transform.h"
