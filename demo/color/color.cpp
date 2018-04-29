#include <ctime>
#include <ndl/image.h>
using namespace ndl;

int main()
{
    std::clock_t start;
    int i=0, ncolor=3, width=3, height=3, depth=3;
    std::vector<int> imageData( width*height*depth*ncolor);
    Image<int, 4> colorVolume(imageData.data(), { width, height, depth, ncolor });

    start = std::clock();
    std::cout << "start1\n";
    for (auto it = imageData.begin(); it != imageData.end(); ++it)
        *it = ++i;
    std::cout << "end: " << ( std::clock() - start ) / (double) CLOCKS_PER_SEC << "\n"  << colorVolume << "\n";

    i=0;
    start = std::clock();
    std::cout << "start2\n";
    for (auto it = colorVolume.begin(); it != colorVolume.end(); ++it)
        *it = ++i;
    std::cout << "end: " << ( std::clock() - start ) / (double) CLOCKS_PER_SEC << "\n" << colorVolume << "\n";
}
