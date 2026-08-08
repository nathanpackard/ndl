#include <ndl/image.h>
using namespace ndl;

// Demonstrates memory-sharing "views" into a single n-dimensional image:
// regions of interest, mirroring, and decimation, all without copying data.
//
// Image::operator() takes up to three initializer_lists (start, end, step),
// one value per dimension consumed left-to-right; any dimension past the end
// of a list gets its default (start=0, end=last index, step=1). So a
// trailing run of default dimensions can simply be omitted from the list.

int main()
{
    //setup a 4D image, use a vector for the memory storage
    std::array<int, 4> s{4, 3, 3, 3};
    std::vector<int> imageData(Image<int, 4>::size(s));
    Image<int, 4> ndImage(imageData.data(), s);

    // initialize image values using the underlying vector
    int i=0;
    for (auto it = imageData.begin(); it != imageData.end(); ++it)
        *it = ++i;

    // display the image
    std::cout << "ndImage: \n" << ndImage << std::endl;

    // display a region of interest
    std::cout << "image roi, ndImage({0}, {2}): \n" << ndImage({0}, {2}) << std::endl;
    std::cout << "image roi, ndImage({1,0,1}, {2,-1,2}): \n" << ndImage({1,0,1}, {2,-1,2}) << std::endl;
    std::cout << "image roi, negative notation, ndImage({1,0,1}, {-2,-1,-1}): \n" << ndImage({1,0,1}, {-2,-1,-1}) << std::endl;

    // display a mirror along each dimension
    std::cout << "image mirroredX, ndImage({}, {}, {-1,1,1,1}): \n" << ndImage({}, {}, {-1,1,1,1}) << std::endl;
    std::cout << "image mirroredY, ndImage({}, {}, {1,-1,1,1}): \n" << ndImage({}, {}, {1,-1,1,1}) << std::endl;
    std::cout << "image mirroredZ, ndImage({}, {}, {1,1,-1,1}): \n" << ndImage({}, {}, {1,1,-1,1}) << std::endl;
    std::cout << "image mirroredC, ndImage({}, {}, {1,1,1,-1}): \n" << ndImage({}, {}, {1,1,1,-1}) << std::endl;

    // display a double mirror
    std::cout << "image mirroredC double, ndImage({}, {}, {1,1,1,-1})({}, {}, {1,1,1,-1}): \n" << ndImage({}, {}, {1,1,1,-1})({}, {}, {1,1,1,-1}) << std::endl;

    // display a simplified notation (trailing default dimensions omitted)
    std::cout << "image mirroredY, ndImage({}, {}, {1,-1,1,1}): \n" << ndImage({}, {}, {1,-1,1,1}) << std::endl;
    std::cout << "image mirroredY simplified notation, ndImage({}, {}, {1,-1}): \n" << ndImage({}, {}, {1,-1}) << std::endl;

    // display a mirrored roi
    std::cout << "roi, then mirror in Z, ndImage({1,0,1}, {-2,-1,-1})({}, {}, {1,1,-1}): \n" << ndImage({1,0,1}, {-2,-1,-1})({}, {}, {1,1,-1}) << std::endl;
    std::cout << "roi and mirror in Z at once, ndImage({1,0,1}, {-2,-1,-1}, {1,1,-1}): \n" << ndImage({1,0,1}, {-2,-1,-1}, {1,1,-1}) << std::endl;

    // display a decimation
    std::cout << "image decimateX, ndImage({}, {}, {2}): \n" << ndImage({}, {}, {2}) << std::endl;
    std::cout << "image decimateXY, ndImage({}, {}, {2,2}): \n" << ndImage({}, {}, {2,2}) << std::endl;
    std::cout << "image decimateXZ, ndImage({}, {}, {2,1,2}): \n" << ndImage({}, {}, {2,1,2}) << std::endl;

    // display a decimated roi
    std::cout << ndImage.state();
    std::cout << "image decimatedX roiX, ndImage({1}, {-1}, {2}): \n" << ndImage({1}, {-1}, {2}) << std::endl;
    std::cout << ndImage({1}, {-1}, {2}).state();
}
