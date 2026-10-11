#pragma once
#include "vec3.h"

struct HitInfo {
    bool hit = false;           // 是否命中
    float distance = 1e9;       // 命中距离
    Vec3 point;
    Vec3 normal;

    float u = 0.0f;
    float v = 0.0f;
};
