#include <gtest/gtest.h>
#include <sstream>
#include <iostream>

#include <ndl/apps/appHelpers.h>

#include "testHelpers.h"

using namespace ndl::apps;

// Deliberately does NOT test --help/-h or any error path (unknown flag,
// missing value) here: those call std::exit() (by design -- see
// appHelpers.h's own comment on why that's the right behavior for a real
// CLI), which would tear down the whole gtest process, not just fail one
// TEST(). Those paths were instead verified directly against the real
// live_video_stream binary (--help, --bogus, --port with no value) when
// this framework was built. This file covers everything that DOESN'T exit:
// registration, parsing well-formed args, and defaults.

TEST(AppHelpers, DefaultsApplyWhenFlagNotGiven) {
	std::stringstream passfail;
	std::cout << std::endl << "APPHELPERS -- DEFAULTS" << std::endl;

	CliParser cli("testapp", "A test app.");
	cli.addOption("video", "<path>", "video file", "default.mp4");
	cli.addOption("port", "<port>", "port to listen on", "8901");
	char prog[] = "testapp";
	char* argv[] = { prog };
	cli.parse(1, argv); // no flags given at all

	bool ok = cli.get("video") == "default.mp4" && cli.getInt("port") == 8901;
	passfail << "get()/getInt() fall back to each option's own registered default when not given: " << (ok ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(AppHelpers, ParsesGivenValuesOverridingDefaults) {
	std::stringstream passfail;
	std::cout << std::endl << "APPHELPERS -- PARSED VALUES OVERRIDE DEFAULTS" << std::endl;

	CliParser cli("testapp", "A test app.");
	cli.addOption("video", "<path>", "video file", "default.mp4");
	cli.addOption("port", "<port>", "port to listen on", "8901");
	char prog[] = "testapp", flag1[] = "--video", val1[] = "custom.mp4", flag2[] = "--port", val2[] = "9999";
	char* argv[] = { prog, flag1, val1, flag2, val2 };
	cli.parse(5, argv);

	bool ok = cli.get("video") == "custom.mp4" && cli.getInt("port") == 9999;
	passfail << "explicitly given values override the registered defaults: " << (ok ? "Pass" : "Fail") << " (video=" << cli.get("video") << ", port=" << cli.getInt("port") << ")" << std::endl;

	reportPassFail(passfail);
}

TEST(AppHelpers, BooleanFlagsReportPresence) {
	std::stringstream passfail;
	std::cout << std::endl << "APPHELPERS -- BOOLEAN FLAGS" << std::endl;

	CliParser cli("testapp", "A test app.");
	cli.addFlag("verbose", "enable verbose output");
	cli.addFlag("quiet", "suppress output");
	char prog[] = "testapp", flag[] = "--verbose";
	char* argv[] = { prog, flag };
	cli.parse(2, argv);

	bool ok = cli.present("verbose") && !cli.present("quiet");
	passfail << "present() correctly reports a given boolean flag as present and an ungiven one as absent: " << (ok ? "Pass" : "Fail") << std::endl;

	reportPassFail(passfail);
}

TEST(AppHelpers, GetDoubleParsesFloatingPointValues) {
	std::stringstream passfail;
	std::cout << std::endl << "APPHELPERS -- getDouble()" << std::endl;

	CliParser cli("testapp", "A test app.");
	cli.addOption("fps", "<fps>", "frame rate", "10.0");
	char prog[] = "testapp", flag[] = "--fps", val[] = "29.97";
	char* argv[] = { prog, flag, val };
	cli.parse(3, argv);

	bool ok = cli.getDouble("fps") == 29.97;
	passfail << "getDouble() parses a floating-point value correctly: " << (ok ? "Pass" : "Fail") << " (got " << cli.getDouble("fps") << ")" << std::endl;

	reportPassFail(passfail);
}
