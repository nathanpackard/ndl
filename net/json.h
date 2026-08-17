#pragma once
#include <string>
#include <vector>
#include <array>
#include <map>
#include <stdexcept>
#include <sstream>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdio>

// A small, hand-rolled JSON value type + parser + writer -- dependency-free
// by design, not an oversight: the viewport control protocol (viewport.h,
// net/websocket_server.h) only ever exchanges small, flat-ish messages
// (an op name, a handful of numbers, a couple of strings, at most one
// level of nesting), and this codebase controls BOTH ends of that
// protocol, so full RFC 8259 conformance (surrogate-pair escapes,
// arbitrary-precision numbers, exotic whitespace) buys nothing worth a
// vendored dependency for. Scoped to what's actually needed: objects,
// arrays, strings (with the standard backslash escapes), numbers, bool,
// null. A sibling of net/websocket_server.h, not part of image.h's core
// -- #include this directly if you use it.
//
// Deliberately NOT a general-purpose "any JSON in the wild" parser: a
// lone (unpaired) \uXXXX escape is decoded as a single UTF-8-encoded code
// point rather than being combined with an adjacent surrogate, since
// nothing this protocol produces ever emits characters outside the Basic
// Multilingual Plane in the first place.
namespace ndl
{
	namespace net
	{
		/// A JSON value: Null/Bool/Number/String/Array/Object. See this header's own top comment for scope.
		/// @ingroup net
		class JsonValue
		{
		public:
			enum class Type { Null, Bool, Number, String, Array, Object };

			JsonValue() : type_(Type::Null) {}
			JsonValue(std::nullptr_t) : type_(Type::Null) {}
			JsonValue(bool b) : type_(Type::Bool), bool_(b) {}
			JsonValue(double n) : type_(Type::Number), number_(n) {}
			JsonValue(int n) : type_(Type::Number), number_(n) {}
			JsonValue(const char* s) : type_(Type::String), string_(s) {}
			JsonValue(std::string s) : type_(Type::String), string_(std::move(s)) {}

			static JsonValue makeArray() { JsonValue v; v.type_ = Type::Array; return v; }
			static JsonValue makeObject() { JsonValue v; v.type_ = Type::Object; return v; }

			Type type() const { return type_; }
			bool isNull() const { return type_ == Type::Null; }
			bool isObject() const { return type_ == Type::Object; }
			bool isArray() const { return type_ == Type::Array; }
			bool isNumber() const { return type_ == Type::Number; }
			bool isString() const { return type_ == Type::String; }

			bool asBool() const { requireType(Type::Bool); return bool_; }
			double asNumber() const { requireType(Type::Number); return number_; }
			int asInt() const { return (int)std::lround(asNumber()); }
			const std::string& asString() const { requireType(Type::String); return string_; }
			const std::vector<JsonValue>& asArray() const { requireType(Type::Array); return array_; }
			const std::map<std::string, JsonValue>& asObject() const { requireType(Type::Object); return object_; }

			/// True if this is an object and it has the given key.
			bool has(const std::string& key) const { return type_ == Type::Object && object_.count(key) != 0; }

			/// Reads an existing object key. Throws if this isn't an object, or the key is missing.
			const JsonValue& operator[](const std::string& key) const
			{
				requireType(Type::Object);
				auto it = object_.find(key);
				if (it == object_.end()) throw std::runtime_error("JsonValue: missing key \"" + key + "\"");
				return it->second;
			}
			/// Writes an object key, creating it (and turning a Null value into an empty Object first) if needed.
			JsonValue& operator[](const std::string& key)
			{
				if (type_ == Type::Null) type_ = Type::Object;
				requireType(Type::Object);
				return object_[key];
			}
			/// Appends to an array, creating it (turning a Null value into an empty Array first) if needed.
			void push_back(JsonValue v)
			{
				if (type_ == Type::Null) type_ = Type::Array;
				requireType(Type::Array);
				array_.push_back(std::move(v));
			}

			/// obj[key] as a number if present, else fallback -- for optional numeric params.
			double numberOr(const std::string& key, double fallback) const
			{
				return has(key) && object_.at(key).isNumber() ? object_.at(key).asNumber() : fallback;
			}
			/// obj[key] as a string if present, else fallback -- for optional string params.
			std::string stringOr(const std::string& key, const std::string& fallback) const
			{
				return has(key) && object_.at(key).isString() ? object_.at(key).asString() : fallback;
			}
			/// obj[key] as a fixed-size array of N numbers, else `fallback` -- for an optional flat-array param
			/// (e.g. viewer/viewport.h's own `rotation`, a 9-number flat matrix). Starts from a COPY of
			/// `fallback` (not zero-filled) and overwrites only as many leading entries as the JSON array
			/// actually has, bounded by both N and the array's own length -- so a missing key, a short array,
			/// or a too-long one are all handled the same forgiving way: whatever isn't present just keeps its
			/// fallback value, nothing throws or reads/writes out of bounds.
			template<std::size_t N>
			std::array<double, N> numbersOr(const std::string& key, const std::array<double, N>& fallback) const
			{
				std::array<double, N> result = fallback;
				if (!has(key) || !object_.at(key).isArray()) return result;
				const auto& arr = object_.at(key).asArray();
				for (std::size_t i = 0; i < N && i < arr.size(); i++) result[i] = arr[i].asNumber();
				return result;
			}

			/// Serializes back to compact JSON text.
			std::string toString() const
			{
				std::ostringstream os;
				write(os);
				return os.str();
			}

		private:
			void requireType(Type t) const
			{
				if (type_ != t) throw std::runtime_error("JsonValue: wrong type accessed (expected " + typeName(t) + ", got " + typeName(type_) + ")");
			}
			static std::string typeName(Type t)
			{
				switch (t)
				{
				case Type::Null: return "Null"; case Type::Bool: return "Bool"; case Type::Number: return "Number";
				case Type::String: return "String"; case Type::Array: return "Array"; case Type::Object: return "Object";
				}
				return "?";
			}
			static void writeEscapedString(std::ostringstream& os, const std::string& s)
			{
				os << '"';
				for (unsigned char c : s)
				{
					switch (c)
					{
					case '"': os << "\\\""; break;
					case '\\': os << "\\\\"; break;
					case '\b': os << "\\b"; break;
					case '\f': os << "\\f"; break;
					case '\n': os << "\\n"; break;
					case '\r': os << "\\r"; break;
					case '\t': os << "\\t"; break;
					default:
						if (c < 0x20)
						{
							char buf[8];
							snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
							os << buf;
						}
						else os << (char)c;
					}
				}
				os << '"';
			}
			void write(std::ostringstream& os) const
			{
				switch (type_)
				{
				case Type::Null: os << "null"; break;
				case Type::Bool: os << (bool_ ? "true" : "false"); break;
				case Type::Number:
					if (number_ == (double)(long long)number_) os << (long long)number_;
					else os << number_;
					break;
				case Type::String: writeEscapedString(os, string_); break;
				case Type::Array:
					os << '[';
					for (std::size_t i = 0; i < array_.size(); i++) { if (i) os << ','; array_[i].write(os); }
					os << ']';
					break;
				case Type::Object:
					os << '{';
					bool first = true;
					for (const auto& kv : object_)
					{
						if (!first) os << ',';
						first = false;
						writeEscapedString(os, kv.first);
						os << ':';
						kv.second.write(os);
					}
					os << '}';
					break;
				}
			}

			Type type_;
			bool bool_ = false;
			double number_ = 0;
			std::string string_;
			std::vector<JsonValue> array_;
			std::map<std::string, JsonValue> object_;
		};

		namespace detail_json
		{
			// Minimal recursive-descent parser over the raw text -- one
			// cursor position (pos), advanced as each construct is
			// consumed. Every parse* function assumes leading whitespace
			// has already been skipped by its caller.
			class Parser
			{
			public:
				explicit Parser(const std::string& text) : text_(text) {}

				JsonValue parse()
				{
					skipWhitespace();
					JsonValue v = parseValue();
					skipWhitespace();
					if (pos_ != text_.size())
						throw std::runtime_error("parseJson(): unexpected trailing content at offset " + std::to_string(pos_));
					return v;
				}

			private:
				void skipWhitespace()
				{
					while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' || text_[pos_] == '\r'))
						pos_++;
				}
				char peek() const
				{
					if (pos_ >= text_.size()) throw std::runtime_error("parseJson(): unexpected end of input");
					return text_[pos_];
				}
				void expect(char c)
				{
					if (peek() != c) throw std::runtime_error(std::string("parseJson(): expected '") + c + "' at offset " + std::to_string(pos_));
					pos_++;
				}
				bool consumeLiteral(const char* lit)
				{
					std::size_t len = std::strlen(lit);
					if (text_.compare(pos_, len, lit) == 0) { pos_ += len; return true; }
					return false;
				}

				JsonValue parseValue()
				{
					skipWhitespace();
					char c = peek();
					if (c == '{') return parseObject();
					if (c == '[') return parseArray();
					if (c == '"') return JsonValue(parseString());
					if (c == 't') { if (!consumeLiteral("true")) throw std::runtime_error("parseJson(): invalid literal at offset " + std::to_string(pos_)); return JsonValue(true); }
					if (c == 'f') { if (!consumeLiteral("false")) throw std::runtime_error("parseJson(): invalid literal at offset " + std::to_string(pos_)); return JsonValue(false); }
					if (c == 'n') { if (!consumeLiteral("null")) throw std::runtime_error("parseJson(): invalid literal at offset " + std::to_string(pos_)); return JsonValue(nullptr); }
					if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
					throw std::runtime_error("parseJson(): unexpected character at offset " + std::to_string(pos_));
				}

				JsonValue parseObject()
				{
					expect('{');
					JsonValue obj = JsonValue::makeObject();
					skipWhitespace();
					if (peek() == '}') { pos_++; return obj; }
					while (true)
					{
						skipWhitespace();
						std::string key = parseString();
						skipWhitespace();
						expect(':');
						obj[key] = parseValue();
						skipWhitespace();
						char c = peek();
						if (c == ',') { pos_++; continue; }
						if (c == '}') { pos_++; break; }
						throw std::runtime_error("parseJson(): expected ',' or '}' at offset " + std::to_string(pos_));
					}
					return obj;
				}

				JsonValue parseArray()
				{
					expect('[');
					JsonValue arr = JsonValue::makeArray();
					skipWhitespace();
					if (peek() == ']') { pos_++; return arr; }
					while (true)
					{
						arr.push_back(parseValue());
						skipWhitespace();
						char c = peek();
						if (c == ',') { pos_++; continue; }
						if (c == ']') { pos_++; break; }
						throw std::runtime_error("parseJson(): expected ',' or ']' at offset " + std::to_string(pos_));
					}
					return arr;
				}

				// Encodes a single UTF-16-ish code unit (from a \uXXXX
				// escape) as UTF-8 -- see this header's own top comment on
				// why lone surrogates aren't combined into one code point.
				static void appendUtf8(std::string& out, unsigned codeUnit)
				{
					if (codeUnit < 0x80) out += (char)codeUnit;
					else if (codeUnit < 0x800)
					{
						out += (char)(0xC0 | (codeUnit >> 6));
						out += (char)(0x80 | (codeUnit & 0x3F));
					}
					else
					{
						out += (char)(0xE0 | (codeUnit >> 12));
						out += (char)(0x80 | ((codeUnit >> 6) & 0x3F));
						out += (char)(0x80 | (codeUnit & 0x3F));
					}
				}

				std::string parseString()
				{
					expect('"');
					std::string out;
					while (true)
					{
						if (pos_ >= text_.size()) throw std::runtime_error("parseJson(): unterminated string");
						char c = text_[pos_++];
						if (c == '"') break;
						if (c == '\\')
						{
							if (pos_ >= text_.size()) throw std::runtime_error("parseJson(): unterminated escape");
							char e = text_[pos_++];
							switch (e)
							{
							case '"': out += '"'; break;
							case '\\': out += '\\'; break;
							case '/': out += '/'; break;
							case 'b': out += '\b'; break;
							case 'f': out += '\f'; break;
							case 'n': out += '\n'; break;
							case 'r': out += '\r'; break;
							case 't': out += '\t'; break;
							case 'u':
							{
								if (pos_ + 4 > text_.size()) throw std::runtime_error("parseJson(): truncated \\u escape");
								unsigned codeUnit = (unsigned)std::strtoul(text_.substr(pos_, 4).c_str(), nullptr, 16);
								pos_ += 4;
								appendUtf8(out, codeUnit);
								break;
							}
							default: throw std::runtime_error("parseJson(): invalid escape character at offset " + std::to_string(pos_ - 1));
							}
						}
						else out += c;
					}
					return out;
				}

				JsonValue parseNumber()
				{
					std::size_t start = pos_;
					if (peek() == '-') pos_++;
					while (pos_ < text_.size() && isdigit((unsigned char)text_[pos_])) pos_++;
					if (pos_ < text_.size() && text_[pos_] == '.')
					{
						pos_++;
						while (pos_ < text_.size() && isdigit((unsigned char)text_[pos_])) pos_++;
					}
					if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E'))
					{
						pos_++;
						if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) pos_++;
						while (pos_ < text_.size() && isdigit((unsigned char)text_[pos_])) pos_++;
					}
					std::string tok = text_.substr(start, pos_ - start);
					if (tok.empty() || tok == "-")
						throw std::runtime_error("parseJson(): invalid number at offset " + std::to_string(start));
					return JsonValue(std::strtod(tok.c_str(), nullptr));
				}

				const std::string& text_;
				std::size_t pos_ = 0;
			};
		}

		/// Parses a JSON text into a JsonValue tree. Throws std::runtime_error with a descriptive message (including
		/// the byte offset) on malformed input. See this header's own top comment for the (deliberately narrow) scope.
		/// @ingroup net
		inline JsonValue parseJson(const std::string& text)
		{
			detail_json::Parser parser(text);
			return parser.parse();
		}
	}
}
