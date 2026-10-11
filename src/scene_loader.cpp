#include "scene_loader.h"
#include "sphere.h"
#include "cube.h"
#include "plane.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <memory>
#include <cmath>
#include <stdexcept>

static std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    size_t e = s.find_last_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    return s.substr(b, e - b + 1);
}

float parseFloat(const std::string& s) {
    std::size_t consumed = 0;
    float value = std::stof(s, &consumed);
    if (consumed != s.size() || !std::isfinite(value)) {
        throw std::runtime_error("Invalid finite scene number: " + s);
    }
    return value;
}

static std::string readRequiredLine(std::ifstream& f) {
    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (!line.empty()) return line;
    }
    throw std::runtime_error("Unexpected end of scene file");
}

static void requireArrayEnd(const std::string& line) {
    if (line != "]" && line != "],") {
        throw std::runtime_error("Expected a closing scene array bracket");
    }
}

static int parseResolution(const std::string& s) {
    std::size_t consumed = 0;
    int value = std::stoi(s, &consumed);
    if (consumed != s.size() || value <= 0) {
        throw std::runtime_error("Camera resolution must be a positive integer");
    }
    return value;
}

static Vec3 parsePlaneCorner(std::ifstream& f) {
    std::string line = readRequiredLine(f);
    if (!line.empty() && line.front() == '[')
        line.erase(line.begin());

    Vec3 v;

    if (!line.empty()) {
        if (line.back() == ',') line.pop_back();
        v.x = parseFloat(trim(line));
    } else {
        line = readRequiredLine(f);
        if (line.back() == ',') line.pop_back();
        v.x = parseFloat(line);
    }

    // y
    line = readRequiredLine(f);
    if (line.back() == ',') line.pop_back();
    v.y = parseFloat(line);

    // z
    line = readRequiredLine(f);
    if (line.back() == ',') line.pop_back();
    v.z = parseFloat(line);

    requireArrayEnd(readRequiredLine(f));
    return v;
}

bool readKeyValue(const std::string& line, std::string& key, std::string& value) {
    size_t k1 = line.find('"');
    if (k1 == std::string::npos) return false;
    size_t k2 = line.find('"', k1 + 1);
    if (k2 == std::string::npos) return false;

    key = line.substr(k1 + 1, k2 - k1 - 1);

    size_t colon = line.find(':', k2);
    if (colon == std::string::npos) return false;

    value = trim(line.substr(colon + 1));
    if (!value.empty() && value.back() == ',')
        value.pop_back();

    return true;
}

static Vec3 parseVec3Array(std::ifstream& f) {
    std::string line;
    Vec3 v;

    for (int i = 0; i < 3; i++) {
        line = readRequiredLine(f);
        if (!line.empty() && line.back() == ',')
            line.pop_back();
        v[i] = parseFloat(line);
    }

    requireArrayEnd(readRequiredLine(f));
    return v;
}

bool loadScene(const std::string& filename, Scene& scene) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        std::cerr << "Cannot open scene file: " << filename << "\n";
        return false;
    }

    std::string line, key, value;
    enum Block { NONE, CAMERA, LIGHTS, SPHERES, CUBES, PLANES };
    Block block = NONE;

    Vec3 camPos, camRot;
    float focal = 35, sWidth = 36, sHeight = 24;
    int rX = 800, rY = 600;
    bool hasCamera = false;
    std::shared_ptr<Sphere> s;
    std::shared_ptr<Cube> c;

    while (std::getline(f, line)) {
        line = trim(line);

        if (line == "\"cameras\": [") { block = CAMERA; continue; }
        if (line == "\"lights\": [")  { block = LIGHTS;  continue; }
        if (line == "\"spheres\": [") { block = SPHERES; continue; }
        if (line == "\"cubes\": [")   { block = CUBES;   continue; }
        if (line == "\"planes\": [")  { block = PLANES;  continue; }

        if (line == "],") { block = NONE; continue; }
        if (line == "]")  { block = NONE; continue; }

        if (!readKeyValue(line, key, value)) continue;

        //
        // CAMERA
        //
        if (block == CAMERA) {
            if (key == "location") {
                camPos = parseVec3Array(f);
                hasCamera = true;
            }
            else if (key == "rotation_euler") camRot = parseVec3Array(f);
            else if (key == "focal_length") focal = parseFloat(value);
            else if (key == "sensor_width") sWidth = parseFloat(value);
            else if (key == "sensor_height") sHeight = parseFloat(value);
            else if (key == "resolution_x") rX = parseResolution(value);
            else if (key == "resolution_y") rY = parseResolution(value);
        }

        //
        // LIGHTS
        //
        if (block == LIGHTS) {
            if (key == "location") {
                Light L;
                L.position = parseVec3Array(f);

                line = readRequiredLine(f);
                if (!readKeyValue(line, key, value) || key != "intensity") {
                    throw std::runtime_error("Expected light intensity after location");
                }
                L.intensity = parseFloat(value) * 0.4f;
                if (L.intensity < 0) throw std::runtime_error("Light intensity must be nonnegative");

                scene.lights.push_back(L);
            }
        }

        //
        // SPHERES
        //
        if (block == SPHERES) {
            if (key == "location") {
                s = std::make_shared<Sphere>();
                s->center = parseVec3Array(f);
            }
            else if (key == "radius") {
                if (!s) throw std::runtime_error("Expected sphere location before radius");
                s->radius = parseFloat(value);
                if (s->radius <= 0) throw std::runtime_error("Sphere radius must be positive");
                scene.addObject(s);
                s = nullptr;
            }
        }

        //
        // CUBES
        //
        if (block == CUBES) {
            if (key == "translation") {
                c = std::make_shared<Cube>();
                c->translation = parseVec3Array(f);
            }
            else if (key == "rotation") {
                if (!c) throw std::runtime_error("Expected cube translation before rotation");
                c->rotation = parseVec3Array(f);
            }
            else if (key == "scale") {
                if (!c) throw std::runtime_error("Expected cube translation before scale");
                c->scale = parseFloat(value);
                if (c->scale <= 0) throw std::runtime_error("Cube scale must be positive");
                scene.addObject(c);
                c = nullptr;
            }
        }

        //
        // PLANES
        //
        if (block == PLANES) {
            if (key == "corners") {
                auto p = std::make_shared<Plane>();

                // The course exporter places each vector on separate lines.
                // parsePlaneCorner consumes the opening bracket for each corner.

                Vec3 corners[4];
                for (int i = 0; i < 4; i++) {
                    corners[i] = parsePlaneCorner(f);
                }

                requireArrayEnd(readRequiredLine(f));

                p->p0 = corners[0];
                p->p1 = corners[1];
                p->p2 = corners[2];
                p->p3 = corners[3];
                const float areaA = length(cross(p->p1 - p->p0, p->p2 - p->p0));
                const float areaB = length(cross(p->p1 - p->p2, p->p3 - p->p2));
                if (!std::isfinite(areaA) || !std::isfinite(areaB) || areaA <= 0 || areaB <= 0) {
                    throw std::runtime_error("Plane corners must define two nondegenerate triangles");
                }

                scene.addObject(p);
            }
        }
    }

    if (f.bad()) throw std::runtime_error("Failed to read scene file: " + filename);
    if (!hasCamera) throw std::runtime_error("Scene must contain a camera location");
    if (focal <= 0 || sWidth <= 0 || sHeight <= 0) {
        throw std::runtime_error("Camera focal length and sensor dimensions must be positive");
    }
    if (s || c) throw std::runtime_error("Incomplete sphere or cube in scene");

    scene.camera.position = camPos;
    scene.camera.rotation = camRot;
    scene.camera.focalLength = focal;
    scene.camera.sensorWidth = sWidth;
    scene.camera.sensorHeight = sHeight;
    scene.camera.resX = rX;
    scene.camera.resY = rY;

    return true;
}
