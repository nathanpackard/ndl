#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <array>
#include <vector>

// A small, standalone SHA-1 implementation (the classic public-domain-style
// textbook algorithm) -- needed by net/websocket_server.h's own handshake
// (RFC 6455's Sec-WebSocket-Accept is base64(SHA1(key + a fixed GUID))),
// but otherwise unrelated to WebSockets and reusable on its own. Not a
// general cryptographic toolkit: SHA-1 is not fit for anything security-
// sensitive today, and this exists purely because the WebSocket handshake
// protocol itself, as specified, requires computing one.
namespace ndl
{
	namespace net
	{
		/// Computes the SHA-1 digest of `data` (20 raw bytes, not hex-encoded).
		/// @ingroup net
		inline std::array<uint8_t, 20> sha1(const uint8_t* data, std::size_t length)
		{
			uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;

			// Pre-processing: append a single '1' bit, then zero-pad, then
			// the original bit length as a big-endian 64-bit integer, so
			// the total length is a multiple of 64 bytes -- the standard
			// Merkle-Damgard padding every SHA-1 message needs.
			std::vector<uint8_t> msg(data, data + length);
			uint64_t bitLength = (uint64_t)length * 8;
			msg.push_back(0x80);
			while (msg.size() % 64 != 56) msg.push_back(0x00);
			for (int i = 7; i >= 0; i--) msg.push_back((uint8_t)(bitLength >> (i * 8)));

			for (std::size_t chunkStart = 0; chunkStart < msg.size(); chunkStart += 64)
			{
				uint32_t w[80];
				for (int i = 0; i < 16; i++)
					w[i] = ((uint32_t)msg[chunkStart + i * 4] << 24) | ((uint32_t)msg[chunkStart + i * 4 + 1] << 16)
						| ((uint32_t)msg[chunkStart + i * 4 + 2] << 8) | (uint32_t)msg[chunkStart + i * 4 + 3];
				for (int i = 16; i < 80; i++)
				{
					uint32_t v = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
					w[i] = (v << 1) | (v >> 31);
				}

				uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
				for (int i = 0; i < 80; i++)
				{
					uint32_t f, k;
					if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999; }
					else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
					else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
					else { f = b ^ c ^ d; k = 0xCA62C1D6; }

					uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
					e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = temp;
				}

				h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
			}

			std::array<uint8_t, 20> digest;
			uint32_t h[5] = { h0, h1, h2, h3, h4 };
			for (int i = 0; i < 5; i++)
				for (int j = 0; j < 4; j++)
					digest[i * 4 + j] = (uint8_t)(h[i] >> (24 - j * 8));
			return digest;
		}

		/// Convenience overload over a std::string (treated as raw bytes, not re-encoded).
		inline std::array<uint8_t, 20> sha1(const std::string& data)
		{
			return sha1(reinterpret_cast<const uint8_t*>(data.data()), data.size());
		}
	}
}
