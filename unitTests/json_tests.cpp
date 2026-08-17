#include <gtest/gtest.h>
#include <string>
#include <array>
#include <sstream>
#include <iostream>

#include <ndl/net/json.h>

#include "testHelpers.h"

using namespace ndl::net;

TEST(Json, ParsesARealViewportControlMessage) {
	std::stringstream passfail;
	std::cout << std::endl << "JSON -- PARSES A REAL VIEWPORT-PROTOCOL-SHAPED MESSAGE" << std::endl;

	// The exact shape of an updateViewport message (see viewport.h's own
	// wire protocol comment) -- nested object, numbers (int and float),
	// strings, one level deep. If this stops parsing correctly, the whole
	// live-streaming protocol built on top of it breaks silently.
	std::string text = R"({"type":"updateViewport","id":"panelA","params":{"axisI":1,"axisJ":2,"cropMin":[10,20],"cropMax":[110,220],"outputWidth":256,"outputHeight":256,"windowMin":0,"windowMax":255.5,"timeIndex":"latest"}})";
	JsonValue msg = parseJson(text);

	bool topLevelOk = msg.isObject() && msg["type"].asString() == "updateViewport" && msg["id"].asString() == "panelA";
	passfail << "top-level type/id fields parse correctly: " << (topLevelOk ? "Pass" : "Fail") << std::endl;

	const JsonValue& params = msg["params"];
	bool numbersOk = params["axisI"].asInt() == 1 && params["axisJ"].asInt() == 2
		&& params["outputWidth"].asInt() == 256 && params["windowMax"].asNumber() == 255.5;
	passfail << "nested numeric params (int and float) parse correctly: " << (numbersOk ? "Pass" : "Fail") << std::endl;

	const auto& cropMin = params["cropMin"].asArray();
	bool arrayOk = cropMin.size() == 2 && cropMin[0].asInt() == 10 && cropMin[1].asInt() == 20;
	passfail << "nested array (cropMin) parses correctly: " << (arrayOk ? "Pass" : "Fail") << std::endl;

	bool stringValueOk = params["timeIndex"].asString() == "latest";
	passfail << "a string-typed param value (\"latest\") parses correctly: " << (stringValueOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Json, NumbersOrHandlesPresentMissingAndShortArrays) {
	std::stringstream passfail;
	std::cout << std::endl << "JSON -- numbersOr()" << std::endl;

	std::array<double, 3> fallback = { 1, 2, 3 };

	// Present, exact length -- every entry overwritten.
	JsonValue full = parseJson(R"({"rotation":[9,8,7]})");
	auto r1 = full.numbersOr("rotation", fallback);
	bool exactLen = r1[0] == 9 && r1[1] == 8 && r1[2] == 7;
	passfail << "present, exact-length array overwrites every entry: " << (exactLen ? "Pass" : "Fail") << std::endl;

	// Missing key -- fallback returned unchanged.
	JsonValue empty = parseJson(R"({})");
	auto r2 = empty.numbersOr("rotation", fallback);
	bool missingOk = r2 == fallback;
	passfail << "missing key returns the fallback unchanged: " << (missingOk ? "Pass" : "Fail") << std::endl;

	// Short array -- only the leading entries overwritten, the rest keep
	// their own fallback value (viewer/viewport.h's own renderVolume()
	// relies on exactly this for a partial "rotation" array).
	JsonValue partial = parseJson(R"({"rotation":[42]})");
	auto r3 = partial.numbersOr("rotation", fallback);
	bool partialOk = r3[0] == 42 && r3[1] == 2 && r3[2] == 3;
	passfail << "short array overwrites only its own leading entries: " << (partialOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Json, RoundTripsThroughToString) {
	std::stringstream passfail;
	std::cout << std::endl << "JSON -- toString() ROUND-TRIP" << std::endl;

	JsonValue obj = JsonValue::makeObject();
	obj["name"] = std::string("slice");
	obj["count"] = 42;
	obj["ratio"] = 3.5;
	obj["enabled"] = true;
	obj["nothing"] = nullptr;
	JsonValue arr = JsonValue::makeArray();
	arr.push_back(1); arr.push_back(2); arr.push_back(3);
	obj["values"] = arr;

	JsonValue reparsed = parseJson(obj.toString());
	bool ok = reparsed["name"].asString() == "slice" && reparsed["count"].asInt() == 42
		&& reparsed["ratio"].asNumber() == 3.5 && reparsed["enabled"].asBool() == true
		&& reparsed["nothing"].isNull() && reparsed["values"].asArray().size() == 3
		&& reparsed["values"].asArray()[2].asInt() == 3;
	passfail << "parseJson(obj.toString()) reproduces every field's value and type: " << (ok ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Json, StringEscapesRoundTrip) {
	std::stringstream passfail;
	std::cout << std::endl << "JSON -- STRING ESCAPES" << std::endl;

	std::string original = "line1\nline2\ttabbed \"quoted\" back\\slash";
	JsonValue v(original);
	JsonValue reparsed = parseJson(v.toString());
	bool ok = reparsed.asString() == original;
	passfail << "a string with newline/tab/quote/backslash round-trips through toString()+parseJson() unchanged: " << (ok ? "Pass" : "Fail") << std::endl;

	JsonValue fromEscape = parseJson("\"unicode: \\u0041\\u00e9\"");
	bool unicodeOk = fromEscape.asString() == "unicode: A\xc3\xa9"; // 'A' (U+0041) + U+00E9 as UTF-8 (0xC3 0xA9)
	passfail << "\\uXXXX escapes decode to the correct UTF-8 bytes: " << (unicodeOk ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Json, NumberFormsParseCorrectly) {
	std::stringstream passfail;
	std::cout << std::endl << "JSON -- NUMBER FORMS" << std::endl;

	bool ok = parseJson("42").asNumber() == 42.0
		&& parseJson("-17").asNumber() == -17.0
		&& parseJson("3.5").asNumber() == 3.5
		&& parseJson("-0.25").asNumber() == -0.25
		&& parseJson("1e3").asNumber() == 1000.0
		&& parseJson("2.5e-2").asNumber() == 0.025;
	passfail << "integer, negative, decimal, and exponent forms all parse to the correct value: " << (ok ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Json, MalformedInputThrows) {
	std::stringstream passfail;
	std::cout << std::endl << "JSON -- MALFORMED INPUT" << std::endl;

	auto throws = [](const std::string& text) {
		try { parseJson(text); return false; }
		catch (const std::runtime_error&) { return true; }
	};

	bool unterminatedString = throws("{\"a\":\"b}");
	passfail << "unterminated string throws: " << (unterminatedString ? "Pass" : "Fail") << std::endl;

	bool missingColon = throws("{\"a\" 1}");
	passfail << "missing ':' throws: " << (missingColon ? "Pass" : "Fail") << std::endl;

	bool trailingComma = throws("[1,2,]");
	passfail << "trailing comma in an array throws: " << (trailingComma ? "Pass" : "Fail") << std::endl;

	bool trailingGarbage = throws("{}  garbage");
	passfail << "trailing content after a complete value throws: " << (trailingGarbage ? "Pass" : "Fail") << std::endl;

	bool emptyInput = throws("");
	passfail << "empty input throws: " << (emptyInput ? "Pass" : "Fail") << std::endl;

	bool invalidLiteral = throws("nul");
	passfail << "a truncated literal (\"nul\") throws: " << (invalidLiteral ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(Json, WrongTypeAccessThrows) {
	std::stringstream passfail;
	std::cout << std::endl << "JSON -- WRONG-TYPE ACCESS" << std::endl;

	JsonValue num(5.0);
	bool threw = false;
	try { num.asString(); } catch (const std::runtime_error&) { threw = true; }
	passfail << "asString() on a Number value throws rather than returning garbage: " << (threw ? "Pass" : "Fail") << std::endl;

	JsonValue obj = JsonValue::makeObject();
	bool missingKeyThrew = false;
	try { obj["missing"].asNumber(); (void)obj.has("missing"); JsonValue copy = obj; (void)copy; obj.asObject().at("missing"); }
	catch (const std::out_of_range&) { missingKeyThrew = true; }
	catch (const std::runtime_error&) { missingKeyThrew = true; }
	passfail << "accessing a missing object key throws: " << (missingKeyThrew ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}
