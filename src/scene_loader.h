// scene_loader.h
#pragma once
#include <string>
#include "scene.h"

// 加载整个 scene.json
bool loadScene(const std::string& filename, Scene& scene);

// 一个极简 JSON 工具：读取 "key": value
bool readKeyValue(const std::string& line, std::string& key, std::string& value);

// 将 "[1.0, 2.0, 3.0]" 转成 Vec3
Vec3 parseVec3(const std::string& value);

// 将 "[x, y, z, w]" 截断为 Vec3
Vec3 parseVec3FromArray(const std::string& arrayString);

// 解析一个 float
float parseFloat(const std::string& value);
