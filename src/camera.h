// camera.h
#pragma once
#include "vec3.h"
#include <string>
#include "ray.h"

class Camera {
public:
    Vec3 position;
    Vec3 rotation; // 欧拉角 (x, y, z)
    float focalLength;
    float sensorWidth;
    float sensorHeight;
    int resX;
    int resY;

    Camera() = default;
    void loadFromJSON(const std::string& filename);
    Ray generateRay(float px, float py) const;
};
