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
// color -- the same way a matplotlib subplot shows its own axis limits,
// rather than a separate legend a reader has to cross-reference against
// the panels. The one thing that has no single axis/panel to live on, the
// volume's own native value range, sits as a static line above the hover
// readout instead.
//
// Click or drag inside any panel to move the cursor: the two axes that
// panel shows update to the clicked position, and every panel is
// redrawn -- panels sharing one of those two axes get a new slice (their
// fixed coordinate along that axis changed); every panel's crosshair
// overlay is redrawn to the new cursor position projected onto its own
// two axes. This generalizes the classic synchronized axial/coronal/
// sagittal medical-image viewer (click in one plane, the other two jump
// to that position) to arbitrary N.
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
	// on-panel rather than a separate legend.
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
	// one glance away from the image itself.
	function drawAxisLimitLabels(panel, labelI, labelJ) {
		var ctx = panel.frame.getContext('2d');
		var w = panel.width, h = panel.height;
		ctx.font = '10px monospace';
		ctx.textBaseline = 'alphabetic';

		ctx.fillStyle = panel.colorI;
		ctx.textAlign = 'left';
		ctx.fillText(labelI.min, 3, h - 3);
		ctx.textAlign = 'right';
		ctx.fillText(labelI.max, w - 3, h - 3);

		ctx.fillStyle = panel.colorJ;
		ctx.textAlign = 'left';
		ctx.fillText(labelJ.min, 3, 11);
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
	function Panel(axisI, axisJ, width, height, palette, excludedColors) {
		this.axisI = axisI;
		this.axisJ = axisJ;
		this.colorI = palette[axisI % palette.length];
		this.colorJ = palette[axisJ % palette.length];

		var wrap = document.createElement('div');
		wrap.style.position = 'relative';
		wrap.style.width = width + 'px';
		wrap.style.height = height + 'px';
		wrap.style.gridColumn = String(axisI + 1);
		wrap.style.gridRow = String(axisJ);

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

	Panel.prototype.uploadSlice = function (pixels, width, height) {
		if (this.sliceOffscreen.width !== width || this.sliceOffscreen.height !== height) {
			this.sliceOffscreen.width = width;
			this.sliceOffscreen.height = height;
		}
		var offCtx = this.sliceOffscreen.getContext('2d');
		var imgData = offCtx.createImageData(width, height);
		var data = imgData.data;
		for (var i = 0, p = 0; i < pixels.length; i++, p += 4) {
			var v = pixels[i];
			data[p] = v; data[p + 1] = v; data[p + 2] = v; data[p + 3] = 255;
		}
		offCtx.putImageData(imgData, 0, 0);

		var ctx = this.sliceCanvas.getContext('2d');
		ctx.imageSmoothingEnabled = false;
		ctx.clearRect(0, 0, this.width, this.height);
		ctx.drawImage(this.sliceOffscreen, 0, 0, width, height, 0, 0, this.width, this.height);
	};

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
		var range = computeMinMax(volume.data);
		var perAxisPx = computePerAxisPixelSizes(volume.extent, volume.spacing, volume.unit, maxPanelPx, minPanelPx);
		var valueIsFloat = isFloatingDtype(volume.data);
		var valueDecimals = valueDecimalPlaces(range.max - range.min);

		var cursor = new Array(volume.dim);
		for (var k = 0; k < volume.dim; k++) cursor[k] = Math.floor(volume.extent[k] / 2);

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
		readoutRange.textContent = 'value range: ' + formatValue(range.min, valueDecimals, valueIsFloat) + ' – ' + formatValue(range.max, valueDecimals, valueIsFloat);
		readout.appendChild(readoutRange);
		var readoutVoxel = document.createElement('div');
		readoutVoxel.style.minHeight = '1.2em';
		readout.appendChild(readoutVoxel);
		var readoutPhysical = document.createElement('div');
		if (volume.spacing) readout.appendChild(readoutPhysical);
		container.appendChild(readout);

		var grid = document.createElement('div');
		grid.style.display = 'inline-grid';
		grid.style.gap = '4px';
		// Explicit per-track sizes (not a single gridAutoColumns/Rows) --
		// column c (1-indexed) is axis c-1's own width, row r is axis r's
		// own height, matching the axisI+1/axisJ placement Panel() itself
		// uses below.
		var colSizes = [];
		for (var c = 0; c < volume.dim - 1; c++) colSizes.push(perAxisPx[c] + 'px');
		var rowSizes = [];
		for (var r = 1; r < volume.dim; r++) rowSizes.push(perAxisPx[r] + 'px');
		grid.style.gridTemplateColumns = colSizes.join(' ');
		grid.style.gridTemplateRows = rowSizes.join(' ');
		container.appendChild(grid);

		var panels = [];
		for (var i = 0; i < volume.dim; i++) {
			for (var j = i + 1; j < volume.dim; j++) {
				var excludedColors = [];
				for (var k = 0; k < volume.dim; k++)
					if (k !== i && k !== j) excludedColors.push(palette[k % palette.length]);
				var panel = new Panel(i, j, perAxisPx[i], perAxisPx[j], palette, excludedColors);
				drawAxisLimitLabels(panel, axisLabels[i], axisLabels[j]);
				grid.appendChild(panel.wrap);
				panels.push(panel);
			}
		}

		function redrawPanel(panel) {
			var pixels = extractSliceU8(volume, strides, panel.axisI, panel.axisJ, cursor, range.min, range.max);
			panel.uploadSlice(pixels, volume.extent[panel.axisI], volume.extent[panel.axisJ]);
			panel.drawCrosshair(volume.extent[panel.axisI], volume.extent[panel.axisJ], cursor[panel.axisI], cursor[panel.axisJ]);
		}

		// Every panel is re-sliced and redrawn on any cursor change, rather
		// than only the panels sharing the moved axes -- simpler and, for
		// the C(DIM,2) panel counts a pairwise-view grid actually has
		// (e.g. 6 for DIM=4, 10 for DIM=5), cheap enough that the
		// selective-redraw optimization isn't worth the extra bookkeeping.
		function redrawAll() { for (var p = 0; p < panels.length; p++) redrawPanel(panels[p]); }

		function setCursor(newCursor) {
			for (var k = 0; k < volume.dim; k++)
				cursor[k] = Math.max(0, Math.min(volume.extent[k] - 1, Math.round(newCursor[k])));
			redrawAll();
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
			var voxel = cursor.slice();
			voxel[panel.axisI] = dataI;
			voxel[panel.axisJ] = dataJ;
			var offset = 0;
			for (var k = 0; k < volume.dim; k++) offset += voxel[k] * strides[k];
			readoutVoxel.textContent = 'voxel (' + voxel.join(', ') + ') = ' + formatValue(volume.data[offset], valueDecimals, valueIsFloat);
			if (volume.spacing) {
				var phys = new Array(volume.dim);
				for (var k2 = 0; k2 < volume.dim; k2++) {
					var coord = formatPhysical(voxel[k2] * volume.spacing[k2]);
					phys[k2] = volume.unit[k2] ? coord + volume.unit[k2] : coord;
				}
				readoutPhysical.textContent = 'physical (' + phys.join(', ') + ')';
			}
		}

		function attachInteraction(panel) {
			var dragging = false;
			function moveTo(evt) {
				var c = dataCoordsFromEvent(panel, evt);
				var next = cursor.slice();
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
		}
		for (var p = 0; p < panels.length; p++) attachInteraction(panels[p]);

		redrawAll();

		return {
			cursor: cursor,
			setCursor: setCursor,
			destroy: function () { container.removeChild(grid); container.removeChild(readout); }
		};
	}

	global.NDLViewer = {
		create: create,
		parseVolume: parseVolume // exposed for testing/inspection
	};
})(typeof window !== 'undefined' ? window : this);
