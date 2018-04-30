#include <ndl/image.h>
using namespace ndl;

int main()
{
    //setup a 4D image, use a vector for the memory storage
    int ncolor=3, width=4, height=3, depth=3;
    std::vector<int> imageData( width*height*depth*ncolor);
    Image<int, 4> ndImage(imageData.data(), { width, height, depth, ncolor });

    // initialize image values using the underlying vector
    int i=0;
    for (auto it = imageData.begin(); it != imageData.end(); ++it)
        *it = ++i;
    
    // display the image
    std::cout << "ndImage: \n" << ndImage << std::endl;

    // region of interest
    std::cout << "image roi, ndImage({{0,2},_,_}): \n" << ndImage({{0,2},_,_,_}) << std::endl;
    std::cout << "image roi, ndImage({{1,2},_,{1,2},_}): \n" << ndImage({{1,2},_,{1,2},_}) << std::endl;
    std::cout << "image roi, negative notation, ndImage({{1,-3},_,{1,-1},_}): \n" << ndImage({{1,-2},_,{1,-1},_}) << std::endl;

    // mirror along each dimension
    std::cout << "image mirroredX, ndImage({{0,-1,-1},_,_,_}): \n" << ndImage({{0,-1,-1},_,_,_}) << std::endl;
    std::cout << "image mirroredY, ndImage({_,{0,-1,-1},_,_}): \n" << ndImage({_,{0,-1,-1},_,_}) << std::endl;
    std::cout << "image mirroredZ, ndImage({_,_,{0,-1,-1},_}): \n" << ndImage({_,_,{0,-1,-1},_}) << std::endl;
    std::cout << "image mirroredC, ndImage({_,_,_,{0,-1,-1}}): \n" << ndImage({_,_,_,{0,-1,-1}}) << std::endl;

    // mirror twice
    std::cout << "image mirroredC twice, ndImage({_,_,_,{0,-1,-1}})({_,_,_,{0,-1,-1}}): \n" << ndImage({_,_,_,{0,-1,-1}})({_,_,_,{0,-1,-1}}) << std::endl;

    // simplified notation
    std::cout << "image mirroredY, ndImage({_,{0,-1,-1},_,_}): \n" << ndImage({_,{0,-1,-1},_,_}) << std::endl;
    std::cout << "image mirroredY simplified notation1, ndImage({_,{0,-1,-1}}): \n" << ndImage({_,{0,-1,-1}}) << std::endl;
    std::cout << "image mirroredY simplified notation2, ndImage({_,{_,_,-1}}): \n" << ndImage({_,{_,_,-1}}) << std::endl;

    // mirrored roi
    std::cout << "roi, then mirror in Z, ndImage({{1,-2},_,{1,-1}})({_,_,{0,-1,-1}}): \n" << ndImage({{1,-2},_,{1,-1}})({_,_,{0,-1,-1}}) << std::endl;
    std::cout << "roi and mirror in Z at once, ndImage({{1,-2},_,{1,-1, -1}}): \n" << ndImage({{1,-2},_,{1,-1, -1}}) << std::endl;

    // decimate
    std::cout << "image decimateX, ndImage({{0,-1, 2}}): \n" << ndImage({{0,-1, 2}}) << std::endl;
    std::cout << "image decimateXY, ndImage({{0,-1, 2},{0,-1, 2}}): \n" << ndImage({{0,-1, 2},{0,-1, 2}}) << std::endl;
    std::cout << "image decimateXZ, ndImage({{0,-1, 2},_,{0,-1, 2}}): \n" << ndImage({{0,-1, 2},_,{0,-1, 2}}) << std::endl;

    // decimated roi
    std::cout << "image decimatedX roiX, ndImage({{1,-1, 2}}): \n" << ndImage({{1,-1, 2}}) << std::endl;
}
