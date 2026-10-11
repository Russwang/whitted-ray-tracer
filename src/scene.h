#pragma once
#include <vector>
#include <memory>
#include "shape.h"
#include "camera.h"
#include "light.h"
#include "bvh.h"

struct Scene {
    Camera camera;
    std::vector<Light> lights;
    std::vector<std::shared_ptr<Shape>> objects;

    // BVH 根节点
    std::shared_ptr<BVHNode> bvhRoot;

    void addObject(const std::shared_ptr<Shape>& s) {
        objects.push_back(s);
    }

    // 所有 objects 加载完之后调用一次，构建 BVH
    void buildBVH() {
        std::vector<Shape*> raw;
        raw.reserve(objects.size());
        for (auto& s : objects) {
            raw.push_back(s.get());
        }
        if (raw.empty()) {
            bvhRoot.reset();
        } else {
            bvhRoot = BVHNode::build(raw, 0, static_cast<int>(raw.size()));
        }
    }
};
