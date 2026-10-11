#pragma once
#include "ray.h"
#include "hit.h"
#include "vec3.h"
#include "image.h"
#include "aabb.h"

struct Material {
    Vec3 diffuse = Vec3(1,1,1);
    float specular = 0.5f;
    float shininess = 32.0f;
    float reflectivity = 0.0f;
    Image* texture = nullptr;
};

class Shape {
public:
    Material material;   // 每个物体带一个材质

    // 基础求交接口
    virtual bool Intersect(const Ray& ray, HitInfo& hit) const = 0;

    // 每个物体都需要能给出自己的轴对齐包围盒，用于 BVH
    virtual AABB getAABB() const = 0;

    virtual ~Shape() {}
};
