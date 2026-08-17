#pragma once
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <cstdlib>
#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

// A small, genuinely reusable CLI framework for apps/ programs -- named
// flags (`--flag value`) plus boolean flags and an auto-generated
// `--help`/`-h`, rather than each app hand-rolling its own raw positional
// `argv[1]`/`argv[2]` parsing (which is what live_video_stream did before
// this existed, and doesn't scale past one or two arguments, or self-
// document at all). Shared by every apps/ program, matching the plural
// "apps" in how this was asked for -- demoHelpers.h is the equivalent for
// demo/, this is the equivalent for apps/.
//
// Deliberately small: no short-flag combining, no `--flag=value` syntax,
// no positional arguments alongside named ones -- just what these
// programs' own small, flat option sets actually need. #include this
// directly; it's not pulled in by anything else.
namespace ndl
{
	namespace apps
	{
		namespace detail_apps
		{
			struct CliOption
			{
				std::string name;             // matched as --name
				std::string valuePlaceholder; // e.g. "<path>"; empty means a boolean flag, not a value-taking one
				std::string description;
				std::string defaultValue;     // stringified; shown in --help, and what get()/getInt() fall back to
				bool hasValue;
			};
		}

		/// A small named-flag CLI parser with an auto-generated `--help`/`-h`. See this header's own top comment.
		/// @ingroup apps
		class CliParser
		{
		public:
			/// @param programName Shown in usage/error text (typically the executable's own name).
			/// @param description One-line (or short multi-line) summary shown at the top of `--help`.
			CliParser(std::string programName, std::string description) :
				programName_(std::move(programName)), description_(std::move(description))
			{
			}

			/// Registers a value-taking option (`--name <valuePlaceholder>`).
			/// @param name             Flag name, without the leading `--`.
			/// @param valuePlaceholder Shown in `--help` (e.g. `"<path>"`); must be non-empty for a value-taking option.
			/// @param description      Shown in `--help`.
			/// @param defaultValue     Stringified default; what get()/getInt()/getDouble() return when the flag wasn't given.
			/// @return `*this`, for chaining.
			CliParser& addOption(const std::string& name, const std::string& valuePlaceholder, const std::string& description, const std::string& defaultValue = "")
			{
				options_.push_back({ name, valuePlaceholder, description, defaultValue, true });
				return *this;
			}

			/// Registers a boolean flag (`--name`, no value) -- present() reports whether it was given.
			/// @return `*this`, for chaining.
			CliParser& addFlag(const std::string& name, const std::string& description)
			{
				options_.push_back({ name, "", description, "", false });
				return *this;
			}

			/// Parses argv[1..argc). On `--help`/`-h`, prints usage to stdout and calls std::exit(0) (never
			/// returns in that case). On an unrecognized flag, a missing required value, or a stray
			/// (unrecognized-as-any-flag) argument, prints an error + usage to stderr and calls std::exit(1) --
			/// matching the standard CLI convention every apps/ program should behave the same way for.
			void parse(int argc, char** argv)
			{
				for (int i = 1; i < argc; i++)
				{
					std::string arg = argv[i];
					if (arg == "--help" || arg == "-h") { printUsage(std::cout); std::exit(0); }
					if (arg.size() > 2 && arg[0] == '-' && arg[1] == '-')
					{
						std::string name = arg.substr(2);
						auto it = findOption(name);
						if (it == options_.end())
						{
							std::cerr << programName_ << ": unknown option --" << name << "\n\n";
							printUsage(std::cerr);
							std::exit(1);
						}
						if (it->hasValue)
						{
							if (i + 1 >= argc)
							{
								std::cerr << programName_ << ": --" << name << " requires a value\n\n";
								printUsage(std::cerr);
								std::exit(1);
							}
							values_[name] = argv[++i];
						}
						else values_[name] = "1";
					}
					else
					{
						std::cerr << programName_ << ": unexpected argument \"" << arg << "\"\n\n";
						printUsage(std::cerr);
						std::exit(1);
					}
				}
			}

			/// The value for `name` (as given on the command line, or its registered default if not given).
			std::string get(const std::string& name) const
			{
				auto v = values_.find(name);
				if (v != values_.end()) return v->second;
				auto opt = findOption(name);
				return opt != options_.end() ? opt->defaultValue : "";
			}
			int getInt(const std::string& name) const { return std::atoi(get(name).c_str()); }
			double getDouble(const std::string& name) const { return std::atof(get(name).c_str()); }
			/// True if a boolean flag (addFlag()) was actually given on the command line.
			bool present(const std::string& name) const { return values_.count(name) != 0; }

			void printUsage(std::ostream& os) const
			{
				os << description_ << "\n\nUsage: " << programName_ << " [options]\n\nOptions:\n";
				for (const auto& opt : options_)
				{
					std::string left = "  --" + opt.name;
					if (opt.hasValue) left += " " + opt.valuePlaceholder;
					os << left;
					for (int pad = (int)left.size(); pad < 28; pad++) os << ' ';
					os << opt.description;
					if (!opt.defaultValue.empty()) os << " (default: " << opt.defaultValue << ")";
					os << "\n";
				}
				os << "  --help, -h                show this help message\n";
			}

		private:
			std::vector<detail_apps::CliOption>::const_iterator findOption(const std::string& name) const
			{
				for (auto it = options_.begin(); it != options_.end(); ++it) if (it->name == name) return it;
				return options_.end();
			}

			std::string programName_, description_;
			std::vector<detail_apps::CliOption> options_;
			std::map<std::string, std::string> values_;
		};

		namespace detail_apps
		{
			// std::signal() takes a plain function pointer, not a capturing
			// lambda/std::function, so a SIGINT handler needs somewhere
			// static to reach the flag it should clear -- one at a time is
			// all any apps/ program here ever needs (each only calls
			// installSigintHandler() once, from its own main()).
			inline std::atomic<bool>* g_runningFlag = nullptr;
			inline void handleSigintFlag(int) { if (g_runningFlag) *g_runningFlag = false; }
		}

		// Installs a SIGINT handler that clears `running` -- the same
		// "Ctrl-C stops the main loop" wiring every apps/ live-streaming
		// program needs (see e.g. apps/live_video_stream.cpp's own main()).
		inline void installSigintHandler(std::atomic<bool>& running)
		{
			detail_apps::g_runningFlag = &running;
			std::signal(SIGINT, detail_apps::handleSigintFlag);
		}

		/// True if `envVar` is set in the environment.
		inline bool selfTestMode(const char* envVar) { return std::getenv(envVar) != nullptr; }

		// Every apps/ live-streaming program is SUPPOSED to run forever
		// until stopped -- unlike demo/'s own run-to-completion programs,
		// which ctest smoke-tests with the ordinary "run it, expect exit
		// 0" pattern. Rather than have ctest juggle external process
		// signaling (timeout/kill, with its own portability and exit-code
		// subtleties), self-test mode makes the process behave like an
		// ordinary demo for exactly this purpose: it still does everything
		// a real run does (opens its source, starts the WebSocket server,
		// streams a few samples), it just also stops itself after a short,
		// fixed duration.
		/// Returns a thread that, if selfTestMode(envVar), sleeps `ms` then clears `running` -- join it (if joinable()) before returning from main(). A default-constructed (not joinable) thread otherwise.
		inline std::thread selfTestWatchdog(const char* envVar, std::atomic<bool>& running, int ms = 800)
		{
			if (!selfTestMode(envVar)) return std::thread();
			return std::thread([&running, ms] { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); running = false; });
		}
	}
}
