#include <ndl/image.h>
using namespace ndl;

int main()
{
    //setup a 4D image, use a vector for the memory storage
    int ncolor=3, width=3, height=3, depth=3;
    std::vector<int> imageData( width*height*depth*ncolor);
    Image<int, 4> ndImage(imageData.data(), { width, height, depth, ncolor });

    // initialize image values using the underlying vector
    int i=0;
    for (auto it = imageData.begin(); it != imageData.end(); ++it)
        *it = ++i;
    
    // display the image
    std::cout << "image: \n" << ndImage << std::endl;

    // mirror along each dimension
    std::cout << "image mirroredX: \n" << ndImage({{0,-1,-1},_,_,_}) << std::endl;
    std::cout << "image mirroredY: \n" << ndImage({_,{0,-1,-1},_,_}) << std::endl;
    std::cout << "image mirroredZ: \n" << ndImage({_,_,{0,-1,-1},_}) << std::endl;
    std::cout << "image mirroredC: \n" << ndImage({_,_,_,{0,-1,-1}}) << std::endl;

    // mirror twice
    std::cout << "image mirroredC twice: \n" << ndImage({_,_,_,{0,-1,-1}})({_,_,_,{0,-1,-1}}) << std::endl;

    // region of interest
    std::cout << "image roi: \n" << ndImage({{1,-2},_,{1,-1},_}) << std::endl;

    // mirrored roi
    std::cout << "roi, then mirror in Z: \n" << ndImage({{1,-2},_,{1,-1},_})({_,_,{0,-1,-1},_}) << std::endl;
    std::cout << "roi and mirror in Z at once: \n" << ndImage({{1,-2, -1},_,{1,-1, -1},_}) << std::endl;

    // decimate
    std::cout << "image decimateXY: \n" << ndImage({{0,-1, 2},{0,-1, 2},_,_}) << std::endl;
    std::cout << "image decimateXZ: \n" << ndImage({{0,-1, 2},_,{0,-1, 2},_}) << std::endl;

    // decimated roi
    // TODO: check this one!!
    std::cout << "image decimatedX roiX: \n" << ndImage({{1,-1, 2},_,_,_}) << std::endl;
}
