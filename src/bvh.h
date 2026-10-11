#pragma once
#include <memory>
#include <vector>
#include <algorithm>
#include "shape.h"
#include "aabb.h"

// 简单的 BVH：按最长轴划分（不做 SAH）
struct BVHNode {
    AABB box;
    std::shared_ptr<BVHNode> left;
    std::shared_ptr<BVHNode> right;
    Shape* object = nullptr;   // 叶子节点指向一个形状

    // 递归构建 BVH，objects 中存的是场景中 Shape* 的数组
    static std::shared_ptr<BVHNode> build(std::vector<Shape*>& objects, int start, int end) {
        auto node = std::make_shared<BVHNode>();

        // 叶子：只有一个对象
        if (end - start == 1) {
            node->object = objects[start];
            node->box    = objects[start]->getAABB();
            return node;
        }

        // 1. 计算这一段所有对象的整体包围盒
        AABB globalBox = objects[start]->getAABB();
        for (int i = start + 1; i < end; ++i) {
            AABB b = objects[i]->getAABB();
            globalBox.min.x = std::min(globalBox.min.x, b.min.x);
            globalBox.min.y = std::min(globalBox.min.y, b.min.y);
            globalBox.min.z = std::min(globalBox.min.z, b.min.z);
            globalBox.max.x = std::max(globalBox.max.x, b.max.x);
            globalBox.max.y = std::max(globalBox.max.y, b.max.y);
            globalBox.max.z = std::max(globalBox.max.z, b.max.z);
        }
        node->box = globalBox;

        // 2. 按最长轴排序 & 分割
        Vec3 diag = globalBox.max - globalBox.min;
        int axis = 0;
        if (diag.y > diag.x && diag.y >= diag.z) {
            axis = 1;
        } else if (diag.z > diag.x && diag.z >= diag.y) {
            axis = 2;
        }

        int mid = (start + end) / 2;
        std::nth_element(objects.begin() + start,
                         objects.begin() + mid,
                         objects.begin() + end,
                         [axis](Shape* a, Shape* b) {
                             float ca = a->getAABB().center()[axis];
                             float cb = b->getAABB().center()[axis];
                             return ca < cb;
                         });

        // 3. 递归构建左右子树
        node->left  = build(objects, start, mid);
        node->right = build(objects, mid, end);

        return node;
    }

    // 递归求交：通过 bestHit / bestObj 记录“当前最近的交点”
    bool intersect(const Ray& ray, HitInfo& bestHit, const Shape*& bestObj) const {
        float tMin = 0.0f;
        float tMax = bestHit.distance;  // 只关心当前最近交点之前的盒子
        if (!box.intersect(ray, tMin, tMax)) {
            return false;
        }

        bool hitAnything = false;

        // 叶子节点：直接和物体求交
        if (object) {
            HitInfo tempHit;
            tempHit.hit      = false;
            tempHit.distance = bestHit.distance;  // 让 Plane::Intersect 里 "t >= hit.distance" 判定生效

            if (object->Intersect(ray, tempHit) &&
                tempHit.hit && tempHit.distance < bestHit.distance) {

                // 注意：这里是整体拷贝 tempHit，包括 u,v
                // 所以平面的纹理坐标不会丢失
                bestHit = tempHit;
                bestObj = object;
                hitAnything = true;
            }
            return hitAnything;
        }

        // 内部节点：递归左右子树
        if (left) {
            if (left->intersect(ray, bestHit, bestObj)) {
                hitAnything = true;
            }
        }
        if (right) {
            if (right->intersect(ray, bestHit, bestObj)) {
                hitAnything = true;
            }
        }
        return hitAnything;
    }
};
