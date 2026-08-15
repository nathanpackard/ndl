#include <gtest/gtest.h>
#include <sstream>
#include <iostream>
#include <vector>
#include <array>
#include <cstdint>
#include <cstring>
#include <set>
#include <utility>

#include <ndl/image.h>
#include <ndl/viewer.h>

#include "testHelpers.h"

using namespace ndl;

// A 3D volume where element (x,y,z) == x + 10*y + 100*z, so any slice's
// content can be checked against direct multi-index arithmetic without a
// second, independent slicing implementation to trust.
static Image<int, 3> makeLabeled3D(std::vector<int>& storage, int nx, int ny, int nz)
{
	storage.assign(static_cast<std::size_t>(nx) * ny * nz, 0);
	Image<int, 3> img(storage.data(), { nx, ny, nz });
	for (const auto& c : img.coordinates()) img.at(c) = c[0] + 10 * c[1] + 100 * c[2];
	return img;
}

TEST(Viewer, PairwiseSlice3DMatchesDirectIndexing) {
	std::stringstream passfail;
	std::vector<int> storage;
	int nx = 4, ny = 5, nz = 6;
	Image<int, 3> img = makeLabeled3D(storage, nx, ny, nz);
	std::array<int, 3> cursor = { 2, 3, 4 };

	// axes (0,1) fixed at z=cursor[2] -- the "axial" plane, generalized.
	{
		auto view = pairwise_slice<0, 1>(img, cursor);
		bool ok = view.extent()[0] == nx && view.extent()[1] == ny;
		for (const auto& c : view.coordinates()) ok = ok && view.at(c) == img.at({ c[0], c[1], cursor[2] });
		passfail << "pairwise_slice<0,1> matches direct indexing at fixed z: " << (ok ? "Pass" : "Fail") << std::endl;
	}
	// axes (0,2) fixed at y=cursor[1] -- "coronal".
	{
		auto view = pairwise_slice<0, 2>(img, cursor);
		bool ok = view.extent()[0] == nx && view.extent()[1] == nz;
		for (const auto& c : view.coordinates()) ok = ok && view.at(c) == img.at({ c[0], cursor[1], c[1] });
		passfail << "pairwise_slice<0,2> matches direct indexing at fixed y: " << (ok ? "Pass" : "Fail") << std::endl;
	}
	// axes (1,2) fixed at x=cursor[0] -- "sagittal".
	{
		auto view = pairwise_slice<1, 2>(img, cursor);
		bool ok = view.extent()[0] == ny && view.extent()[1] == nz;
		for (const auto& c : view.coordinates()) ok = ok && view.at(c) == img.at({ cursor[0], c[0], c[1] });
		passfail << "pairwise_slice<1,2> matches direct indexing at fixed x: " << (ok ? "Pass" : "Fail") << std::endl;
	}

	reportPassFail(passfail);
}

TEST(Viewer, PairwiseSliceIsZeroCopy) {
	std::stringstream passfail;
	std::vector<int> storage;
	Image<int, 3> img = makeLabeled3D(storage, 4, 5, 6);
	std::array<int, 3> cursor = { 1, 1, 1 };

	auto view = pairwise_slice<0, 1>(img, cursor);
	int original = img.at({ 2, 2, cursor[2] });
	view.at({ 2, 2 }) = original + 1000;
	passfail << "writing through the view mutates the source buffer (no copy was made): "
	         << (img.at({ 2, 2, cursor[2] }) == original + 1000 ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Viewer, PairwiseSliceAxisOrderIsNormalized) {
	std::stringstream passfail;
	std::vector<int> storage;
	Image<int, 3> img = makeLabeled3D(storage, 4, 5, 6);
	std::array<int, 3> cursor = { 1, 2, 3 };

	auto forward = pairwise_slice<0, 2>(img, cursor);
	auto backward = pairwise_slice<2, 0>(img, cursor);
	bool same = forward.extent() == backward.extent();
	for (const auto& c : forward.coordinates()) same = same && forward.at(c) == backward.at(c);
	passfail << "pairwise_slice<2,0> and pairwise_slice<0,2> return the same view (dim0=min axis, dim1=max axis, regardless of argument order): "
	         << (same ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Viewer, PairwiseSlice5DTrueNDCase) {
	std::stringstream passfail;
	// element = i0 + 10*i1 + 100*i2 + 1000*i3 + 10000*i4, DIM=5 -- not a
	// disguised 2D/3D case, a real check that the recursive axis-removal
	// generalizes past the 3D "clinical" shape.
	int extent[5] = { 2, 3, 2, 3, 2 };
	std::size_t total = 1; for (int e : extent) total *= e;
	std::vector<int> storage(total);
	Image<int, 5> img(storage.data(), { extent[0], extent[1], extent[2], extent[3], extent[4] });
	for (const auto& c : img.coordinates()) img.at(c) = c[0] + 10 * c[1] + 100 * c[2] + 1000 * c[3] + 10000 * c[4];

	std::array<int, 5> cursor = { 1, 2, 1, 2, 1 };
	auto view = pairwise_slice<1, 3>(img, cursor);
	bool ok = view.extent()[0] == extent[1] && view.extent()[1] == extent[3];
	for (const auto& c : view.coordinates())
		ok = ok && view.at(c) == img.at({ cursor[0], c[0], cursor[2], c[1], cursor[4] });
	passfail << "pairwise_slice<1,3> on a 5D image matches direct indexing: " << (ok ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Viewer, ForEachAxisPairVisitsEveryPairExactlyOnce) {
	std::stringstream passfail;
	std::set<std::pair<int, int>> seen;
	for_each_axis_pair<4>([&](auto i, auto j) { seen.insert({ i(), j() }); });

	bool countOk = seen.size() == 6; // C(4,2)
	bool everyIJOrdered = true;
	for (auto& p : seen) if (p.first >= p.second) everyIJOrdered = false;
	std::set<std::pair<int, int>> expected = { {0,1},{0,2},{0,3},{1,2},{1,3},{2,3} };
	passfail << "for_each_axis_pair<4> visits exactly C(4,2)=6 pairs: " << (countOk ? "Pass" : "Fail") << std::endl;
	passfail << "every visited pair has i<j: " << (everyIJOrdered ? "Pass" : "Fail") << std::endl;
	passfail << "the visited set is exactly {(0,1),(0,2),(0,3),(1,2),(1,3),(2,3)}: " << (seen == expected ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Viewer, NormalizeToU8ClampsAndRescales) {
	std::stringstream passfail;
	float data[6] = { -50.0f, 0.0f, 50.0f, 100.0f, 150.0f, 300.0f };
	Image<float, 1> src(data, { 6 });
	uint8_t out[6];
	Image<uint8_t, 1> dst(out, { 6 });

	normalize_to_u8(src, dst, 0.0, 200.0);
	bool ok = out[0] == 0            // below lo, clamped
	        && out[1] == 0           // exactly lo
	        && out[2] > 0 && out[2] < 255
	        && out[4] == 191         // (150/200)*255 rounded
	        && out[5] == 255;        // above hi, clamped
	passfail << "normalize_to_u8 clamps below lo to 0 and above hi to 255, linear in between: " << (ok ? "Pass" : "Fail") << std::endl;

	uint8_t degenerate[6];
	Image<uint8_t, 1> dst2(degenerate, { 6 });
	normalize_to_u8(src, dst2, 100.0, 100.0); // hi<=lo
	bool allZero = true;
	for (auto v : degenerate) if (v != 0) allZero = false;
	passfail << "a degenerate window (hi<=lo) maps every value to 0 rather than dividing by zero: " << (allZero ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Viewer, WriteWebVolumeRoundTrip) {
	std::stringstream passfail;
	uint16_t data[2 * 3 * 4];
	for (int i = 0; i < 24; i++) data[i] = static_cast<uint16_t>(i * 7);
	Image<uint16_t, 3> img(data, { 2, 3, 4 });

	std::ostringstream out(std::ios::binary);
	write_web_volume(img, out);
	std::string bytes = out.str();

	bool magicOk = bytes.size() >= 7 && bytes.substr(0, 4) == "NDLV";
	uint8_t version = static_cast<uint8_t>(bytes[4]);
	uint8_t dtype = static_cast<uint8_t>(bytes[5]);
	uint8_t dim = static_cast<uint8_t>(bytes[6]);
	passfail << "magic bytes are \"NDLV\": " << (magicOk ? "Pass" : "Fail") << std::endl;
	passfail << "version byte is 1: " << (version == 1 ? "Pass" : "Fail") << std::endl;
	passfail << "dtype byte is 2 (uint16_t, per WebDTypeCode): " << (dtype == 2 ? "Pass" : "Fail") << std::endl;
	passfail << "dim byte is 3: " << (dim == 3 ? "Pass" : "Fail") << std::endl;

	// 3 x uint32 extents, little-endian, starting at byte 7.
	auto readU32LE = [&](std::size_t offset) -> uint32_t {
		return static_cast<uint32_t>(static_cast<unsigned char>(bytes[offset]))
		     | (static_cast<uint32_t>(static_cast<unsigned char>(bytes[offset + 1])) << 8)
		     | (static_cast<uint32_t>(static_cast<unsigned char>(bytes[offset + 2])) << 16)
		     | (static_cast<uint32_t>(static_cast<unsigned char>(bytes[offset + 3])) << 24);
	};
	bool extentsOk = readU32LE(7) == 2 && readU32LE(11) == 3 && readU32LE(15) == 4;
	passfail << "extents are written as 3 little-endian uint32s (2,3,4): " << (extentsOk ? "Pass" : "Fail") << std::endl;

	std::size_t dataStart = 19;
	bool sizeOk = bytes.size() == dataStart + 24 * sizeof(uint16_t);
	passfail << "payload size matches header (24 elements x 2 bytes): " << (sizeOk ? "Pass" : "Fail") << std::endl;

	bool dataOk = true;
	for (int i = 0; i < 24 && sizeOk; i++)
	{
		uint16_t v;
		std::memcpy(&v, bytes.data() + dataStart + i * sizeof(uint16_t), sizeof(uint16_t));
		if (v != static_cast<uint16_t>(i * 7)) dataOk = false;
	}
	passfail << "raw data is written in Image's own axis-0-fastest order, native byte order, verbatim: " << (dataOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}
