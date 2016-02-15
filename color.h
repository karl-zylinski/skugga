#pragma once

struct Color
{
    float r, g, b, a;
};

namespace color
{

Color random();
bool almost_equal(const Color& c1, const Color& c2);

}
