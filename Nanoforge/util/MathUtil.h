#pragma once
#include "common/Typedefs.h"
#define _USE_MATH_DEFINES
#include <math.h>

static f32 ToRadians(f32 angleDegrees)
{
    return angleDegrees * (M_PI / 180.0f);
}

static f32 ToDegrees(f32 angleRadians)
{
    return angleRadians * (180.0f / M_PI);
}