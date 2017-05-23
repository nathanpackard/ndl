#pragma once
#include <chrono>
#include <utility>
namespace ndl
{
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

	template<typename datatype, typename size_type>
	void change_endian(datatype* data, size_type count)
	{
		for (unsigned int index = 0; index < count; ++index)
			change_endian(data[index]);
	}

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