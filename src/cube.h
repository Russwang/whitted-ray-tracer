#pragma once
#include "shape.h"
#include "vec3.h"
#include "ray.h"
#include "rotation.h"
#include <cmath>
#include <algorithm>

class Cube : public Shape {
public:
    Vec3 translation;   // Blender translation
    Vec3 rotation;      // Euler angles (XYZ, radians)
    float scale;        // Uniform scale

    Cube() {
        translation = Vec3(0,0,0);
        rotation = Vec3(0,0,0);
        scale = 1;

        material.diffuse = Vec3(0.0f, 0.0f, 1.0f);  // 蓝色
        material.specular = 0.5f;
        material.shininess = 32.0f;
        material.reflectivity = 0.1f;
    }


    // Inverse rotate
    Vec3 inverseRotate(const Vec3& euler, const Vec3& v) const {
        Vec3 inv(-euler.x, -euler.y, -euler.z);
        return rotateEulerXYZ(inv, v);
    }

    // Intersect routine
    bool Intersect(const Ray& ray, HitInfo& hit) const override {
        // 1) Transform ray into cube local space
        Vec3 ro = inverseRotate(rotation, ray.origin - translation);
        Vec3 rd = inverseRotate(rotation, ray.direction);

        float h = scale;

        float tmin = -1e9, tmax = 1e9;
        Vec3 signNormal(0,0,0);

        // 3 axes: x, y, z
        for (int i = 0; i < 3; i++) {
            float originA = (i==0? ro.x : (i==1? ro.y : ro.z));
            float dirA    = (i==0? rd.x : (i==1? rd.y : rd.z));

            if (std::fabs(dirA) < 1e-6f) {
                if (std::fabs(originA) > h) return false;
            } else {
                float t1 = (-h - originA) / dirA;
                float t2 = ( h - originA) / dirA;
                float tn = t1, tf = t2;
                if (tn > tf) std::swap(tn, tf);

                if (tn > tmin) {
                    tmin = tn;
                    signNormal = Vec3(0,0,0);
                    signNormal[i] = (originA + tn * dirA > 0 ? 1 : -1);
                }
                if (tf < tmax) tmax = tf;

                if (tmin > tmax) return false;
            }
        }

        if (tmin < 0) return false;

        // local hit point
        Vec3 localHit = ro + rd * tmin;

        // local normal
        Vec3 localNormal = normalize(signNormal);

        // transform back to world
        Vec3 hitWorld = rotateEulerXYZ(rotation, localHit) + translation;
        Vec3 nWorld   = rotateEulerXYZ(rotation, localNormal);

        hit.hit = true;
        hit.distance = length(hitWorld - ray.origin);
        hit.point = hitWorld;
        hit.normal = normalize(nWorld);
        return true;
    }

    AABB getAABB() const override {
        AABB box;
        float h = scale;

        Vec3 corners[8] = {
            Vec3(-h, -h, -h),
            Vec3( h, -h, -h),
            Vec3(-h,  h, -h),
            Vec3( h,  h, -h),
            Vec3(-h, -h,  h),
            Vec3( h, -h,  h),
            Vec3(-h,  h,  h),
            Vec3( h,  h,  h)
        };

        for (int i = 0; i < 8; ++i) {
            Vec3 world = rotateEulerXYZ(rotation, corners[i]) + translation;
            if (i == 0) {
                box.min = box.max = world;
            } else {
                box.min.x = std::min(box.min.x, world.x);
                box.min.y = std::min(box.min.y, world.y);
                box.min.z = std::min(box.min.z, world.z);
                box.max.x = std::max(box.max.x, world.x);
                box.max.y = std::max(box.max.y, world.y);
                box.max.z = std::max(box.max.z, world.z);
            }
        }
        return box;
    }
};
