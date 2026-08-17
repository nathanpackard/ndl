// ndlviewer.js -- a standalone, dependency-free viewer for the binary
// volume format ndl/viewer.h's write_web_volume() produces.
//
// This is deliberately NOT part of ndl's C++ library (which stays
// header-only with no GUI-toolkit dependency of any kind -- in particular,
// no Qt) -- it's the other half of the split described in viewer.h's own
// top comment: ndl (C++) computes/exports N-D slice data; this file does
// all the actual interactive rendering. It has no build step and no
// dependency beyond a browser's own Canvas2D API, so it can be dropped into
// any static page (in particular, ndl's own Doxygen-generated tutorial
// pages -- see docs/generate_tutorial.py) with a single
// `<script src="ndlviewer.js"></script>`.
//
// Shows an N-dimensional volume as a scatterplot-matrix-style grid of
// pairwise-axis 2D slices: for axes 0..DIM-1, the panel at grid position
// (column i, row j) for i<j displays the (axis i, axis j) plane, with
// every other axis fixed at a single shared N-D cursor position -- exactly
// what ndl::pairwise_slice() computes on the C++ side, just re-sliced
// client-side from the whole downloaded volume so no server round-trip is
// needed as the cursor moves. Every panel in the same column shares axis i
// (its horizontal axis); every panel in the same row shares axis j (its
// vertical axis) -- lining shared axes up between views is exactly what
// that grid placement gives for free, the same way a scatterplot matrix
// aligns. Each panel's own pixel width/height is proportional to its two
// axes' size (not forced square), so a long axis next to a short one reads
// as a wide or tall rectangle rather than an isotropically squashed square
// -- proportional to voxel count by default, or to true physical extent
// for any axis with a unit attached (ndl::VoxelSpacing, see viewer.h's own
// comment, and computePerAxisPixelSizes()'s here) -- and the hover readout
// shows a physical coordinate line under the voxel-index one whenever the
// volume carries that calibration at all. Every panel also prints its own
// two axes' limits directly on itself -- axisI's (min,max) along its bottom
// edge, axisJ's along its left edge, each in that axis's own crosshair
// color, the min corner additionally bracketed with that axis's own index
// (e.g. "[0] 0") so the panel names which axis a color is, not just its
// range -- the same way a matplotlib subplot shows its own axis limits,
// rather than a separate legend a reader has to cross-reference against
// the panels. The one thing that has no single axis/panel to live on, the
// volume's own native value range, sits as a static line above the hover
// readout instead. A toolbar above the grid holds Level/Window sliders
// (the standard medical-imaging window/level convention -- Level is the
// center of the visible intensity range, Window its width; equivalent to
// min/max, just the more natural pair to drag when exploring a window) --
// one shared intensity mapping driving both the flat panels' grayscale and
// the volume panel's opacity -- plus "Reset slices" (cursor only), "Reset
// window/level" (the shared intensity mapping only), and a Fullscreen
// toggle. "Reset 3D view" (rotation only) lives with the volume panel's own
// controls instead of this toolbar, alongside the rotation sliders it
// resets -- each reset button sits next to the controls it actually
// affects, rather than all three being grouped in one place regardless of
// which view they belong to. Each of the three touches exactly its own one
// concern and leaves the other two (plus alpha, reset by none of them)
// alone -- resetting the cursor shouldn't silently undo a deliberately
// tuned window/level, any more than resetting window/level should
// recenter the cursor or touch the volume's rotation. Every slider shows
// its own live value, with a unit suffix where one applies (rotation in °,
// alpha in %).
//
// options.colorAxis (undefined by default, i.e. every panel grayscale,
// unchanged from before) names an axis holding RGB channels (must have
// extent exactly 3): every panel whose own two axes BOTH differ from
// colorAxis then renders true color -- reading colorAxis's 3 values at each
// spatial position as R/G/B (extractSliceRGB()) through the SAME shared
// window/level as every grayscale panel, rather than treating colorAxis as
// just another grayscale axis -- while a panel that DOES show colorAxis as
// one of its own two axes is unaffected, still ordinary grayscale (color
// compositing has nothing to composite when the channel axis itself is one
// of the two plotted dimensions). Deliberately explicit opt-in, not
// auto-detected from "some axis happens to have extent 3."
//
// When the volume is at least 3D and the browser has WebGL2, one
// additional panel -- never more than one, regardless of DIM, see
// VolumePanel()'s own comment -- renders a rotatable, ray-marched 3D view
// of a 3-axis sub-block (every other axis fixed at the cursor, same as any
// 2D panel), the N-D generalization of the classic 4th "3D render" corner
// in a clinical viewer. It sits IN the pairwise-slice grid itself, at the
// one cell (row 1, column 2) that's always empty in the ordinary panel
// layout, right after the (axis 0, axis 1) panel -- not as a separate
// element beside the grid -- with its own rotation/alpha/axis-picker
// controls to its right, inside that same cell. DIM=3 has only one
// possible 3-axis choice, shown automatically; DIM>3 adds a small picker
// for which 3 axes to render. The
// volume panel draws its own crosshair too, at wherever the shared cursor
// currently projects to under the active rotation, as three colored lines
// along the volume's own rotated axis directions (one per shown axis, each
// tinted the same palette color that axis uses everywhere else in the
// viewer) rather than a flat on-screen "+" -- so the crosshair itself shows
// the current 3D orientation via its own foreshortening. A small fixed
// orientation gizmo in the panel's own corner (drawAxisGizmo()) shows the
// same three rotated/colored directions labeled X/Y/Z regardless of where
// the cursor is, and each rotation slider's own label/color swatch names
// which real axis that role currently rotates (e.g. "X [2]") -- the same
// role letters, so the slider, the gizmo line, and the axis's color
// everywhere else are all naming the same thing three different ways.
// LEFT-click-drag (or the 3 rotation-plane sliders, XY/XZ/YZ) rotates;
// RIGHT-click(-drag) instead moves the shared cursor to the clicked position, same as a flat
// panel, except only the two IN-PLANE degrees of freedom move -- the
// cursor's own position along the current view axis (its "depth") is
// preserved, not reset to the volume's center, so navigating in the volume
// panel behaves like turning a page, not like resetting to a fixed slice
// each click. Both interactions keep the flat panels and the volume panel
// in sync in both directions.
//
// Click or drag inside any panel to move the cursor: the two axes that
// panel shows update to the clicked position, and every panel is
// redrawn -- panels sharing one of those two axes get a new slice (their
// fixed coordinate along that axis changed); every panel's crosshair
// overlay is redrawn to the new cursor position projected onto its own
// two axes. This generalizes the classic synchronized axial/coronal/
// sagittal medical-image viewer (click in one plane, the other two jump
// to that position) to arbitrary N. The scroll wheel over a flat panel
// steps through its own "depth" axis (the first axis the panel doesn't
// already show -- see scrollAxisFor()'s own comment) one voxel at a time,
// the familiar "scroll to change slice" convention; over the volume
// panel, it steps through whatever depth the CURRENT rotation happens to
// be looking down (scrollVolumeDepth()), so it still means "the slice
// you're on" even mid-rotation, not one fixed real axis.
//
// Usage:
//   fetch("volume.ndlv").then(r => r.arrayBuffer()).then(buf => {
//       NDLViewer.create(document.getElementById("container"), buf);
//   });
// or, for a self-contained page with no separate fetch (what
// generate_tutorial.py's embedding actually does): decode a base64 string
// into a Uint8Array's .buffer and pass that directly.
(function (global) {
	'use strict';

	// ---- Binary format parsing (see ndl/viewer.h's write_web_volume() for
	// the authoritative byte-layout comment; this is its JS-side mirror) ----

	var DTYPE_CTORS = [
		Uint8Array, Int8Array, Uint16Array, Int16Array,
		Uint32Array, Int32Array, Float32Array, Float64Array
	];

	function parseVolume(arrayBuffer) {
		var view = new DataView(arrayBuffer);
		var magic = String.fromCharCode(view.getUint8(0), view.getUint8(1), view.getUint8(2), view.getUint8(3));
		if (magic !== 'NDLV') throw new Error('NDLViewer: not an NDLV volume (bad magic bytes)');
		var version = view.getUint8(4);
		if (version !== 1 && version !== 2) throw new Error('NDLViewer: unsupported NDLV format version ' + version);
		var dtypeCode = view.getUint8(5);
		if (dtypeCode < 0 || dtypeCode >= DTYPE_CTORS.length) throw new Error('NDLViewer: unrecognized dtype code ' + dtypeCode);
		var dim = view.getUint8(6);
		if (dim < 2) throw new Error('NDLViewer: volume has DIM=' + dim + ', need at least 2 to show any pairwise view');

		var extent = new Array(dim);
		var offset = 7;
		var elementCount = 1;
		for (var i = 0; i < dim; i++) {
			extent[i] = view.getUint32(offset, /*littleEndian=*/true);
			elementCount *= extent[i];
			offset += 4;
		}

		// version 2 only: per-axis physical calibration (ndl::VoxelSpacing,
		// see viewer.h's own comment) -- spacing[k]=null/unit[k]=''  means
		// axis k has no physical calibration; a volume with NO VoxelSpacing
		// at all (version 1) leaves both arrays null outright, which is
		// what every physical-coordinate/physical-sizing code path below
		// checks before doing anything unit-aware.
		var spacing = null, unit = null;
		if (version === 2) {
			spacing = new Array(dim);
			for (var s = 0; s < dim; s++) { spacing[s] = view.getFloat64(offset, true); offset += 8; }
			unit = new Array(dim);
			var utf8Decoder = new TextDecoder('utf-8');
			for (var u = 0; u < dim; u++) {
				var len = view.getUint8(offset); offset += 1;
				unit[u] = len > 0 ? utf8Decoder.decode(new Uint8Array(arrayBuffer, offset, len)) : '';
				offset += len;
			}
		}

		var Ctor = DTYPE_CTORS[dtypeCode];
		// Typed arrays require their backing buffer's byte offset to be a
		// multiple of the element size for anything wider than one byte;
		// the header isn't guaranteed aligned to e.g. 4 or 8 bytes for an
		// arbitrary dim (and version, now that version 2's own header is a
		// variable length depending on each unit string), so the data is
		// copied into a freshly-allocated (and therefore aligned) buffer
		// rather than constructing the typed array directly over
		// arrayBuffer at `offset`.
		var byteLength = elementCount * Ctor.BYTES_PER_ELEMENT;
		var dataBytes = new Uint8Array(byteLength);
		dataBytes.set(new Uint8Array(arrayBuffer, offset, byteLength));
		var data = new Ctor(dataBytes.buffer);

		return { dim: dim, extent: extent, data: data, spacing: spacing, unit: unit };
	}

	// Global (whole-volume) min/max, used for a simple auto window/level --
	// every panel shares the same normalization, so brightness is
	// comparable across panels (a per-slice auto-window would make flat
	// slices look artificially high-contrast). Adjustable per-user window/
	// level controls are a natural follow-up, not implemented here.
	function computeMinMax(data) {
		var min = data[0], max = data[0];
		for (var i = 1; i < data.length; i++) {
			var v = data[i];
			if (v < min) min = v;
			if (v > max) max = v;
		}
		return { min: min, max: max };
	}

	// True for the two floating-point dtypes (Float32Array/Float64Array);
	// false for every integer dtype write_web_volume() supports. Integer
	// values are always printed verbatim (they're already exact -- no
	// rounding decision to make), so this only gates whether the readout's
	// value line needs any decimal-place logic at all.
	function isFloatingDtype(data) {
		return data instanceof Float32Array || data instanceof Float64Array;
	}

	// How many decimal places to print a floating-point voxel value with,
	// derived from the volume's own dynamic range (max-min) rather than a
	// fixed constant -- otherwise a native double prints with its full
	// ~17-digit round-trip precision (e.g. "0.19999999999999996" for what
	// is, physically, just 0.2), which is noise: those trailing digits are
	// floating-point representation error, not real precision the source
	// data actually has. Picking ~4 significant figures OF THE RANGE (not
	// of each individual value) means a small-range volume (density in
	// [0,1]) shows meaningful sub-integer precision ("0.200") while a
	// large-range one (density in [0,300]) doesn't drown in digits that
	// don't matter at that scale ("220.3", not "220.30000000000001") --
	// and, since it's computed once from the whole volume rather than
	// per-value, every voxel in a given volume prints with the SAME
	// decimal-place count, so hovering around stays visually comparable
	// instead of the digit count jumping per voxel.
	function valueDecimalPlaces(range) {
		if (!(range > 0)) return 3;
		return Math.max(0, Math.min(10, 3 - Math.floor(Math.log10(range))));
	}

	function formatValue(v, decimals, isFloat) {
		if (!isFloat || !isFinite(v)) return String(v);
		var s = v.toFixed(decimals);
		// A tiny negative float (e.g. -1e-16, well within normal floating-
		// point noise) that's genuinely meant to be zero rounds to "-0.000"
		// here -- correct arithmetically, but reads as a real negative
		// value to anyone glancing at it. Strip the sign only when the
		// ROUNDED result is exactly zero, so an actually-negative value
		// that merely rounds close to zero (e.g. -0.001) keeps its sign.
		if (parseFloat(s) === 0) s = s.replace('-', '');
		return s;
	}

	// ---- 3D volume rendering: a single WebGL2, ray-marched, rotatable view
	// of a 3-axis sub-block (every other axis fixed at the shared cursor,
	// same as a 2D panel fixes every axis but its own two) -- the N-D
	// generalization of the classic 4th "3D render" quadrant in a clinical
	// axial/coronal/sagittal viewer. Genuinely optional: a browser without
	// WebGL2 just doesn't get this one panel, everything else (the
	// pairwise-slice grid) works exactly the same either way. Unlike the
	// flat 2D panels (moved OFF WebGL earlier specifically because one
	// context per panel blows past a browser's context ceiling at
	// C(DIM,2) panels), there is only ever ONE of these per viewer instance
	// regardless of DIM -- see create()'s own comment on why one, not
	// C(DIM,3) -- so the context-ceiling problem that ruled out WebGL for
	// the flat grid never applies here. ----

	// vScreen flips Y relative to aPos (gl_Position keeps the raw,
	// unflipped aPos -- this changes only which texture value each screen
	// position samples, not the quad's own geometry): without the flip,
	// screen-top would sample the HIGH end of the in-plane vertical axis,
	// backwards from every 2D panel's own convention (drawCrosshair() maps
	// axisJ's low end to the panel's top edge). create()'s own
	// navigateFromEvent()/VolumePanel.drawCrosshair() both work in this
	// same flipped vScreen space, so screen-top consistently means "low
	// index" everywhere in the viewer, not just in the 2D panels.
	var VOLUME_VERTEX_SRC =
		'#version 300 es\n' +
		'in vec2 aPos;\n' +
		'out vec2 vScreen;\n' +
		'void main() {\n' +
		'  vScreen = vec2(aPos.x, -aPos.y);\n' +
		'  gl_Position = vec4(aPos, 0.0, 1.0);\n' +
		'}\n';

	// Orthographic ray-march: every ray shares the same (rotated) direction,
	// only the start point varies per-pixel -- true of any orthographic
	// camera, and simpler than perspective's per-pixel ray direction for a
	// first version (see the "orthographic vs. perspective" discussion this
	// feature came out of: orthographic keeps proportions honest the same
	// way the flat panels' own physical-extent sizing already does).
	// uVolume is pre-windowed to [0,1] on the CPU side (extractVolumeBlock())
	// using the SAME [windowMin,windowMax] the flat panels use for their own
	// grayscale mapping -- so one shared control drives both grayscale
	// brightness in the 2D panels and opacity here. uAlphaScale is a SEPARATE,
	// purely-opacity control (the "how see-through is the whole volume"
	// slider) -- turning it down makes the volume more transparent without
	// darkening what IS visible, unlike windowing (which changes what counts
	// as bright/dark data in the first place).
	//
	// Per-sample alpha is a Beer-Lambert absorption term
	// (1-exp(-density*sigma*tStep)), NOT a raw per-sample multiply
	// (density*uAlphaScale) -- an earlier version used the raw multiply,
	// and it made uAlphaScale nearly useless: with ~220 samples along a ray
	// (several samples per voxel), accum.a = 1-(1-alpha)^n saturates to
	// ~1 within just a few samples of entering ANY bright region for
	// almost any alpha above a tiny value, since a raw per-sample multiply
	// doesn't account for how many samples make up a given physical
	// thickness -- turning uAlphaScale down to ~100% vs ~1% was the only
	// perceptible range, everything between looked the same (solid).
	// Beer-Lambert's tStep factor makes the accumulated opacity along a
	// ray of a given PHYSICAL length independent of how finely it's
	// sampled (the correct, standard volume-rendering behavior), which is
	// exactly what makes uAlphaScale behave smoothly across its whole
	// 0..1 range instead of being a near-binary switch. ALPHA_DENSITY is a
	// tuned constant, not derived from anything: it just sets how "dense"
	// uAlphaScale=1.0 (100%) reads as for typical data (large enough that
	// a solid, moderately-thick object still looks fully opaque at 100%,
	// small enough that intermediate slider values are visibly different
	// from both 0% and 100%).
	var VOLUME_FRAGMENT_SRC_GRAYSCALE =
		'#version 300 es\n' +
		'precision highp float;\n' +
		'precision highp sampler3D;\n' +
		'uniform sampler3D uVolume;\n' +
		'uniform mat3 uRotation;\n' +
		'uniform float uAlphaScale;\n' +
		'in vec2 vScreen;\n' +
		'out vec4 fragColor;\n' +
		'const int STEPS = 220;\n' +
		'const float ALPHA_DENSITY = 12.0;\n' +
		'void main() {\n' +
		'  vec3 rayDir = normalize(uRotation * vec3(0.0, 0.0, 1.0));\n' +
		'  vec3 rayOrigin = uRotation * vec3(vScreen, 0.0);\n' +
		'  float tStep = 2.0 * sqrt(3.0) / float(STEPS);\n' +
		'  vec3 pos = rayOrigin - rayDir * sqrt(3.0);\n' +
		'  vec4 accum = vec4(0.0);\n' +
		'  for (int i = 0; i < STEPS; i++) {\n' +
		'    vec3 texCoord = pos * 0.5 + 0.5;\n' +
		'    if (all(greaterThanEqual(texCoord, vec3(0.0))) && all(lessThanEqual(texCoord, vec3(1.0)))) {\n' +
		'      float v = texture(uVolume, texCoord).r;\n' +
		'      float alpha = 1.0 - exp(-v * uAlphaScale * ALPHA_DENSITY * tStep);\n' +
		'      accum.rgb += (1.0 - accum.a) * alpha * vec3(v);\n' +
		'      accum.a += (1.0 - accum.a) * alpha;\n' +
		'      if (accum.a > 0.98) break;\n' +
		'    }\n' +
		'    pos += rayDir * tStep;\n' +
		'  }\n' +
		'  fragColor = vec4(accum.rgb, 1.0);\n' +
		'}\n';

	// Color counterpart to VOLUME_FRAGMENT_SRC_GRAYSCALE above -- used
	// instead whenever options.colorAxis is set (see VolumePanel()'s own
	// isColor parameter), for a texture uploaded by
	// extractVolumeBlockRGB()/uploadBlock()'s RGB8 path rather than the
	// grayscale R8 one. Identical ray-marching and Beer-Lambert absorption
	// (see the grayscale version's own comment above for the full
	// rationale) -- the only two differences are sampling `.rgb` instead
	// of `.r`, and using that RGB triple's own LUMINANCE (standard
	// perceptual weights) as the density driving alpha, rather than the
	// single windowed value standing in for both color AND density at
	// once. accum.rgb is composited with the sampled color itself, not
	// vec3(v) -- a bright red voxel stays visibly red, not just "bright."
	var VOLUME_FRAGMENT_SRC_COLOR =
		'#version 300 es\n' +
		'precision highp float;\n' +
		'precision highp sampler3D;\n' +
		'uniform sampler3D uVolume;\n' +
		'uniform mat3 uRotation;\n' +
		'uniform float uAlphaScale;\n' +
		'in vec2 vScreen;\n' +
		'out vec4 fragColor;\n' +
		'const int STEPS = 220;\n' +
		'const float ALPHA_DENSITY = 12.0;\n' +
		'void main() {\n' +
		'  vec3 rayDir = normalize(uRotation * vec3(0.0, 0.0, 1.0));\n' +
		'  vec3 rayOrigin = uRotation * vec3(vScreen, 0.0);\n' +
		'  float tStep = 2.0 * sqrt(3.0) / float(STEPS);\n' +
		'  vec3 pos = rayOrigin - rayDir * sqrt(3.0);\n' +
		'  vec4 accum = vec4(0.0);\n' +
		'  for (int i = 0; i < STEPS; i++) {\n' +
		'    vec3 texCoord = pos * 0.5 + 0.5;\n' +
		'    if (all(greaterThanEqual(texCoord, vec3(0.0))) && all(lessThanEqual(texCoord, vec3(1.0)))) {\n' +
		'      vec3 rgb = texture(uVolume, texCoord).rgb;\n' +
		'      float v = dot(rgb, vec3(0.299, 0.587, 0.114));\n' +
		'      float alpha = 1.0 - exp(-v * uAlphaScale * ALPHA_DENSITY * tStep);\n' +
		'      accum.rgb += (1.0 - accum.a) * alpha * rgb;\n' +
		'      accum.a += (1.0 - accum.a) * alpha;\n' +
		'      if (accum.a > 0.98) break;\n' +
		'    }\n' +
		'    pos += rayDir * tStep;\n' +
		'  }\n' +
		'  fragColor = vec4(accum.rgb, 1.0);\n' +
		'}\n';

	function compileVolumeShader(gl, type, src) {
		var shader = gl.createShader(type);
		gl.shaderSource(shader, src);
		gl.compileShader(shader);
		if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS))
			throw new Error('NDLViewer: volume shader compile failed: ' + gl.getShaderInfoLog(shader));
		return shader;
	}

	// Column-major 3x3 matrices throughout (matching gl.uniformMatrix3fv's
	// own expected layout with transpose=false, so the JS side never needs
	// to transpose before uploading).
	function mat3RotationX(a) { var c = Math.cos(a), s = Math.sin(a); return [1, 0, 0, 0, c, s, 0, -s, c]; }
	function mat3RotationY(a) { var c = Math.cos(a), s = Math.sin(a); return [c, 0, -s, 0, 1, 0, s, 0, c]; }
	function mat3RotationZ(a) { var c = Math.cos(a), s = Math.sin(a); return [c, s, 0, -s, c, 0, 0, 0, 1]; }
	function mat3Multiply(a, b) {
		var r = new Array(9);
		for (var col = 0; col < 3; col++)
			for (var row = 0; row < 3; row++) {
				var sum = 0;
				for (var k = 0; k < 3; k++) sum += a[k * 3 + row] * b[col * 3 + k];
				r[col * 3 + row] = sum;
			}
		return r;
	}
	// m*v, for the same column-major m every other mat3 helper here uses.
	function mat3MultiplyVec3(m, v) {
		return [
			m[0] * v[0] + m[3] * v[1] + m[6] * v[2],
			m[1] * v[0] + m[4] * v[1] + m[7] * v[2],
			m[2] * v[0] + m[5] * v[1] + m[8] * v[2]
		];
	}
	// m^T*v -- since a rotation matrix's inverse is its transpose, this is
	// how create()'s click-to-navigate math converts a SCREEN-space point
	// back into the volume's own LOCAL (unrotated) space: m*v rotates
	// local->view (as VOLUME_FRAGMENT_SRC's own rayOrigin/rayDir do), so
	// m^T*v is exactly the inverse, view->local.
	function mat3TransposeMultiplyVec3(m, v) {
		return [
			m[0] * v[0] + m[1] * v[1] + m[2] * v[2],
			m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
			m[6] * v[0] + m[7] * v[1] + m[8] * v[2]
		];
	}

	// Extracts the (extent[ax0] x extent[ax1] x extent[ax2]) sub-block for
	// the volume-render panel's own 3 chosen axes, every other axis fixed
	// at cursor -- the direct 3-axis generalization of extractSliceU8()'s
	// 2-axis extraction, windowed to a single [0,1] byte the same way (so
	// the shader can read it as both grayscale color and opacity, per
	// VOLUME_FRAGMENT_SRC's own comment). Layout is x-fastest, then y, then
	// z, matching gl.texImage3D()'s own expected row-major-with-z-outermost
	// layout for a WIDTHxHEIGHTxDEPTH upload.
	function extractVolumeBlock(volume, strides, ax0, ax1, ax2, cursor, min, max) {
		var extent = volume.extent, data = volume.data;
		var w = extent[ax0], h = extent[ax1], d = extent[ax2];
		var out = new Uint8Array(w * h * d);
		var range = max - min;
		var base = 0;
		for (var k = 0; k < extent.length; k++)
			if (k !== ax0 && k !== ax1 && k !== ax2) base += cursor[k] * strides[k];

		var s0 = strides[ax0], s1 = strides[ax1], s2 = strides[ax2];
		var p = 0;
		for (var z = 0; z < d; z++) {
			var zBase = base + z * s2;
			for (var y = 0; y < h; y++) {
				var yBase = zBase + y * s1;
				for (var x = 0; x < w; x++) {
					var v = data[yBase + x * s0];
					out[p++] = range > 0 ? Math.max(0, Math.min(255, Math.round(((v - min) / range) * 255))) : 0;
				}
			}
		}
		return { data: out, w: w, h: h, d: d };
	}

	// Color counterpart to extractVolumeBlock() above, for a volume-render
	// panel whose 3 chosen axes (ax0/ax1/ax2) all exclude options.colorAxis
	// (guaranteed by construction -- see create()'s own volAxes/axis-picker
	// comments, colorAxis is never offered as one of the 3 volume-view
	// axes) -- reads colorAxis's 3 values at each voxel position (every
	// other axis, INCLUDING colorAxis's own siblings, held at cursor, same
	// base-offset trick as extractVolumeBlock()) instead of 1, windowed the
	// same way, packed as interleaved R,G,B (3 bytes/voxel) for
	// VolumePanel.uploadBlock()'s own RGB8 texture path.
	function extractVolumeBlockRGB(volume, strides, ax0, ax1, ax2, colorAxis, cursor, min, max) {
		var extent = volume.extent, data = volume.data;
		var w = extent[ax0], h = extent[ax1], d = extent[ax2];
		var out = new Uint8Array(w * h * d * 3);
		var range = max - min;
		var base = 0;
		for (var k = 0; k < extent.length; k++)
			if (k !== ax0 && k !== ax1 && k !== ax2 && k !== colorAxis) base += cursor[k] * strides[k];

		var s0 = strides[ax0], s1 = strides[ax1], s2 = strides[ax2], sC = strides[colorAxis];
		var p = 0;
		for (var z = 0; z < d; z++) {
			var zBase = base + z * s2;
			for (var y = 0; y < h; y++) {
				var yBase = zBase + y * s1;
				for (var x = 0; x < w; x++) {
					var voxelBase = yBase + x * s0;
					for (var ch = 0; ch < 3; ch++) {
						var v = data[voxelBase + ch * sC];
						out[p++] = range > 0 ? Math.max(0, Math.min(255, Math.round(((v - min) / range) * 255))) : 0;
					}
				}
			}
		}
		return { data: out, w: w, h: h, d: d };
	}

	// The one 3D volume-render panel: a size x size WebGL2 canvas, ray-
	// marching whichever 3-axis sub-block it's currently showing, with a
	// transparent Canvas2D overlay stacked on top (crosshair only -- unlike
	// a flat Panel's overlay, this one is purely visual, pointer-events:
	// none, since click/drag handling lives on the WebGL canvas itself so
	// mouse coordinates don't need translating between two canvases).
	// Returns null (no panel) if WebGL2 isn't available -- see this
	// section's own top comment on why that's an acceptable degradation
	// and not a hard dependency for the rest of the viewer. isColor
	// (create() passes colorAxis>=0) picks the fragment shader
	// (VOLUME_FRAGMENT_SRC_COLOR vs. _GRAYSCALE) and, correspondingly,
	// which texture format uploadBlock() uses (RGB8 vs. R8) -- a
	// one-time, whole-viewer-instance decision, not something that
	// changes per-frame: colorAxis is never one of this panel's own 3
	// shown axes (see create()'s own volAxes/axis-picker comments), so
	// "is this panel in color mode" never needs to change after
	// construction the way which 3 axes it shows can.
	// 50%, not 100%: a fully-opaque default tends to hide interior
	// structure behind whatever's on the outside of the volume, which is
	// rarely what a reader wants to see FIRST -- 50% is a more generally
	// useful starting point to see some depth right away, still leaving
	// the slider room to go either more transparent or more opaque.
	// Shared by both VolumePanel's own constructor default (below, what
	// the volume actually renders with before any slider is touched) and
	// create()'s own Alpha slider/reset-alpha button (so the slider's
	// displayed initial value, the actual initial render, and what "reset"
	// snaps back to can never drift out of sync with each other).
	var DEFAULT_ALPHA_SCALE = 0.5;
	function VolumePanel(size, palette, isColor) {
		var wrap = document.createElement('div');
		wrap.style.position = 'relative';
		wrap.style.width = size + 'px';
		wrap.style.height = size + 'px';

		var canvas = document.createElement('canvas');
		canvas.width = size;
		canvas.height = size;
		canvas.style.position = 'absolute';
		canvas.style.left = '0';
		canvas.style.top = '0';
		canvas.style.cursor = 'grab';
		var gl = canvas.getContext('webgl2');
		if (!gl) return null;
		wrap.appendChild(canvas);

		var overlay = document.createElement('canvas');
		overlay.width = size;
		overlay.height = size;
		overlay.style.position = 'absolute';
		overlay.style.left = '0';
		overlay.style.top = '0';
		overlay.style.pointerEvents = 'none';
		wrap.appendChild(overlay);

		this.wrap = wrap;
		this.canvas = canvas;
		this.overlay = overlay;
		this.gl = gl;
		this.size = size;
		this.isColor = !!isColor;
		this.program = gl.createProgram();
		gl.attachShader(this.program, compileVolumeShader(gl, gl.VERTEX_SHADER, VOLUME_VERTEX_SRC));
		gl.attachShader(this.program, compileVolumeShader(gl, gl.FRAGMENT_SHADER, this.isColor ? VOLUME_FRAGMENT_SRC_COLOR : VOLUME_FRAGMENT_SRC_GRAYSCALE));
		gl.linkProgram(this.program);
		if (!gl.getProgramParameter(this.program, gl.LINK_STATUS))
			throw new Error('NDLViewer: volume program link failed: ' + gl.getProgramInfoLog(this.program));

		var quad = new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]);
		this.quadBuffer = gl.createBuffer();
		gl.bindBuffer(gl.ARRAY_BUFFER, this.quadBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, quad, gl.STATIC_DRAW);
		this.aPos = gl.getAttribLocation(this.program, 'aPos');
		this.uVolume = gl.getUniformLocation(this.program, 'uVolume');
		this.uRotation = gl.getUniformLocation(this.program, 'uRotation');
		this.uAlphaScale = gl.getUniformLocation(this.program, 'uAlphaScale');

		this.texture = gl.createTexture();
		gl.bindTexture(gl.TEXTURE_3D, this.texture);
		gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
		gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
		gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_R, gl.CLAMP_TO_EDGE);
		// LINEAR (trilinear across the 3D texture), unlike the flat panels'
		// deliberate NEAREST -- a rotated ray-marched volume shows sampling
		// artifacts far more readily than an axis-aligned 2D slice does, so
		// smoothing here is the right default rather than the flat panels'
		// own "show exact voxels" choice.
		gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
		gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);

		this.angleX = 0;
		this.angleY = 0;
		this.angleZ = 0;
		this.alphaScale = DEFAULT_ALPHA_SCALE;
	}

	// The current XYZ-Euler rotation as a single 3x3 matrix -- shared by
	// render() (applied to every ray) and create()'s own click-to-navigate
	// math (which needs the SAME matrix, and its transpose/inverse, to map
	// a screen click back into the volume's own local space). Computing it
	// fresh each call (rather than caching) is deliberate: it's three 3x3
	// multiplies, cheap enough that caching would only risk the two call
	// sites disagreeing after an angle changes and the cache doesn't.
	VolumePanel.prototype.rotationMatrix = function () {
		return mat3Multiply(mat3RotationZ(this.angleZ), mat3Multiply(mat3RotationY(this.angleY), mat3RotationX(this.angleX)));
	};

	// Re-extracts and re-uploads the 3D texture -- the expensive half of an
	// update, needed whenever the actual DATA being shown changes (cursor
	// moved on a fixed axis, window min/max changed, or which 3 axes are
	// selected changed). Deliberately separate from render() (the cheap,
	// GPU-only half): a pure rotation change never needs this.
	VolumePanel.prototype.uploadBlock = function (block) {
		var gl = this.gl;
		gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1); // single/triple-byte texels at arbitrary width aren't 4-byte-row-aligned in general, grayscale (R8) or color (RGB8) alike
		gl.bindTexture(gl.TEXTURE_3D, this.texture);
		if (this.isColor) gl.texImage3D(gl.TEXTURE_3D, 0, gl.RGB8, block.w, block.h, block.d, 0, gl.RGB, gl.UNSIGNED_BYTE, block.data);
		else gl.texImage3D(gl.TEXTURE_3D, 0, gl.R8, block.w, block.h, block.d, 0, gl.RED, gl.UNSIGNED_BYTE, block.data);
	};

	// The cheap half: draws the current texture with the current rotation
	// and alpha scale. Safe to call on every slider tick / mousemove-while-
	// dragging with no CPU-side re-extraction.
	VolumePanel.prototype.render = function () {
		var gl = this.gl;
		var rotation = this.rotationMatrix();

		gl.viewport(0, 0, this.canvas.width, this.canvas.height);
		gl.useProgram(this.program);
		gl.bindBuffer(gl.ARRAY_BUFFER, this.quadBuffer);
		gl.enableVertexAttribArray(this.aPos);
		gl.vertexAttribPointer(this.aPos, 2, gl.FLOAT, false, 0, 0);
		gl.activeTexture(gl.TEXTURE0);
		gl.bindTexture(gl.TEXTURE_3D, this.texture);
		gl.uniform1i(this.uVolume, 0);
		gl.uniformMatrix3fv(this.uRotation, false, rotation);
		gl.uniform1f(this.uAlphaScale, this.alphaScale);
		gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
	};

	// Draws the crosshair as three lines along the volume's own ROTATED axis
	// directions, one per shown axis, rather than a flat on-screen "+" --
	// localCursorPos is the cursor's own 3 volAxes coordinates, each already
	// normalized to [-1,1] (create() computes this; VolumePanel itself has
	// no notion of "which axes" or "extent", same separation of concerns as
	// everywhere else in this file); axisColors is the matching 3 palette
	// colors, in the same order, so a given axis's line is the same color
	// here as its crosshair line/frame-ring color everywhere else in the
	// viewer. For axis k, the local-space segment runs from the cursor's own
	// position all the way out to that axis's [-1,1] extremes (edgeMin,
	// edgeMax) -- i.e. the same "spans the whole panel" convention the flat
	// panels use -- each endpoint mapped into view space via the rotation
	// (mat3TransposeMultiplyVec3, local->view, see its own comment) and then
	// orthographically projected (only x,y kept). Because that whole map
	// (rotate + drop z) is linear, a straight local-space line stays
	// straight on screen, so the two endpoints alone determine it and the
	// cursor's own screen position is just the same interpolation parameter
	// tCursor = (localCursorPos[k]+1)/2 along that segment -- letting the
	// gap be computed and the line split without a third projection. Screen
	// length varies with the axis's current projected foreshortening
	// (deliberately NOT normalized to a fixed length): a line that projects
	// nearly edge-on to the current rotation goes short, one nearly
	// face-on goes long, so the crosshair's own shape communicates the
	// current 3D orientation. Two-pass (black-then-axis-color) stroke: the
	// volume itself can be anywhere from black to white depending on what's
	// rendered, so a black-outlined colored line stays visible against
	// either. Same gap-not-dot convention as Panel.prototype.drawCrosshair:
	// the point is to locate the exact voxel, so the lines stop short of it
	// rather than covering it. Also draws a small fixed-position orientation
	// gizmo in the corner (drawAxisGizmo(), below) on the same overlay --
	// this function already recomputes the rotation matrix and clears/
	// redraws the overlay every call, so piggybacking the gizmo here avoids
	// a second redundant clear+redraw pass.
	var VOLUME_CROSSHAIR_GAP = 6;
	VolumePanel.prototype.drawCrosshair = function (localCursorPos, axisColors, axisIndices) {
		var ctx = this.overlay.getContext('2d');
		var w = this.size, h = this.size;
		ctx.clearRect(0, 0, w, h);
		var R = this.rotationMatrix();
		var gap = VOLUME_CROSSHAIR_GAP;

		// view is in the SAME (flipped) vScreen space VOLUME_VERTEX_SRC's
		// own comment describes, so view[1]*0.5+0.5 already increases
		// top-to-bottom with no extra flip needed here.
		function toScreen(local) {
			var view = mat3TransposeMultiplyVec3(R, local);
			return [(view[0] * 0.5 + 0.5) * w, (view[1] * 0.5 + 0.5) * h];
		}

		for (var k = 0; k < 3; k++) {
			var edgeMin = localCursorPos.slice(); edgeMin[k] = -1;
			var edgeMax = localCursorPos.slice(); edgeMax[k] = 1;
			var p0 = toScreen(edgeMin), p1 = toScreen(edgeMax);
			var dx = p1[0] - p0[0], dy = p1[1] - p0[1];
			var len = Math.sqrt(dx * dx + dy * dy);
			var tCursor = (localCursorPos[k] + 1) / 2;
			var tGap = len > 0 ? gap / len : 0;
			var t0 = Math.max(0, tCursor - tGap), t1 = Math.min(1, tCursor + tGap);
			[['#000000', 3], [axisColors[k], 1]].forEach(function (pass) {
				ctx.strokeStyle = pass[0];
				ctx.lineWidth = pass[1];
				if (t0 > 0) {
					ctx.beginPath();
					ctx.moveTo(p0[0], p0[1]);
					ctx.lineTo(p0[0] + dx * t0, p0[1] + dy * t0);
					ctx.stroke();
				}
				if (t1 < 1) {
					ctx.beginPath();
					ctx.moveTo(p0[0] + dx * t1, p0[1] + dy * t1);
					ctx.lineTo(p1[0], p1[1]);
					ctx.stroke();
				}
			});
		}

		drawAxisGizmo(ctx, R, axisColors, axisIndices);
	};

	// A small fixed-position orientation gizmo in the volume panel's
	// bottom-left corner: one short line per shown axis, in that axis's own
	// color, labeled with that axis's own real index (the same "[N]"
	// identity every panel corner label, frame-ring arc, and rotation
	// slider swatch already uses -- see makeRotationSlider()'s own comment
	// -- rather than an arbitrary X/Y/Z role letter with no meaning outside
	// this one widget). Unlike the crosshair lines above (which span the
	// whole panel, anchored to the cursor), this is anchored to a FIXED
	// screen point with a small fixed radius -- its job is purely "which
	// way does each shown axis currently point," answered the same way a 3D
	// modeling tool's corner axis widget does, not "where is the cursor".
	// Each line's on-screen length still varies with the current rotation
	// (an axis pointing toward/away from the viewer draws short), the same
	// foreshortening-shows-orientation idea as the crosshair.
	//
	// Every constant below is a FRACTION of the canvas's own current size,
	// not a fixed pixel count: this canvas isn't always drawn at the same
	// NATIVE resolution its own CSS display size implies -- a live
	// "volume"-op page can request a smaller/larger render (e.g.
	// apps/bouncing_donut.html's own Size slider, see Component 8's own
	// on-demand-resolution design) while its CSS box stays fixed, so a
	// FIXED-pixel gizmo would end up a wildly different ON-SCREEN size
	// depending on whatever resolution happened to be requested (tiny at a
	// high native resolution, comically oversized once upscaled from a low
	// one) -- a real bug this project's own bouncing_donut.html hit as
	// soon as its Size slider existed. Sizing every constant off the
	// canvas's own CURRENT pixel dimensions instead keeps the gizmo's own
	// ON-SCREEN proportions constant regardless of what native resolution
	// it's actually drawn at. The fractions themselves (0.0875/0.0625/...)
	// are simply the OLD fixed pixel values (28/20/...) divided by 320,
	// the panel size this gizmo was originally tuned to look right at --
	// so a 320x320 canvas renders pixel-identical to before this fix.
	function drawAxisGizmo(ctx, R, axisColors, axisIndices) {
		var size = Math.min(ctx.canvas.width, ctx.canvas.height);
		var margin = size * 0.0875, radius = size * 0.0625, labelGap = size * 0.03125;
		var fontPx = Math.max(8, Math.round(size * 0.034375));
		var haloWidth = Math.max(1.5, size * 0.009375), lineWidth = Math.max(0.5, size * 0.003125);
		var anchorX = margin, anchorY = ctx.canvas.height - margin;
		ctx.font = 'bold ' + fontPx + 'px monospace';
		ctx.textAlign = 'center';
		ctx.textBaseline = 'middle';
		for (var k = 0; k < 3; k++) {
			var local = [0, 0, 0]; local[k] = 1;
			var view = mat3TransposeMultiplyVec3(R, local);
			var tipX = anchorX + view[0] * radius, tipY = anchorY + view[1] * radius;
			[['#000000', haloWidth], [axisColors[k], lineWidth]].forEach(function (pass) {
				ctx.strokeStyle = pass[0];
				ctx.lineWidth = pass[1];
				ctx.beginPath(); ctx.moveTo(anchorX, anchorY); ctx.lineTo(tipX, tipY); ctx.stroke();
			});
			var labelX = anchorX + view[0] * (radius + labelGap), labelY = anchorY + view[1] * (radius + labelGap);
			var labelText = String(axisIndices[k]);
			ctx.lineWidth = haloWidth;
			ctx.strokeStyle = '#000000';
			ctx.strokeText(labelText, labelX, labelY);
			ctx.fillStyle = axisColors[k];
			ctx.fillText(labelText, labelX, labelY);
		}
	}

	// Rounds a physical coordinate (voxel-index * spacing) to 3 decimal
	// places for display -- that multiplication routinely lands on
	// something like 12.800000000000001 for perfectly ordinary inputs,
	// which is float representation noise, not real precision.
	function formatPhysical(v) { return String(Math.round(v * 1000) / 1000); }

	// The (min, max) label text for one axis's own limits -- physical
	// (spacing-scaled, unit-suffixed) if this axis has a unit, otherwise
	// the raw voxel-index range [0, extent-1]. Precomputed once per axis
	// (not per panel, since a given axis's limits are the same everywhere
	// it appears) and drawn directly on every panel that shows this axis,
	// at its own edge -- see drawAxisLimitLabels()'s own comment for why
	// on-panel rather than a separate legend. Deliberately no unit-word
	// fallback here the way outerAxisLabelText() has ("voxels") -- units
	// belong next to the axis's own IDENTITY label (the outer-edge "[N]
	// unit" ones), not smeared into these numeric range endpoints too.
	function axisLimitLabel(extent, spacing, unit) {
		if (unit) return { min: '0' + unit, max: formatPhysical((extent - 1) * spacing) + unit };
		return { min: '0', max: String(extent - 1) };
	}

	// Draws this panel's own two axis-limit labels directly on its frame
	// canvas (the same static, drawn-once layer the excluded-axis ring
	// uses) -- axisI's (min,max) along the bottom edge in axisI's own
	// color, axisJ's along the left edge in axisJ's own color, the same
	// colors their crosshair lines use. This is deliberately drawn ON the
	// panel rather than in a separate legend block above the grid: a
	// legend requires memorizing "color X = axis Y" and cross-referencing
	// it against each panel, where labeling the panel's own axes directly
	// -- the way matplotlib prints axis limits right on each subplot's own
	// axes -- means the scale of what you're looking at is never more than
	// one glance away from the image itself. The min-corner label also
	// carries the axis's own INDEX in brackets (e.g. "[0] 0") rather than
	// just its value -- color alone ties a panel edge to the same axis
	// everywhere else it appears (other panels' edges, the volume
	// crosshair, the rotation sliders' own swatches -- see
	// makeRotationSlider()'s comment), but doesn't say WHICH axis number
	// that color is, which is exactly the gap this closes. Only on the
	// min corner, not both, since one axis identity per edge is enough and
	// the max corner already has no spare room next to axisJ's own second
	// line (see the h-14 baseline below).
	function drawAxisLimitLabels(panel, labelI, labelJ) {
		var ctx = panel.frame.getContext('2d');
		var w = panel.width, h = panel.height;
		ctx.font = '10px monospace';
		ctx.textBaseline = 'alphabetic';

		ctx.fillStyle = panel.colorI;
		ctx.textAlign = 'left';
		ctx.fillText('[' + panel.axisI + '] ' + labelI.min, 3, h - 3);
		ctx.textAlign = 'right';
		ctx.fillText(labelI.max, w - 3, h - 3);

		ctx.fillStyle = panel.colorJ;
		ctx.textAlign = 'left';
		ctx.fillText('[' + panel.axisJ + '] ' + labelJ.min, 3, 11);
		ctx.fillText(labelJ.max, 3, h - 14);
	}

	// ---- Slice extraction: CPU-side flat-array indexing, mirroring
	// ndl::pairwise_slice()'s own semantics exactly (axis i -> result x,
	// axis j -> result y, every other axis fixed at cursor's value) but
	// operating on the already-downloaded flat volume instead of re-fetching
	// anything -- this is the client-side half of the "gotopoint" sync this
	// whole viewer generalizes. ----

	function flatStrides(extent) {
		var strides = new Array(extent.length);
		strides[0] = 1;
		for (var k = 1; k < extent.length; k++) strides[k] = strides[k - 1] * extent[k - 1];
		return strides;
	}

	// Returns a Uint8ClampedArray of width*height (width=extent[axisI],
	// height=extent[axisJ]), row-major with x (axisI) fastest -- matching
	// the layout uploadTexture() expects -- normalized to [0,255] via the
	// shared [min,max] window.
	function extractSliceU8(volume, strides, axisI, axisJ, cursor, min, max) {
		var extent = volume.extent, data = volume.data;
		var width = extent[axisI], height = extent[axisJ];
		var out = new Uint8ClampedArray(width * height);
		var range = max - min;
		// Base flat offset from every axis OTHER than axisI/axisJ, held at
		// cursor's value; axisI/axisJ's own contribution is added per-pixel
		// below.
		var base = 0;
		for (var k = 0; k < extent.length; k++)
			if (k !== axisI && k !== axisJ) base += cursor[k] * strides[k];

		var strideI = strides[axisI], strideJ = strides[axisJ];
		var p = 0;
		for (var y = 0; y < height; y++) {
			var rowBase = base + y * strideJ;
			for (var x = 0; x < width; x++) {
				var v = data[rowBase + x * strideI];
				out[p++] = range > 0 ? Math.round(((v - min) / range) * 255) : 0;
			}
		}
		return out;
	}

	// Like extractSliceU8() above, but for a TRUE-color panel: reads 3
	// values per (axisI,axisJ) position (colorAxis fixed at 0, 1, 2 in
	// turn, every other axis -- including colorAxis's own siblings --
	// held at cursor, same base-offset trick as extractSliceU8()) instead
	// of 1, windowed through the SAME shared [min,max] as every grayscale
	// panel (one shared intensity control for the whole viewer, color
	// panels included, rather than a separate one just for these), packed
	// as interleaved R,G,B (3 bytes/pixel, no alpha -- Panel.uploadSliceRGB()
	// adds that byte itself). Only ever called for a panel whose OWN two
	// axes both differ from colorAxis (see create()'s own redrawPanel()) --
	// a panel where colorAxis IS one of the two plotted axes has nothing
	// to composite (each of its 3 channel positions is just an ordinary
	// axis position along colorAxis) and stays on the plain grayscale path.
	function extractSliceRGB(volume, strides, axisI, axisJ, colorAxis, cursor, min, max) {
		var extent = volume.extent, data = volume.data;
		var width = extent[axisI], height = extent[axisJ];
		var out = new Uint8ClampedArray(width * height * 3);
		var range = max - min;
		var base = 0;
		for (var k = 0; k < extent.length; k++)
			if (k !== axisI && k !== axisJ && k !== colorAxis) base += cursor[k] * strides[k];

		var strideI = strides[axisI], strideJ = strides[axisJ], strideC = strides[colorAxis];
		var p = 0;
		for (var y = 0; y < height; y++) {
			var rowBase = base + y * strideJ;
			for (var x = 0; x < width; x++) {
				var pixelBase = rowBase + x * strideI;
				for (var ch = 0; ch < 3; ch++) {
					var v = data[pixelBase + ch * strideC];
					out[p++] = range > 0 ? Math.round(((v - min) / range) * 255) : 0;
				}
			}
		}
		return out;
	}

	// A point at distance `dist` (clockwise from the top-left corner, along
	// the top edge first) around the perimeter of a width x height
	// rectangle. Wraps mod the full perimeter (2*(width+height)) so callers
	// don't have to.
	function pointOnPerimeter(width, height, dist) {
		var total = 2 * (width + height);
		dist = ((dist % total) + total) % total;
		if (dist < width) return [dist, 0];
		dist -= width;
		if (dist < height) return [width, dist];
		dist -= height;
		if (dist < width) return [width - dist, height];
		dist -= width;
		return [0, height - dist];
	}

	// Strokes the portion of a width x height rectangle's perimeter from
	// startDist to endDist (clockwise, both measured the same way as
	// pointOnPerimeter) in a single color -- used to draw one axis's arc
	// of the panel's excluded-axis frame ring (see its own comment in
	// Panel() for what that ring means). Any corners crossed between
	// startDist and endDist are inserted as explicit polyline vertices so
	// the stroke follows the rectangle's actual outline instead of cutting
	// across its interior.
	function strokePerimeterSegment(ctx, width, height, startDist, endDist, color, lineWidth) {
		var corners = [width, width + height, 2 * width + height];
		var pts = [pointOnPerimeter(width, height, startDist)];
		for (var i = 0; i < corners.length; i++)
			if (corners[i] > startDist && corners[i] < endDist) pts.push(pointOnPerimeter(width, height, corners[i]));
		pts.push(pointOnPerimeter(width, height, endDist));
		ctx.strokeStyle = color;
		ctx.lineWidth = lineWidth;
		ctx.beginPath();
		ctx.moveTo(pts[0][0], pts[0][1]);
		for (var k = 1; k < pts.length; k++) ctx.lineTo(pts[k][0], pts[k][1]);
		ctx.stroke();
	}

	// One panel: a plain Canvas2D canvas (the slice image) with a
	// transparent Canvas2D overlay stacked exactly on top of it (the
	// crosshair + click/drag handling). This used to be a WebGL-textured
	// quad -- switched to Canvas2D because a browser caps a page at a
	// small, fixed number of live WebGL contexts (Chrome: 16), silently
	// losing the *oldest* ones past that -- and a pairwise-view grid's own
	// panel count is C(DIM,2), so a single DIM=6 viewer (15 panels) is
	// already at that ceiling and DIM=7 (21 panels) blows past it outright,
	// even before considering multiple viewers sharing one page (as this
	// library's own generated tutorial does: 3D+4D+5D on one page is
	// 3+6+10=19 contexts, which silently blanked the earliest-created
	// panels). A slice here is just a static grayscale image with no
	// per-pixel GPU work, so Canvas2D's putImageData -- with no comparable
	// context ceiling -- does the same job without that failure mode at
	// any DIM.
	// width/height: this panel's own pixel size -- proportional to
	// extent[axisI] x extent[axisJ] (see create()'s own comment on
	// perAxisPixelSize), not forced square, so a panel showing a
	// 180-long axis next to a 64-long one reads as a wide rectangle
	// instead of squashing the long axis down to match the short one.
	// excludedColors: one color per axis this panel does NOT show, in
	// axis order -- see the frame-ring comment below for what it's used
	// for.
	// gridCol/gridRow: this panel's own 1-indexed CSS grid position --
	// ordinarily just axisI+1/axisJ (every real axis gets its own grid
	// track), but NOT the same thing once options.colorAxis excludes an
	// axis from the grid entirely (see create()'s own displayAxes comment):
	// the remaining, displayed axes' grid tracks are consecutive positions
	// among THEMSELVES, not among every real axis index, so the caller
	// passes those separately rather than this constructor assuming
	// axisI/axisJ (the real, data-space axis indices, still needed for
	// extraction/coloring/labels) double as grid coordinates too.
	function Panel(axisI, axisJ, gridCol, gridRow, width, height, palette, excludedColors) {
		this.axisI = axisI;
		this.axisJ = axisJ;
		this.colorI = palette[axisI % palette.length];
		this.colorJ = palette[axisJ % palette.length];

		var wrap = document.createElement('div');
		wrap.style.position = 'relative';
		wrap.style.width = width + 'px';
		wrap.style.height = height + 'px';
		wrap.style.gridColumn = String(gridCol);
		wrap.style.gridRow = String(gridRow);

		this.sliceCanvas = document.createElement('canvas');
		this.sliceCanvas.width = width;
		this.sliceCanvas.height = height;
		this.sliceCanvas.style.position = 'absolute';
		this.sliceCanvas.style.left = '0';
		this.sliceCanvas.style.top = '0';
		wrap.appendChild(this.sliceCanvas);

		this.overlay = document.createElement('canvas');
		this.overlay.width = width;
		this.overlay.height = height;
		this.overlay.style.position = 'absolute';
		this.overlay.style.left = '0';
		this.overlay.style.top = '0';
		this.overlay.style.cursor = 'crosshair';
		wrap.appendChild(this.overlay);

		// A non-interactive frame ring on top of the slice + crosshair,
		// split into one equal-length arc per axis this panel does NOT
		// show, each arc colored that excluded axis's own color -- the same
		// color its crosshair line is drawn in everywhere it IS shown. This
		// is the direct N-D generalization of the classic axial/coronal/
		// sagittal convention: axial's single frame color is the one axis
		// (Z) it excludes, and that's exactly the color of the crosshair
		// line marking axial's position in the coronal/sagittal views. For
		// DIM=3 a panel excludes exactly 1 axis, so this ring collapses to
		// that same single solid-color frame; for DIM>3 a panel excludes
		// more than one axis at once, so instead of picking one arbitrarily
		// the ring is split so every excluded axis still gets its own
		// crosshair-matching arc somewhere on the frame. A dedicated canvas
		// rather than a CSS border: the ring's arcs need to trace an
		// equal-length path around all 4 sides (crossing corners once
		// there are more than 4 excluded axes), which per-side CSS
		// border-color properties can't express; pointer-events:none keeps
		// it from stealing the overlay canvas's click/hover handling.
		this.frame = document.createElement('canvas');
		this.frame.width = width;
		this.frame.height = height;
		this.frame.style.position = 'absolute';
		this.frame.style.left = '0';
		this.frame.style.top = '0';
		this.frame.style.pointerEvents = 'none';
		wrap.appendChild(this.frame);
		var frameCtx = this.frame.getContext('2d');
		var perimeter = 2 * (width + height);
		var n = excludedColors.length;
		for (var e = 0; e < n; e++)
			strokePerimeterSegment(frameCtx, width, height, e * perimeter / n, (e + 1) * perimeter / n, excludedColors[e], 4);

		this.wrap = wrap;
		this.width = width;
		this.height = height;
		// Slice pixels arrive at (extent[axisI] x extent[axisJ]) resolution,
		// not necessarily width x height -- built up at that native
		// resolution on this offscreen canvas, then drawn scaled onto
		// sliceCanvas (nearest-neighbor, matching the WebGL predecessor's
		// NEAREST filtering) so a low-res slice doesn't need re-deriving
		// its own scaled pixel buffer by hand.
		this.sliceOffscreen = document.createElement('canvas');
	}

	// Shared by uploadSlice() (1 byte/pixel, grayscale -- R=G=B) and
	// uploadSliceRGB() (3 bytes/pixel, true color) below: both just need to
	// expand their own source array into a full RGBA ImageData and blit it,
	// differing only in how many source bytes make up one pixel and
	// whether those bytes go straight to R/G/B or get replicated across
	// all three.
	Panel.prototype._uploadPixels = function (pixels, width, height, channels) {
		if (this.sliceOffscreen.width !== width || this.sliceOffscreen.height !== height) {
			this.sliceOffscreen.width = width;
			this.sliceOffscreen.height = height;
		}
		var offCtx = this.sliceOffscreen.getContext('2d');
		var imgData = offCtx.createImageData(width, height);
		var data = imgData.data;
		if (channels === 1) {
			for (var i = 0, p = 0; i < pixels.length; i++, p += 4) {
				var v = pixels[i];
				data[p] = v; data[p + 1] = v; data[p + 2] = v; data[p + 3] = 255;
			}
		} else {
			for (var i3 = 0, p3 = 0; p3 < data.length; i3 += 3, p3 += 4) {
				data[p3] = pixels[i3]; data[p3 + 1] = pixels[i3 + 1]; data[p3 + 2] = pixels[i3 + 2]; data[p3 + 3] = 255;
			}
		}
		offCtx.putImageData(imgData, 0, 0);

		var ctx = this.sliceCanvas.getContext('2d');
		ctx.imageSmoothingEnabled = false;
		ctx.clearRect(0, 0, this.width, this.height);
		ctx.drawImage(this.sliceOffscreen, 0, 0, width, height, 0, 0, this.width, this.height);
	};
	Panel.prototype.uploadSlice = function (pixels, width, height) { this._uploadPixels(pixels, width, height, 1); };
	Panel.prototype.uploadSliceRGB = function (pixels, width, height) { this._uploadPixels(pixels, width, height, 3); };

	// cursorXY: this panel's own two cursor coordinates, already in data
	// space (0..extent[axisI], 0..extent[axisJ]). Draws a vertical line at
	// the axisI position (colored by axisI's own palette color) and a
	// horizontal line at the axisJ position (colored by axisJ's own color)
	// -- so a given axis's cursor line is always the same color everywhere
	// it's drawn, letting a viewer visually track "where axis k currently
	// is" across every panel that shows it, the same spirit as
	// clinicalvolumeview's cross-referenced per-view line coloring. Each
	// line stops CROSSHAIR_GAP px short of the cursor point on both sides,
	// rather than crossing straight through it (and rather than a filled
	// dot marking it, as earlier versions did) -- the whole point of a
	// crosshair here is to locate a single selected voxel, so painting over
	// that exact voxel (and its immediate neighbors, under a dot) defeats
	// its own purpose. The lines still converge close enough to be
	// unambiguous, just without ever touching the pixel they're pointing
	// at.
	var CROSSHAIR_GAP = 6;
	Panel.prototype.drawCrosshair = function (extentI, extentJ, cx, cy) {
		var ctx = this.overlay.getContext('2d');
		var w = this.width, h = this.height;
		ctx.clearRect(0, 0, w, h);
		var px = ((cx + 0.5) / extentI) * w;
		var py = ((cy + 0.5) / extentJ) * h;

		ctx.lineWidth = 1;
		ctx.strokeStyle = this.colorI;
		ctx.beginPath(); ctx.moveTo(px, 0); ctx.lineTo(px, Math.max(0, py - CROSSHAIR_GAP)); ctx.stroke();
		ctx.beginPath(); ctx.moveTo(px, Math.min(h, py + CROSSHAIR_GAP)); ctx.lineTo(px, h); ctx.stroke();

		ctx.strokeStyle = this.colorJ;
		ctx.beginPath(); ctx.moveTo(0, py); ctx.lineTo(Math.max(0, px - CROSSHAIR_GAP), py); ctx.stroke();
		ctx.beginPath(); ctx.moveTo(Math.min(w, px + CROSSHAIR_GAP), py); ctx.lineTo(w, py); ctx.stroke();
	};

	var DEFAULT_PALETTE = ['#e6194b', '#3cb44b', '#4363d8', '#f58231', '#911eb4', '#46f0f0', '#f032e6', '#bcf60c'];

	// Every panel's pixel size is proportional to its own two axes' SIZE,
	// not a uniform square -- so e.g. a 180-view x 64-detector sinogram's
	// (view,detU) panel reads as a wide rectangle instead of squashing 180
	// views down to the same width as 64 detector columns. One pixel size
	// is computed per AXIS (not per panel): then a panel showing axes
	// (i,j) is simply perAxisPx[i] x perAxisPx[j]. Deriving sizes per-axis
	// rather than per-panel is what keeps the scatterplot-matrix grid
	// aligned: every panel in axis i's column still shares the exact same
	// width, every panel in axis j's row the exact same height, just no
	// longer forced equal to each other.
	//
	// "Size" means two different things depending on whether spacing/unit
	// (ndl::VoxelSpacing, see viewer.h's own comment) are present:
	//   - An axis WITH a unit SHARED BY AT LEAST ONE OTHER AXIS is sized
	//     by its true physical extent (spacing[k]*extent[k]), relative to
	//     the largest physical extent among every other axis sharing that
	//     same unit -- so two spatial axes both in "mm" get a physically
	//     honest aspect ratio (a 100mm axis really does draw 2x as wide as
	//     a 50mm one).
	//   - Every other axis -- no unit at all, spacing/unit not supplied at
	//     all (a version-1 volume), OR a unit no other axis shares (e.g.
	//     the lone time axis in a space+time volume where only the spatial
	//     axes share "mm") -- falls back to plain voxel-count
	//     proportionality against the largest extent among every such
	//     axis, exactly the pre-VoxelSpacing behavior. A singleton unit
	//     group deliberately is NOT treated as "physical" despite
	//     technically having a unit: with nothing else in "s" to compare
	//     it to, sizing it by its own physical extent relative to
	//     itself would trivially always yield the full maxPanelPx,
	//     which silently stops it varying with its own voxel count at
	//     all -- voxel-count proportionality against the other
	//     non-physically-grouped axes is the more useful fallback.
	// Every size is floored at minPanelPx so a short axis never shrinks to
	// something too thin to click.
	function computePerAxisPixelSizes(extent, spacing, unit, maxPanelPx, minPanelPx) {
		var dim = extent.length;
		var hasUnit = new Array(dim), physicalSize = new Array(dim);
		for (var k = 0; k < dim; k++) {
			hasUnit[k] = !!(unit && unit[k]);
			physicalSize[k] = hasUnit[k] ? spacing[k] * extent[k] : null;
		}

		var unitMax = {}, unitCount = {};
		for (var k2 = 0; k2 < dim; k2++)
			if (hasUnit[k2]) {
				unitMax[unit[k2]] = Math.max(unitMax[unit[k2]] || 0, physicalSize[k2]);
				unitCount[unit[k2]] = (unitCount[unit[k2]] || 0) + 1;
			}

		var maxExtent = extent[0];
		for (var k3 = 1; k3 < dim; k3++) if (extent[k3] > maxExtent) maxExtent = extent[k3];

		var perAxisPx = new Array(dim);
		for (var k4 = 0; k4 < dim; k4++) {
			var usePhysical = hasUnit[k4] && unitCount[unit[k4]] >= 2;
			var frac;
			if (usePhysical) {
				var m = unitMax[unit[k4]];
				frac = m > 0 ? physicalSize[k4] / m : 0;
			} else {
				frac = extent[k4] / maxExtent;
			}
			perAxisPx[k4] = Math.max(minPanelPx, Math.round(maxPanelPx * frac));
		}
		return perAxisPx;
	}

	// ---- Component 8: a small, composable client framework shared by BOTH
	// this file's own static (.ndlv-upload) viewer and the live-streaming
	// apps (apps/live_video_stream, apps/bouncing_donut) -- see this
	// project's own plan document for the full design discussion. Five
	// independent pieces:
	//   - Viewport: wraps one rendering surface (here, a Panel -- see its
	//     own comment above), owns its own cursor/window-level state,
	//     delegates actual pixel production to a renderer object (below,
	//     LocalRenderer; a live page instead uses a RemoteRenderer that
	//     speaks the createViewport/updateViewport wire protocol
	//     viewer/viewport.h's own Viewport/RendererRegistry implement --
	//     see apps/live_video_stream.html's own top comment).
	//   - LinkGroup: ties N Viewports' chosen properties together by
	//     subscribing to each member's own paramsChanged and rebroadcasting
	//     to the others -- this file's own pairwise-slice grid is simply
	//     "every panel's Viewport in one LinkGroup, linked on cursor and
	//     window," not a special case.
	//   - LocalRenderer: the renderer half for THIS file's own case -- a
	//     synchronous wrapper around the existing extractSliceU8()/
	//     extractSliceRGB() + Panel.uploadSlice()/drawCrosshair(), i.e. the
	//     exact same pixel-producing code create() always used, just called
	//     through the Viewport/renderer interface instead of a bespoke
	//     redrawPanel() closure -- so this reimplementation produces
	//     BYTE-IDENTICAL output to before, not just visually similar output.
	//   - Controls: standalone factory functions whose `target` argument is
	//     anything exposing {getCursor,setCursor,getWindow,setWindow,on} --
	//     a bare Viewport or a LinkGroup are interchangeable here, so a
	//     control never needs to know which one it's actually driving.
	//   - GridLayout/FreeformLayout: pluggable placement strategies. Both
	//     expose the same "a plain DOM container callers can append into or
	//     nest under another layout's own cell" contract, so a layout item
	//     slot accepts a bare element, a Viewport's own surface, or another
	//     nested Layout with no special-casing.
	// create() (below) is reimplemented on top of these for its own 2D
	// pairwise grid -- functionally identical output (see this project's
	// own before/after regression verification), now composed from
	// Viewport+LocalRenderer+LinkGroup+GridLayout instead of one bespoke
	// closure-heavy function. The existing WebGL 3D volume panel is
	// deliberately NOT torn into this abstraction (see VolumePanel()'s own
	// top comment) -- it stays exactly as it always was, just reading the
	// shared cursor/window from the new LinkGroup instead of a bare closure
	// variable, so it keeps working in lockstep with the flat panels
	// without being rewritten itself. ----

	// One independently addressable view: its own cursor (a full N-D
	// array -- axisI/axisJ's own entries double as "where this viewport's
	// crosshair currently is," every other entry is what a 2D slice
	// renderer holds fixed, exactly viewer/viewport.h's own renderSlice()
	// "fixed" convention) and its own window (min/max intensity mapping).
	// `surface` is renderer-specific (a Panel instance for LocalRenderer;
	// a live page's own canvas element for a RemoteRenderer) -- Viewport
	// itself never touches it directly, only ever hands it to `renderer`.
	// The extra fields below (id/cropMin/cropMax/axisA-C/colorAxis/
	// channelReduction/outputWidth/outputHeight/rotation/alphaScale/fixed)
	// are all optional and unused by LocalRenderer/create()'s own static
	// grid (which only ever reads axisI/axisJ/cursor/windowMin/windowMax,
	// see LocalRenderer.prototype.render above) -- they exist purely for
	// RemoteRenderer (below), whose live "slice"/"volume"-op viewports
	// need cropMin/cropMax (a pan/zoom REGION, not a single cursor point --
	// genuinely different from the static grid's shared-cursor model, see
	// this section's own top comment) plus whatever else that op's own
	// wire params need. A plain, generic bag of optional fields rather
	// than two separate Viewport subclasses: RemoteRenderer's own
	// _paramsFor() (below) just forwards whichever of these happen to be
	// set, so one Viewport class serves both renderer kinds without either
	// caring about the other's fields.
	function Viewport(surface, renderer, options) {
		options = options || {};
		this.surface = surface;
		this.renderer = renderer;
		this.id = options.id;
		this.axisI = options.axisI;
		this.axisJ = options.axisJ;
		this.cursor = (options.cursor || []).slice();
		this.windowMin = options.windowMin;
		this.windowMax = options.windowMax;
		this.cropMin = options.cropMin || null;
		this.cropMax = options.cropMax || null;
		this.axisA = options.axisA;
		this.axisB = options.axisB;
		this.axisC = options.axisC;
		this.colorAxis = options.colorAxis;
		this.channelReduction = options.channelReduction;
		this.outputWidth = options.outputWidth;
		this.outputHeight = options.outputHeight;
		this.rotation = options.rotation;
		this.alphaScale = options.alphaScale;
		this.fixed = options.fixed;
		this._listeners = {};
	}
	Viewport.prototype.on = function (event, fn) {
		(this._listeners[event] = this._listeners[event] || []).push(fn);
	};
	Viewport.prototype._emit = function (event, detail) {
		(this._listeners[event] || []).forEach(function (fn) { fn(detail); });
	};
	Viewport.prototype.getCursor = function () { return this.cursor; };
	// `opts.silent` skips the paramsChanged emit -- used by LinkGroup's own
	// rebroadcast so applying an already-linked change to every OTHER
	// member doesn't itself trigger another round of rebroadcasting.
	// Always copies (never aliases) the passed-in array, so two Viewports
	// (or a Viewport and whatever external caller passed the array in) can
	// never end up silently sharing -- and one mutating in place
	// corrupting -- the same backing array.
	Viewport.prototype.setCursor = function (fullCursor, opts) {
		this.cursor = fullCursor.slice();
		this.redraw();
		if (!opts || !opts.silent) this._emit('paramsChanged', { type: 'cursor' });
	};
	Viewport.prototype.getWindow = function () { return { min: this.windowMin, max: this.windowMax }; };
	Viewport.prototype.setWindow = function (min, max, opts) {
		this.windowMin = min; this.windowMax = max;
		this.redraw();
		if (!opts || !opts.silent) this._emit('paramsChanged', { type: 'window' });
	};
	// The pan/zoom REGION a live "slice"-op viewport currently shows along
	// its own axisI/axisJ -- see this constructor's own top comment on why
	// this is a separate, linkable property from `cursor` (the static
	// grid's own single-point model) rather than reusing it. Linkable via
	// a LinkGroup constructed with `{crop: true}` -- e.g.
	// apps/live_video_stream.html's own "Link pan/zoom" checkbox.
	Viewport.prototype.getCrop = function () { return { min: this.cropMin, max: this.cropMax }; };
	Viewport.prototype.setCrop = function (cropMin, cropMax, opts) {
		this.cropMin = cropMin; this.cropMax = cropMax;
		this.redraw();
		if (!opts || !opts.silent) this._emit('paramsChanged', { type: 'crop' });
	};
	// A live "volume"-op viewport's own rotation (a 9-number flat
	// column-major matrix, viewer/viewport.h's own renderVolume() wire
	// convention -- see Controls.createRotationControl's own comment).
	// Deliberately NOT linkable/emitting paramsChanged: neither app this
	// framework currently drives (apps/bouncing_donut.html has exactly one
	// viewport; apps/live_video_stream.html never uses the "volume" op at
	// all) ever needs two volume viewports' rotations kept in sync, so
	// this stays a plain direct setter+redraw, the same simple shape
	// bouncing_donut.html's own pre-migration rotation sliders already had.
	Viewport.prototype.setRotation = function (rotation) {
		this.rotation = rotation;
		this.redraw();
	};
	// A live "volume"-op viewport's own opacity scale (VOLUME_FRAGMENT_SRC's
	// own uAlphaScale, viewer/viewport.h's own `alphaScale` param) -- same
	// simple direct-setter shape as setRotation above, for the same reason
	// (not a linked/shared property).
	Viewport.prototype.setAlphaScale = function (alphaScale) {
		this.alphaScale = alphaScale;
		this.redraw();
	};
	// A no-op when this Viewport has no renderer (`null`, e.g. a purely
	// state-holding "hub" Viewport with no rendering surface of its own --
	// see create()'s own `cursorState` for exactly this use, a Viewport
	// that exists only to be a LinkGroup's own source-of-truth member
	// before any real, surface-backed Viewport exists yet to hold that
	// role instead).
	Viewport.prototype.redraw = function () { if (this.renderer) this.renderer.render(this); };

	// Ties N Viewports' chosen properties together: whenever any ONE
	// member emits a linked property's paramsChanged, every OTHER member
	// is updated to match (silently, so it doesn't re-emit and cause a
	// second round) -- see this section's own top comment. `linked` is
	// e.g. `{cursor: true, window: true}` (both, what the pairwise-slice
	// grid uses -- one shared N-D cursor and one shared intensity window
	// across every panel) -- either can be omitted/false to link only the
	// other. `_propagating` guards against re-entrant rebroadcast loops
	// (member A's change rebroadcasts to B, which -- without this guard --
	// would itself try to rebroadcast back to A and everyone else again).
	function LinkGroup(members, linked) {
		this.members = [];
		this.linked = linked || {};
		this._propagating = false;
		var self = this;
		(members || []).forEach(function (m) { self.addMember(m); });
	}
	// Adds one more member to an already-constructed group -- needed
	// because not every member necessarily exists yet at the moment a
	// group is first formed (create()'s own case: the shared cursor/
	// window state needs to exist before the toolbar's Level/Window
	// sliders are built, which happens before any actual panel -- and
	// therefore any actual per-panel Viewport -- exists to join the
	// group). The newly added member is expected to already carry
	// up-to-date values (create() always constructs one from the group's
	// own current getCursor()/getWindow() right before adding it) --
	// addMember() itself doesn't force a sync, matching setParams()'s own
	// "the caller is responsible" philosophy elsewhere in this project.
	LinkGroup.prototype.addMember = function (member) {
		this.members.push(member);
		var self = this;
		member.on('paramsChanged', function (detail) {
			if (self._propagating) return;
			if (detail.type === 'cursor' && !self.linked.cursor) return;
			if (detail.type === 'window' && !self.linked.window) return;
			if (detail.type === 'crop' && !self.linked.crop) return;
			self._propagating = true;
			self.members.forEach(function (other) {
				if (other === member) return;
				if (detail.type === 'cursor') other.setCursor(member.cursor, { silent: true });
				else if (detail.type === 'window') other.setWindow(member.windowMin, member.windowMax, { silent: true });
				else if (detail.type === 'crop') other.setCrop(member.cropMin, member.cropMax, { silent: true });
			});
			self._propagating = false;
		});
	};
	// Duck-typed the same {getCursor,setCursor,getWindow,setWindow,
	// getCrop,setCrop,on} interface a bare Viewport exposes (see this
	// section's own top comment) -- routed through members[0], whose own
	// paramsChanged (see the constructor above) then rebroadcasts to every
	// other member, so a single call here updates the whole group in one
	// pass, exactly like this file's old, single shared `cursor`/`range`
	// closure variables did before this reimplementation.
	LinkGroup.prototype.getCursor = function () { return this.members[0].getCursor(); };
	LinkGroup.prototype.setCursor = function (fullCursor) { this.members[0].setCursor(fullCursor); };
	LinkGroup.prototype.getWindow = function () { return this.members[0].getWindow(); };
	LinkGroup.prototype.setWindow = function (min, max) { this.members[0].setWindow(min, max); };
	LinkGroup.prototype.getCrop = function () { return this.members[0].getCrop(); };
	LinkGroup.prototype.setCrop = function (cropMin, cropMax) { this.members[0].setCrop(cropMin, cropMax); };
	LinkGroup.prototype.on = function (event, fn) { this.members[0].on(event, fn); };

	// The renderer half for THIS file's own (static, fully-local) case: a
	// thin wrapper around the SAME extractSliceU8()/extractSliceRGB() +
	// Panel.uploadSlice()/uploadSliceRGB()/drawCrosshair() this file always
	// used -- see this section's own top comment on why reusing rather
	// than re-deriving equivalent logic is what makes the reimplemented
	// create() produce byte-identical output. `colorAxis` is a fixed,
	// whole-viewer-instance setting (matches VolumePanel's own isColor,
	// same reasoning), not per-render state.
	function LocalRenderer(volume, strides, colorAxis) {
		this.volume = volume;
		this.strides = strides;
		this.colorAxis = colorAxis;
	}
	LocalRenderer.prototype.render = function (viewport) {
		var volume = this.volume, strides = this.strides, panel = viewport.surface;
		if (this.colorAxis >= 0 && viewport.axisI !== this.colorAxis && viewport.axisJ !== this.colorAxis) {
			var rgb = extractSliceRGB(volume, strides, viewport.axisI, viewport.axisJ, this.colorAxis, viewport.cursor, viewport.windowMin, viewport.windowMax);
			panel.uploadSliceRGB(rgb, volume.extent[viewport.axisI], volume.extent[viewport.axisJ]);
		} else {
			var pixels = extractSliceU8(volume, strides, viewport.axisI, viewport.axisJ, viewport.cursor, viewport.windowMin, viewport.windowMax);
			panel.uploadSlice(pixels, volume.extent[viewport.axisI], volume.extent[viewport.axisJ]);
		}
		panel.drawCrosshair(volume.extent[viewport.axisI], volume.extent[viewport.axisJ], viewport.cursor[viewport.axisI], viewport.cursor[viewport.axisJ]);
	};

	// ---- RemoteRenderer: the live-streaming counterpart to LocalRenderer
	// above -- speaks the createViewport/updateViewport/closeViewport/
	// queryValue wire protocol viewer/viewport.h's own Viewport/
	// RendererRegistry and net/websocket_server.h implement server-side
	// (see apps/live_video_stream.cpp and apps/bouncing_donut.cpp), rather
	// than rendering pixels itself -- the server does that, and hands back
	// ordinary binary frames over the SAME shared WebSocket every viewport
	// on the page multiplexes over (RemoteConnection, below), keyed by
	// each Viewport's own `id`. A Viewport doesn't need to know or care
	// which renderer kind it holds; only RemoteRenderer's own render()
	// differs in KIND from LocalRenderer's (a network send instead of a
	// synchronous draw -- the actual pixels arrive asynchronously, via
	// RemoteConnection's own onFrame dispatch below, whenever the server
	// gets around to it). ----

	// Wire format for one rendered viewport update (a single binary WS
	// frame) -- see apps/live_video_stream.cpp's own encodeWireFrame()
	// comment for the authoritative byte layout:
	//   byte 0    id length (uint8)
	//   N bytes   id string
	//   8 bytes   globalIndex (uint64 LE)
	//   4 bytes   width (uint32 LE)
	//   4 bytes   height (uint32 LE)
	//   1 byte    channels (1=grayscale, 3=RGB)
	//   remaining raw pixel bytes
	function decodeWireFrame(buf) {
		var view = new DataView(buf);
		var idLen = view.getUint8(0);
		var id = new TextDecoder('utf-8').decode(new Uint8Array(buf, 1, idLen));
		var off = 1 + idLen;
		var globalIndex = Number(view.getBigUint64(off, true));
		var width = view.getUint32(off + 8, true);
		var height = view.getUint32(off + 12, true);
		var channels = view.getUint8(off + 16);
		var pixels = new Uint8Array(buf, off + 17);
		return { id: id, globalIndex: globalIndex, width: width, height: height, channels: channels, pixels: pixels };
	}
	// Draws one decoded wire frame directly onto a plain `<canvas>` (a live
	// viewport's own `surface` -- unlike LocalRenderer's Panel-based
	// surface, there's no separate overlay/frame canvas here: a live
	// viewport has no crosshair to draw, since there's no shared N-D
	// cursor concept in the live/pan-zoom model -- see Viewport's own
	// constructor comment on why cropMin/cropMax is a different thing).
	function drawFrameToCanvas(canvas, frame) {
		if (canvas.width !== frame.width || canvas.height !== frame.height) { canvas.width = frame.width; canvas.height = frame.height; }
		var ctx = canvas.getContext('2d');
		var imgData = ctx.createImageData(frame.width, frame.height);
		var data = imgData.data, pixels = frame.pixels;
		if (frame.channels === 3) {
			for (var i = 0, p = 0; p < data.length; i += 3, p += 4) { data[p] = pixels[i]; data[p + 1] = pixels[i + 1]; data[p + 2] = pixels[i + 2]; data[p + 3] = 255; }
		} else {
			for (var i2 = 0, p2 = 0; p2 < data.length; i2++, p2 += 4) { var v = pixels[i2]; data[p2] = v; data[p2 + 1] = v; data[p2 + 2] = v; data[p2 + 3] = 255; }
		}
		ctx.putImageData(imgData, 0, 0);
	}

	// One shared, multiplexed WebSocket connection for every Viewport on a
	// live page -- avoids a separate TCP/handshake per viewport, matching
	// this project's own wire-protocol design (see viewer/viewport.h's own
	// top comment). Viewports register themselves (by id) so an incoming
	// binary frame or `valueResult` JSON message can be routed to the
	// right one.
	//
	// `_pending`: messages sent before the socket finishes connecting are
	// queued here and flushed once it opens, rather than silently dropped
	// -- this is the actual fix for a real bug this project's own apps hit
	// during development (apps/live_video_stream.html's very first
	// createViewport() call ran synchronously right after `new
	// WebSocket(...)`, before the socket reached OPEN, and a bare
	// `if (ws.readyState===OPEN) ws.send(...)` guard silently ate it) --
	// making the connection itself queue-and-flush means no caller of
	// send() ever needs to know or care whether the handshake has finished
	// yet, closing off that whole class of bug rather than just patching
	// the one call site that happened to hit it first.
	function RemoteConnection(url) {
		this.url = url;
		this.viewports = {};
		this.ws = null;
		this._pending = [];
		this._statusListeners = [];
		this._connect();
	}
	RemoteConnection.prototype._connect = function () {
		var self = this;
		this.ws = new WebSocket(this.url);
		this.ws.binaryType = 'arraybuffer';
		this.ws.onopen = function () {
			self._setStatus('connected to ' + self.url);
			var pending = self._pending;
			self._pending = [];
			pending.forEach(function (text) { self.ws.send(text); });
		};
		this.ws.onclose = function () { self._setStatus('disconnected'); };
		this.ws.onerror = function () { self._setStatus('connection error'); };
		this.ws.onmessage = function (evt) {
			if (typeof evt.data === 'string') self._handleJson(JSON.parse(evt.data));
			else self._handleBinary(evt.data);
		};
	};
	/// Registers a callback for human-readable connection-state text (e.g. to drive a status line in the page).
	RemoteConnection.prototype.onStatus = function (fn) { this._statusListeners.push(fn); };
	RemoteConnection.prototype._setStatus = function (text) { this._statusListeners.forEach(function (fn) { fn(text); }); };
	/// Sends one JSON control message -- queues it (see this section's own top comment) if the socket isn't OPEN yet.
	RemoteConnection.prototype.send = function (obj) {
		var text = JSON.stringify(obj);
		if (this.ws && this.ws.readyState === WebSocket.OPEN) this.ws.send(text);
		else this._pending.push(text);
	};
	RemoteConnection.prototype.registerViewport = function (viewport) { this.viewports[viewport.id] = viewport; };
	RemoteConnection.prototype.unregisterViewport = function (viewport) { delete this.viewports[viewport.id]; };
	RemoteConnection.prototype._handleBinary = function (buf) {
		var frame = decodeWireFrame(buf);
		var vp = this.viewports[frame.id];
		if (vp && vp.renderer && vp.renderer.onFrame) vp.renderer.onFrame(vp, frame);
	};
	RemoteConnection.prototype._handleJson = function (msg) {
		if (msg.type === 'valueResult') {
			var vp = this.viewports[msg.id];
			if (vp && vp.renderer && vp.renderer.onValueResult) vp.renderer.onValueResult(vp, msg);
		}
	};
	/// Sends a queryValue request for `coord` (a full N-D array) against `viewportId` -- see viewer/viewport.h's own queryValue() comment for why this returns the RAW, unwindowed value, not a rendered pixel. Replies via RemoteRenderer.onValueResult -> viewport.onValueResult(value).
	RemoteConnection.prototype.queryValue = function (viewportId, coord) {
		this.send({ type: 'queryValue', id: viewportId, coord: coord });
	};
	/// Like queryValue() but for several full N-D coordinates at once (e.g. the same spatial position with only the channel axis varying, one entry per RGB channel) -- one round trip instead of one per coordinate. Replies via RemoteRenderer.onValueResult -> viewport.onValuesResult(values), an array in the same order as `coords`.
	RemoteConnection.prototype.queryValues = function (viewportId, coords) {
		this.send({ type: 'queryValue', id: viewportId, coords: coords });
	};

	// Per-viewport renderer half: builds the wire `params` object from
	// whatever fields happen to be set on the Viewport (see its own
	// constructor comment -- axisI/axisJ/axisA-C/colorAxis/
	// channelReduction/outputWidth/outputHeight/windowMin/windowMax/
	// rotation/alphaScale/cropMin/cropMax/fixed), rather than one fixed
	// shape per op: the server-side RendererRegistry (viewer/viewport.h)
	// already validates which keys a given op actually needs, so this
	// stays generic over "slice" and "volume" alike instead of needing a
	// separate RemoteRenderer subclass per op.
	function RemoteRenderer(connection, op) {
		this.connection = connection;
		this.op = op;
	}
	RemoteRenderer.prototype._paramsFor = function (viewport) {
		var p = {};
		['axisI', 'axisJ', 'axisA', 'axisB', 'axisC', 'colorAxis', 'channelReduction', 'outputWidth', 'outputHeight', 'windowMin', 'windowMax', 'rotation', 'alphaScale'].forEach(function (k) {
			if (viewport[k] !== undefined && viewport[k] !== null) p[k] = viewport[k];
		});
		if (viewport.cropMin) p.cropMin = viewport.cropMin;
		if (viewport.cropMax) p.cropMax = viewport.cropMax;
		if (viewport.fixed) p.fixed = viewport.fixed;
		return p;
	};
	/// Registers `viewport` with the shared connection and sends its initial createViewport -- call once, right after constructing the Viewport.
	RemoteRenderer.prototype.create = function (viewport) {
		this.connection.registerViewport(viewport);
		this.connection.send({ type: 'createViewport', id: viewport.id, op: this.op, params: this._paramsFor(viewport) });
	};
	/// Viewport.prototype.redraw()'s own call into this renderer -- for a live viewport this MEANS "tell the server our latest params," not "draw now" (the actual pixels arrive later, asynchronously, via onFrame below).
	RemoteRenderer.prototype.render = function (viewport) {
		this.connection.send({ type: 'updateViewport', id: viewport.id, params: this._paramsFor(viewport) });
	};
	RemoteRenderer.prototype.close = function (viewport) {
		this.connection.send({ type: 'closeViewport', id: viewport.id });
		this.connection.unregisterViewport(viewport);
	};
	// Called by RemoteConnection whenever a binary frame arrives for this
	// viewport's own id. Seeds cropMin/cropMax to the source's full native
	// extent on the FIRST frame only (the server's own default when no
	// crop has been requested yet -- see viewer/viewport.h's own
	// renderSlice() comment), draws the frame, and lets the page itself
	// react (`viewport.onFrame`, optional) for anything renderer-agnostic
	// code here shouldn't need to know about (a page's own readout, e.g.).
	RemoteRenderer.prototype.onFrame = function (viewport, frame) {
		viewport.lastGlobalIndex = frame.globalIndex;
		if (!viewport.nativeWidth) {
			viewport.nativeWidth = frame.width;
			viewport.nativeHeight = frame.height;
			if (!viewport.cropMin) { viewport.cropMin = [0, 0]; viewport.cropMax = [frame.width, frame.height]; }
		}
		// viewport.fps: an exponentially-smoothed frames-per-second
		// estimate from consecutive frame ARRIVAL times -- maintained here
		// (not left to each page to hand-roll) since every RemoteRenderer-
		// backed page wants the same thing, "how fast is this viewport
		// actually updating." A plain instantaneous 1000/dt would jitter
		// wildly frame to frame (network/render timing noise); the 0.8/0.2
		// blend favors stability while still tracking real, sustained
		// changes (e.g. dragging an output-size slider should visibly move
		// this number within roughly a second, not need many seconds to
		// settle).
		var now = (typeof performance !== 'undefined' ? performance.now() : Date.now());
		if (viewport._lastFrameTime !== undefined) {
			var dt = now - viewport._lastFrameTime;
			if (dt > 0) {
				var instantFps = 1000 / dt;
				viewport.fps = viewport.fps === undefined ? instantFps : viewport.fps * 0.8 + instantFps * 0.2;
			}
		}
		viewport._lastFrameTime = now;
		drawFrameToCanvas(viewport.surface, frame);
		if (viewport.onFrame) viewport.onFrame(frame);
	};
	// Called by RemoteConnection whenever a `valueResult` JSON reply
	// arrives -- the native-value-hover query client (Component 11's own
	// client half). Two shapes, matching RemoteConnection.queryValue()/
	// queryValues()'s own request pair: a single `msg.value` (from a
	// `"coord"` request) dispatches to `viewport.onValueResult`; an array
	// `msg.values` (from a `"coords"` request -- e.g. one entry per RGB
	// channel at the same spatial position) dispatches to
	// `viewport.onValuesResult` instead. Both optional -- how a page
	// itself displays the result (its own readout box).
	RemoteRenderer.prototype.onValueResult = function (viewport, msg) {
		if (msg.values !== undefined) { if (viewport.onValuesResult) viewport.onValuesResult(msg.values); }
		else if (viewport.onValueResult) viewport.onValueResult(msg.value);
	};

	// A single slider factory shared by every Controls.* factory below AND
	// (unchanged) create()'s own volume-panel rotation/alpha sliders --
	// hoisted to module scope (it never referenced anything from create()'s
	// own closure besides the already-module-level formatValue()) rather
	// than duplicated, for the exact same "reuse, don't re-derive" reason
	// LocalRenderer above reuses extractSliceU8()/extractSliceRGB(). See
	// this function's own original comment (preserved verbatim) for the
	// full rationale on window/level formatting, fixed-width labels, and
	// setLabel()'s own two call patterns.
	function makeSlider(labelText, min, max, step, initial, decimals, unitSuffix, onInput, labelColor) {
		var row = document.createElement('span');
		row.style.whiteSpace = 'nowrap';
		var swatch = null;
		if (labelColor) {
			swatch = document.createElement('span');
			swatch.style.display = 'inline-block';
			swatch.style.width = '0.7em';
			swatch.style.height = '0.7em';
			swatch.style.marginRight = '0.3em';
			swatch.style.verticalAlign = 'middle';
			swatch.style.background = labelColor;
			row.appendChild(swatch);
		}
		var label = document.createElement('span');
		label.textContent = labelText + ': ';
		row.appendChild(label);
		var slider = document.createElement('input');
		slider.type = 'range';
		slider.min = String(min);
		slider.max = String(max);
		slider.step = String(step || 'any');
		slider.value = String(initial);
		slider.style.verticalAlign = 'middle';
		row.appendChild(slider);
		var valueLabel = document.createElement('span');
		valueLabel.style.marginLeft = '0.4em';
		valueLabel.style.display = 'inline-block';
		valueLabel.style.fontFamily = 'monospace';
		var widestChars = Math.max(formatValue(min, decimals, true).length, formatValue(max, decimals, true).length) + (unitSuffix ? unitSuffix.length : 0);
		valueLabel.style.width = (widestChars + 0.5) + 'ch';
		row.appendChild(valueLabel);
		function setValue(v) { slider.value = String(v); valueLabel.textContent = formatValue(v, decimals, true) + (unitSuffix || ''); }
		setValue(initial);
		function setLabel(text, color) {
			label.textContent = text + ': ';
			if (swatch && color) swatch.style.background = color;
		}
		slider.addEventListener('input', function () { onInput(parseFloat(slider.value), setValue); });
		return { row: row, slider: slider, setValue: setValue, setLabel: setLabel };
	}

	// Standalone control factories -- `target` is anything exposing
	// {getCursor,setCursor,getWindow,setWindow,on} (see this section's own
	// top comment): a bare Viewport or a LinkGroup work identically here.
	var Controls = {};
	// Level/Window sliders (see this file's own top comment on the medical-
	// imaging level/window <-> min/max convention) -- `dataRange`/`decimals`
	// are the volume's own fixed properties (slider bounds, display
	// formatting), not part of `target`'s own state. Appends both slider
	// rows directly to `container`. Subscribes to target's own
	// paramsChanged so the sliders' own displayed position stays in sync
	// with a window change that happened some OTHER way (a reset button,
	// or -- once a LinkGroup is involved -- another linked member).
	Controls.createWindowLevelControl = function (container, target, dataRange, decimals) {
		var windowSpan = (dataRange.max - dataRange.min) || 1;
		var w0 = target.getWindow();
		var levelCtl = makeSlider('Level', dataRange.min, dataRange.max, windowSpan / 500, (w0.min + w0.max) / 2, decimals, '', function (level) {
			var cur = target.getWindow(), width = cur.max - cur.min;
			target.setWindow(level - width / 2, level + width / 2);
		});
		var windowCtl = makeSlider('Window', windowSpan / 500, windowSpan, windowSpan / 500, w0.max - w0.min, decimals, '', function (winWidth) {
			var cur = target.getWindow(), level = (cur.min + cur.max) / 2;
			target.setWindow(level - winWidth / 2, level + winWidth / 2);
		});
		function sync() {
			var w = target.getWindow();
			levelCtl.setValue((w.min + w.max) / 2);
			windowCtl.setValue(w.max - w.min);
		}
		target.on('paramsChanged', function (detail) { if (detail.type === 'window') sync(); });
		container.appendChild(levelCtl.row);
		container.appendChild(windowCtl.row);
		return { levelCtl: levelCtl, windowCtl: windowCtl, refresh: sync };
	};
	// A plain labeled button wired to an arbitrary callback -- shared shape
	// for every reset button (cursor/window/rotation/alpha all use this
	// same small factory, whatever they actually reset).
	Controls.createButton = function (container, label, onClick) {
		var btn = document.createElement('button');
		btn.type = 'button';
		btn.textContent = label;
		btn.addEventListener('click', onClick);
		container.appendChild(btn);
		return btn;
	};
	// Three angle sliders (X/Y/Z, in degrees) driving a live "volume"-op
	// Viewport's own rotation -- the client-side half of Component 9/16's
	// comparison: builds the SAME column-major 3x3 matrix convention (see
	// detail_viewport::matrix3FromColumnMajor()'s own comment in
	// viewer/viewport.h) via the SAME mat3RotationX/Y/Z + mat3Multiply
	// helpers (module scope, above) the static viewer's own VolumePanel
	// rotation controls already use, and in the SAME combination order
	// (`mat3Multiply(Rz, mat3Multiply(Ry, Rx))`, matching
	// VolumePanel.prototype.rotationMatrix()) -- so a given (x,y,z) degree
	// triple means the identical rotation whether it ends up ray-marched
	// on the GPU (static) or the CPU (live), the actual point of keeping
	// the two conventions byte-for-byte identical rather than just
	// "similar." `target` is a Viewport (not a LinkGroup -- see
	// Viewport.prototype.setRotation's own comment on why rotation isn't
	// a linked property here).
	Controls.createRotationControl = function (container, target, initialDeg) {
		initialDeg = initialDeg || { x: 0, y: 0, z: 0 };
		var angles = { x: initialDeg.x, y: initialDeg.y, z: initialDeg.z };
		function currentRotation() {
			return mat3Multiply(mat3RotationZ(angles.z * Math.PI / 180), mat3Multiply(mat3RotationY(angles.y * Math.PI / 180), mat3RotationX(angles.x * Math.PI / 180)));
		}
		function makeAxisSlider(axisLabel, key) {
			var ctl = makeSlider(axisLabel, -180, 180, 1, angles[key], 0, '°', function (deg, setValue) {
				angles[key] = deg;
				setValue(deg);
				target.setRotation(currentRotation());
			});
			container.appendChild(ctl.row);
			return ctl;
		}
		var ctls = { x: makeAxisSlider('X', 'x'), y: makeAxisSlider('Y', 'y'), z: makeAxisSlider('Z', 'z') };
		target.setRotation(currentRotation());
		return {
			ctls: ctls,
			reset: function () {
				angles.x = angles.y = angles.z = 0;
				ctls.x.setValue(0); ctls.y.setValue(0); ctls.z.setValue(0);
				target.setRotation(currentRotation());
			}
		};
	};
	// A single slider driving a "volume"-op Viewport's own opacity
	// (Viewport.prototype.setAlphaScale, VOLUME_FRAGMENT_SRC's own
	// uAlphaScale) -- the live-viewport counterpart to the static viewer's
	// own "Alpha" slider (create(), below).
	Controls.createAlphaControl = function (container, target, initialPct) {
		initialPct = initialPct === undefined ? 50 : initialPct;
		var ctl = makeSlider('Alpha', 0, 100, 1, initialPct, 0, '%', function (pct, setValue) {
			setValue(pct);
			target.setAlphaScale(pct / 100);
		});
		container.appendChild(ctl.row);
		target.setAlphaScale(initialPct / 100);
		return ctl;
	};

	// CSS-grid placement strategy: explicit per-track sizes (not a single
	// gridAutoColumns/Rows), matching this file's own pairwise-slice grid
	// (column c is one axis's own pixel width, row r is another's own
	// height -- see create()'s own grid-track comment). `container` is an
	// existing DOM element this turns into the grid itself (its own
	// gridTemplateColumns/Rows are set here); items place themselves into
	// it via place() or -- for a Panel/VolumePanel whose own constructor
	// already sets its own wrap's gridColumn/gridRow (both do, given their
	// own di+2/dj-style coordinates up front) -- via a plain
	// container.appendChild(), either is fine since GridLayout doesn't
	// require every child to have been placed through it.
	function GridLayout(container, options) {
		options = options || {};
		container.style.display = 'inline-grid';
		container.style.gap = options.gap || '4px';
		if (options.columns) container.style.gridTemplateColumns = options.columns.join(' ');
		if (options.rows) container.style.gridTemplateRows = options.rows.join(' ');
		this.container = container;
	}
	// Places `element` (a plain DOM node, a Viewport's own `.surface.wrap`,
	// or another nested Layout's own `.container`, see this section's own
	// top comment on nestability) at 1-indexed grid position (col,row) and
	// appends it.
	GridLayout.prototype.place = function (element, col, row) {
		element.style.gridColumn = String(col);
		element.style.gridRow = String(row);
		this.container.appendChild(element);
	};
	GridLayout.prototype.mount = function (parent) { parent.appendChild(this.container); };

	// Freeform placement strategy: no shared grid, each item explicitly
	// positioned/sized (or simply appended and left to the container's own
	// CSS/flow) -- the pluggable alternative GridLayout's own comment
	// promises, for a layout that isn't naturally a grid at all (e.g. a
	// live page's own "however many independent viewports the user has
	// added" panel row -- see apps/live_video_stream.html).
	function FreeformLayout(container) {
		this.container = container;
	}
	FreeformLayout.prototype.add = function (element, style) {
		if (style) for (var k in style) if (Object.prototype.hasOwnProperty.call(style, k)) element.style[k] = style[k];
		this.container.appendChild(element);
	};
	FreeformLayout.prototype.mount = function (parent) { parent.appendChild(this.container); };

	// Creates the full pairwise-view grid inside `container` for the parsed
	// volume in `arrayBuffer`, wires up click/drag-to-navigate, and returns
	// a small handle ({cursor, setCursor(), destroy()}) for programmatic
	// control.
	function create(container, arrayBuffer, options) {
		options = options || {};
		var maxPanelPx = options.panelSize || 320; // the largest-extent axis's own pixel size; every other axis scales down proportionally to it
		var minPanelPx = options.minPanelSize || 50; // keeps roughly maxPanelPx's own 200:32 ratio from before this default was bumped up, so a short axis stays proportionally as small relative to a long one, just not shrunk to an unclickable sliver
		var palette = options.palette || DEFAULT_PALETTE;

		var volume = parseVolume(arrayBuffer);
		var strides = flatStrides(volume.extent);

		// options.colorAxis: which axis (if any) holds RGB channels --
		// undefined/null (the default) means "no color axis, every panel
		// grayscale," exactly today's behavior; an axis index means every
		// panel EXCLUDING that axis renders as true color instead (see
		// redrawPanel() below), reading the 3 values along colorAxis at each
		// spatial position as R/G/B rather than treating it as just another
		// grayscale axis. Deliberately explicit opt-in rather than
		// auto-detecting "axis 0 has extent 3" -- this file's own {channel,
		// width, height, ...} convention makes that a reasonable guess most
		// of the time, but a guess is still the wrong default for a plain
		// data axis that happens to be extent-3 for unrelated reasons.
		// Validated (not just trusted) since a bad index would otherwise
		// silently corrupt every color panel's own base-offset math.
		var colorAxis = (options.colorAxis !== undefined && options.colorAxis !== null) ? options.colorAxis : -1;
		if (colorAxis < 0 || colorAxis >= volume.dim || volume.extent[colorAxis] !== 3) colorAxis = -1;
		// dataRange: the volume's own true, fixed native range -- used for
		// the readout's static "value range" line and as the window
		// sliders' own min/max bounds, and never mutated. range: the
		// user-adjustable WINDOW within it (starts equal to dataRange),
		// shared by both the flat panels' grayscale mapping and the volume
		// panel's opacity mapping -- see the window-slider wiring below and
		// VOLUME_FRAGMENT_SRC's own comment on why one control drives both.
		var dataRange = computeMinMax(volume.data);
		var perAxisPx = computePerAxisPixelSizes(volume.extent, volume.spacing, volume.unit, maxPanelPx, minPanelPx);
		var valueIsFloat = isFloatingDtype(volume.data);
		var valueDecimals = valueDecimalPlaces(dataRange.max - dataRange.min);

		var initialCursor = new Array(volume.dim);
		for (var k = 0; k < volume.dim; k++) initialCursor[k] = Math.floor(volume.extent[k] / 2);
		var defaultCursor = initialCursor.slice();
		// The shared N-D cursor + shared intensity window, as a LinkGroup
		// (Component 8 -- see this file's own top comment above) --
		// `cursorState` is a purely state-holding Viewport (no surface, no
		// renderer: see Viewport.prototype.redraw's own null-guard) that
		// exists before any real, Panel-backed Viewport does, since the
		// toolbar's Level/Window sliders (built next) need a `target` to
		// read/drive immediately. Every flat panel's own Viewport (built
		// further down, once each Panel exists) joins this SAME group via
		// linkGroup.addMember() -- "the static grid's shared cursor/
		// window-level is just a LinkGroup containing every panel, linked
		// on everything," per this project's own plan document, exactly.
		var cursorState = new Viewport(null, null, { cursor: initialCursor, windowMin: dataRange.min, windowMax: dataRange.max });
		var linkGroup = new LinkGroup([cursorState], { cursor: true, window: true });

		// Axis limits (see axisLimitLabel()'s own comment), precomputed
		// once per axis here since a given axis's own limits are the same
		// wherever it's shown, then drawn directly on every panel that
		// shows it (drawAxisLimitLabels(), called per panel below).
		var axisLabels = new Array(volume.dim);
		for (var la = 0; la < volume.dim; la++)
			axisLabels[la] = axisLimitLabel(volume.extent[la], volume.spacing ? volume.spacing[la] : 1, volume.unit ? volume.unit[la] : '');

		// The volume's own native value range -- not axis-specific (unlike
		// axisLabels above), so it has no single panel to live on; kept as
		// a static first line of the readout box instead of a separate
		// legend element, so there's one info box near the grid rather
		// than two. Two lines below it: the hovered voxel's own index +
		// native value (always shown once hovering starts), and -- only
		// for a volume with physical calibration (ndl::VoxelSpacing,
		// version-2 NDLV) -- the same voxel's physical coordinate, one
		// component per axis, in that axis's own unit where it has one.
		var readout = document.createElement('div');
		readout.style.fontFamily = 'monospace';
		readout.style.marginBottom = '4px';
		var readoutRange = document.createElement('div');
		readoutRange.textContent = 'value range: ' + formatValue(dataRange.min, valueDecimals, valueIsFloat) + ' – ' + formatValue(dataRange.max, valueDecimals, valueIsFloat);
		readout.appendChild(readoutRange);
		// minHeight reserves the line's own height so it doesn't collapse to
		// 0 while empty (between hovers) and pop back in on the next hover,
		// shifting everything below it -- but that reservation only covers
		// ONE line; nowrap/ellipsis is equally required, or a narrow enough
		// container (a real jump this had, caught by testing at container
		// widths well below the toolbar/grid's own default) wraps the
		// populated text onto a second line, growing past the reserved
		// height and jumping anyway. Overflow just clips (ellipsis) rather
		// than reflowing -- this is supplementary hover info, not content
		// worth widening the readout box for.
		var readoutVoxel = document.createElement('div');
		readoutVoxel.style.minHeight = '1.2em';
		readoutVoxel.style.whiteSpace = 'nowrap';
		readoutVoxel.style.overflow = 'hidden';
		readoutVoxel.style.textOverflow = 'ellipsis';
		readout.appendChild(readoutVoxel);
		var readoutPhysical = document.createElement('div');
		readoutPhysical.style.minHeight = '1.2em';
		readoutPhysical.style.whiteSpace = 'nowrap';
		readoutPhysical.style.overflow = 'hidden';
		readoutPhysical.style.textOverflow = 'ellipsis';
		if (volume.spacing) readout.appendChild(readoutPhysical);
		container.appendChild(readout);

		// Toolbar: window/level (see makeSlider()'s own comment on the
		// level/window <-> min/max conversion), one shared intensity
		// mapping driving both the flat panels' grayscale and the volume
		// panel's opacity, plus "Reset slices" (cursor only, i.e. what a 2D
		// panel click changes), "Reset window/level" (the shared intensity
		// mapping only), and a Fullscreen toggle. "Reset 3D view" (rotation
		// only) is built further down, alongside the rotation sliders it
		// resets, rather than here -- each reset button lives next to the
		// controls it actually affects instead of all three being grouped
		// together regardless of which view they belong to. Each of the
		// three touches exactly its own one concern: resetting the cursor
		// shouldn't wipe out a deliberately tuned window/level, any more
		// than resetting window/level should recenter the cursor or touch
		// the volume's own rotation. A row, not a stack, to stay compact:
		// see this function's own top-level layout comment.
		var controls = document.createElement('div');
		controls.style.fontFamily = 'monospace';
		controls.style.fontSize = '0.85em';
		controls.style.marginBottom = '6px';
		controls.style.display = 'flex';
		controls.style.flexWrap = 'wrap';
		controls.style.alignItems = 'center';
		controls.style.gap = '4px 14px';
		container.appendChild(controls);

		// Window/level, not min/max: LEVEL is the center of the visible
		// range, WINDOW is its width -- the standard medical-imaging
		// convention (equivalent to min/max, just a different two numbers
		// describing the same interval: min=level-window/2,
		// max=level+window/2) since centering-and-widening is usually the
		// more natural way to explore a window, not independently dragging
		// two endpoints. Built via Controls.createWindowLevelControl()
		// (Component 8, module scope, above) targeting `linkGroup` directly
		// -- the exact same makeSlider()-based DOM (also hoisted to module
		// scope, reused verbatim, not re-derived) this file always built
		// here, so this reimplementation produces byte-identical markup,
		// just wired through the shared framework instead of a bespoke
		// `range`/`refreshWindow()` closure pair.
		Controls.createWindowLevelControl(controls, linkGroup, dataRange, valueDecimals);

		var resetSlicesButton = document.createElement('button');
		resetSlicesButton.type = 'button';
		resetSlicesButton.textContent = 'Reset slices';
		resetSlicesButton.addEventListener('click', resetSlices);
		controls.appendChild(resetSlicesButton);

		// Separate from "Reset slices" (cursor only) for the same reason
		// that button is itself separate from "Reset 3D view" (rotation
		// only, lives with the volume panel's own controls) -- window/level
		// is its own independent concern a reader may want to snap back to
		// the volume's native range without also recentering the cursor,
		// same as navigating shouldn't silently undo a window/level a
		// reader deliberately tuned.
		var resetWindowLevelButton = document.createElement('button');
		resetWindowLevelButton.type = 'button';
		resetWindowLevelButton.textContent = 'Reset window/level';
		resetWindowLevelButton.addEventListener('click', resetWindowLevel);
		controls.appendChild(resetWindowLevelButton);
		function resetWindowLevel() {
			linkGroup.setWindow(dataRange.min, dataRange.max);
		}

		var fullscreenButton = document.createElement('button');
		fullscreenButton.type = 'button';
		fullscreenButton.textContent = 'Fullscreen';
		fullscreenButton.addEventListener('click', function () {
			// requestFullscreen()'s promise rejects (without throwing
			// synchronously) if the browser denies the request for any
			// reason -- e.g. no permissions-policy grant in an embedding
			// iframe -- and an un-caught rejection would otherwise surface
			// as a console error for something the user can't act on
			// anyway; there's nothing more useful to do here than leave the
			// button un-toggled, which fullscreenchange (never firing)
			// already achieves on its own.
			if (document.fullscreenElement === container) document.exitFullscreen().catch(function () {});
			else container.requestFullscreen().catch(function () {});
		});
		// Scales mainRow (the grid + volume-view row -- NOT controls/readout,
		// which stay at normal reading size) up to fill whatever extra
		// space Fullscreen actually opens up, via the same transform:scale()
		// technique focus mode uses above -- the panels' own canvases keep
		// their original pixel resolution, just displayed bigger, rather
		// than needing a real higher-resolution re-extraction. Measures
		// mainRow's OWN natural (unscaled) size fresh each call (clearing
		// any previous transform first) rather than caching it, since it can
		// change (a different volume, a different panelSize option) between
		// calls; available space subtracts the readout/toolbar's own
		// height and a little breathing room, not just window.innerHeight,
		// so the scaled-up grid doesn't push them off-screen.
		function rescaleFullscreen() {
			if (document.fullscreenElement !== container) { mainRow.style.transform = ''; return; }
			mainRow.style.transform = '';
			mainRow.style.transformOrigin = 'top left';
			var mainRect = mainRow.getBoundingClientRect();
			var usedHeight = readout.getBoundingClientRect().height + controls.getBoundingClientRect().height;
			var availableWidth = window.innerWidth - 32;
			var availableHeight = window.innerHeight - usedHeight - 48;
			var scale = Math.min(availableWidth / mainRect.width, availableHeight / mainRect.height);
			if (scale > 1) mainRow.style.transform = 'scale(' + scale + ')';
		}
		container.addEventListener('fullscreenchange', function () {
			var isFullscreen = document.fullscreenElement === container;
			fullscreenButton.textContent = isFullscreen ? 'Exit fullscreen' : 'Fullscreen';
			// The container has no background of its own outside fullscreen
			// (it's just whatever page it's embedded in) -- needs one once
			// it IS the whole screen, or it shows through to the OS's own
			// fullscreen backdrop (typically black) instead, which reads as
			// broken rather than intentional.
			container.style.background = isFullscreen ? '#ffffff' : '';
			container.style.overflow = isFullscreen ? 'auto' : '';
			container.style.padding = isFullscreen ? '8px' : '';
			rescaleFullscreen();
		});
		// Only listens while actually in fullscreen (added/removed here,
		// not a permanent window-level listener) -- a fullscreen window can
		// still be resized (moved to a different monitor, OS-level display
		// scaling change), and the scale computed above should track that.
		window.addEventListener('resize', function () { if (document.fullscreenElement === container) rescaleFullscreen(); });
		controls.appendChild(fullscreenButton);

		var mainRow = document.createElement('div');
		mainRow.style.display = 'flex';
		mainRow.style.flexWrap = 'wrap';
		mainRow.style.alignItems = 'flex-start';
		mainRow.style.gap = '12px';
		// A block-level flex container stretches to fill its own containing
		// block's width by default -- it does NOT shrink-wrap to its
		// children the way an inline element would, even though its
		// children (the grid, the volume-view canvas row) only take up
		// their own much smaller natural size and simply left-align
		// within the extra space. That's invisible outside Fullscreen (the
		// leftover space just reads as page margin), but it breaks
		// rescaleFullscreen() below: it measures mainRow's OWN box via
		// getBoundingClientRect(), which -- without this -- would be
		// however wide the surrounding page happens to be, not how wide
		// the actual panels are, making the computed scale far too small.
		// `fit-content` makes mainRow shrink-wrap to its real content
		// width (while still respecting the parent's available width, so
		// flex-wrap still wraps panels onto new rows exactly as before).
		mainRow.style.width = 'fit-content';
		mainRow.style.maxWidth = '100%';
		container.appendChild(mainRow);

		// Focus mode: double-click any panel (a flat 2D one or the volume
		// render) to temporarily view ONLY that one, scaled up to fill most
		// of the viewport; double-click it again (or click the dark
		// backdrop, or Escape) to go back to the full grid. Works the same
		// in and out of Fullscreen -- focusOverlay is `position: fixed`
		// (viewport-relative) and appended directly to `container`, a
		// SIBLING of mainRow rather than a descendant, so mainRow's own
		// fullscreen-fill transform (see the Fullscreen button's own
		// comment below) can't affect it: a `transform` on an ancestor
		// would otherwise make `position:fixed` descendants position
		// relative to THAT ancestor instead of the real viewport, which is
		// exactly the bug reparenting into a sibling avoids. The focused
		// panel's own DOM node is MOVED here (not cloned) so its live
		// canvas content, event listeners, and all keep working untouched
		// -- only a CSS transform: scale(...) is added, then removed again
		// on exit, and the node is moved back to exactly where it came
		// from (recorded via _origParent/_origNext) rather than assuming
		// any fixed position to return it to.
		var focusOverlay = document.createElement('div');
		focusOverlay.style.display = 'none';
		focusOverlay.style.position = 'fixed';
		focusOverlay.style.left = '0';
		focusOverlay.style.top = '0';
		focusOverlay.style.width = '100vw';
		focusOverlay.style.height = '100vh';
		focusOverlay.style.background = 'rgba(0,0,0,0.88)';
		focusOverlay.style.zIndex = '2147483647';
		focusOverlay.style.alignItems = 'center';
		focusOverlay.style.justifyContent = 'center';
		container.appendChild(focusOverlay);

		var focused = null; // { wrap, origParent, origNext, naturalW, naturalH }
		function enterFocus(wrap, naturalW, naturalH) {
			if (focused) exitFocus();
			focused = { wrap: wrap, origParent: wrap.parentNode, origNext: wrap.nextSibling, naturalW: naturalW, naturalH: naturalH };
			focusOverlay.appendChild(wrap);
			resizeFocused();
			focusOverlay.style.display = 'flex';
		}
		function exitFocus() {
			if (!focused) return;
			focused.wrap.style.transform = '';
			if (focused.origNext) focused.origParent.insertBefore(focused.wrap, focused.origNext);
			else focused.origParent.appendChild(focused.wrap);
			focusOverlay.style.display = 'none';
			focused = null;
		}
		// Scales the focused panel to fill most of the viewport (90%
		// wide, 85% tall -- a little short of 100% so the dark backdrop
		// stays visibly a backdrop, not indistinguishable from the panel's
		// own black background) while preserving its own aspect ratio,
		// via a single transform:scale() rather than resizing its actual
		// width/height -- the panel's own canvas keeps rendering at its
		// ORIGINAL pixel resolution throughout (same nearest-neighbor
		// "show exact voxels" choice as always), just displayed bigger,
		// rather than needing a real higher-resolution re-extraction.
		// Re-run on window resize (see its own listener below) so a
		// focused panel keeps filling the viewport if it changes size.
		function resizeFocused() {
			if (!focused) return;
			var scale = Math.min((window.innerWidth * 0.9) / focused.naturalW, (window.innerHeight * 0.85) / focused.naturalH);
			focused.wrap.style.transform = 'scale(' + scale + ')';
		}
		focusOverlay.addEventListener('click', function (evt) { if (evt.target === focusOverlay) exitFocus(); });
		window.addEventListener('keydown', function (evt) { if (evt.key === 'Escape') exitFocus(); });
		window.addEventListener('resize', resizeFocused);

		// Attaches the double-click-to-focus behavior to one panel's own
		// wrap div -- shared by every flat Panel and the one VolumePanel,
		// called right after each is created, below. A dblclick on
		// anything inside wrap (the slice/overlay/frame canvases, or the
		// volume panel's WebGL canvas) still fires its own click/mousedown
		// pair first (that's how dblclick always works), so double-
		// clicking to focus a flat panel also moves the cursor to that
		// point same as a single click would -- a harmless, arguably
		// useful side effect, not something worth suppressing.
		function makeFocusable(wrap, naturalW, naturalH) {
			wrap.addEventListener('dblclick', function () {
				if (focused && focused.wrap === wrap) exitFocus();
				else enterFocus(wrap, naturalW, naturalH);
			});
		}

		// displayAxes: every axis EXCEPT colorAxis, in ascending order --
		// once an axis's own 3 values are being read as R/G/B (see
		// colorAxis's own comment above), that data is already visually
		// present in every color panel; giving it ITS OWN row/column in the
		// pairwise grid too (as a grayscale "channel vs. something" panel)
		// would just be redundant, not a second, independent view of new
		// information the way every other axis pair is. So when colorAxis is
		// set, the grid is built over displayAxes instead of every real
		// axis 0..DIM-1 -- one fewer axis, C(DIM-1,2) panels instead of
		// C(DIM,2), colorAxis never appearing as a plotted dimension
		// anywhere. displayAxes[k]'s own grid column/row is k+1/k (its
		// position WITHIN displayAxes, not its real axis index) -- Panel()
		// itself still uses the real axis index for everything data-related
		// (extraction, cursor, palette color, axis-limit labels), just not
		// for grid placement; see its own comment. With no colorAxis set,
		// displayAxes is just [0..DIM-1] and every one of this reduces back
		// to exactly the un-reduced behavior from before colorAxis existed.
		var displayAxes = [];
		for (var da = 0; da < volume.dim; da++) if (da !== colorAxis) displayAxes.push(da);

		// Explicit per-track sizes (not a single gridAutoColumns/Rows) --
		// column c (1-indexed) is displayAxes[c-1]'s own width, row r is
		// displayAxes[r]'s own height. Column 1 and the LAST row are each an
		// extra, narrow track of their own (OUTER_LABEL_TRACK_PX), not tied
		// to any single axis's own size -- see the outer-edge-label loop
		// below for what lives there -- so every real panel's own column
		// shifts one to the right (di+2, not di+1) to make room, while rows
		// are unaffected (the label row is APPENDED after every real row,
		// not prepended).
		var grid = document.createElement('div');
		var OUTER_LABEL_TRACK_PX = 18;
		var colSizes = [OUTER_LABEL_TRACK_PX + 'px'];
		for (var c = 0; c < displayAxes.length - 1; c++) colSizes.push(perAxisPx[displayAxes[c]] + 'px');
		var rowSizes = [];
		for (var r = 1; r < displayAxes.length; r++) rowSizes.push(perAxisPx[displayAxes[r]] + 'px');
		rowSizes.push(OUTER_LABEL_TRACK_PX + 'px');
		// GridLayout (Component 8, module scope) -- centralizes exactly the
		// track-size/display/gap assignment this file always did inline
		// here; every panel below still self-places into it (Panel()'s own
		// constructor already sets its own wrap's gridColumn/gridRow from
		// the di+2/dj coordinates passed in, so a plain grid.appendChild()
		// is enough -- see GridLayout.prototype.place's own comment on why
		// not every child needs to go through place() itself).
		var gridLayout = new GridLayout(grid, { columns: colSizes, rows: rowSizes });
		gridLayout.mount(mainRow);

		// Every flat panel: a Panel (the actual canvas/frame/crosshair
		// surface, unchanged) wrapped in a Viewport (Component 8) targeting
		// a single shared LocalRenderer -- see this section's own top
		// comment on why LocalRenderer's render() is exactly the same
		// extractSliceU8()/extractSliceRGB() + upload/crosshair code this
		// file always used. Each Viewport joins `linkGroup` (constructed
		// above, alongside the toolbar's own Level/Window control) so a
		// change to any ONE of them -- cursor OR window -- propagates to
		// (and redraws) every other, exactly the old shared `cursor`/
		// `range` closure variables' own behavior.
		var localRenderer = new LocalRenderer(volume, strides, colorAxis);
		var viewports = [];
		var panels = [];
		for (var di = 0; di < displayAxes.length; di++) {
			for (var dj = di + 1; dj < displayAxes.length; dj++) {
				var i = displayAxes[di], j = displayAxes[dj];
				var excludedColors = [];
				for (var k = 0; k < volume.dim; k++)
					if (k !== i && k !== j && k !== colorAxis) excludedColors.push(palette[k % palette.length]);
				var panel = new Panel(i, j, di + 2, dj, perAxisPx[i], perAxisPx[j], palette, excludedColors);
				drawAxisLimitLabels(panel, axisLabels[i], axisLabels[j]);
				grid.appendChild(panel.wrap);
				makeFocusable(panel.wrap, perAxisPx[i], perAxisPx[j]);
				panels.push(panel);
				var w0 = linkGroup.getWindow();
				var vp = new Viewport(panel, localRenderer, { axisI: i, axisJ: j, cursor: linkGroup.getCursor(), windowMin: w0.min, windowMax: w0.max });
				linkGroup.addMember(vp);
				viewports.push(vp);
			}
		}

		// Outer edge labels: column 1 (left of every row's own leftmost
		// panel, di===0 -- always present, see this function's own
		// grid-track comment) gets that row's own axis identity, rotated
		// 90 for space, one per row 1..len-1; the new last row (below every
		// column's own bottom-most panel, dj===len-1 -- likewise always
		// present) gets that column's own axis identity, upright, one per
		// column 2..len. Each panel already carries its own two axes'
		// (min,max) limits directly on itself (drawAxisLimitLabels(), in
		// that axis's own color, bracketed with its index) -- this is a
		// DIFFERENT thing: which axis a whole row/column IS, once per
		// row/column rather than repeated on every panel that happens to
		// border the edge, the way a plot's own outer axis label sits once
		// outside a grid of subplots rather than on each one.
		// Explicit calibration (volume.unit[axis], from ndl::VoxelSpacing --
		// see this file's own top comment) wins when present; otherwise
		// falls back to "voxels" rather than showing no unit at all (an
		// uncalibrated axis is still measured in SOMETHING: index positions
		// along it) -- shown here, next to the axis's own identity label,
		// rather than smeared into the per-panel corner range numbers
		// (drawAxisLimitLabels()/axisLimitLabel(), deliberately left plain).
		// axis here is always one of displayAxes (see this function's own
		// caller, the loops below), which by construction never includes
		// colorAxis, so there's no "channels" case to default to here --
		// colorAxis's own identity isn't shown as a separate axis anywhere
		// once it's set (see displayAxes's own comment above).
		function outerAxisLabelText(axis) {
			var unit = (volume.unit && volume.unit[axis]) ? volume.unit[axis] : 'voxels';
			return '[' + axis + '] ' + unit;
		}
		function makeOuterLabel(axis, gridCol, gridRow, rotate) {
			var cell = document.createElement('div');
			cell.style.gridColumn = String(gridCol);
			cell.style.gridRow = String(gridRow);
			cell.style.display = 'flex';
			cell.style.alignItems = 'center';
			cell.style.justifyContent = 'center';
			cell.style.overflow = 'hidden';
			var text = document.createElement('span');
			text.textContent = outerAxisLabelText(axis);
			text.style.fontFamily = 'monospace';
			text.style.fontSize = '10px';
			text.style.color = palette[axis % palette.length];
			text.style.whiteSpace = 'nowrap';
			if (rotate) text.style.transform = 'rotate(-90deg)';
			cell.appendChild(text);
			grid.appendChild(cell);
		}
		for (var lr = 1; lr < displayAxes.length; lr++) makeOuterLabel(displayAxes[lr], 1, lr, true);
		for (var lc = 0; lc < displayAxes.length - 1; lc++) makeOuterLabel(displayAxes[lc], lc + 2, displayAxes.length, false);

		// Every panel is re-sliced and redrawn on any cursor OR window
		// change, rather than only the panels sharing the moved axes --
		// simpler and, for the C(DIM,2) panel counts a pairwise-view grid
		// actually has (e.g. 6 for DIM=4, 10 for DIM=5), cheap enough that
		// the selective-redraw optimization isn't worth the extra
		// bookkeeping. This is no longer a separate redrawPanel()/
		// redrawAll() pair: `linkGroup`'s own rebroadcast (Component 8,
		// module scope -- LinkGroup.prototype.addMember) already calls
		// every member Viewport's own redraw() -- which calls
		// LocalRenderer.render(), the exact same extraction+upload+
		// crosshair code redrawPanel() used to run inline -- whenever
		// setCursor()/setWindow() changes ANY member (in practice, always
		// `cursorState`, the group's own hub -- see this function's own
		// top comment on why). No separate "redraw all" call is needed
		// anywhere below; every setCursor()/setWindow() call already does.

		// ---- The one 3D volume-render panel (see VolumePanel()'s own
		// top comment for why exactly one, not one per axis-triple). Placed
		// directly IN the pairwise-slice grid, at the (row 1, column 3)
		// cell -- the cell right after the (outer-label track, then the
		// first real) column's own panel -- rather than as a separate
		// element beside the grid: that cell is always empty in the
		// ordinary panel layout (column 1 is the outer-label track, not a
		// panel column at all -- see this function's own grid-track
		// comment above -- and a real panel only exists for di<dj, so
		// column 3/row 1 would need di===dj===1, which never happens), and
		// its own track sizes (colSizes[2] for the column, rowSizes[0] for
		// the row -- both perAxisPx[displayAxes[1]]) already come out
		// exactly square, so the volume panel is sized to match (not
		// maxPanelPx) to fill that cell exactly the same way every other
		// panel fills its own -- ONLY when displayAxes has at least 3 axes,
		// i.e. there are at least 2 real panel columns for column 3 to
		// actually exist in; with colorAxis removing an axis from the grid entirely
		// (see displayAxes's own comment above), a small enough DIM can
		// leave too few displayed axes for this trick, in which case the
		// volume panel instead falls back to living beside the grid rather
		// than in a cell that doesn't exist (see volumeInGrid's own use
		// below). volAxes picks which 3 of the volume's DIM axes it
		// currently shows -- fixed at (0,1,2) with no picker UI when DIM===3
		// (there's only one possible choice); a 3-dropdown picker when
		// DIM>3. colorAxis is never a valid 3D-view axis (its own 3 values
		// are what makes the render color in the first place -- see
		// extractVolumeBlockRGB()'s own comment -- not a 4th spatial
		// dimension to pick among), so with colorAxis set, "displayAxes has
		// at least 3 axes" is doing double duty: the same
		// volumeInGrid===true condition that gates the in-grid placement
		// trick above is ALSO exactly "are there at least 3 non-color axes
		// to render a volume from at all" -- reused below as the volume
		// panel's own existence condition, in place of the colorAxis-blind
		// volume.dim>=3 check this used before colorAxis existed. null
		// throughout this whole block whenever that's false or the browser
		// has no WebGL2, in which case none of it runs. ----
		var volumePanel = null, volAxes = colorAxis >= 0 ? displayAxes.slice(0, 3) : [0, 1, 2];
		var volumeInGrid = displayAxes.length >= 3;
		if (colorAxis >= 0 ? volumeInGrid : volume.dim >= 3) volumePanel = new VolumePanel(volumeInGrid ? perAxisPx[displayAxes[1]] : maxPanelPx, palette, colorAxis >= 0);
		if (volumePanel) {
			var axisSelects = [];

			// Canvas beside its own sliders (a row), not above/below them --
			// see this function's own top-level layout comment on keeping
			// vertical space down. Rotation: 3 sliders (one per rotation
			// plane, XY/XZ/YZ, i.e. standard Euler angles) in degrees,
			// converted to radians for VolumePanel's own angleX/Y/Z, plus
			// one alpha (opacity scale) slider -- see VOLUME_FRAGMENT_SRC's
			// own comment on how alpha differs from the shared window/level.
			// Left-click-drag on the canvas adjusts the same two
			// underlying rotation angles (yaw from horizontal drag, pitch
			// from vertical) and keeps the sliders in sync, rather than the
			// drag and the sliders being two independent representations
			// that could drift apart; right-click(-drag) instead moves the
			// shared cursor (see attachVolumeInteraction() below).
			var volCanvasRow = document.createElement('div');
			volCanvasRow.style.display = 'flex';
			volCanvasRow.style.gap = '8px';
			volCanvasRow.style.alignItems = 'flex-start';
			if (volumeInGrid) {
				volCanvasRow.style.gridColumn = '3'; // 3, not 2: column 1 is the outer left-edge label track (see this function's own grid-track comment above), column 2 is the first real panel column, so "the cell right after it" is 3
				volCanvasRow.style.gridRow = '1';
			}
			// This flex row's own grid cell is exactly one track wide
			// (perAxisPx[displayAxes[1]], same as the canvas -- see this block's own top
			// comment), narrower than canvas+controls combined, so without
			// flex-shrink:0 the flex algorithm would shrink volumePanel.wrap
			// to fit -- but wrap's own children (the WebGL canvas + overlay)
			// are position:absolute at their own fixed pixel size, which
			// doesn't shrink with a shrunk parent, so the canvas would keep
			// rendering at full size while its now-narrower wrap div (and
			// everything positioned after it in the flex row) reflows
			// underneath/behind it -- invisible controls, not a resized
			// canvas. flex-shrink:0 on both children instead lets the row
			// overflow its grid cell to the right (into empty page space
			// past the grid's last column for the common DIM=3 case; some
			// visual overlap with column 3 for DIM>3, an accepted tradeoff
			// for now) rather than silently breaking either one.
			volumePanel.wrap.style.flexShrink = '0';
			volCanvasRow.appendChild(volumePanel.wrap);
			makeFocusable(volumePanel.wrap, volumePanel.size, volumePanel.size);

			var rotSection = document.createElement('div');
			rotSection.style.fontFamily = 'monospace';
			rotSection.style.fontSize = '0.85em';
			rotSection.style.flexShrink = '0';

			// The axis picker lives here, with the volume panel's OTHER
			// controls (rotation, alpha, reset), rather than as its own row
			// above the canvas -- it's one more control governing what the
			// volume panel shows, same as the rest of rotSection. Shown
			// only when there's an actual CHOICE to make -- more than 3
			// axes to pick 3 from -- which is displayAxes.length>3, not
			// volume.dim>3: colorAxis (when set) is never offered as an
			// option (see volAxes' own comment above), so e.g. a 4D
			// {channel,W,H,time} color volume has exactly one possible
			// choice (W,H,time) despite DIM being 4, same as an ordinary
			// (non-color) 3D volume having exactly one choice with no
			// picker shown at all.
			if (displayAxes.length > 3) {
				var axisPickerRow = document.createElement('div');
				axisPickerRow.style.marginBottom = '4px';
				['X', 'Y', 'Z'].forEach(function (label, idx) {
					var span = document.createElement('span');
					span.textContent = label + ':';
					axisPickerRow.appendChild(span);
					var select = document.createElement('select');
					for (var dax = 0; dax < displayAxes.length; dax++) {
						var ax = displayAxes[dax];
						var opt = document.createElement('option');
						opt.value = String(ax);
						opt.textContent = String(ax);
						if (ax === volAxes[idx]) opt.selected = true;
						select.appendChild(opt);
					}
					select.style.marginRight = '0.75em';
					select.addEventListener('change', function () {
						// Swap with whichever OTHER selector currently holds
						// the newly-picked axis, rather than allowing two
						// selectors to both point at the same axis (which
						// would make the "3rd" axis of the sub-block
						// degenerate -- extent 1, not a real volume).
						var newAxis = parseInt(select.value, 10);
						var clashIdx = volAxes.indexOf(newAxis);
						if (clashIdx !== -1 && clashIdx !== idx) {
							volAxes[clashIdx] = volAxes[idx];
							axisSelects[clashIdx].value = String(volAxes[idx]);
						}
						volAxes[idx] = newAxis;
						refreshRotationLabels();
						updateVolumeTexture();
					});
					axisSelects.push(select);
					axisPickerRow.appendChild(select);
				});
				rotSection.appendChild(axisPickerRow);
			}
			// Each rotation slider's label is just "[<axis>]" -- the real
			// volume axis currently assigned to that slider's own FIXED
			// rotation-plane/gizmo-line slot (see
			// VolumePanel.prototype.drawCrosshair's own gizmo comment), i.e.
			// volAxes[slot] -- plus a color swatch matching that axis's own
			// crosshair/frame-ring color. Deliberately the actual axis
			// index, not an X/Y/Z role letter: every other axis identity in
			// this viewer (panel corner labels, frame-ring arcs, the gizmo
			// itself) already names axes by their real index number, and an
			// arbitrary role letter with no meaning anywhere else in the
			// viewer was one axis-naming convention too many. `role` is
			// still tracked internally (ctl.role, passed into
			// makeRotationSlider() below) purely as bookkeeping for which
			// Euler angle/rotation-plane each slider actually drives --
			// it's just not shown. Both the bracketed number and the swatch
			// are recomputed by refreshRotationLabels() (below) whenever
			// the DIM>3 axis picker reassigns a slot, so a slider always
			// names and colors the axis it ACTUALLY rotates.
			function axisLabelText(role, slot) { return '[' + volAxes[slot] + ']'; }
			function axisLabelColor(slot) { return palette[volAxes[slot] % palette.length]; }
			function makeRotationSlider(role, slot, setAngle) {
				var ctl = makeSlider(axisLabelText(role, slot), -180, 180, 1, 0, 0, '°', function (deg, setValue) {
					setAngle(deg * Math.PI / 180);
					setValue(deg);
					renderVolumeOnly();
				}, axisLabelColor(slot));
				ctl.row.style.display = 'block';
				ctl.role = role;
				ctl.slot = slot;
				rotSection.appendChild(ctl.row);
				return ctl;
			}
			var rotXCtl = makeRotationSlider('X', 0, function (rad) { volumePanel.angleX = rad; });
			var rotYCtl = makeRotationSlider('Y', 1, function (rad) { volumePanel.angleY = rad; });
			var rotZCtl = makeRotationSlider('Z', 2, function (rad) { volumePanel.angleZ = rad; });
			function refreshRotationLabels() {
				[rotXCtl, rotYCtl, rotZCtl].forEach(function (ctl) {
					ctl.setLabel(axisLabelText(ctl.role, ctl.slot), axisLabelColor(ctl.slot));
				});
			}
			var alphaCtl = makeSlider('Alpha', 0, 100, 1, DEFAULT_ALPHA_SCALE * 100, 0, '%', function (pct, setValue) {
				volumePanel.alphaScale = pct / 100;
				setValue(pct);
				renderVolumeOnly();
			});
			alphaCtl.row.style.display = 'block';
			rotSection.appendChild(alphaCtl.row);
			// Separate from "Reset 3D view" (rotation only, above) for the
			// same reason that button is itself separate from "Reset
			// slices"/"Reset window/level" -- alpha is its own independent
			// display concern, and a reader may want to snap it back to the
			// default without also losing whatever rotation they've dialed
			// in.
			var resetAlphaButton = document.createElement('button');
			resetAlphaButton.type = 'button';
			resetAlphaButton.textContent = 'Reset alpha';
			resetAlphaButton.addEventListener('click', function () {
				volumePanel.alphaScale = DEFAULT_ALPHA_SCALE;
				alphaCtl.setValue(DEFAULT_ALPHA_SCALE * 100);
				renderVolumeOnly();
			});
			rotSection.appendChild(resetAlphaButton);
			volCanvasRow.appendChild(rotSection);
			if (volumeInGrid) grid.appendChild(volCanvasRow);
			else mainRow.appendChild(volCanvasRow);

			var DEG_PER_PIXEL = 0.5;
			function wrapDegrees(d) { return ((d + 180) % 360 + 360) % 360 - 180; }
			function setRotationDeg(ctl, deg) {
				deg = wrapDegrees(deg);
				ctl.slider.value = String(deg);
				ctl.slider.dispatchEvent(new Event('input'));
			}

			// Right-click(-drag): move the shared cursor to wherever the
			// click projects to under the CURRENT rotation, preserving
			// depth (the component of the cursor's own position along the
			// current view axis) -- i.e. only the two in-plane degrees of
			// freedom move, exactly like clicking a flat panel only moves
			// that panel's own two axes. Left-click-drag: rotate.
			// contextmenu is suppressed so a right-click drag doesn't also
			// pop the browser's own menu.
			function navigateFromEvent(evt) {
				var rect = volumePanel.canvas.getBoundingClientRect();
				// vScreen space, matching VOLUME_VERTEX_SRC's own flipped Y
				// (see its comment) -- NOT raw clip space, since that's what
				// the rotation matrix's inverse (mat3TransposeMultiplyVec3)
				// needs to land back in the same space drawCrosshair() and
				// the shader both use.
				var vsX = ((evt.clientX - rect.left) / rect.width) * 2 - 1;
				var vsY = ((evt.clientY - rect.top) / rect.height) * 2 - 1;
				var R = volumePanel.rotationMatrix();
				var sharedCursor = linkGroup.getCursor();
				var localCursor = volAxes.map(function (ax) { return ((sharedCursor[ax] + 0.5) / volume.extent[ax]) * 2 - 1; });
				var viewCursor = mat3TransposeMultiplyVec3(R, localCursor);
				var newLocal = mat3MultiplyVec3(R, [vsX, vsY, viewCursor[2]]); // keep depth (view-space z), only replace the in-plane (x,y)
				var next = sharedCursor.slice();
				volAxes.forEach(function (ax, k) {
					var idx = Math.round(((newLocal[k] + 1) / 2) * volume.extent[ax] - 0.5);
					next[ax] = Math.max(0, Math.min(volume.extent[ax] - 1, idx));
				});
				setCursor(next);
			}
			// Scroll wheel: move the cursor along the CURRENT view axis --
			// whatever depth the current rotation happens to be looking
			// down, exactly like scrolling through a stack of ordinary
			// axial slices, just generalized to an arbitrary rotation
			// instead of a fixed axis. The mirror image of
			// navigateFromEvent() above: that one keeps view-space z (depth)
			// fixed and replaces the in-plane (x,y) from a click position;
			// this keeps the in-plane (x,y) fixed and steps view-space z by
			// one voxel's worth of normalized depth (2/extent, since local
			// space spans [-1,1] across `extent` voxels) per wheel tick.
			function scrollVolumeDepth(evt) {
				evt.preventDefault();
				var direction = evt.deltaY > 0 ? 1 : -1;
				var R = volumePanel.rotationMatrix();
				var sharedCursor = linkGroup.getCursor();
				var localCursor = volAxes.map(function (ax) { return ((sharedCursor[ax] + 0.5) / volume.extent[ax]) * 2 - 1; });
				var viewCursor = mat3TransposeMultiplyVec3(R, localCursor);
				var depthStep = 2 / Math.max(volume.extent[volAxes[0]], volume.extent[volAxes[1]], volume.extent[volAxes[2]]);
				viewCursor[2] = Math.max(-1, Math.min(1, viewCursor[2] + direction * depthStep));
				var newLocal = mat3MultiplyVec3(R, viewCursor);
				var next = sharedCursor.slice();
				volAxes.forEach(function (ax, k) {
					var idx = Math.round(((newLocal[k] + 1) / 2) * volume.extent[ax] - 0.5);
					next[ax] = Math.max(0, Math.min(volume.extent[ax] - 1, idx));
				});
				setCursor(next);
			}
			volumePanel.canvas.addEventListener('wheel', scrollVolumeDepth, { passive: false });
			// Mouse-hover readout for the volume view: the SUM of every
			// voxel's own raw value along the ray through the volume at the
			// hovered screen pixel -- built from the EXACT same rayDir/
			// rayOrigin construction as VOLUME_FRAGMENT_SRC's own ray march
			// (same uRotation*vec3(...) convention, mat3MultiplyVec3 here
			// standing in for GLSL's `uRotation * vec3(...)`), just
			// accumulating the underlying DATA instead of a rendered/
			// composited color -- so it reads as a genuine "how much is
			// behind this pixel" projection total, independent of window/
			// alpha (which only affect what's drawn, not what's summed).
			// Steps enough times to land roughly one sample per voxel along
			// the volume's own longest displayed axis (NOT
			// VOLUME_FRAGMENT_SRC's fixed 220 -- that constant is a render-
			// quality knob, unrelated to the data's actual resolution), and
			// dedupes consecutive samples landing in the same voxel (a
			// shallow ray angle would otherwise cross a voxel many times
			// and count it that many times over) so each voxel touched
			// contributes to the sum exactly once.
			function raySumAtEvent(evt) {
				var rect = volumePanel.canvas.getBoundingClientRect();
				var vsX = ((evt.clientX - rect.left) / rect.width) * 2 - 1;
				var vsY = ((evt.clientY - rect.top) / rect.height) * 2 - 1;
				var R = volumePanel.rotationMatrix();
				var rayDir = mat3MultiplyVec3(R, [0, 0, 1]);
				var rayOrigin = mat3MultiplyVec3(R, [vsX, vsY, 0]);
				var maxExtent = Math.max(volume.extent[volAxes[0]], volume.extent[volAxes[1]], volume.extent[volAxes[2]]);
				var steps = Math.max(1, Math.round(maxExtent * Math.sqrt(3)));
				var tStep = 2 * Math.sqrt(3) / steps;
				var pos = [
					rayOrigin[0] - rayDir[0] * Math.sqrt(3),
					rayOrigin[1] - rayDir[1] * Math.sqrt(3),
					rayOrigin[2] - rayDir[2] * Math.sqrt(3)
				];
				var sums = colorAxis >= 0 ? [0, 0, 0] : [0];
				var voxelCount = 0, lastKey = null;
				for (var i = 0; i < steps; i++, pos[0] += rayDir[0] * tStep, pos[1] += rayDir[1] * tStep, pos[2] += rayDir[2] * tStep) {
					if (pos[0] < -1 || pos[0] > 1 || pos[1] < -1 || pos[1] > 1 || pos[2] < -1 || pos[2] > 1) continue;
					var idx = [0, 0, 0], key = '';
					for (var k = 0; k < 3; k++) {
						var extent = volume.extent[volAxes[k]];
						idx[k] = Math.max(0, Math.min(extent - 1, Math.floor((pos[k] * 0.5 + 0.5) * extent)));
						key += idx[k] + ',';
					}
					if (key === lastKey) continue;
					lastKey = key;
					voxelCount++;
					var voxel = linkGroup.getCursor().slice();
					volAxes.forEach(function (ax, k2) { voxel[ax] = idx[k2]; });
					if (colorAxis >= 0) {
						// Same "sum each of the 3 native channels separately"
						// convention showValue() uses for a hovered color
						// panel -- one running total per channel, not a
						// single collapsed luminance number.
						for (var ch = 0; ch < 3; ch++) {
							var cVoxel = voxel.slice();
							cVoxel[colorAxis] = ch;
							var cOffset = 0;
							for (var k3 = 0; k3 < volume.dim; k3++) cOffset += cVoxel[k3] * strides[k3];
							sums[ch] += volume.data[cOffset];
						}
					} else {
						var offset = 0;
						for (var k4 = 0; k4 < volume.dim; k4++) offset += voxel[k4] * strides[k4];
						sums[0] += volume.data[offset];
					}
				}
				return { sums: sums, voxelCount: voxelCount };
			}
			volumePanel.canvas.addEventListener('mousemove', function (evt) {
				var result = raySumAtEvent(evt);
				if (result.voxelCount === 0) { readoutVoxel.textContent = ''; readoutPhysical.textContent = ''; return; }
				var text = result.sums.map(function (s) { return formatValue(s, valueDecimals, valueIsFloat); }).join(', ');
				readoutVoxel.textContent = 'ray sum (' + volAxes.join(',') + ') = ' + text + ' [' + result.voxelCount + ' voxels]';
				readoutPhysical.textContent = '';
			});
			volumePanel.canvas.addEventListener('mouseleave', function () { readoutVoxel.textContent = ''; readoutPhysical.textContent = ''; });
			(function attachVolumeInteraction() {
				var dragMode = null, lastX = 0, lastY = 0; // dragMode: null | 'rotate' | 'navigate'
				volumePanel.canvas.addEventListener('contextmenu', function (evt) { evt.preventDefault(); });
				volumePanel.canvas.addEventListener('mousedown', function (evt) {
					lastX = evt.clientX; lastY = evt.clientY;
					if (evt.button === 0) {
						dragMode = 'rotate';
						volumePanel.canvas.style.cursor = 'grabbing';
					} else if (evt.button === 2) {
						dragMode = 'navigate';
						navigateFromEvent(evt);
					}
				});
				window.addEventListener('mousemove', function (evt) {
					if (dragMode === 'rotate') {
						var dx = evt.clientX - lastX, dy = evt.clientY - lastY;
						lastX = evt.clientX; lastY = evt.clientY;
						setRotationDeg(rotYCtl, parseFloat(rotYCtl.slider.value) + dx * DEG_PER_PIXEL);
						setRotationDeg(rotXCtl, parseFloat(rotXCtl.slider.value) + dy * DEG_PER_PIXEL);
					} else if (dragMode === 'navigate') {
						navigateFromEvent(evt);
					}
				});
				window.addEventListener('mouseup', function () {
					if (dragMode === 'rotate') volumePanel.canvas.style.cursor = 'grab';
					dragMode = null;
				});
			})();

			function resetVolume() {
				setRotationDeg(rotXCtl, 0); setRotationDeg(rotYCtl, 0); setRotationDeg(rotZCtl, 0);
			}
			var resetVolumeButton = document.createElement('button');
			resetVolumeButton.type = 'button';
			resetVolumeButton.textContent = 'Reset 3D view';
			resetVolumeButton.style.display = 'block';
			resetVolumeButton.style.marginTop = '4px';
			resetVolumeButton.addEventListener('click', resetVolume);
			rotSection.appendChild(resetVolumeButton);
		}

		// The shared cursor's own volAxes coordinates, normalized to
		// [-1,1] -- the local-space point drawVolumeCrosshair() (and
		// navigateFromEvent() above) project through the current rotation.
		function volumeLocalCursor() {
			var sharedCursor = linkGroup.getCursor();
			return volAxes.map(function (ax) { return ((sharedCursor[ax] + 0.5) / volume.extent[ax]) * 2 - 1; });
		}
		function updateVolumeCrosshair() {
			if (!volumePanel) return;
			var axisColors = volAxes.map(function (ax) { return palette[ax % palette.length]; });
			volumePanel.drawCrosshair(volumeLocalCursor(), axisColors, volAxes);
		}

		function updateVolumeTexture() {
			if (!volumePanel) return;
			var sharedCursor = linkGroup.getCursor(), sharedWindow = linkGroup.getWindow();
			var block = colorAxis >= 0
				? extractVolumeBlockRGB(volume, strides, volAxes[0], volAxes[1], volAxes[2], colorAxis, sharedCursor, sharedWindow.min, sharedWindow.max)
				: extractVolumeBlock(volume, strides, volAxes[0], volAxes[1], volAxes[2], sharedCursor, sharedWindow.min, sharedWindow.max);
			volumePanel.uploadBlock(block);
			volumePanel.render();
			updateVolumeCrosshair();
		}
		function renderVolumeOnly() {
			if (!volumePanel) return;
			volumePanel.render();
			updateVolumeCrosshair();
		}
		// The volume panel is deliberately NOT part of the Viewport/
		// LinkGroup abstraction (see this file's own top comment on
		// Component 8's explicit scope boundary) -- so it needs its own
		// explicit hook to stay in sync with the shared cursor/window
		// `linkGroup` owns. One subscription here replaces every
		// old individual updateVolumeTexture() call this file used to make
		// inline inside setCursor()/refreshWindow(): linkGroup's own
		// paramsChanged (see LinkGroup.prototype.addMember) fires exactly
		// once per linkGroup.setCursor()/setWindow() call, regardless of
		// which code triggered it (a flat panel click, a reset button, the
		// volume panel's own navigateFromEvent/scrollVolumeDepth), so this
		// one listener is the single source of truth for "keep the volume
		// panel's own texture/crosshair in sync," not a duplicated call at
		// every mutation site.
		linkGroup.on('paramsChanged', function () { updateVolumeTexture(); });

		// Reset slices: cursor only, i.e. exactly what a 2D panel click
		// moves. Reset 3D view (resetVolume(), above): rotation only, i.e.
		// exactly what dragging/rotating the volume panel changes. Reset
		// window/level (resetWindowLevel(), above): the shared intensity
		// mapping only. Each of these three touches exactly one concern and
		// leaves the other two alone -- resetting the cursor shouldn't
		// silently undo a window/level a reader deliberately tuned, any
		// more than resetting window/level should recenter the cursor or
		// snap the volume's rotation back; alpha (opacity) isn't reset by
		// ANY of the three, since it's paired with rotation/axis-picking as
		// "how the 3D view currently looks," not navigation.
		function resetSlices() {
			setCursor(defaultCursor);
		}

		// Clamps into range (every caller below already pre-clamps its own
		// computed coordinate, but this stays as the same defensive final
		// check the old code always had -- cheap, and correct for any
		// FUTURE caller that doesn't happen to pre-clamp), then hands the
		// clamped array to linkGroup.setCursor() -- which is what actually
		// redraws every flat panel (via LinkGroup's own rebroadcast) and,
		// via this function's own subscription above, the volume panel too.
		function setCursor(newCursor) {
			var clamped = new Array(volume.dim);
			for (var k = 0; k < volume.dim; k++)
				clamped[k] = Math.max(0, Math.min(volume.extent[k] - 1, Math.round(newCursor[k])));
			linkGroup.setCursor(clamped);
		}

		// Hovered-voxel data coordinates from a mouse event within panel's
		// overlay -- shared by moveTo() (click/drag navigation) and
		// showValue() (readout on plain hover, no click needed) so both
		// agree on exactly which voxel the mouse is over.
		function dataCoordsFromEvent(panel, evt) {
			var rect = panel.overlay.getBoundingClientRect();
			var px = (evt.clientX - rect.left) / rect.width;
			var py = (evt.clientY - rect.top) / rect.height;
			return {
				i: Math.max(0, Math.min(volume.extent[panel.axisI] - 1, Math.floor(px * volume.extent[panel.axisI]))),
				j: Math.max(0, Math.min(volume.extent[panel.axisJ] - 1, Math.floor(py * volume.extent[panel.axisJ])))
			};
		}

		// The full N-D voxel under the mouse in `panel`: its own two axes at
		// (dataI, dataJ), every other axis at the shared cursor's current
		// value -- exactly the voxel that panel's slice is showing at that
		// pixel.
		function showValue(panel, dataI, dataJ) {
			var voxel = linkGroup.getCursor().slice();
			voxel[panel.axisI] = dataI;
			voxel[panel.axisJ] = dataJ;
			var valueText;
			// Same "is this a color panel" test redrawPanel() uses -- a
			// hovered color panel shows all 3 native channel values
			// (R, G, B) rather than the single value at whatever channel
			// index cursor[colorAxis] happens to currently be, since the
			// panel itself is showing all 3 composited together, not one.
			if (colorAxis >= 0 && panel.axisI !== colorAxis && panel.axisJ !== colorAxis) {
				var rgbText = [];
				for (var ch = 0; ch < 3; ch++) {
					var cVoxel = voxel.slice();
					cVoxel[colorAxis] = ch;
					var cOffset = 0;
					for (var k1 = 0; k1 < volume.dim; k1++) cOffset += cVoxel[k1] * strides[k1];
					rgbText.push(formatValue(volume.data[cOffset], valueDecimals, valueIsFloat));
				}
				valueText = rgbText.join(', ');
			} else {
				var offset = 0;
				for (var k = 0; k < volume.dim; k++) offset += voxel[k] * strides[k];
				valueText = formatValue(volume.data[offset], valueDecimals, valueIsFloat);
			}
			readoutVoxel.textContent = 'voxel (' + voxel.join(', ') + ') = ' + valueText;
			if (volume.spacing) {
				var phys = new Array(volume.dim);
				for (var k2 = 0; k2 < volume.dim; k2++) {
					var coord = formatPhysical(voxel[k2] * volume.spacing[k2]);
					phys[k2] = volume.unit[k2] ? coord + volume.unit[k2] : coord;
				}
				readoutPhysical.textContent = 'physical (' + phys.join(', ') + ')';
			}
		}

		// The scroll-wheel target for a flat panel: the first (lowest real
		// axis index, among displayAxes -- so colorAxis is never a
		// candidate, same reasoning as everywhere else it's excluded)
		// axis that ISN'T one of the panel's own two plotted axes -- e.g.
		// for the classic DIM=3 case a panel excludes exactly one other
		// axis, so this is unambiguous, the same "scroll to change slice"
		// convention every clinical viewer uses; for DIM>3 a panel
		// excludes several, and this picks one deterministically (lowest
		// index) rather than trying to scroll all of them at once or
		// requiring the reader to pick. Returns -1 if there's no other
		// axis left to scroll (e.g. DIM=2, or colorAxis leaves nothing).
		function scrollAxisFor(panel) {
			for (var k = 0; k < displayAxes.length; k++) {
				var ax = displayAxes[k];
				if (ax !== panel.axisI && ax !== panel.axisJ) return ax;
			}
			return -1;
		}
		function attachInteraction(panel) {
			var dragging = false;
			function moveTo(evt) {
				var c = dataCoordsFromEvent(panel, evt);
				var next = linkGroup.getCursor().slice();
				next[panel.axisI] = c.i;
				next[panel.axisJ] = c.j;
				setCursor(next);
			}
			panel.overlay.addEventListener('mousedown', function (evt) { dragging = true; moveTo(evt); });
			panel.overlay.addEventListener('mousemove', function (evt) {
				var c = dataCoordsFromEvent(panel, evt);
				showValue(panel, c.i, c.j);
			});
			panel.overlay.addEventListener('mouseleave', function () { readoutVoxel.textContent = ''; readoutPhysical.textContent = ''; });
			window.addEventListener('mousemove', function (evt) { if (dragging) moveTo(evt); });
			window.addEventListener('mouseup', function () { dragging = false; });

			var scrollAxis = scrollAxisFor(panel);
			if (scrollAxis >= 0) {
				panel.overlay.addEventListener('wheel', function (evt) {
					evt.preventDefault();
					var direction = evt.deltaY > 0 ? 1 : -1;
					var sharedCursor = linkGroup.getCursor();
					var next = sharedCursor.slice();
					next[scrollAxis] = Math.max(0, Math.min(volume.extent[scrollAxis] - 1, sharedCursor[scrollAxis] + direction));
					setCursor(next);
				}, { passive: false });
			}
		}
		for (var p = 0; p < panels.length; p++) attachInteraction(panels[p]);

		// Initial render: every Viewport already holds correct cursor/
		// window values (each was constructed from linkGroup's own current
		// state, right before joining it -- see the panel-creation loop
		// above), but construction itself never draws anything (Viewport
		// has no auto-render-on-construct -- see its own comment), so this
		// is the one explicit "draw everything for the first time" pass,
		// the direct counterpart to the old redrawAll() call this replaced.
		viewports.forEach(function (vp) { vp.redraw(); });
		updateVolumeTexture();

		return {
			get cursor() { return linkGroup.getCursor(); },
			setCursor: setCursor,
			destroy: function () { container.removeChild(mainRow); container.removeChild(controls); container.removeChild(readout); }
		};
	}

	global.NDLViewer = {
		create: create,
		parseVolume: parseVolume, // exposed for testing/inspection
		// Component 8's own composable framework -- see this file's own
		// top-of-section comment (right before create()) for the full
		// design. Exposed so a live page (apps/live_video_stream.html,
		// apps/bouncing_donut.html) can build its own Viewport(s) on top
		// of RemoteRenderer/RemoteConnection instead of hand-rolling its
		// own WebSocket/canvas plumbing.
		Viewport: Viewport,
		LinkGroup: LinkGroup,
		LocalRenderer: LocalRenderer,
		RemoteRenderer: RemoteRenderer,
		RemoteConnection: RemoteConnection,
		Controls: Controls,
		GridLayout: GridLayout,
		FreeformLayout: FreeformLayout,
		// The small fixed-position X/Y/Z orientation gizmo VolumePanel's
		// own drawCrosshair() (static viewer) draws in its corner --
		// exposed directly since it's already fully generic (just a 2D
		// context, a flat column-major rotation matrix, and per-axis
		// colors/labels, no VolumePanel-specific state), so a live
		// "volume"-op page (apps/bouncing_donut.html) can draw the same
		// rotating orientation indicator against its own Viewport.rotation
		// without re-deriving the same trig/projection a second time.
		drawAxisGizmo: drawAxisGizmo,
		DEFAULT_PALETTE: DEFAULT_PALETTE
	};
})(typeof window !== 'undefined' ? window : this);
