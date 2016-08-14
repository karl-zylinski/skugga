#include "image.h"
#include "memory.h"

namespace image
{

void init_data(Image* i, Allocator* alloc)
{
    i->data = (unsigned char*)alloc->alloc(size(i->pixel_format, i->width, i->height));
}

unsigned pixel_size(PixelFormat pf)
{
    switch (pf)
    {
        case PixelFormat::R8G8B8A8_UINT:
            return 4;
        case PixelFormat::R8G8B8A8_UINT_NORM:
            return 4;
        case PixelFormat::R32G32B32A32_FLOAT:
            return 16;
        case PixelFormat::R32_UINT:
            return 4;
        case PixelFormat::R8_UINT_NORM:
            return 1;
        default:
            Error("Unknown pixel format."); return 0;
    }
}

unsigned size(PixelFormat pf, unsigned size_x, unsigned size_y)
{
    return size_x * size_y * pixel_size(pf);
}

unsigned size(const Image& image)
{
    return size(image.pixel_format, image.width, image.height);
}

} // namespace image
