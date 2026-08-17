#pragma once
#include <string>
#include <cmath>
#include <stdexcept>

// Collapses N per-position channel values (e.g. RGB, or a trailing
// real+imag complex-as-channel-axis pair -- see fft.h's own
// fftn_channels()) down to either 1 (grayscale) or 3 (true color) output
// values, via a small set of named reduction modes. Independent of any
// image/viewport machinery -- operates on a plain array of N doubles, not
// an Image or a source type -- so anything that reads N channel values at
// a position (a renderer, a compositor, a plain per-pixel loop) can reuse
// this instead of hand-rolling the same "which mode reduces N values to
// what" logic. Originally lived inside viewer/viewport.h (renderSlice()'s/
// renderVolume()'s own channelReduction param, the only caller at the
// time); promoted here once it became clear the logic itself has nothing
// to do with rendering. A sibling of histogram.h/visualize.h, not part of
// image.h's core -- #include this directly if you use it.
namespace ndl
{
	/// How many output values (1 or 3) a channelReduction mode produces -- see reduceChannels()'s own comment.
	/// @ingroup channels
	inline int channelReductionOutputCount(const std::string& mode) { return mode == "rgb" ? 3 : 1; }

	/// Reduces `n` raw channel values (in their ORIGINAL native units -- not windowed/quantized) down to
	/// either 3 output values (`"rgb"`) or 1 (every other mode), written into `out`. Deliberately does NOT
	/// apply any windowing/scaling here: a caller displaying the result (e.g. mapping it into [0,255]) does
	/// that afterward, uniformly, the same way it would for a plain unreduced value -- so e.g. `"phase"`
	/// mode's own natural [-pi,pi] range gets windowed through whatever generic pipeline the caller already
	/// has, no special-casing needed here for what "the right range" is per mode.
	/// @param values The n raw channel values.
	/// @param n      How many values `values` holds.
	/// @param mode   `"rgb"` (needs exactly 3, passthrough), `"phase"` (needs exactly 2 -- real,imag --
	///               `atan2(values[1],values[0])`), `"magnitude"` (any n, `sqrt(sum of squares)`),
	///               `"sum"`/`"mean"`/`"max"` (any n).
	/// @param out    Destination for the reduced value(s) -- must have room for 3 (the largest possible output).
	/// @return The number of values actually written to `out` (1 or 3).
	/// @throws std::invalid_argument for an unknown mode, or a channel count `n` a mode can't work with
	///         (`"rgb"` needs exactly 3, `"phase"` needs exactly 2).
	/// @ingroup channels
	inline int reduceChannels(const double* values, int n, const std::string& mode, double* out)
	{
		if (mode == "rgb")
		{
			if (n != 3) throw std::invalid_argument("reduceChannels(): \"rgb\" mode requires exactly 3 channel values, got " + std::to_string(n));
			out[0] = values[0]; out[1] = values[1]; out[2] = values[2];
			return 3;
		}
		if (mode == "phase")
		{
			if (n != 2) throw std::invalid_argument("reduceChannels(): \"phase\" mode requires exactly 2 channel values (real, imag), got " + std::to_string(n));
			out[0] = std::atan2(values[1], values[0]);
			return 1;
		}
		if (mode == "magnitude")
		{
			double sumSq = 0;
			for (int i = 0; i < n; i++) sumSq += values[i] * values[i];
			out[0] = std::sqrt(sumSq);
			return 1;
		}
		if (mode == "sum" || mode == "mean")
		{
			double s = 0;
			for (int i = 0; i < n; i++) s += values[i];
			out[0] = (mode == "mean" && n > 0) ? s / n : s;
			return 1;
		}
		if (mode == "max")
		{
			double m = n > 0 ? values[0] : 0.0;
			for (int i = 1; i < n; i++) if (values[i] > m) m = values[i];
			out[0] = m;
			return 1;
		}
		throw std::invalid_argument("reduceChannels(): unknown channelReduction mode \"" + mode + "\"");
	}
}
