#pragma once
#include "shape.h"
#include "vec3.h"
#include "ray.h"
#include "hit.h"
#include <cmath>
#include <algorithm>
#include <iostream>

class Plane : public Shape {
public:
    Vec3 p0, p1, p2, p3;  // 四个点可以是任意顺序，但构成凸四边形

    Plane() {
        material.diffuse = Vec3(0.8f, 0.8f, 0.8f);
        material.specular = 0.2f;
        material.shininess = 8.0f;
        material.reflectivity = 0.02f;
    }

    Plane(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d)
        : p0(a), p1(b), p2(c), p3(d) {}

    // 判断点 P 是否在三角形 ABC 内
    bool pointInTriangle(const Vec3& P, const Vec3& A, const Vec3& B, const Vec3& C) const {
        Vec3 v0 = C - A;
        Vec3 v1 = B - A;
        Vec3 v2 = P - A;

        float dot00 = dot(v0, v0);
        float dot01 = dot(v0, v1);
        float dot02 = dot(v0, v2);
        float dot11 = dot(v1, v1);
        float dot12 = dot(v1, v2);

        float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
        float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
        float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

        return (u >= 0) && (v >= 0) && (u + v <= 1);
    }

    bool Intersect(const Ray& ray, HitInfo& hit) const override {
        // 用 p0-p1-p2 定义平面法线（顺序不会影响正确性）
        Vec3 normal = normalize(cross(p1 - p0, p2 - p0));

        if (dot(normal, ray.direction) > 0)
            normal = normal * -1.0f;

        float denom = dot(normal, ray.direction);
        if (std::fabs(denom) < 1e-6) return false; // 平行

        float t = dot(p0 - ray.origin, normal) / denom;
        if (t <= 0.0f || t >= hit.distance) return false;

        Vec3 P = ray.origin + ray.direction * t;

        // 四边形拆成两个三角形，无论点顺序如何都能判断
        if (!pointInTriangle(P, p0, p1, p2) &&
            !pointInTriangle(P, p2, p1, p3)) {
            return false;
        }

        hit.hit = true;
        hit.distance = t;
        hit.point = P;
        hit.normal = normal;

        Vec3 Uaxis = normalize(p1 - p0);
        Vec3 Vaxis = normalize(p2 - p0);

        Vec3 rel = P - p0;

        float u = dot(rel, Uaxis) * 0.1f;
        float v = dot(rel, Vaxis) * 0.1f;

        hit.u = u - floor(u);
        hit.v = v - floor(v);

        // std::cout << "[Plane::Intersect] HIT at: "
        //         << P.x << ", " << P.y << ", " << P.z
        //         << "  UV: " << hit.u << ", " << hit.v << std::endl;

        return true;
    }

    AABB getAABB() const override {
        AABB box;
        // 直接用四个角点的最小/最大坐标
        box.min = Vec3(
            std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x)),
            std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y)),
            std::min(std::min(p0.z, p1.z), std::min(p2.z, p3.z))
        );
        box.max = Vec3(
            std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x)),
            std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y)),
            std::max(std::max(p0.z, p1.z), std::max(p2.z, p3.z))
        );
        return box;
    }
};
