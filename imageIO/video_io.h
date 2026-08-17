#pragma once
// Video I/O for ndl: real .mp4 files, {3,width,height,frameCount} RGB
// images -- one more (slowest-varying) axis appended to this file's own
// {channel,width,height} still-image convention, so a loaded/saved video
// is just another ordinary image as far as the rest of ndl is concerned
// (viewer.h, distance_transform.h, etc. all just see a 4D uint8_t image).
//
// This is deliberately NOT bundled into imageIO.h's own #include list the
// way bitmap.h/jpeg_decoder.h/png/*.h are -- see distance_transform.h's own
// top comment for the same "opt-in, #include it directly if you use it"
// reasoning; video support pulls in enough vendored code (below) that it
// shouldn't be paid for by every translation unit that just wants
// load()/save() for stills.
//
// save_video() is fully NATIVE: minih264e.h (imageIO/mp4/minih264e.h,
// vendored verbatim, CC0/public domain) encodes each frame to H.264
// baseline-profile NAL units, minimp4.h (imageIO/mp4/minimp4.h, vendored
// verbatim, CC0) muxes those into a real MP4 container -- no external
// process, no non-permissive dependency, both genuinely single-header.
//
// load_video() is NOT native -- it shells out to the system `ffmpeg`/
// `ffprobe` binaries instead. This is a deliberate, considered asymmetry,
// not an oversight: correctly decoding arbitrary real-world video (which
// is routinely H.264 HIGH profile with CABAC entropy coding -- e.g. this
// library's own nd_viewer color demo's source clip) requires a full H.264
// decoder, and there is no permissively-licensed decoder that's both
// small/vendorable AND CABAC-capable -- every option small enough to vendor
// (openh264, h264bsd) is Baseline-profile/CAVLC-only and simply cannot
// decode most real files; the one permissive decoder that CAN (edge264) is
// ~20 source files, not single-header, and still short of full conformance
// as of this writing. Given that choice, shelling out to a real, already-
// correct, already-installed decoder is more honest than either vendoring
// something that can't actually decode the target file, or hand-rolling a
// CABAC decoder (a multi-month undertaking prone to subtly wrong output --
// not something to attempt inside an image-processing library).
//
// UNLIKE every other ndl header, this one isn't purely header-only: the
// two vendored libraries' actual function bodies live in two small
// dedicated C++ translation units (imageIO/mp4/minimp4_impl.cpp and
// minih264e_impl.cpp) rather than being #included with their
// IMPLEMENTATION macro defined directly here. Two independent problems
// forced that, not one:
//  - minimp4.h and minih264e.h each define some same-named internal helpers
//    (a `bs_t` bitstream-reader type, a `nal_put_esc` function, ...) --
//    fine individually, but colliding if both IMPLEMENTATIONs ever land in
//    the same translation unit. This alone could be fixed by wrapping each
//    in its own C++ namespace (and was, in an earlier version of this
//    file) -- but:
//  - minih264e.h's implementation #defines roughly 130 internal helper
//    macros (SWAP32, TEST, ...) that are NOT scoped to the file at all --
//    the C preprocessor has no concept of C++ namespaces, so every one of
//    them leaks into whatever comes after this #include in the same
//    translation unit. `TEST` specifically collides head-on with GTest's
//    own `TEST(suite, name)` macro the moment any test file includes both
//    gtest.h and this header -- not a hypothetical, an actual build
//    failure hit while writing unitTests/video_io_tests.cpp. Namespacing
//    can't fix this at all; only keeping each library's implementation in
//    its own, separate translation unit does, which is exactly what
//    minimp4_impl.cpp/minih264e_impl.cpp are for.
// minih264e.h itself HAS been patched (each site marked with a comment) to
// add explicit casts at the handful of places its original C source relied
// on implicit void*-to-typed-pointer conversions that C++ doesn't allow --
// so both files compile as ordinary C++ now, no C compiler needed, just
// real separate .cpp translation units. This is the one place this
// vendored copy isn't byte-identical to upstream lieff/minih264; both
// remain CC0/public domain.
//
// This header only ever sees their DECLARATIONS (no IMPLEMENTATION macro
// defined here), so it's safe to #include from as many .cpp files as you
// like, same as any other ndl header -- but linking an executable that
// uses save_video()/load_video() needs those two .cpp files' object code
// too (see CMakeLists.txt's own ndl_video_codec target). Those two files,
// specifically, still carry the usual stb-style "at most one .cpp per link
// target" caveat every IMPLEMENTATION-macro single-header C library has
// (their PUBLIC API functions are `extern "C"`, so their symbol names stay
// unmangled/global regardless of anything on the including side) -- ndl's
// own demos/tests each already compile to their own separate executable
// from a single .cpp, so this is a non-issue there.
#include "mp4/minimp4.h"
#include "mp4/minih264e.h"

#include "../image.h"
#include "../viewer/viewer.h"
#include <array>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <tuple>
#include <iostream>

namespace ndl
{
	namespace image_io
	{
		namespace detail_video
		{
			// minimp4's own MP4E_open() write-callback contract: `token` is
			// opaque to minimp4, whatever this callback needs to actually
			// write -- here, the FILE* opened by save_video() below.
			inline int mp4WriteCallback(int64_t offset, const void* buffer, size_t size, void* token)
			{
				FILE* f = (FILE*)token;
				if (fseek(f, (long)offset, SEEK_SET) != 0) return 1;
				return fwrite(buffer, 1, size, f) != size;
			}

			// BT.601 RGB->YUV, the standard 8-bit fixed-point integer
			// approximation (matches what most encoders/ffmpeg itself use
			// for "bt601" input) -- Y is full-resolution; U/V are computed
			// per-pixel here and averaged over each 2x2 block by the caller
			// to produce the 4:2:0 chroma planes H264E_io_yuv_t expects.
			inline uint8_t clamp255(int v) { return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); }
			inline uint8_t rgbToY(int r, int g, int b) { return clamp255((77 * r + 150 * g + 29 * b + 128) >> 8); }
			inline int rgbToUComponent(int r, int g, int b) { return (-43 * r - 85 * g + 128 * b + 128) >> 8; }
			inline int rgbToVComponent(int r, int g, int b) { return (128 * r - 107 * g - 21 * b + 128) >> 8; }

			// Shell-quotes a path for safe interpolation into a popen()
			// command line (popen always runs it via `/bin/sh -c`) --
			// wraps in single quotes, escaping any embedded single quote as
			// '\'' (close quote, escaped literal quote, reopen quote), the
			// standard POSIX-safe technique. Both load_video() and
			// commandExists() below build a command string that includes
			// caller-supplied text (a filename, a program name); without
			// this, a filename containing shell metacharacters would be a
			// command-injection hole, not just a decode error.
			inline std::string shellQuote(const std::string& s)
			{
				std::string out = "'";
				for (char c : s) { if (c == '\'') out += "'\\''"; else out += c; }
				out += "'";
				return out;
			}

			inline bool commandExists(const std::string& name)
			{
				std::string cmd = "command -v " + shellQuote(name) + " > /dev/null 2>&1";
				return std::system(cmd.c_str()) == 0;
			}

			struct VideoInfo { int width = 0, height = 0; double fps = 0; };

			// width/height/fps all come from ffprobe -- frame COUNT is the
			// one exception, deliberately NOT read from container metadata
			// here (routinely absent or wrong, e.g. some fragmented/
			// streamed mp4s never record it at all); load_video() instead
			// derives it from how many whole frames' worth of bytes ffmpeg
			// actually produced. fps specifically is the SOURCE file's own
			// native rate (r_frame_rate, ffprobe's own "num/den" text form,
			// e.g. "25/1" or the NTSC-ish "30000/1001") -- load_video()'s
			// own caller may still override the actual decoded rate via
			// targetFps, in which case this native value isn't what ends
			// up in the output, only what the SOURCE was; see load_video()
			// itself for how the two get reconciled into one actual value.
			inline VideoInfo probeVideo(const std::string& fileName)
			{
				std::string cmd = "ffprobe -v error -select_streams v:0 -show_entries stream=width,height,r_frame_rate -of csv=p=0 " + shellQuote(fileName);
				FILE* pipe = popen(cmd.c_str(), "r");
				if (!pipe) throw std::runtime_error("load_video(): failed to run ffprobe");
				char buf[256] = { 0 };
				char* got = fgets(buf, sizeof(buf), pipe);
				pclose(pipe);
				VideoInfo info;
				int fpsNum = 0, fpsDen = 1;
				int parsed = got ? sscanf(buf, "%d,%d,%d/%d", &info.width, &info.height, &fpsNum, &fpsDen) : 0;
				if (parsed < 2 || info.width <= 0 || info.height <= 0)
					throw std::runtime_error("load_video(): ffprobe couldn't determine video dimensions for: " + fileName);
				if (parsed >= 3 && fpsDen > 0) info.fps = (double)fpsNum / fpsDen;
				return info;
			}

			struct ResolvedScale { int width; int height; std::string scaleFilter; };

			// Shared by load_video() and VideoStreamReader below: given the
			// source's own probed size and a caller's target width/height
			// (either, both, or neither may be 0 -- see load_video()'s own
			// comment on what each combination means), resolves the actual
			// width/height to decode at and the ffmpeg `-vf scale=W:H`
			// filter to request it (empty if decoding at native size, so
			// callers don't add a no-op filter to their ffmpeg command
			// line). Factored out here specifically so this aspect-
			// preserving-when-only-one-target-is-given logic (already
			// tested via load_video()'s own tests) has exactly one
			// implementation, not one per caller.
			inline ResolvedScale resolveScale(const VideoInfo& info, int targetWidth, int targetHeight)
			{
				int width, height;
				if (targetWidth > 0 && targetHeight > 0)
				{
					width = targetWidth; height = targetHeight;
				}
				else if (targetWidth > 0)
				{
					width = targetWidth;
					height = (int)((double)targetWidth * info.height / info.width + 0.5);
					height -= height % 2;
				}
				else if (targetHeight > 0)
				{
					height = targetHeight;
					width = (int)((double)targetHeight * info.width / info.height + 0.5);
					width -= width % 2;
				}
				else
				{
					width = info.width; height = info.height;
				}
				ResolvedScale result;
				result.width = width;
				result.height = height;
				if (width != info.width || height != info.height)
					result.scaleFilter = "scale=" + std::to_string(width) + ":" + std::to_string(height);
				return result;
			}
		}

		// Saves frames as a real, standalone .mp4 (H.264 baseline-profile
		// video, no audio track) -- fully native, see this header's own top
		// comment for the encode/mux pipeline. frames' extent is
		// {3, width, height, frameCount} (channel fastest, matching every
		// other RGB image load()/save() use elsewhere in this file), and
		// width/height must each be a multiple of 16 (H.264's own
		// macroblock size, see H264E_create_param_t's own comment in
		// mp4/minih264e.h) -- this throws rather than silently padding,
		// since a caller-invisible resize would change what the saved
		// video actually shows.
		/// @tparam SrcImageT Any minimal-interface image type, extent {3,width,height,frameCount}, value_type uint8_t.
		/// @param  frames    Frames to encode.
		/// @param  fileName  Path to write.
		/// @param  fps       Playback frame rate.
		/// @param  qp        H.264 quantizer, 10 (best quality, biggest file) .. 51 (worst, smallest).
		/// @throws std::runtime_error on any encode/mux/file-write failure, or if width/height aren't multiples of 16.
		/// @ingroup image_io
		template<class SrcImageT>
		void save_video(const SrcImageT& frames, std::string fileName, int fps = 25, int qp = 28)
		{
			auto extent = frames.extent();
			constexpr int DIM = std::tuple_size<decltype(extent)>::value;
			static_assert(DIM == 4, "save_video() requires a 4D source, extent {3,width,height,frameCount}");
			if (extent[0] != 3) throw std::runtime_error("save_video() requires a 3-channel (RGB) source, extent[0]==3");
			int width = extent[1], height = extent[2], frameCount = extent[3];
			if (width % 16 || height % 16) throw std::runtime_error("save_video() requires width and height to each be a multiple of 16 (H.264 macroblock size), got " + std::to_string(width) + "x" + std::to_string(height));
			if (frameCount <= 0) throw std::runtime_error("save_video() requires at least one frame");

			H264E_create_param_t createParam;
			memset(&createParam, 0, sizeof(createParam));
			createParam.width = width;
			createParam.height = height;
			createParam.gop = 20;
			createParam.const_input_flag = 1;
#if H264E_SVC_API
			createParam.num_layers = 1;
#endif

			int sizeofPersist = 0, sizeofScratch = 0;
			if (H264E_sizeof(&createParam, &sizeofPersist, &sizeofScratch) != 0)
				throw std::runtime_error("save_video(): H264E_sizeof failed (invalid parameters)");
			std::vector<uint8_t> persistBuf(sizeofPersist), scratchBuf(sizeofScratch);
			H264E_persist_t* enc = (H264E_persist_t*)persistBuf.data();
			H264E_scratch_t* scratch = (H264E_scratch_t*)scratchBuf.data();
			if (H264E_init(enc, &createParam) != 0)
				throw std::runtime_error("save_video(): H264E_init failed");

			std::cout << "saving output file: " << fileName << std::endl;
			FILE* fout = fopen(fileName.c_str(), "wb");
			if (!fout) throw std::runtime_error("save_video(): failed to open output file: " + fileName);

			MP4E_mux_t* mux = MP4E_open(0, 0, fout, detail_video::mp4WriteCallback);
			if (!mux) { fclose(fout); throw std::runtime_error("save_video(): MP4E_open failed"); }
			mp4_h26x_writer_t writer;
			if (mp4_h26x_write_init(&writer, mux, width, height, 0) != MP4E_STATUS_OK)
			{
				MP4E_close(mux); fclose(fout);
				throw std::runtime_error("save_video(): mp4_h26x_write_init failed");
			}

			int halfW = width / 2, halfH = height / 2;
			std::vector<uint8_t> yPlane((size_t)width * height), uPlane((size_t)halfW * halfH), vPlane((size_t)halfW * halfH);

			for (int f = 0; f < frameCount; f++)
			{
				for (int y = 0; y < height; y++)
					for (int x = 0; x < width; x++)
					{
						int r = (int)frames.at({ 0, x, y, f });
						int g = (int)frames.at({ 1, x, y, f });
						int b = (int)frames.at({ 2, x, y, f });
						yPlane[(size_t)y * width + x] = detail_video::rgbToY(r, g, b);
					}
				for (int cy = 0; cy < halfH; cy++)
					for (int cx = 0; cx < halfW; cx++)
					{
						int sumU = 0, sumV = 0;
						for (int dy = 0; dy < 2; dy++)
							for (int dx = 0; dx < 2; dx++)
							{
								int x = cx * 2 + dx, y = cy * 2 + dy;
								int r = (int)frames.at({ 0, x, y, f });
								int g = (int)frames.at({ 1, x, y, f });
								int b = (int)frames.at({ 2, x, y, f });
								sumU += detail_video::rgbToUComponent(r, g, b);
								sumV += detail_video::rgbToVComponent(r, g, b);
							}
						uPlane[(size_t)cy * halfW + cx] = detail_video::clamp255(128 + sumU / 4);
						vPlane[(size_t)cy * halfW + cx] = detail_video::clamp255(128 + sumV / 4);
					}

				H264E_io_yuv_t yuv;
				yuv.yuv[0] = yPlane.data(); yuv.stride[0] = width;
				yuv.yuv[1] = uPlane.data(); yuv.stride[1] = halfW;
				yuv.yuv[2] = vPlane.data(); yuv.stride[2] = halfW;

				H264E_run_param_t runParam;
				memset(&runParam, 0, sizeof(runParam));
				runParam.qp_min = runParam.qp_max = qp;

				uint8_t* codedData = nullptr;
				int sizeofCodedData = 0;
				if (H264E_encode(enc, scratch, &runParam, &yuv, &codedData, &sizeofCodedData) != 0)
				{
					MP4E_close(mux); mp4_h26x_write_close(&writer); fclose(fout);
					throw std::runtime_error("save_video(): H264E_encode failed on frame " + std::to_string(f));
				}
				// mp4_h26x_write_nal() scans `codedData` for however many
				// NAL units it actually contains (SPS+PPS+IDR-slice, all
				// concatenated, on the first frame; just a slice on most
				// others) and muxes each in turn -- one call per FRAME, not
				// per NAL, is correct.
				if (mp4_h26x_write_nal(&writer, codedData, sizeofCodedData, 90000 / fps) != MP4E_STATUS_OK)
				{
					MP4E_close(mux); mp4_h26x_write_close(&writer); fclose(fout);
					throw std::runtime_error("save_video(): mp4_h26x_write_nal failed on frame " + std::to_string(f));
				}
			}

			mp4_h26x_write_close(&writer);
			MP4E_close(mux);
			fclose(fout);
		}

		// Loads a real-world video file -- any format/codec the system
		// `ffmpeg` supports, H.264 High profile/CABAC included (unlike this
		// same header's own native save_video(), which only WRITES
		// baseline) -- by shelling out to `ffmpeg`/`ffprobe` rather than
		// decoding natively; see this header's own top comment for why.
		//
		// targetWidth/targetHeight and targetFps let a caller ask ffmpeg
		// itself to scale/decimate the video during decode, via its own
		// `-vf scale=W:H` and `-r fps` filters, rather than ndl
		// re-implementing image resizing here -- e.g. pulling a small,
		// tutorial-sized clip out of a full 1280x720/25fps source without
		// ever materializing that source at full resolution in memory.
		// Since a caller loading an arbitrary file often doesn't know its
		// aspect ratio up front, EITHER targetWidth or targetHeight (not
		// necessarily both) may be left 0: this function always probes the
		// source's own native size first (see below), so it can compute
		// whichever one was left unspecified itself, preserving the
		// source's aspect ratio (rounded to the nearest even pixel, same
		// convention as ffmpeg's own `scale=W:-2`) -- only give both when
		// an exact, possibly aspect-distorting, size is actually wanted.
		//
		// spacingOut (optional): when non-null, filled in with what this
		// load actually knows about its own 4 axes' meaning -- channel
		// (0), the two spatial axes (1,2), and time (3) -- as an
		// ndl::VoxelSpacing<4> (viewer.h) ready to hand straight to
		// embedNDViewer()/write_web_volume(), rather than a caller having
		// to already know and re-derive this by hand. Channel gets a
		// "channels" unit (spacing 1 -- there's no meaningful physical
		// distance between channel indices, just enough to give the unit a
		// home); the two spatial axes get "px" (ffmpeg's own decode is
		// unscaled square pixels with no real-world size attached, so
		// that's the honest unit, not an invented physical one); time gets
		// a REAL physical unit, seconds, at 1/fps spacing, where fps is
		// targetFps when the caller gave one (that's exactly what decoding
		// was resampled to) or otherwise the SOURCE file's own native rate
		// (ffprobe's r_frame_rate, probed regardless of whether
		// targetWidth/targetHeight were also given). Left with unit[3]
		// empty (uncalibrated) in the one edge case ffprobe can't
		// determine a rate at all, rather than claiming a bogus one.
		/// @param  fileName     Path to load; any format/codec the system ffmpeg supports.
		/// @param  extent       Output parameter, set to {3, width, height, frameCount} on return.
		/// @param  targetWidth  If >0, scale to this width during decode. May be given alone (targetHeight left 0) to preserve the source's own aspect ratio; both 0 (the default) loads at the source's native resolution.
		/// @param  targetHeight If >0, scale to this height during decode. May be given alone (targetWidth left 0) to preserve the source's own aspect ratio; both 0 (the default) loads at the source's native resolution.
		/// @param  targetFps    If >0, resample to this frame rate (via frame drop/duplication) during decode.
		/// @param  spacingOut   Optional; filled in with this load's own axis units (channels/px/px/s) if non-null.
		/// @return          `size(extent)` bytes, packed RGB (channel fastest), frames in playback order.
		/// @throws std::runtime_error if ffmpeg/ffprobe aren't found on PATH, or decoding fails.
		/// @ingroup image_io
		inline std::vector<uint8_t> load_video(std::string fileName, std::array<int, 4>& extent, int targetWidth = 0, int targetHeight = 0, double targetFps = 0, VoxelSpacing<4>* spacingOut = nullptr)
		{
			if (!detail_video::commandExists("ffprobe") || !detail_video::commandExists("ffmpeg"))
				throw std::runtime_error("load_video() requires ffmpeg/ffprobe on PATH to decode real-world video files (see imageIO/video_io.h's own top comment for why); neither was found");

			std::cout << "loading file: " << fileName << std::endl;

			// Always probed: spacingOut's own time axis needs the SOURCE's
			// native fps whenever targetFps isn't given, and the width/height
			// logic below needs the source's own aspect ratio whenever only
			// one of targetWidth/targetHeight was given, regardless of
			// whether either was.
			detail_video::VideoInfo info = detail_video::probeVideo(fileName);

			detail_video::ResolvedScale resolved = detail_video::resolveScale(info, targetWidth, targetHeight);
			int width = resolved.width, height = resolved.height;

			std::string cmd = "ffmpeg -v error -i " + detail_video::shellQuote(fileName);
			if (!resolved.scaleFilter.empty()) cmd += " -vf " + detail_video::shellQuote(resolved.scaleFilter);
			if (targetFps > 0) cmd += " -r " + std::to_string(targetFps);
			cmd += " -f rawvideo -pix_fmt rgb24 -";
			FILE* pipe = popen(cmd.c_str(), "r");
			if (!pipe) throw std::runtime_error("load_video(): failed to run ffmpeg");

			size_t frameBytes = (size_t)width * height * 3;
			std::vector<uint8_t> data;
			std::vector<uint8_t> chunk(frameBytes);
			size_t n;
			while ((n = fread(chunk.data(), 1, chunk.size(), pipe)) > 0)
				data.insert(data.end(), chunk.begin(), chunk.begin() + n);
			int status = pclose(pipe);
			if (status != 0)
				throw std::runtime_error("load_video(): ffmpeg exited with an error decoding: " + fileName);
			if (data.empty() || data.size() % frameBytes != 0)
				throw std::runtime_error("load_video(): decoded byte count isn't a whole number of frames for: " + fileName);

			extent = { 3, width, height, (int)(data.size() / frameBytes) };

			if (spacingOut)
			{
				double actualFps = targetFps > 0 ? targetFps : info.fps;
				spacingOut->unit = { "channels", "px", "px", actualFps > 0 ? "s" : "" };
				spacingOut->spacing = { 1, 1, 1, actualFps > 0 ? 1.0 / actualFps : 1 };
			}
			return data;
		}

		// Wraps load_video() above's own extent-out-param + raw-vector dance
		// into a single call -- see load_owned()'s own comment for the same
		// tradeoff (one extra deep copy, worth it for not juggling a raw
		// vector and a separate Image view).
		/// @param  fileName     Path to load; any format/codec the system ffmpeg supports.
		/// @param  targetWidth  If >0, scale to this width during decode. May be given alone (targetHeight left 0) to preserve the source's own aspect ratio; both 0 (the default) loads at the source's native resolution.
		/// @param  targetHeight If >0, scale to this height during decode. May be given alone (targetWidth left 0) to preserve the source's own aspect ratio; both 0 (the default) loads at the source's native resolution.
		/// @param  targetFps    If >0, resample to this frame rate (via frame drop/duplication) during decode.
		/// @param  spacingOut   Optional; filled in with this load's own axis units (channels/px/px/s) if non-null -- see load_video()'s own comment.
		/// @return An OwnedImage<uint8_t,4> with extent {3, width, height, frameCount}.
		/// @throws std::runtime_error if ffmpeg/ffprobe aren't found on PATH, or decoding fails.
		/// @ingroup image_io
		inline OwnedImage<uint8_t, 4> load_video_owned(std::string fileName, int targetWidth = 0, int targetHeight = 0, double targetFps = 0, VoxelSpacing<4>* spacingOut = nullptr)
		{
			std::array<int, 4> extent;
			std::vector<uint8_t> data = load_video(fileName, extent, targetWidth, targetHeight, targetFps, spacingOut);
			Image<uint8_t, 4> view(data.data(), extent);
			return OwnedImage<uint8_t, 4>(view);
		}

		// The incremental counterpart to load_video() above: opens the
		// ffmpeg pipe ONCE, in the constructor, and keeps it open across
		// calls, so a caller can read a real-world video file one frame at
		// a time (readFrame()) instead of load_video()'s "decode the
		// whole thing into one std::vector before returning" -- the
		// difference that matters once a file is too large to hold
		// entirely in memory (or the source is effectively unbounded --
		// a live feed piped through ffmpeg rather than a finite file).
		//
		// Pairs directly with RingBufferImage's own zero-copy write path
		// (ring_buffer.h): `if (reader.readFrame(ring.nextWriteSlot()))
		// ring.commitWrite();` reads straight into the ring's own backing
		// storage, no intermediate frame buffer.
		//
		// Move-only (like the FILE* pipe it owns): reopening a fresh file
		// (e.g. looping a finite demo clip to simulate a continuous live
		// source) is `reader = VideoStreamReader(path, ...);`, not a
		// separate "reset" method.
		/// Incrementally reads one video frame at a time from an open ffmpeg pipe, for files too large (or
		/// effectively unbounded) to decode all at once the way load_video()/load_video_owned() do.
		/// @ingroup image_io
		class VideoStreamReader
		{
		public:
			/// Opens fileName and starts the ffmpeg process immediately (not lazily on the first readFrame()).
			/// @param fileName     Path to open; any format/codec the system ffmpeg supports.
			/// @param targetWidth  If >0, scale to this width during decode. May be given alone (targetHeight left 0) to preserve the source's own aspect ratio; both 0 (the default) decodes at the source's native resolution.
			/// @param targetHeight If >0, scale to this height during decode. May be given alone (targetWidth left 0) to preserve the source's own aspect ratio; both 0 (the default) decodes at the source's native resolution.
			/// @param targetFps    If >0, resample to this frame rate (via frame drop/duplication) during decode.
			/// @throws std::runtime_error if ffmpeg/ffprobe aren't found on PATH, or the ffmpeg process fails to start.
			explicit VideoStreamReader(std::string fileName, int targetWidth = 0, int targetHeight = 0, double targetFps = 0) :
				fileName_(std::move(fileName))
			{
				if (!detail_video::commandExists("ffprobe") || !detail_video::commandExists("ffmpeg"))
					throw std::runtime_error("VideoStreamReader requires ffmpeg/ffprobe on PATH to decode real-world video files (see imageIO/video_io.h's own top comment for why); neither was found");

				detail_video::VideoInfo info = detail_video::probeVideo(fileName_);
				detail_video::ResolvedScale resolved = detail_video::resolveScale(info, targetWidth, targetHeight);
				width_ = resolved.width;
				height_ = resolved.height;
				fps_ = targetFps > 0 ? targetFps : info.fps;
				frameBytes_ = (size_t)width_ * height_ * 3;

				std::string cmd = "ffmpeg -v error -i " + detail_video::shellQuote(fileName_);
				if (!resolved.scaleFilter.empty()) cmd += " -vf " + detail_video::shellQuote(resolved.scaleFilter);
				if (targetFps > 0) cmd += " -r " + std::to_string(targetFps);
				cmd += " -f rawvideo -pix_fmt rgb24 -";
				pipe_ = popen(cmd.c_str(), "r");
				if (!pipe_) throw std::runtime_error("VideoStreamReader: failed to run ffmpeg for: " + fileName_);
			}

			VideoStreamReader(const VideoStreamReader&) = delete;
			VideoStreamReader& operator=(const VideoStreamReader&) = delete;
			VideoStreamReader(VideoStreamReader&& other) noexcept :
				fileName_(std::move(other.fileName_)), pipe_(other.pipe_),
				width_(other.width_), height_(other.height_), fps_(other.fps_), frameBytes_(other.frameBytes_)
			{
				other.pipe_ = nullptr;
			}
			VideoStreamReader& operator=(VideoStreamReader&& other) noexcept
			{
				if (this != &other)
				{
					if (pipe_) pclose(pipe_);
					fileName_ = std::move(other.fileName_);
					pipe_ = other.pipe_;
					width_ = other.width_; height_ = other.height_; fps_ = other.fps_; frameBytes_ = other.frameBytes_;
					other.pipe_ = nullptr;
				}
				return *this;
			}
			~VideoStreamReader()
			{
				if (pipe_) pclose(pipe_);
			}

			int width() const { return width_; }
			int height() const { return height_; }
			double fps() const { return fps_; }
			/// Byte count readFrame() writes on a successful read (width()*height()*3, packed RGB).
			size_t frameBytes() const { return frameBytes_; }

			/// Same channels/px/px/s convention load_video()'s own spacingOut parameter uses -- see that function's own comment.
			VoxelSpacing<4> spacing() const
			{
				VoxelSpacing<4> s;
				s.unit = { "channels", "px", "px", fps_ > 0 ? "s" : "" };
				s.spacing = { 1, 1, 1, fps_ > 0 ? 1.0 / fps_ : 1 };
				return s;
			}

			/// Reads exactly one frame (frameBytes() bytes, packed RGB) directly into dst -- no intermediate
			/// copy, so it pairs with RingBufferImage::nextWriteSlot() for a genuinely zero-copy read.
			/// @param  dst Must have room for frameBytes() bytes.
			/// @return     true if a full frame was read; false at a clean end of stream.
			/// @throws std::runtime_error if the stream ends mid-frame (a truncated/corrupt source).
			bool readFrame(uint8_t* dst)
			{
				size_t n = fread(dst, 1, frameBytes_, pipe_);
				if (n == 0) return false;
				if (n != frameBytes_)
					throw std::runtime_error("VideoStreamReader::readFrame(): stream ended mid-frame for: " + fileName_);
				return true;
			}

		private:
			std::string fileName_;
			FILE* pipe_ = nullptr;
			int width_ = 0, height_ = 0;
			double fps_ = 0;
			size_t frameBytes_ = 0;
		};
	}
}
