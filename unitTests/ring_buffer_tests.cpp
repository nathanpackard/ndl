#include <gtest/gtest.h>
#include <array>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iostream>

#include <ndl/image.h>
#include <ndl/ring_buffer.h>

#include "testHelpers.h"

using namespace ndl;

TEST(RingBuffer, LogicalOrderBeforeWraparound) {
	std::stringstream passfail;
	std::cout << std::endl << "RING BUFFER -- LOGICAL ORDER BEFORE FIRST WRAPAROUND" << std::endl;

	// A 1-wide, 1-tall, capacity-4 ring along axis 0 -- push 3 scalar
	// samples (never enough to wrap), and confirm the logical view
	// (oldest-to-newest, count()-1 = newest) matches push order exactly,
	// with count()/totalWritten()/oldestGlobalIndex() all consistent with
	// "still filling up, never wrapped."
	RingBufferImage<int, 1> ring({ 4 }, 0);
	for (int v : { 10, 20, 30 })
		ring.push(&v);

	bool countOk = ring.count() == 3 && ring.totalWritten() == 3 && ring.oldestGlobalIndex() == 0;
	passfail << "count()==3, totalWritten()==3, oldestGlobalIndex()==0 before first wraparound: " << (countOk ? "Pass" : "Fail") << std::endl;

	bool orderOk = ring.at({ 0 }) == 10 && ring.at({ 1 }) == 20 && ring.at({ 2 }) == 30;
	passfail << "logical at(0..2) matches push order (10,20,30): " << (orderOk ? "Pass" : "Fail") << std::endl;

	bool extentOk = ring.extent()[0] == 3 && ring.capacity() == 4;
	passfail << "extent()[0] reports current count (3), capacity() stays 4: " << (extentOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(RingBuffer, WraparoundKeepsNewestDropsOldest) {
	std::stringstream passfail;
	std::cout << std::endl << "RING BUFFER -- WRAPAROUND" << std::endl;

	// Capacity 4, push 10 samples (0..9) -- only the newest 4 (6,7,8,9)
	// should still be readable, oldest-to-newest, regardless of how much
	// physical wraparound happened underneath.
	RingBufferImage<int, 1> ring({ 4 }, 0);
	for (int v = 0; v < 10; v++) ring.push(&v);

	bool countOk = ring.count() == 4 && ring.totalWritten() == 10 && ring.oldestGlobalIndex() == 6;
	passfail << "count()==4 (capped), totalWritten()==10, oldestGlobalIndex()==6 after 10 pushes into capacity 4: " << (countOk ? "Pass" : "Fail") << std::endl;

	bool orderOk = ring.at({ 0 }) == 6 && ring.at({ 1 }) == 7 && ring.at({ 2 }) == 8 && ring.at({ 3 }) == 9;
	passfail << "logical at(0..3) reads the 4 newest samples (6,7,8,9), oldest-to-newest: " << (orderOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(RingBuffer, ZeroCopyWritePathMatchesPush) {
	std::stringstream passfail;
	std::cout << std::endl << "RING BUFFER -- ZERO-COPY nextWriteSlot()/commitWrite()" << std::endl;

	// A capacity-3 ring of 2-element "frames" (extent {2,3}, ring axis 1)
	// -- write directly into nextWriteSlot() (as a real streaming reader
	// would via fread()) rather than push(), and confirm it round-trips
	// identically to push()'s own memcpy-based path.
	RingBufferImage<int, 2> ring({ 2, 3 }, 1);
	ASSERT_EQ(ring.frameSize(), 2u);

	int frame0[2] = { 1, 2 };
	int* slot = ring.nextWriteSlot();
	slot[0] = frame0[0]; slot[1] = frame0[1];
	ring.commitWrite();

	int frame1[2] = { 3, 4 };
	ring.push(frame1);

	bool ok = ring.at({ 0, 0 }) == 1 && ring.at({ 1, 0 }) == 2 &&
	          ring.at({ 0, 1 }) == 3 && ring.at({ 1, 1 }) == 4;
	passfail << "nextWriteSlot()/commitWrite() and push() both land at the expected logical coordinates: " << (ok ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(RingBuffer, SnapshotMatchesLogicalOrder) {
	std::stringstream passfail;
	std::cout << std::endl << "RING BUFFER -- snapshot()" << std::endl;

	RingBufferImage<int, 1> ring({ 3 }, 0);
	for (int v = 0; v < 5; v++) ring.push(&v); // wraps once; retains {2,3,4}

	OwnedImage<int, 1> snap = ring.snapshot();
	bool extentOk = snap.extent()[0] == 3;
	bool valuesOk = snap.at({ 0 }) == 2 && snap.at({ 1 }) == 3 && snap.at({ 2 }) == 4;
	passfail << "snapshot() extent matches ring's current count: " << (extentOk ? "Pass" : "Fail") << std::endl;
	passfail << "snapshot() values match the ring's own logical oldest-to-newest order: " << (valuesOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(RingBuffer, CoordinatesCoversExactlyCurrentCount) {
	std::stringstream passfail;
	std::cout << std::endl << "RING BUFFER -- coordinates()" << std::endl;

	RingBufferImage<int, 1> ring({ 5 }, 0);
	for (int v = 0; v < 3; v++) ring.push(&v); // not yet full

	auto coords = ring.coordinates();
	bool ok = coords.size() == 3 && coords[0] == std::array<int, 1>{0} && coords[2] == std::array<int, 1>{2};
	passfail << "coordinates() enumerates exactly count() (not capacity()) logical positions: " << (ok ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(RingBuffer, NonRingAxesAreOrdinaryFixedDimensions) {
	std::stringstream passfail;
	std::cout << std::endl << "RING BUFFER -- non-ring axes stay ordinary (2D: channel x time)" << std::endl;

	// extent {2,4}: axis 0 (size 2, e.g. "channel") is an ordinary fixed
	// dimension; axis 1 (size 4, the ring axis) is what slides.
	RingBufferImage<int, 2> ring({ 2, 4 }, 1);
	for (int t = 0; t < 6; t++)
	{
		int frame[2] = { t * 10, t * 10 + 1 }; // channel 0, channel 1
		ring.push(frame);
	}
	// Retains t=2..5 (4 samples); logical index 0 is t=2, index 3 is t=5.
	bool ok = ring.at({ 0, 0 }) == 20 && ring.at({ 1, 0 }) == 21 &&
	          ring.at({ 0, 3 }) == 50 && ring.at({ 1, 3 }) == 51;
	passfail << "non-ring axis (channel) indexes independently of the ring axis's own wraparound: " << (ok ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}
