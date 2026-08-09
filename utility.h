#pragma once
#include <chrono>
#include <utility>
#include <functional>
#include <iostream>
#include <string>
namespace ndl
{
	/// Byte-swaps `value` in place (generic fallback; see the overloads below for common sizes).
	/// @tparam type Any trivially-copyable type; swapped byte-for-byte, so this is only meaningful for the fixed-width numeric types real files actually store.
	/// @param  value Reversed in place.
	/// @ingroup utilities
	template<typename type>
	void change_endian(type& value)
	{
		type data = value;
		unsigned char temp[sizeof(type)];
		unsigned char* pdata = ((unsigned char*)&data) + sizeof(type) - 1;
		for (char i = 0; i < sizeof(type); ++i, --pdata)
			temp[i] = *pdata;
		value = *(type*)temp;
	}
	inline void change_endian(unsigned short& data)
	{
		unsigned char* h = (unsigned char*)&data;
		std::swap(*h, *(h + 1));
	}

	inline void change_endian(short& data)
	{
		unsigned char* h = (unsigned char*)&data;
		std::swap(*h, *(h + 1));
	}


	inline void change_endian(unsigned int& data)
	{
		unsigned char* h = (unsigned char*)&data;
		std::swap(*h, *(h + 3));
		std::swap(*(h + 1), *(h + 2));
	}

	inline void change_endian(int& data)
	{
		unsigned char* h = (unsigned char*)&data;
		std::swap(*h, *(h + 3));
		std::swap(*(h + 1), *(h + 2));
	}

	inline void change_endian(float& data)
	{
		change_endian(*(int*)&data);
	}

	/// Byte-swaps `count` consecutive elements in place.
	/// @tparam datatype  Element type.
	/// @tparam size_type Count's type (any integral type).
	/// @param  data      Array of `count` elements, each reversed in place.
	/// @param  count     Number of elements.
	/// @ingroup utilities
	template<typename datatype, typename size_type>
	void change_endian(datatype* data, size_type count)
	{
		for (unsigned int index = 0; index < count; ++index)
			change_endian(data[index]);
	}

	/// A simple stopwatch, in milliseconds.
	/// @ingroup utilities
	class Timer {
		typedef std::chrono::high_resolution_clock high_resolution_clock;
		typedef std::chrono::milliseconds milliseconds;
	public:
		/// @param run If true, starts the clock immediately (equivalent to constructing then calling reset()). Defaults to false.
		explicit Timer(bool run = false)
		{
			if (run) reset();
		}
		/// Restarts the clock at zero, counting from now.
		void reset()
		{
			_start = high_resolution_clock::now();
		}
		/// @return Elapsed time since construction (or the last reset()), in milliseconds.
		milliseconds elapsed() const
		{
			return std::chrono::duration_cast<milliseconds>(high_resolution_clock::now() - _start);
		}
		template <typename T, typename Traits>
		friend std::basic_ostream<T, Traits>& operator<<(std::basic_ostream<T, Traits>& out, const Timer& timer)
		{
			return out << timer.elapsed().count();
		}
	private:
		high_resolution_clock::time_point _start;
	};
	/// Times `func_obj` (run `iterations` times), prints and returns the elapsed milliseconds.
	/// @param text       Label printed alongside the timing.
	/// @param func_obj   Callable to time; invoked `iterations` times back to back.
	/// @param iterations Number of times to run `func_obj`. Defaults to 1.
	/// @return           Total elapsed milliseconds across all `iterations` runs.
	/// @ingroup utilities
	double code_timer(std::string text, std::function<void()> func_obj, int iterations = 1)
	{
		Timer timer(true);
		for (int i = 0; i < iterations; i++) func_obj();
		double time = timer.elapsed().count();
		if (iterations == 1) std::cout << text << " took " << timer << " milliseconds." << std::endl;
		else std::cout << iterations << " iterations of " << text << " took " << timer << " milliseconds." << std::endl;
		return time;
	}
}