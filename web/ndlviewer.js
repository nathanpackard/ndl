// ndlviewer.js -- a standalone, dependency-free WebGL viewer for the
// binary volume format ndl/viewer.h's write_web_volume() produces.
//
// This is deliberately NOT part of ndl's C++ library (which stays
// header-only with no GUI-toolkit dependency of any kind -- in particular,
// no Qt) -- it's the other half of the split described in viewer.h's own
// top comment: ndl (C++) computes/exports N-D slice data; this file does
// all the actual interactive rendering. It has no build step and no
// dependency beyond a browser's own WebGL + Canvas2D APIs, so it can be
// dropped into any static page (in particular, ndl's own Doxygen-generated
// tutorial pages -- see docs/generate_tutorial.py) with a single
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
// aligns.
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
		if (version !== 1) throw new Error('NDLViewer: unsupported NDLV format version ' + version);
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

		var Ctor = DTYPE_CTORS[dtypeCode];
		// Typed arrays require their backing buffer's byte offset to be a
		// multiple of the element size for anything wider than one byte;
		// the header (7 + 4*dim bytes) isn't guaranteed aligned to e.g. 4
		// or 8 bytes for an arbitrary dim, so the data is copied into a
		// freshly-allocated (and therefore aligned) buffer rather than
		// constructing the typed array directly over arrayBuffer at
		// `offset`.
		var byteLength = elementCount * Ctor.BYTES_PER_ELEMENT;
		var dataBytes = new Uint8Array(byteLength);
		dataBytes.set(new Uint8Array(arrayBuffer, offset, byteLength));
		var data = new Ctor(dataBytes.buffer);

		return { dim: dim, extent: extent, data: data };
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

	// ---- Minimal WebGL: one textured quad per panel, no window/level
	// shader uniform (normalization already happened on the CPU side in
	// extractSliceU8, keeping the shader itself trivial) ----

	var VERTEX_SRC =
		'attribute vec2 aPos;\n' +
		'varying vec2 vTexCoord;\n' +
		'void main() {\n' +
		'  vTexCoord = aPos * 0.5 + 0.5;\n' +
		'  gl_Position = vec4(aPos, 0.0, 1.0);\n' +
		'}\n';

	var FRAGMENT_SRC =
		'precision mediump float;\n' +
		'varying vec2 vTexCoord;\n' +
		'uniform sampler2D uTex;\n' +
		'void main() {\n' +
		'  float v = texture2D(uTex, vTexCoord).r;\n' +
		'  gl_FragColor = vec4(v, v, v, 1.0);\n' +
		'}\n';

	function compileShader(gl, type, src) {
		var shader = gl.createShader(type);
		gl.shaderSource(shader, src);
		gl.compileShader(shader);
		if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS))
			throw new Error('NDLViewer: shader compile failed: ' + gl.getShaderInfoLog(shader));
		return shader;
	}

	function createProgram(gl) {
		var program = gl.createProgram();
		gl.attachShader(program, compileShader(gl, gl.VERTEX_SHADER, VERTEX_SRC));
		gl.attachShader(program, compileShader(gl, gl.FRAGMENT_SHADER, FRAGMENT_SRC));
		gl.linkProgram(program);
		if (!gl.getProgramParameter(program, gl.LINK_STATUS))
			throw new Error('NDLViewer: program link failed: ' + gl.getProgramInfoLog(program));
		return program;
	}

	// One panel: a WebGL canvas (the slice image) with a transparent
	// Canvas2D overlay stacked exactly on top of it (the crosshair + click/
	// drag handling) -- keeping crosshair drawing on a plain 2D canvas
	// rather than folding it into the WebGL shader is a deliberate
	// simplicity choice: line/text drawing is Canvas2D's whole job, and
	// this way the WebGL side stays a single trivial textured quad.
	function Panel(axisI, axisJ, size, palette) {
		this.axisI = axisI;
		this.axisJ = axisJ;
		this.colorI = palette[axisI % palette.length];
		this.colorJ = palette[axisJ % palette.length];

		var wrap = document.createElement('div');
		wrap.style.position = 'relative';
		wrap.style.width = size + 'px';
		wrap.style.height = size + 'px';
		wrap.style.gridColumn = String(axisI + 1);
		wrap.style.gridRow = String(axisJ);

		this.glCanvas = document.createElement('canvas');
		this.glCanvas.width = size;
		this.glCanvas.height = size;
		this.glCanvas.style.position = 'absolute';
		this.glCanvas.style.left = '0';
		this.glCanvas.style.top = '0';
		wrap.appendChild(this.glCanvas);

		this.overlay = document.createElement('canvas');
		this.overlay.width = size;
		this.overlay.height = size;
		this.overlay.style.position = 'absolute';
		this.overlay.style.left = '0';
		this.overlay.style.top = '0';
		this.overlay.style.cursor = 'crosshair';
		wrap.appendChild(this.overlay);

		// A non-interactive frame on top of the slice + crosshair, colored to
		// match this panel's own two axis colors (left/right = axisI's
		// vertical-crosshair color, top/bottom = axisJ's horizontal-crosshair
		// color) -- every panel sharing an axis shares that axis's border
		// color, so which panels are "the same axis" is visible at a glance
		// without reading labels. A separate absolutely-positioned div rather
		// than a CSS border on wrap itself: a border on wrap would grow past
		// the WebGL/overlay canvases (which stay fixed at size x size) into
		// the grid's own 4px gap, and pointer-events:none keeps it from
		// stealing the overlay canvas's click/hover handling.
		this.frame = document.createElement('div');
		this.frame.style.position = 'absolute';
		this.frame.style.left = '0';
		this.frame.style.top = '0';
		this.frame.style.width = size + 'px';
		this.frame.style.height = size + 'px';
		this.frame.style.boxSizing = 'border-box';
		this.frame.style.pointerEvents = 'none';
		this.frame.style.borderLeft = '3px solid ' + this.colorI;
		this.frame.style.borderRight = '3px solid ' + this.colorI;
		this.frame.style.borderTop = '3px solid ' + this.colorJ;
		this.frame.style.borderBottom = '3px solid ' + this.colorJ;
		wrap.appendChild(this.frame);

		this.wrap = wrap;
		this.size = size;

		var gl = this.glCanvas.getContext('webgl') || this.glCanvas.getContext('experimental-webgl');
		if (!gl) throw new Error('NDLViewer: WebGL is not available in this browser');
		this.gl = gl;
		this.program = createProgram(gl);
		this.aPos = gl.getAttribLocation(this.program, 'aPos');
		this.uTex = gl.getUniformLocation(this.program, 'uTex');

		var quad = new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]);
		this.quadBuffer = gl.createBuffer();
		gl.bindBuffer(gl.ARRAY_BUFFER, this.quadBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, quad, gl.STATIC_DRAW);

		this.texture = gl.createTexture();
		gl.bindTexture(gl.TEXTURE_2D, this.texture);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
	}

	Panel.prototype.uploadSlice = function (pixels, width, height) {
		var gl = this.gl;
		gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true); // data row 0 -> top of canvas, matching the overlay's own (non-flipped) top-down mapping
		gl.bindTexture(gl.TEXTURE_2D, this.texture);
		gl.texImage2D(gl.TEXTURE_2D, 0, gl.LUMINANCE, width, height, 0, gl.LUMINANCE, gl.UNSIGNED_BYTE, pixels);

		gl.viewport(0, 0, this.glCanvas.width, this.glCanvas.height);
		gl.useProgram(this.program);
		gl.bindBuffer(gl.ARRAY_BUFFER, this.quadBuffer);
		gl.enableVertexAttribArray(this.aPos);
		gl.vertexAttribPointer(this.aPos, 2, gl.FLOAT, false, 0, 0);
		gl.activeTexture(gl.TEXTURE0);
		gl.bindTexture(gl.TEXTURE_2D, this.texture);
		gl.uniform1i(this.uTex, 0);
		gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
	};

	// cursorXY: this panel's own two cursor coordinates, already in data
	// space (0..extent[axisI], 0..extent[axisJ]). Draws a vertical line at
	// the axisI position (colored by axisI's own palette color) and a
	// horizontal line at the axisJ position (colored by axisJ's own color)
	// -- so a given axis's cursor line is always the same color everywhere
	// it's drawn, letting a viewer visually track "where axis k currently
	// is" across every panel that shows it, the same spirit as
	// clinicalvolumeview's cross-referenced per-view line coloring.
	Panel.prototype.drawCrosshair = function (extentI, extentJ, cx, cy) {
		var ctx = this.overlay.getContext('2d');
		var w = this.size, h = this.size;
		ctx.clearRect(0, 0, w, h);
		var px = ((cx + 0.5) / extentI) * w;
		var py = ((cy + 0.5) / extentJ) * h;

		ctx.lineWidth = 1;
		ctx.strokeStyle = this.colorI;
		ctx.beginPath(); ctx.moveTo(px, 0); ctx.lineTo(px, h); ctx.stroke();

		ctx.strokeStyle = this.colorJ;
		ctx.beginPath(); ctx.moveTo(0, py); ctx.lineTo(w, py); ctx.stroke();

		ctx.fillStyle = '#fff';
		ctx.beginPath(); ctx.arc(px, py, 3, 0, 2 * Math.PI); ctx.fill();
	};

	var DEFAULT_PALETTE = ['#e6194b', '#3cb44b', '#4363d8', '#f58231', '#911eb4', '#46f0f0', '#f032e6', '#bcf60c'];

	// Creates the full pairwise-view grid inside `container` for the parsed
	// volume in `arrayBuffer`, wires up click/drag-to-navigate, and returns
	// a small handle ({cursor, setCursor(), destroy()}) for programmatic
	// control.
	function create(container, arrayBuffer, options) {
		options = options || {};
		var panelSize = options.panelSize || 200;
		var palette = options.palette || DEFAULT_PALETTE;

		var volume = parseVolume(arrayBuffer);
		var strides = flatStrides(volume.extent);
		var range = computeMinMax(volume.data);

		var cursor = new Array(volume.dim);
		for (var k = 0; k < volume.dim; k++) cursor[k] = Math.floor(volume.extent[k] / 2);

		var readout = document.createElement('div');
		readout.style.fontFamily = 'monospace';
		readout.style.minHeight = '1.2em';
		readout.style.marginBottom = '4px';
		container.appendChild(readout);

		var grid = document.createElement('div');
		grid.style.display = 'inline-grid';
		grid.style.gap = '4px';
		grid.style.gridAutoColumns = panelSize + 'px';
		grid.style.gridAutoRows = panelSize + 'px';
		container.appendChild(grid);

		var panels = [];
		for (var i = 0; i < volume.dim; i++) {
			for (var j = i + 1; j < volume.dim; j++) {
				var panel = new Panel(i, j, panelSize, palette);
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
			readout.textContent = 'voxel (' + voxel.join(', ') + ') = ' + volume.data[offset];
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
			panel.overlay.addEventListener('mouseleave', function () { readout.textContent = ''; });
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
