#pragma once
#include <array>
#include <sstream>
#include <iomanip>
#include <functional>
#include <algorithm>
#include "core.h"

namespace ndl
{
	//operator overloads
	//
	// Prints dim0 as rows (comma-separated values) and dim1 as which row within a
	// block, both implicit the way any 2D grid is. That stops being enough at 3+
	// dimensions -- there's nothing to distinguish "a blank line because dim2 just
	// advanced" from "a blank line because dim3 just advanced" other than counting
	// them. So for N >= 3, every 2D (dim1 x dim0) block is preceded by an explicit
	// "[dim2=.., dim3=.., ...]" header naming every higher dimension's current
	// index, in addition to the blank-line spacing (kept for readability).
	template<class T, int N>
	std::ostream& operator<<(std::ostream& sb, const Image<T, N>& r)
	{
		sb << std::fixed << std::setprecision(2);

		// A fixed field width, wide enough for every element, keeps columns
		// aligned down the grid regardless of how many digits or a minus sign
		// any individual value needs -- without it, a single wider number
		// (e.g. 100.00 next to 1.00) staggers every column after it. Measured
		// by actually formatting each element to a scratch stream rather than
		// guessing from min/max, so precision rounding (e.g. -0.00) can't
		// throw the width off.
		std::size_t width = 0;
		for (const auto& coord : r.coordinates())
		{
			std::ostringstream probe;
			probe << std::fixed << std::setprecision(2) << static_cast<double>(r.at(coord));
			width = std::max(width, probe.str().size());
		}

		std::array<int, N> indices = {0}; // Initialize index array

		// Lambda to handle recursion within the same function
		std::function<void(int)> printImage = [&](int dim)
		{
			if (dim == 0) // Base case: last dimension, print the elements
			{
				for (int i = 0; i < r.extent()[dim]; i++)
				{
					indices[dim] = i;
					sb << std::setw(width) << static_cast<double>(r.at(indices));
					sb << ", ";
				}
				sb << std::endl;
			}
			else // Recursive case: iterate through the current dimension
			{
				for (int i = 0; i < r.extent()[dim]; i++)
				{
					indices[dim] = i;
					// dim==2 is always exactly one level above the 2D (dim1 x dim0)
					// block that dim==1's loop is about to print, regardless of how
					// large N is -- every dimension above it (indices[3..N-1]) has
					// already been fixed by the enclosing recursive calls by now.
					if (dim == 2)
					{
						sb << "[";
						for (int d = 2; d < N; d++)
						{
							if (d > 2) sb << ", ";
							sb << "dim" << d << "=" << indices[d];
						}
						sb << "]" << std::endl;
					}
					printImage(dim - 1);
				}
				sb << std::endl;
			}
		};

		printImage(N - 1); // Start recursion from the first dimension
		return sb;
	}


	template<class T, class U, int DIM>
	bool operator<(const T& lhs, const Image<U, DIM>& rhs) {
		for (auto rhsIt = rhs.begin(); rhsIt != rhs.end(); ++rhsIt) if (lhs >= *rhsIt) return false;
		return true;
	}
}
