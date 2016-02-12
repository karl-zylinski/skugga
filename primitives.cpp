namespace primitives
{
static const unsigned box_size = 36;
static const Vector3 box[] {
    // Front
    {-0.5f, -0.5f, -0.5f},
    {0.5f, 0.5f, -0.5f},
    {0.5f, -0.5f, -0.5f},
    {-0.5f, -0.5f, -0.5f},
    {-0.5f, 0.5f, -0.5f},
    {0.5f, 0.5f, -0.5f},

    // Back
    {0.5f, 0.5f, 0.5f},
    {-0.5f, -0.5f, 0.5f},
    {0.5f, -0.5f, 0.5f},
    {-0.5f, 0.5f, 0.5f},
    {-0.5f, -0.5f, 0.5f},
    {0.5f, 0.5f, 0.5f},

    // Left
    {-0.5f, -0.5f, 0.5f},
    {-0.5f, 0.5f, -0.5f},
    {-0.5f, -0.5f, -0.5f},
    {-0.5f, -0.5f, 0.5f},
    {-0.5f, 0.5f, 0.5f},
    {-0.5f, 0.5f, -0.5f},

    // Right
    {0.5f, 0.5f, -0.5f},
    {0.5f, -0.5f, 0.5f},
    {0.5f, -0.5f, -0.5f},
    {0.5f, 0.5f, 0.5f},
    {0.5f, -0.5f, 0.5f},
    {0.5f, 0.5f, -0.5f},

    // Bottom
    {0.5f, -0.5f, 0.5f},
    {-0.5f, -0.5f, -0.5f},
    {0.5f, -0.5f, -0.5f},
    {-0.5f, -0.5f, 0.5f},
    {-0.5f, -0.5f, -0.5f},
    {0.5f, -0.5f, 0.5f},

    // Top
    {-0.5f, 0.5f, -0.5f},
    {0.5f, 0.5f, 0.5f},
    {0.5f, 0.5f, -0.5f},
    {-0.5f, 0.5f, -0.5f},
    {-0.5f, 0.5f, 0.5f},
    {0.5f, 0.5f, 0.5f}
};
}