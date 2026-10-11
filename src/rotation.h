// rotation.h
#pragma once

#include "vec3.h"
#include <cmath>

inline Vec3 rotateEulerXYZ(const Vec3& e, const Vec3& v) {
    // Precompute sin/cos
    float cx = cos(e.x), sx = sin(e.x);
    float cy = cos(e.y), sy = sin(e.y);
    float cz = cos(e.z), sz = sin(e.z);

    // Rotation X
    Vec3 rx(
        v.x,
        cx * v.y - sx * v.z,
        sx * v.y + cx * v.z
    );

    // Rotation Y
    Vec3 ry(
        cy * rx.x + sy * rx.z,
        rx.y,
        -sy * rx.x + cy * rx.z
    );

    // Rotation Z
    Vec3 rz(
        cz * ry.x - sz * ry.y,
        sz * ry.x + cz * ry.y,
        ry.z
    );

    // Note: This is Rz * Ry * Rx applied in code order
    return rz;
}
