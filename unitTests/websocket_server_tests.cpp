#include <gtest/gtest.h>
#include <string>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>
#include <fstream>
#include <filesystem>

#include <ndl/net/sha1.h>
#include <ndl/net/websocket_server.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "testHelpers.h"

using namespace ndl::net;

namespace
{
	std::string toHex(const std::array<uint8_t, 20>& digest)
	{
		static const char* hexDigits = "0123456789abcdef";
		std::string out;
		for (uint8_t b : digest) { out += hexDigits[b >> 4]; out += hexDigits[b & 0xF]; }
		return out;
	}
}

TEST(WebSocketServer, Sha1MatchesKnownVectors) {
	std::stringstream passfail;
	std::cout << std::endl << "SHA1 -- KNOWN TEST VECTORS" << std::endl;

	bool emptyOk = toHex(sha1(std::string(""))) == "da39a3ee5e6b4b0d3255bfef95601890afd80709";
	passfail << "SHA1(\"\") matches the well-known empty-string digest: " << (emptyOk ? "Pass" : "Fail") << std::endl;

	bool abcOk = toHex(sha1(std::string("abc"))) == "a9993e364706816aba3e25717850c26c9cd0d89d";
	passfail << "SHA1(\"abc\") matches the well-known FIPS 180 test vector: " << (abcOk ? "Pass" : "Fail") << std::endl;

	// A message > 64 bytes (SHA1's own block size) so the multi-block path
	// (more than one chunk through the compression loop) is actually
	// exercised, not just the single-block short-message case above.
	std::string longMsg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	bool longOk = toHex(sha1(longMsg)) == "84983e441c3bd26ebaae4aa1f95129e5e54670f1";
	passfail << "SHA1 of a 56-byte message (spans a block boundary) matches its known FIPS 180 digest: " << (longOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(WebSocketServer, HandshakeAcceptKeyMatchesRfc6455Example) {
	std::stringstream passfail;
	std::cout << std::endl << "WEBSOCKET HANDSHAKE -- RFC 6455's OWN WORKED EXAMPLE" << std::endl;

	// RFC 6455 section 1.3 gives this exact key/accept pair as its own
	// illustration of the handshake computation -- the strongest possible
	// check that base64Encode()+sha1()+the GUID concatenation are all
	// wired together correctly, independent of any socket I/O. Reaches
	// into detail_ws (an implementation detail, not WebSocketServer's own
	// public surface) since no real caller needs this outside the
	// handshake performHandshake() already does internally -- this test
	// is the one place it's useful to call in isolation.
	std::string accept = detail_ws::computeAcceptKey("dGhlIHNhbXBsZSBub25jZQ==");
	bool ok = accept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
	passfail << "computeAcceptKey() matches RFC 6455's own published example: " << (ok ? "Pass" : "Fail") << " (got " << accept << ")" << std::endl;

	reportPassFail(passfail);
}

namespace
{
	// A tiny hand-rolled WebSocket CLIENT, just enough to test
	// WebSocketServer end-to-end without needing a real browser: performs
	// the upgrade handshake, sends one masked text frame (client frames
	// must be masked per RFC 6455), and can read one server (unmasked)
	// frame back.
	class TestClient
	{
	public:
		bool connect(int port)
		{
			fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
			if (fd_ < 0) return false;
			sockaddr_in addr{};
			addr.sin_family = AF_INET;
			addr.sin_port = htons((uint16_t)port);
			inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
			if (::connect(fd_, (sockaddr*)&addr, sizeof(addr)) != 0) return false;

			std::string request =
				"GET / HTTP/1.1\r\n"
				"Host: localhost\r\n"
				"Upgrade: websocket\r\n"
				"Connection: Upgrade\r\n"
				"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
				"Sec-WebSocket-Version: 13\r\n\r\n";
			if (::send(fd_, request.data(), request.size(), 0) < 0) return false;

			std::string response;
			char c;
			while (response.size() < 4096)
			{
				ssize_t n = ::recv(fd_, &c, 1, 0);
				if (n <= 0) return false;
				response += c;
				if (response.size() >= 4 && response.compare(response.size() - 4, 4, "\r\n\r\n") == 0) break;
			}
			return response.find("101") != std::string::npos;
		}

		bool sendText(const std::string& text)
		{
			std::vector<uint8_t> frame;
			frame.push_back(0x81); // FIN=1, opcode=text
			uint8_t maskKey[4] = { 0x12, 0x34, 0x56, 0x78 };
			std::size_t len = text.size();
			if (len <= 125) frame.push_back((uint8_t)(0x80 | len));
			else { frame.push_back((uint8_t)(0x80 | 126)); frame.push_back((uint8_t)(len >> 8)); frame.push_back((uint8_t)(len & 0xFF)); }
			for (int i = 0; i < 4; i++) frame.push_back(maskKey[i]);
			for (std::size_t i = 0; i < len; i++) frame.push_back((uint8_t)text[i] ^ maskKey[i % 4]);
			return ::send(fd_, frame.data(), frame.size(), 0) == (ssize_t)frame.size();
		}

		// Reads exactly one server->client frame (assumed unmasked, small,
		// unfragmented -- everything WebSocketServer itself ever sends).
		bool recvBinary(std::vector<uint8_t>& payloadOut)
		{
			uint8_t header[2];
			if (!readExact(header, 2)) return false;
			uint64_t len = header[1] & 0x7F;
			if (len == 126) { uint8_t ext[2]; if (!readExact(ext, 2)) return false; len = ((uint64_t)ext[0] << 8) | ext[1]; }
			else if (len == 127) { uint8_t ext[8]; if (!readExact(ext, 8)) return false; len = 0; for (int i = 0; i < 8; i++) len = (len << 8) | ext[i]; }
			payloadOut.resize(len);
			return len == 0 || readExact(payloadOut.data(), len);
		}

		~TestClient() { if (fd_ >= 0) ::close(fd_); }

	private:
		bool readExact(uint8_t* buf, std::size_t len)
		{
			std::size_t got = 0;
			while (got < len) { ssize_t n = ::recv(fd_, buf + got, len - got, 0); if (n <= 0) return false; got += (std::size_t)n; }
			return true;
		}
		int fd_ = -1;
	};
}

TEST(WebSocketServer, LoopbackHandshakeAndMessageRoundTrip) {
	std::stringstream passfail;
	std::cout << std::endl << "WEBSOCKET SERVER -- LOOPBACK HANDSHAKE + MESSAGE ROUND TRIP" << std::endl;

	const int port = 47821;
	std::atomic<bool> gotMessage{ false };
	std::string receivedText;
	WebSocketServer::ClientId connectedClient = -1;

	WebSocketServer server(port,
		[&](WebSocketServer::ClientId id, const std::string& text) { receivedText = text; connectedClient = id; gotMessage = true; },
		nullptr, nullptr);

	TestClient client;
	bool connected = client.connect(port);
	passfail << "TestClient completes the upgrade handshake (gets HTTP 101) against a real WebSocketServer: " << (connected ? "Pass" : "Fail") << std::endl;

	bool sent = connected && client.sendText("{\"type\":\"ping\"}");
	passfail << "client sends a masked text frame successfully: " << (sent ? "Pass" : "Fail") << std::endl;

	for (int i = 0; i < 200 && !gotMessage; i++) std::this_thread::sleep_for(std::chrono::milliseconds(5));
	bool messageOk = gotMessage && receivedText == "{\"type\":\"ping\"}";
	passfail << "server's onMessage callback receives the exact text the client sent: " << (messageOk ? "Pass" : "Fail") << std::endl;

	std::vector<uint8_t> payload = { 1, 2, 3, 4, 5 };
	if (connectedClient >= 0) server.sendBinary(connectedClient, payload.data(), payload.size());
	std::vector<uint8_t> received;
	bool binaryRoundTrip = connectedClient >= 0 && client.recvBinary(received) && received == payload;
	passfail << "server->client sendBinary() delivers the exact bytes sent: " << (binaryRoundTrip ? "Pass" : "Fail") << std::endl;

	if (connectedClient >= 0) server.sendText(connectedClient, "{\"type\":\"valueResult\",\"value\":42}");
	std::vector<uint8_t> receivedTextBytes;
	// recvBinary() just reads whatever frame's payload comes next
	// (opcode-agnostic), so it works for a text frame too.
	bool textRoundTrip = connectedClient >= 0 && client.recvBinary(receivedTextBytes)
		&& std::string(receivedTextBytes.begin(), receivedTextBytes.end()) == "{\"type\":\"valueResult\",\"value\":42}";
	passfail << "server->client sendText() delivers the exact text sent: " << (textRoundTrip ? "Pass" : "Fail") << std::endl;

	server.stop();
	reportPassFail(passfail);
}

namespace
{
	// A minimal raw-HTTP GET client, just enough to test
	// serveStaticFile() -- no WS handshake, no framing, just "connect,
	// send a GET request line, read the whole response."
	struct HttpGetResult { bool connected = false; std::string response; };

	HttpGetResult httpGet(int port, const std::string& path)
	{
		HttpGetResult result;
		int fd = ::socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0) return result;
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons((uint16_t)port);
		inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
		if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) != 0) { ::close(fd); return result; }
		result.connected = true;

		std::string request = "GET " + path + " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
		::send(fd, request.data(), request.size(), 0);

		char buf[4096];
		ssize_t n;
		while ((n = ::recv(fd, buf, sizeof(buf), 0)) > 0) result.response.append(buf, (std::size_t)n);
		::close(fd);
		return result;
	}
}

TEST(WebSocketServer, StaticFileServingSuccessAnd404) {
	std::stringstream passfail;
	std::cout << std::endl << "WEBSOCKET SERVER -- STATIC FILE SERVING" << std::endl;

	std::string dir = std::string(NDL_TEST_OUTPUT_DIR);
	std::filesystem::create_directories(dir);
	std::string filePath = dir + "/ws_static_test.html";
	{
		std::ofstream f(filePath);
		f << "<html><body>hello</body></html>";
	}

	const int port = 47822;
	WebSocketServer server(port, [](WebSocketServer::ClientId, const std::string&) {}, nullptr, nullptr, dir);

	HttpGetResult ok = httpGet(port, "/ws_static_test.html");
	bool okStatus = ok.connected && ok.response.find("HTTP/1.1 200") == 0;
	passfail << "GET of an existing file returns HTTP 200: " << (okStatus ? "Pass" : "Fail") << std::endl;
	bool okContentType = ok.response.find("Content-Type: text/html") != std::string::npos;
	passfail << "response includes the correct Content-Type for .html: " << (okContentType ? "Pass" : "Fail") << std::endl;
	bool okBody = ok.response.find("<html><body>hello</body></html>") != std::string::npos;
	passfail << "response body matches the file's own exact contents: " << (okBody ? "Pass" : "Fail") << std::endl;

	HttpGetResult missing = httpGet(port, "/does_not_exist.html");
	bool missingStatus = missing.connected && missing.response.find("HTTP/1.1 404") == 0;
	passfail << "GET of a missing file returns HTTP 404: " << (missingStatus ? "Pass" : "Fail") << std::endl;

	HttpGetResult traversal = httpGet(port, "/../../../etc/passwd");
	bool traversalRejected = traversal.connected && traversal.response.find("HTTP/1.1 403") == 0;
	passfail << "a path containing \"..\" is rejected with HTTP 403 rather than resolved: " << (traversalRejected ? "Pass" : "Fail") << std::endl;

	server.stop();
	reportPassFail(passfail);
}

TEST(WebSocketServer, StaticFileServingCoexistsWithWebSocketUpgrade) {
	std::stringstream passfail;
	std::cout << std::endl << "WEBSOCKET SERVER -- STATIC SERVING + WS UPGRADE ON THE SAME SERVER" << std::endl;

	std::string dir = std::string(NDL_TEST_OUTPUT_DIR);
	std::filesystem::create_directories(dir);
	{ std::ofstream f(dir + "/index.html"); f << "index page"; }

	const int port = 47823;
	std::atomic<bool> gotMessage{ false };
	WebSocketServer server(port, [&](WebSocketServer::ClientId, const std::string&) { gotMessage = true; }, nullptr, nullptr, dir);

	// "/" maps to index.html.
	HttpGetResult indexResult = httpGet(port, "/");
	bool indexOk = indexResult.connected && indexResult.response.find("HTTP/1.1 200") == 0 && indexResult.response.find("index page") != std::string::npos;
	passfail << "GET / serves index.html on a server that ALSO has a staticRoot configured: " << (indexOk ? "Pass" : "Fail") << std::endl;

	// The very same server still handles a real WS upgrade + message correctly.
	TestClient wsClient;
	bool wsConnected = wsClient.connect(port);
	bool wsSent = wsConnected && wsClient.sendText("{\"type\":\"ping\"}");
	for (int i = 0; i < 200 && !gotMessage; i++) std::this_thread::sleep_for(std::chrono::milliseconds(5));
	passfail << "the same server instance still completes a real WS handshake and delivers a message: " << ((wsConnected && wsSent && gotMessage) ? "Pass" : "Fail") << std::endl;

	server.stop();
	reportPassFail(passfail);
}
