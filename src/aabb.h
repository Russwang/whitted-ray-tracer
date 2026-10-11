#pragma once
#include "vec3.h"
#include "ray.h"
#include <algorithm>

struct AABB {
    Vec3 min;
    Vec3 max;

    Vec3 center() const {
        return Vec3(
            0.5f * (min.x + max.x),
            0.5f * (min.y + max.y),
            0.5f * (min.z + max.z)
        );
    }

    bool intersect(const Ray& ray, float& tMin, float& tMax) const {
        for (int i = 0; i < 3; ++i) {
            float invD = 1.0f / ray.direction[i];
            float t0 = (min[i] - ray.origin[i]) * invD;
            float t1 = (max[i] - ray.origin[i]) * invD;
            if (invD < 0.0f) std::swap(t0, t1);
            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);
            if (tMax <= tMin) return false;
        }
        return true;
    }
};
