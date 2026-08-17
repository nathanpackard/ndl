#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <sstream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "sha1.h"

// A small, hand-rolled, server-only WebSocket implementation (RFC 6455) --
// POSIX sockets, Linux target, not a portability layer. Deliberately not a
// vendored library: the actual protocol surface this needs (accept a
// browser's upgrade handshake, exchange small JSON text control messages
// and binary pixel frames, react to ping/close) is small enough to own
// directly, matching this codebase's existing taste (e.g. patching
// minih264e.h in imageIO/mp4 rather than taking on a bigger dependency for
// video). A sibling of net/json.h/net/sha1.h, not part of image.h's core
// -- #include this directly if you use it. First networking code in this
// repository.
//
// Deliberately does NOT implement message fragmentation (a WS message
// split across multiple frames) -- every message this protocol (viewport.h)
// ever sends or receives is small (a flat JSON control message, or one
// rendered frame's pixels) and fits in a single WS frame; a real browser
// client never fragments messages that small. A frame arriving with FIN=0
// is treated as a protocol error and the connection is closed.
//
// No application-level "keep newest, drop stale" policy lives here --
// that's viewport.h's job (each Viewport's own single-slot mailbox for
// outgoing frames, and single-slot "latest params" for incoming updates).
// This class only provides the raw, thread-safe primitive
// (sendBinary()) and delivers every complete incoming text message via
// onMessage() -- deciding what to do with a burst of updates is the
// caller's concern, not the transport's.
namespace ndl
{
	namespace net
	{
		namespace detail_ws
		{
			inline std::string base64Encode(const uint8_t* data, std::size_t len)
			{
				static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
				std::string out;
				std::size_t i = 0;
				for (; i + 3 <= len; i += 3)
				{
					uint32_t n = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) | (uint32_t)data[i + 2];
					out += table[(n >> 18) & 0x3F]; out += table[(n >> 12) & 0x3F];
					out += table[(n >> 6) & 0x3F]; out += table[n & 0x3F];
				}
				std::size_t rem = len - i;
				if (rem == 1)
				{
					uint32_t n = (uint32_t)data[i] << 16;
					out += table[(n >> 18) & 0x3F]; out += table[(n >> 12) & 0x3F]; out += "==";
				}
				else if (rem == 2)
				{
					uint32_t n = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8);
					out += table[(n >> 18) & 0x3F]; out += table[(n >> 12) & 0x3F]; out += table[(n >> 6) & 0x3F]; out += "=";
				}
				return out;
			}

			// RFC 6455 section 1.3's own fixed GUID -- concatenated onto the
			// client's Sec-WebSocket-Key before hashing, part of the spec
			// itself (not a secret), the same for every WebSocket server.
			constexpr const char* WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

			inline std::string computeAcceptKey(const std::string& clientKey)
			{
				std::string combined = clientKey + WS_GUID;
				auto digest = sha1(combined);
				return base64Encode(digest.data(), digest.size());
			}

			// Case-insensitive substring search for a header name, then
			// reads its value up to the line's own \r\n -- just enough
			// HTTP parsing to pull Sec-WebSocket-Key out of a real
			// browser's upgrade request; not a general HTTP header parser.
			inline bool findHeaderValue(const std::string& request, const std::string& headerName, std::string& valueOut)
			{
				std::string lower = request;
				std::string lowerName = headerName;
				for (auto& c : lower) c = (char)tolower((unsigned char)c);
				for (auto& c : lowerName) c = (char)tolower((unsigned char)c);
				std::size_t pos = lower.find(lowerName + ":");
				if (pos == std::string::npos) return false;
				std::size_t valueStart = pos + headerName.size() + 1;
				while (valueStart < request.size() && (request[valueStart] == ' ' || request[valueStart] == '\t')) valueStart++;
				std::size_t lineEnd = request.find("\r\n", valueStart);
				if (lineEnd == std::string::npos) lineEnd = request.size();
				valueOut = request.substr(valueStart, lineEnd - valueStart);
				return true;
			}

			// Pulls just the path out of an HTTP request line ("GET /foo/bar.html HTTP/1.1"),
			// dropping any query string -- not a general URL parser, just enough for the
			// static-file-serving fallback below.
			inline std::string requestPath(const std::string& request)
			{
				std::size_t firstSpace = request.find(' ');
				std::size_t secondSpace = firstSpace == std::string::npos ? std::string::npos : request.find(' ', firstSpace + 1);
				if (firstSpace == std::string::npos || secondSpace == std::string::npos) return "";
				std::string path = request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
				std::size_t q = path.find('?');
				if (q != std::string::npos) path = path.substr(0, q);
				return path;
			}

			inline bool endsWith(const std::string& s, const std::string& suffix)
			{
				return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
			}

			inline std::string mimeTypeFor(const std::string& path)
			{
				if (endsWith(path, ".html")) return "text/html; charset=utf-8";
				if (endsWith(path, ".js")) return "text/javascript; charset=utf-8";
				if (endsWith(path, ".css")) return "text/css; charset=utf-8";
				if (endsWith(path, ".json")) return "application/json";
				if (endsWith(path, ".png")) return "image/png";
				return "application/octet-stream";
			}

			inline bool sendAll(int fd, const uint8_t* data, std::size_t len)
			{
				std::size_t sent = 0;
				while (sent < len)
				{
					ssize_t n = ::send(fd, data + sent, len - sent, 0);
					if (n <= 0) return false;
					sent += (std::size_t)n;
				}
				return true;
			}
			inline bool readExact(int fd, uint8_t* buf, std::size_t len)
			{
				std::size_t got = 0;
				while (got < len)
				{
					ssize_t n = ::recv(fd, buf + got, len - got, 0);
					if (n <= 0) return false;
					got += (std::size_t)n;
				}
				return true;
			}

			inline std::vector<uint8_t> encodeFrame(uint8_t opcode, const uint8_t* payload, std::size_t len)
			{
				std::vector<uint8_t> out;
				out.push_back((uint8_t)(0x80 | opcode)); // FIN=1, no fragmentation ever sent
				if (len <= 125) out.push_back((uint8_t)len);
				else if (len <= 0xFFFF)
				{
					out.push_back(126);
					out.push_back((uint8_t)(len >> 8)); out.push_back((uint8_t)(len & 0xFF));
				}
				else
				{
					out.push_back(127);
					for (int i = 7; i >= 0; i--) out.push_back((uint8_t)((uint64_t)len >> (i * 8)));
				}
				out.insert(out.end(), payload, payload + len);
				return out;
			}
		}

		/// A minimal, server-only WebSocket server (RFC 6455) -- see this header's own top comment for scope.
		/// One connected client's identity is its own socket fd (never reused while connected).
		/// @ingroup net
		class WebSocketServer
		{
		public:
			using ClientId = int;
			using MessageHandler = std::function<void(ClientId, const std::string& text)>;
			using ConnectHandler = std::function<void(ClientId)>;
			using DisconnectHandler = std::function<void(ClientId)>;

			/// Starts listening on `port` immediately (the accept loop runs on its own background thread).
			/// @param staticRoot Optional. When non-empty, a plain (non-WebSocket-upgrade) GET request is
			///                   served a static file from under this directory instead of the connection
			///                   being closed -- see performHandshake()'s own comment. Empty (the default)
			///                   disables this entirely: a plain GET just gets the connection closed, the
			///                   original WS-only behavior.
			/// @throws std::runtime_error if the listening socket can't be created/bound/listened on.
			explicit WebSocketServer(int port, MessageHandler onMessage, ConnectHandler onConnect = nullptr, DisconnectHandler onDisconnect = nullptr, std::string staticRoot = "") :
				onMessage_(std::move(onMessage)), onConnect_(std::move(onConnect)), onDisconnect_(std::move(onDisconnect)), staticRoot_(std::move(staticRoot))
			{
				listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
				if (listenFd_ < 0) throw std::runtime_error("WebSocketServer: socket() failed");
				int yes = 1;
				setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

				sockaddr_in addr{};
				addr.sin_family = AF_INET;
				addr.sin_addr.s_addr = INADDR_ANY;
				addr.sin_port = htons((uint16_t)port);
				if (::bind(listenFd_, (sockaddr*)&addr, sizeof(addr)) != 0)
				{
					::close(listenFd_);
					throw std::runtime_error("WebSocketServer: bind() failed on port " + std::to_string(port));
				}
				if (::listen(listenFd_, 16) != 0)
				{
					::close(listenFd_);
					throw std::runtime_error("WebSocketServer: listen() failed");
				}
				acceptThread_ = std::thread([this] { acceptLoop(); });
			}

			WebSocketServer(const WebSocketServer&) = delete;
			WebSocketServer& operator=(const WebSocketServer&) = delete;

			~WebSocketServer() { stop(); }

			/// Thread-safe: sends one binary WS frame to the given client. Silently no-ops if that client has
			/// already disconnected -- a render loop racing a client dropping mid-broadcast is a normal, expected
			/// event here, not something worth throwing over.
			void sendBinary(ClientId client, const uint8_t* data, std::size_t len)
			{
				auto c = lookupClient(client);
				if (!c) return;
				std::lock_guard<std::mutex> lock(c->writeMutex);
				auto frame = detail_ws::encodeFrame(0x2, data, len);
				detail_ws::sendAll(c->fd, frame.data(), frame.size());
			}

			/// Same as sendBinary(), but as a text WS frame (opcode 0x1) -- for small JSON control responses
			/// (e.g. viewport.h's own queryValue()'s "valueResult" reply) rather than rendered pixel data.
			void sendText(ClientId client, const std::string& text)
			{
				auto c = lookupClient(client);
				if (!c) return;
				std::lock_guard<std::mutex> lock(c->writeMutex);
				auto frame = detail_ws::encodeFrame(0x1, (const uint8_t*)text.data(), text.size());
				detail_ws::sendAll(c->fd, frame.data(), frame.size());
			}

			/// Number of currently-connected clients.
			std::size_t clientCount() const
			{
				std::lock_guard<std::mutex> lock(clientsMutex_);
				return clients_.size();
			}

			/// Stops accepting new connections, closes every connection, and waits (bounded) for every
			/// in-flight client handler to finish. Safe to call more than once; the destructor calls this too.
			void stop()
			{
				if (!running_.exchange(false)) return;
				if (listenFd_ >= 0)
				{
					::shutdown(listenFd_, SHUT_RDWR);
					::close(listenFd_);
					listenFd_ = -1;
				}
				if (acceptThread_.joinable()) acceptThread_.join();

				std::vector<int> fds;
				{
					std::lock_guard<std::mutex> lock(clientsMutex_);
					for (auto& kv : clients_) fds.push_back(kv.first);
				}
				// shutdown() (unlike close()) is well-defined to wake a
				// DIFFERENT thread's concurrent blocking recv() on the
				// same fd with an immediate error/EOF -- this is what
				// actually unblocks each detached client thread's read
				// loop (see clientLoop()'s own comment on why those
				// threads are detached, not joined, individually).
				for (int fd : fds) { ::shutdown(fd, SHUT_RDWR); ::close(fd); }

				for (int i = 0; i < 500 && clientCount() > 0; i++)
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

		private:
			struct Client { int fd; std::mutex writeMutex; };

			std::shared_ptr<Client> lookupClient(ClientId id) const
			{
				std::lock_guard<std::mutex> lock(clientsMutex_);
				auto it = clients_.find(id);
				return it == clients_.end() ? nullptr : it->second;
			}

			void acceptLoop()
			{
				while (running_)
				{
					sockaddr_in clientAddr{};
					socklen_t addrLen = sizeof(clientAddr);
					int fd = ::accept(listenFd_, (sockaddr*)&clientAddr, &addrLen);
					if (fd < 0) break; // listenFd_ was closed by stop(), or a real error -- either way, stop accepting
					// Detached, not joined individually: a client thread's
					// own natural lifetime (until its socket closes, from
					// either end) is decoupled from anything else -- stop()
					// closes every client fd and does a bounded wait for
					// clientCount() to reach 0 instead, since joining a
					// thread that might be about to remove itself from the
					// very map stop() is iterating would be its own hazard.
					std::thread([this, fd] { clientLoop(fd); }).detach();
				}
			}

			// A plain (non-upgrade) GET is served a static file under
			// staticRoot_ instead of just being closed -- see the
			// WebSocketServer constructor's own comment on why (one port
			// serves both a page and the live protocol, so the app can
			// print a single openable URL). "/" maps to "/index.html";
			// any path containing ".." is rejected outright (403) rather
			// than resolved, the simplest possible guard against escaping
			// staticRoot_ -- this is a small dev-tool file server, not a
			// hardened one, but that one check costs nothing and closes
			// the obvious hole. Always closes the connection afterward
			// (Connection: close) -- this is a one-request-per-connection
			// server, no HTTP/1.1 keep-alive, simplest thing that still
			// works correctly for a browser loading one page's own few
			// resources (each gets its own short-lived connection).
			void serveStaticFile(int fd, const std::string& request)
			{
				std::string path = detail_ws::requestPath(request);
				if (path.empty() || path == "/") path = "/index.html";

				std::string body;
				int status; std::string statusText, contentType = "text/plain";
				if (path.find("..") != std::string::npos)
				{
					status = 403; statusText = "Forbidden"; body = "403 Forbidden";
				}
				else
				{
					std::ifstream file(staticRoot_ + path, std::ios::binary);
					if (!file)
					{
						status = 404; statusText = "Not Found"; body = "404 Not Found: " + path;
					}
					else
					{
						std::ostringstream contents;
						contents << file.rdbuf();
						body = contents.str();
						status = 200; statusText = "OK"; contentType = detail_ws::mimeTypeFor(path);
					}
				}

				std::string headers = "HTTP/1.1 " + std::to_string(status) + " " + statusText + "\r\n"
					"Content-Type: " + contentType + "\r\n"
					"Content-Length: " + std::to_string(body.size()) + "\r\n"
					"Connection: close\r\n\r\n";
				detail_ws::sendAll(fd, (const uint8_t*)headers.data(), headers.size());
				detail_ws::sendAll(fd, (const uint8_t*)body.data(), body.size());
			}

			// Returns true only when the connection actually upgraded to a
			// WebSocket (the caller should proceed to the WS frame loop);
			// false covers BOTH "this was a plain GET, already fully
			// answered by serveStaticFile() (or staticRoot_ isn't
			// configured at all)" and any genuine parse failure -- either
			// way, the caller just closes the connection next, which is
			// correct for both cases.
			bool performHandshake(int fd)
			{
				// Reads byte-by-byte until the blank line ending the HTTP
				// request's headers -- simple, not the most efficient way
				// to read a socket, but this happens exactly once per
				// connection for a small request, not a hot path worth
				// buffering for.
				std::string request;
				char c;
				while (request.size() < 8192)
				{
					ssize_t n = ::recv(fd, &c, 1, 0);
					if (n <= 0) return false;
					request += c;
					if (request.size() >= 4 && request.compare(request.size() - 4, 4, "\r\n\r\n") == 0) break;
				}
				std::string key;
				if (detail_ws::findHeaderValue(request, "Sec-WebSocket-Key", key))
				{
					std::string accept = detail_ws::computeAcceptKey(key);
					std::string response =
						"HTTP/1.1 101 Switching Protocols\r\n"
						"Upgrade: websocket\r\n"
						"Connection: Upgrade\r\n"
						"Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
					return detail_ws::sendAll(fd, (const uint8_t*)response.data(), response.size());
				}
				if (!staticRoot_.empty()) serveStaticFile(fd, request);
				return false;
			}

			void clientLoop(int fd)
			{
				if (!performHandshake(fd)) { ::close(fd); return; }

				auto client = std::make_shared<Client>();
				client->fd = fd;
				{
					std::lock_guard<std::mutex> lock(clientsMutex_);
					clients_[fd] = client;
				}
				if (onConnect_) onConnect_(fd);

				while (running_)
				{
					uint8_t header[2];
					if (!detail_ws::readExact(fd, header, 2)) break;
					bool fin = (header[0] & 0x80) != 0;
					uint8_t opcode = header[0] & 0x0F;
					bool masked = (header[1] & 0x80) != 0;
					uint64_t payloadLen = header[1] & 0x7F;
					if (!fin) break; // fragmentation not supported -- see this header's own top comment

					if (payloadLen == 126)
					{
						uint8_t ext[2];
						if (!detail_ws::readExact(fd, ext, 2)) break;
						payloadLen = ((uint64_t)ext[0] << 8) | ext[1];
					}
					else if (payloadLen == 127)
					{
						uint8_t ext[8];
						if (!detail_ws::readExact(fd, ext, 8)) break;
						payloadLen = 0;
						for (int i = 0; i < 8; i++) payloadLen = (payloadLen << 8) | ext[i];
					}

					uint8_t maskKey[4] = { 0, 0, 0, 0 };
					if (masked && !detail_ws::readExact(fd, maskKey, 4)) break;

					std::vector<uint8_t> payload(payloadLen);
					if (payloadLen > 0 && !detail_ws::readExact(fd, payload.data(), payloadLen)) break;
					if (masked)
						for (std::size_t i = 0; i < payload.size(); i++) payload[i] ^= maskKey[i % 4];

					if (opcode == 0x8) // close
					{
						auto frame = detail_ws::encodeFrame(0x8, nullptr, 0);
						std::lock_guard<std::mutex> lock(client->writeMutex);
						detail_ws::sendAll(fd, frame.data(), frame.size());
						break;
					}
					else if (opcode == 0x9) // ping -> pong, same payload
					{
						auto frame = detail_ws::encodeFrame(0xA, payload.data(), payload.size());
						std::lock_guard<std::mutex> lock(client->writeMutex);
						if (!detail_ws::sendAll(fd, frame.data(), frame.size())) break;
					}
					else if (opcode == 0xA) { /* pong -- nothing to do */ }
					else if (opcode == 0x1) // text
					{
						if (onMessage_) onMessage_(fd, std::string(payload.begin(), payload.end()));
					}
					// else: binary/unknown from a client -- this protocol
					// never expects one; discard the payload and keep
					// looping rather than treating it as fatal.
				}

				{
					std::lock_guard<std::mutex> lock(clientsMutex_);
					clients_.erase(fd);
				}
				if (onDisconnect_) onDisconnect_(fd);
				::close(fd);
			}

			int listenFd_ = -1;
			std::thread acceptThread_;
			std::atomic<bool> running_{ true };

			mutable std::mutex clientsMutex_;
			std::map<int, std::shared_ptr<Client>> clients_;

			MessageHandler onMessage_;
			ConnectHandler onConnect_;
			DisconnectHandler onDisconnect_;
			std::string staticRoot_;
		};
	}
}
