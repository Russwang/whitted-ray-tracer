#pragma once
#include "shape.h"
#include <cmath>
#include "vec3.h"

class Sphere : public Shape {
public:
    Vec3 center;
    float radius;

    Sphere() {
        center = Vec3(0,0,0);
        radius = 1;

        material.diffuse = Vec3(1.0f, 0.0f, 0.0f);  // 红色
        material.specular = 0.5f;
        material.shininess = 32.0f;
        material.reflectivity = 0.2f;
    }

    Sphere(const Vec3& c, float r) : center(c), radius(r) {}

    bool Intersect(const Ray& ray, HitInfo& hit) const override {
        Vec3 oc = ray.origin - center;
        float a = dot(ray.direction, ray.direction);
        float b = 2.0f * dot(oc, ray.direction);
        float c = dot(oc, oc) - radius * radius;
        float discriminant = b*b - 4*a*c;

        if (discriminant < 0) return false;
        float t = (-b - sqrt(discriminant)) / (2.0f * a);
        if (t < 0) return false;

        hit.hit = true;
        hit.distance = t;
        hit.point = ray.origin + t * ray.direction;
        hit.normal = normalize(hit.point - center);
        return true;
    }

    AABB getAABB() const override {
        AABB box;
        Vec3 rVec(radius, radius, radius);
        box.min = center - rVec;
        box.max = center + rVec;
        return box;
    }
};
