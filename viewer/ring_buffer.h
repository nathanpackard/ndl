#pragma once
#include <array>
#include <vector>
#include <cstddef>
#include <cstring>
#include <numeric>
#include <algorithm>
#include <stdexcept>
#include "../image.h"

// RingBufferImage<T,DIM>: a fixed-capacity sliding window along one caller-
// chosen axis (the "ring axis" -- typically time), for real-time sources
// (sensors, video) where new samples keep arriving and old ones should just
// fall off the far end. A sibling of distance_transform.h/convolution.h,
// not part of image.h's core Image object -- #include this directly if you
// use it.
//
// The whole point is to never move memory once allocated: the backing
// std::vector<T> is sized once, at construction, to exactly capacity()
// worth of storage, and writing a new sample means overwriting the oldest
// physical slot in place -- no reallocation, no shifting every other
// element down to make room, regardless of how many samples have streamed
// through by the time you ask. That's the actual reason this is its own
// type rather than an Image<T,DIM> view with some clever stride: Image's
// raw-memory constructor (image/core.h) always uses a plain packed stride,
// and there's no way to express "index k wraps modulo capacity" as a
// stride at all -- wraparound isn't affine.
//
// Two ways to write a new sample, both O(1):
//   - nextWriteSlot()/commitWrite(): the zero-copy path. nextWriteSlot()
//     hands back a pointer directly into the ring's own backing storage --
//     read a video frame (or a sensor sample) straight into it (e.g. via
//     fread()) and call commitWrite() once it's actually there. No
//     intermediate buffer, no extra copy beyond whatever unavoidable copy
//     the data's own source already does getting it into memory at all.
//   - push(const T*): a thin convenience wrapper (memcpy into
//     nextWriteSlot(), then commitWrite()) for a caller who doesn't
//     already control where their source data lands.
//
// Reading back is the ordinary minimal structural interface (extent()/
// at()/coordinates()) morphology.h/convolution.h/viewer.h's free functions
// are already written against -- same pattern as PackedBitImage
// (image/packed_bit.h), sharing that interface without deriving from
// Image. Logical indexing along the ring axis is oldest-to-newest (0 =
// oldest currently-retained sample, count()-1 = newest), matching ordinary
// Image semantics, NOT physical storage order -- at()'s whole job is to
// hide the wraparound from callers doing local analysis over "whatever's
// currently in the window." This read path is NOT the hot path a
// high-rate producer should use every sample (each at() call does a
// division to resolve the physical slot); it exists for occasional local
// analysis and for snapshot() below, not for the write side.
//
// totalWritten()/oldestGlobalIndex() expose the ring's own monotonic
// bookkeeping (never reset, unlike the logical 0..count()-1 view) -- a
// caller streaming this data onward (e.g. tagging outgoing network
// messages with a sample's true sequence number) needs this to let a
// downstream consumer reason about gaps, "how stale is this," and "is the
// sample I'm looking at still retained," none of which the logical view
// alone can answer.
namespace ndl
{
	/// Fixed-capacity sliding window along one axis (the "ring axis") of a DIM-dimensional array -- new samples
	/// overwrite the oldest in place, in O(1), with no reallocation or shifting. See this header's own top
	/// comment for the full rationale and the zero-copy write path (nextWriteSlot()/commitWrite()).
	/// @tparam T   Element type.
	/// @tparam DIM Number of dimensions (>= 1).
	/// @ingroup core_image
	template<class T, int DIM>
	class RingBufferImage
	{
	public:
		using value_type = T;

		/// Allocates the ring's backing storage once, up front.
		/// @param extent   Shape: element count along each dimension. extent[ringAxis] is the ring's capacity (how many samples it retains).
		/// @param ringAxis Which axis slides -- new writes advance along this axis and wrap; every other axis is a fixed, ordinary dimension.
		RingBufferImage(std::array<int, DIM> extent, int ringAxis) :
			extent_(extent),
			ringAxis_(ringAxis),
			stride_(makeStride(extent)),
			storage_(totalSize(extent), T())
		{
			if (ringAxis < 0 || ringAxis >= DIM)
				throw std::invalid_argument("RingBufferImage: ringAxis out of range");
			if (extent[ringAxis] <= 0)
				throw std::invalid_argument("RingBufferImage: capacity (extent[ringAxis]) must be > 0");
			for (int k = 0; k < DIM; k++)
				if (extent[k] <= 0)
					throw std::invalid_argument("RingBufferImage: every extent must be > 0");
			frameSize_ = storage_.size() / (std::size_t)extent[ringAxis];
		}

		int ringAxis() const { return ringAxis_; }
		/// How many samples the ring retains at once (extent[ringAxis] as given at construction).
		int capacity() const { return extent_[ringAxis_]; }
		/// How many samples have EVER been written, monotonically increasing, never reset by wraparound.
		long long totalWritten() const { return totalWritten_; }
		/// How many samples are currently valid to read (grows to capacity() on first fill, then stays there).
		int count() const { return (int)std::min<long long>(totalWritten_, capacity()); }
		/// Global index of the oldest sample still retained (0 before the ring has ever wrapped).
		long long oldestGlobalIndex() const { return totalWritten_ > capacity() ? totalWritten_ - capacity() : 0; }

		/// Current logical shape: same as constructed, except extent[ringAxis] reports count() (how much is
		/// actually valid to read right now), not the ring's full capacity.
		std::array<int, DIM> extent() const
		{
			std::array<int, DIM> e = extent_;
			e[ringAxis_] = count();
			return e;
		}

		// ---- Zero-copy write path (the intended hot path) ----

		/// Pointer to the next physical slot's memory, one frame's worth of T (product of every axis except
		/// ringAxis) -- write directly into it (e.g. fread() a decoded video frame straight in), then call
		/// commitWrite(). Valid only until the next call to nextWriteSlot()/push() (each write reuses the same
		/// physical slot once it wraps back around).
		T* nextWriteSlot() { return storage_.data() + physicalSlotIndex(totalWritten_) * frameSize_; }
		/// Advances the ring by one sample -- call exactly once, after actually populating nextWriteSlot()'s memory.
		void commitWrite() { totalWritten_++; }
		/// Convenience wrapper: memcpy's frameSize() elements from frameData into nextWriteSlot(), then commitWrite().
		/// @param frameData Exactly frameSize() elements (one full slice along every axis except ringAxis).
		void push(const T* frameData)
		{
			std::memcpy(nextWriteSlot(), frameData, frameSize_ * sizeof(T));
			commitWrite();
		}
		/// Element count of one sample (the product of every axis's extent except ringAxis) -- the size nextWriteSlot()/push() expect.
		std::size_t frameSize() const { return frameSize_; }

		// ---- Minimal structural interface (extent()/at()/coordinates()) ----
		// coord[ringAxis] is LOGICAL (0 = oldest currently-retained sample .. count()-1 = newest), matching
		// ordinary Image indexing -- physical wraparound is resolved internally, invisible to callers here.

		/// Reads the element at a logical coordinate (coord[ringAxis] is oldest-to-newest, 0..count()-1).
		const T& at(const std::array<int, DIM>& coord) const { return storage_[physicalOffset(coord)]; }
		/// Mutable access to the element at a logical coordinate (coord[ringAxis] is oldest-to-newest, 0..count()-1).
		T& at(const std::array<int, DIM>& coord) { return storage_[physicalOffset(coord)]; }

		/// Every currently-valid logical coordinate, in row-major order -- same convention as Image::coordinates()/PackedBitImage::coordinates().
		std::vector<std::array<int, DIM>> coordinates() const { return detail::coordinatesOf(extent()); }

		/// Copies the ring's current logical contents (oldest-to-newest) out into an ordinary, real Image-compatible
		/// object -- for the rare case something needs a genuine OwnedImage (e.g. handing a snapshot to code that's
		/// generic over Image<T,DIM> specifically, not just the minimal interface). Deliberately explicit and
		/// infrequent, not something the hot write path ever does on its own.
		OwnedImage<T, DIM> snapshot() const
		{
			auto e = extent();
			OwnedImage<T, DIM> out(e);
			for (const auto& c : out.coordinates())
				out.at(c) = at(c);
			return out;
		}

	private:
		static std::array<std::size_t, DIM> makeStride(const std::array<int, DIM>& extent)
		{
			std::array<std::size_t, DIM> stride{};
			stride[0] = 1;
			for (int k = 1; k < DIM; k++)
				stride[k] = (std::size_t)extent[k - 1] * stride[k - 1];
			return stride;
		}
		static std::size_t totalSize(const std::array<int, DIM>& extent)
		{
			std::size_t n = 1;
			for (int k = 0; k < DIM; k++) n *= (std::size_t)extent[k];
			return n;
		}
		std::size_t physicalSlotIndex(long long writeCounter) const
		{
			return (std::size_t)(writeCounter % (long long)capacity());
		}
		std::size_t physicalOffset(const std::array<int, DIM>& coord) const
		{
			std::size_t physicalRingIndex = physicalSlotIndex(oldestGlobalIndex() + coord[ringAxis_]);
			std::size_t offset = 0;
			for (int k = 0; k < DIM; k++)
				offset += (std::size_t)(k == ringAxis_ ? (int)physicalRingIndex : coord[k]) * stride_[k];
			return offset;
		}

		std::array<int, DIM> extent_;
		int ringAxis_;
		std::array<std::size_t, DIM> stride_;
		std::vector<T> storage_;
		std::size_t frameSize_ = 0;
		long long totalWritten_ = 0;
	};

	// RingBufferImage must satisfy the same minimal structural interface
	// (extent()/at()/coordinates()) Image/PackedBitImage do -- one
	// representative instantiation is enough, same as PackedBitImage's own
	// static_assert (image/packed_bit.h).
	static_assert(detail::satisfies_minimal_interface_v<RingBufferImage<uint8_t, 4>, 4>,
		"RingBufferImage<T,DIM> no longer satisfies the minimal structural interface (extent()/at()/coordinates()) "
		"generic ndl code (and the viewport renderer it's meant to feed) requires -- check for a renamed or reshaped member.");
}
