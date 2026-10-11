#include "camera.h"
#include "rotation.h"
#include "vec3.h"
#include <iostream>

Ray Camera::generateRay(float px, float py) const {
    float ndcX = (px + 0.5f) / (float)resX;
    float ndcY = (py + 0.5f) / (float)resY;

    float sensorX = (ndcX - 0.5f) * sensorWidth;
    float sensorY = (0.5f - ndcY) * sensorHeight;

    Vec3 dirCam(sensorX, sensorY, -focalLength);

    Vec3 dirWorld = rotateEulerXYZ(rotation, dirCam);

    dirWorld = normalize(dirWorld);

    Ray r;
    r.origin = position;
    r.direction = dirWorld;
    return r;
}
