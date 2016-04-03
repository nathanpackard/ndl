#pragma once
#include <chrono>
namespace timeUtils
{
	class Timer {
		typedef std::chrono::high_resolution_clock high_resolution_clock;
		typedef std::chrono::milliseconds milliseconds;
	public:
		explicit Timer(bool run = false)
		{
			if (run) Reset();
		}
		void Reset()
		{
			_start = high_resolution_clock::now();
		}
		milliseconds Elapsed() const
		{
			return std::chrono::duration_cast<milliseconds>(high_resolution_clock::now() - _start);
		}
		template <typename T, typename Traits>
		friend std::basic_ostream<T, Traits>& operator<<(std::basic_ostream<T, Traits>& out, const Timer& timer)
		{
			return out << timer.Elapsed().count();
		}
	private:
		high_resolution_clock::time_point _start;
	};
	double codeTimer(std::string text, std::function<void()> func_obj, int iterations = 1)
	{
		Timer timer(true);
		for (int i = 0; i < iterations; i++) func_obj();
		double time = timer.Elapsed().count();
		if (iterations == 1) std::cout << text << " took " << timer << " milliseconds." << std::endl;
		else std::cout << iterations << " iterations of " << text << " took " << timer << " milliseconds." << std::endl;
		return time;
	}
}