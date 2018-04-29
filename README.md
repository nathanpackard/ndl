# ndl
ndl is a multi-dimensional image processing library written in C++. ndl has the following features:
* multi-dimensional images: Images can be created of any data type with an arbitrary number of dimensions. Colors can be simply represented as an extra dimension
* smart-iterators: a single loop can iterate over a multi-dimensional image (no need for nested loops). The iterator keeps track of the n-dimensional coordinate for fast access to a local neighborhood within the image. This enables clean implementations of multi-dimensional neighborhood based filters.
* memory sharing: image data is provided at construction so that multiple image objects can reference the same memory. This is helpful for multi-view support as well as for embedded or gpu projects where different types of memory are used.
* multi-view: memory sharing allows images to be created fast without any data copies. For example, an image can be reflected, decimated, dimensions can be swapped, or a region of interest can be selected without any iteration through the memory. A new image class is quickly created that simply views and iterates through the smae data differntly.
* lightweight: a templated header only library make for simple and easy integration with any c++ project.
