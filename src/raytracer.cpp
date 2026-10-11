#include "scene.h"
#include "scene_loader.h"
#include "camera.h"
#include "ray.h"
#include "image.h"
#include "shape.h"
#include "plane.h"
#include "rotation.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>

bool gUseAA         = true;   // 是否开启抗锯齿
bool gUseReflection = true;   // 是否开启反射
bool gUseBVH        = true;   // 是否使用 BVH
int  gSamplesPerPixel = 4;    // 每像素样本数 (AA 用)，例如 1、4、9 ...
bool gUseSoftShadow   = false;   // 是否启用软阴影
int  gShadowSamples   = 16;      // 每个光源的阴影采样数
float gLightRadius    = 0.5f;    // 伪“面光源”的半径（越大阴影越软）
// --------- Glossy Reflection 参数 ---------
const float PI_F      = 3.14159265f;
bool gUseGlossy       = false;   // 默认关掉 glossy
int  gGlossySamples   = 8;       // 每次反射的样本数
float gGlossySpread   = 0.12f;   // 粗糙度
// --------- Depth of Field（景深）参数 ---------
bool  gUseDOF         = false;   // 是否开启景深
float gApertureRadius = 0.05f;   // 光圈半径（越大虚化越强）
float gFocusDist      = 10.0f;   // 焦平面距离（从相机沿视线方向）
// --------- Motion Blur（运动模糊）参数 ---------
bool  gUseMotionBlur  = false;   // 是否开启运动模糊
float gMotionAmount   = 0.5f;    // 相机移动幅度（越大越糊）




std::string gOutputFilename = "output/render.ppm";

// 判断某个点对某个光源是不是在阴影里
bool isInShadow(const Vec3& point, const Light& light, const Scene& scene) {
    // 从该点指向光源的向量
    Vec3 toLight = light.position - point;
    float maxDist = length(toLight);          // 点到光源的距离
    Vec3 dir = normalize(toLight);           // 阴影光线方向

    // 阴影光线（Shadow Ray）
    Ray shadowRay;
    shadowRay.origin = point + dir * 0.001f; // 往外挪一点，避免自相交
    shadowRay.direction = dir;

    // 测试是否被任何物体挡住
    for (const auto& obj : scene.objects) {
        HitInfo h;
        h.distance = 1e9f;

        if (obj->Intersect(shadowRay, h)) {
            // 如果命中并且距离在 0 和 点-光源 之间 → 被挡住了
            if (h.hit && h.distance > 0.0f && h.distance < maxDist) {
                return true;                 // 在阴影里
            }
        }
    }

    return false;                            // 没有被任何东西挡住
}

float computeShadowFactor(const Vec3& point, const Light& light, const Scene& scene) {
    // 如果没开软阴影，直接用原来的硬阴影判断
    if (!gUseSoftShadow) {
        return isInShadow(point, light, scene) ? 0.0f : 1.0f;
    }

    int samples = std::max(1, gShadowSamples);
    if (samples == 1 || gLightRadius <= 0.0f) {
        return isInShadow(point, light, scene) ? 0.0f : 1.0f;
    }

    // 构造一个以光源为中心的小圆盘，做随机 / 准随机采样
    Vec3 lightDir = normalize(point - light.position); // 用来构造局部坐标

    // 构造与 lightDir 正交的两个轴（切线/副切线），构成局部平面
    Vec3 up = (std::fabs(lightDir.y) < 0.99f) ? Vec3(0,1,0) : Vec3(1,0,0);
    Vec3 tangent   = normalize(cross(up, lightDir));
    Vec3 bitangent = cross(lightDir, tangent);

    int visibleCount = 0;
    const float PI = 3.1415926535f;

    for (int i = 0; i < samples; ++i) {
        // 简单的伪随机：用 i 做 deterministic 抖动，避免引入 <random>
        float a = float((i * 17) % samples + 0.5f) / float(samples); // [0,1)
        float b = float((i * 31) % samples + 0.5f) / float(samples); // [0,1)

        // 均匀采样圆盘：r = R * sqrt(b), theta = 2pi * a
        float r     = gLightRadius * std::sqrt(b);
        float theta = 2.0f * PI * a;

        float dx = r * std::cos(theta);
        float dy = r * std::sin(theta);

        // 本次采样的“虚拟光源位置”
        Light sampleLight = light;
        sampleLight.position =
            light.position + tangent * dx + bitangent * dy;

        // 如果这次采样下「不在阴影里」，记一次可见
        if (!isInShadow(point, sampleLight, scene)) {
            ++visibleCount;
        }
    }

    return float(visibleCount) / float(samples);
}

Vec3 reflect(const Vec3& I, const Vec3& N) {
    return I - N * 2.0f * dot(I, N);
}

// 从指定相机位置生成一条主视射线（和 Camera::generateRay 一样的模型）
static Ray generateRayWithCamPos(const Camera& cam, const Vec3& camPos, float px, float py){
    float ndcX = (px + 0.5f) / (float)cam.resX;
    float ndcY = (py + 0.5f) / (float)cam.resY;

    float sensorX = (ndcX - 0.5f) * cam.sensorWidth;
    float sensorY = (0.5f - ndcY) * cam.sensorHeight;

    Vec3 dirCam(sensorX, sensorY, -cam.focalLength);

    Vec3 dirWorld = rotateEulerXYZ(cam.rotation, dirCam);
    dirWorld = normalize(dirWorld);

    Ray r;
    r.origin = camPos;
    r.direction = dirWorld;
    return r;
}


// 主光线追踪函数：集成 BVH + Plane 纹理 + Soft Shadow + Reflection
Vec3 trace(const Ray& ray, const Scene& scene, Image* stoneTex, int depth /* = 0 */) {
    if (depth > 4) {
        return Vec3(0, 0, 0);
    }

    // -------- 1. 求最近交点（BVH + Plane 特判） --------
    HitInfo bestHit;
    const Shape* bestObj = nullptr;

    // 1.a 先用 BVH 或线性遍历找一个初始最近命中
    if (gUseBVH && scene.bvhRoot) {
        scene.bvhRoot->intersect(ray, bestHit, bestObj);
    } else {
        for (auto& obj : scene.objects) {
            HitInfo h;
            if (obj->Intersect(ray, h)) {
                if (h.distance < bestHit.distance) {
                    bestHit = h;
                    bestObj = obj.get();
                }
            }
        }
    }

    // 1.b 再对所有 Plane 跑一遍 Intersect，确保 Plane 按原逻辑求交（带 u,v）
    for (auto& obj : scene.objects) {
        if (dynamic_cast<Plane*>(obj.get()) == nullptr)
            continue;

        HitInfo h;
        if (obj->Intersect(ray, h)) {
            if (h.distance < bestHit.distance) {
                bestHit = h;
                bestObj = obj.get();
            }
        }
    }

    // 1.c 没打到任何物体 → 背景
    if (!bestObj || !bestHit.hit) {
        return Vec3(0.2f, 0.3f, 0.5f);
    }

    Vec3 P = bestHit.point;
    Vec3 N = normalize(bestHit.normal);

    // -------- 2. 材质 & 纹理采样（这里保证 Plane 有纹理） --------
    Material mat = bestObj->material;
    Vec3 baseColor = mat.diffuse;

    // ---- 平面专用纹理采样 ----
    // 只对 Plane 采样纹理，包括 UV 原点。
    if (stoneTex && dynamic_cast<const Plane*>(bestObj)) {
        float uu = bestHit.u;
        float vv = bestHit.v;

        // wrap 回 [0,1)
        uu = uu - std::floor(uu);
        vv = vv - std::floor(vv);

        int tx = std::clamp(int(uu * stoneTex->width),  0, stoneTex->width  - 1);
        int ty = std::clamp(int(vv * stoneTex->height), 0, stoneTex->height - 1);

        Pixel texel = stoneTex->getPixel(tx, ty);
        baseColor = Vec3(
            texel.r / 255.0f,
            texel.g / 255.0f,
            texel.b / 255.0f
        );
    }



    // -------- 3. 光照（带 Soft Shadow） --------
    Vec3 result(0, 0, 0);

    // 简单 ambient
    Vec3 ambient = baseColor * 0.15f;
    result += ambient;

    // 对每个光源计算 diffuse + specular
    for (const auto& light : scene.lights) {
        // 3.a 计算 soft shadow factor：0 = 全黑，1 = 完全不遮挡
        float shadowFactor = computeShadowFactor(P, light, scene);
        if (shadowFactor <= 0.0f) {
            continue;
        }

        Vec3 L = normalize(light.position - P);
        float diff = std::max(dot(N, L), 0.0f);

        Vec3 diffuse = baseColor * diff * (light.intensity * 0.1f);

        Vec3 specular(0, 0, 0);
        if (diff > 0.0f) {
            Vec3 V = normalize(-ray.direction);
            Vec3 H = normalize(L + V);
            float specAngle = std::max(dot(N, H), 0.0f);
            float spec = std::pow(specAngle, mat.shininess)
                         * mat.specular * light.intensity;
            specular = Vec3(spec, spec, spec);
        }

        result += (diffuse + specular) * shadowFactor;
    }

    // --- 反射 / Glossy ---
    if (gUseReflection && mat.reflectivity > 0.0f) {

        // 深度>1 时就不要再 glossy 了，防止递归太重
        bool doGlossy = gUseGlossy && (depth == 0);

        if (!doGlossy) {
            // 原来的“单条射线”反射：便宜
            Vec3 R = normalize(reflect(normalize(ray.direction), N));

            Ray r2;
            r2.origin = P + N * 0.01f;
            r2.direction = R;

            Vec3 rc = trace(r2, scene, stoneTex, depth + 1);
            rc.x = std::clamp(rc.x, 0.0f, 1.0f);
            rc.y = std::clamp(rc.y, 0.0f, 1.0f);
            rc.z = std::clamp(rc.z, 0.0f, 1.0f);

            result = result * (1.0f - mat.reflectivity) + rc * mat.reflectivity;
        } else {
            // Glossy：多条射线版本，只在 depth==0 且 gUseGlossy==true 时启用
            Vec3 R = normalize(reflect(normalize(ray.direction), N));

            Vec3 up = (std::fabs(R.y) < 0.99f) ? Vec3(0,1,0) : Vec3(1,0,0);
            Vec3 tangent   = normalize(cross(up, R));
            Vec3 bitangent = cross(R, tangent);

            int   samples = std::max(1, gGlossySamples);
            float spread  = gGlossySpread;

            Vec3 glossyAccum(0,0,0);

            for (int i = 0; i < samples; ++i) {
                float a = ((i * 37) % samples + 0.5f) / float(samples);
                float b = ((i * 61) % samples + 0.5f) / float(samples);

                float r     = spread * std::sqrt(b);
                float theta = 2.0f * 3.14159265f * a;

                float dx = r * std::cos(theta);
                float dy = r * std::sin(theta);

                Vec3 jitterDir = normalize(R + tangent * dx + bitangent * dy);

                Ray r2;
                r2.origin    = P + N * 0.01f;
                r2.direction = jitterDir;

                Vec3 rc = trace(r2, scene, stoneTex, depth + 1);
                rc.x = std::clamp(rc.x, 0.0f, 1.0f);
                rc.y = std::clamp(rc.y, 0.0f, 1.0f);
                rc.z = std::clamp(rc.z, 0.0f, 1.0f);

                glossyAccum += rc;
            }

            Vec3 glossyColor = glossyAccum * (1.0f / float(samples));
            result = result * (1.0f - mat.reflectivity) + glossyColor * mat.reflectivity;
        }
    }

    // -------- 5. clamp & 返回 --------
    result.x = std::clamp(result.x, 0.0f, 1.0f);
    result.y = std::clamp(result.y, 0.0f, 1.0f);
    result.z = std::clamp(result.z, 0.0f, 1.0f);

    return result;
}


namespace {

int positiveInteger(const std::string& value, const std::string& option) {
    std::size_t consumed = 0;
    int result;
    try {
        result = std::stoi(value, &consumed);
    } catch (const std::exception&) {
        throw std::runtime_error(option + " requires a positive integer");
    }
    if (consumed != value.size() || result <= 0) {
        throw std::runtime_error(option + " requires a positive integer");
    }
    return result;
}

float finiteNumber(const std::string& value, const std::string& option, bool positive = false) {
    std::size_t consumed = 0;
    const std::string requirement = option + (positive ? " requires a finite positive number"
                                                      : " requires a finite nonnegative number");
    float result;
    try {
        result = std::stof(value, &consumed);
    } catch (const std::exception&) {
        throw std::runtime_error(requirement);
    }
    if (consumed != value.size() || !std::isfinite(result) ||
        (positive ? result <= 0 : result < 0)) {
        throw std::runtime_error(requirement);
    }
    return result;
}

void printHelp() {
    std::cout << "Usage: raytracer [options]\n"
              << "  --scene PATH             Scene file (default: scenes/demo.json)\n"
              << "  --texture PATH           P3/P6 texture (default: assets/stone.ppm)\n"
              << "  --width N / --height N   Positive resolution overrides\n"
              << "  --output PATH            P6 output (default: output/render.ppm)\n"
              << "  --aa / --no-aa           Enable/disable antialiasing (default: on)\n"
              << "  --refl / --no-refl       Enable/disable reflections (default: on)\n"
              << "  --bvh / --no-bvh         Enable/disable BVH acceleration (default: on)\n"
              << "  --samples N              Positive square sample count: 1, 4, 9... (default: 4)\n"
              << "  --softshadow / --no-softshadow\n"
              << "  --shadow-samples N       Positive soft shadow sample count (default: 16)\n"
              << "  --light-radius R         Nonnegative soft shadow radius (default: 0.5)\n"
              << "  --glossy / --no-glossy   Enable/disable glossy reflection\n"
              << "  --glossy-samples N       Positive glossy sample count (default: 8)\n"
              << "  --glossy-spread S        Nonnegative roughness (default: 0.12)\n"
              << "  --dof / --no-dof         Enable/disable depth of field\n"
              << "  --aperture R             Nonnegative aperture radius (default: 0.05)\n"
              << "  --focus-dist D           Positive focus distance (default: 10)\n"
              << "  --motionblur / --no-motionblur  Enable/disable camera motion blur\n"
              << "  --motion-amount A        Nonnegative camera motion amount (default: 0.5)\n"
              << "  DOF and motion blur require --aa and --samples >= 4.\n"
              << "  All numeric values must be finite.\n";
}

} // namespace

static int runRenderer(int argc, char** argv) {
    std::string sceneFilename = "scenes/demo.json";
    std::string textureFilename = "assets/stone.ppm";
    int widthOverride = 0;
    int heightOverride = 0;

    // ------- 1) 解析并验证命令行选项 -------
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto nextValue = [&]() -> std::string {
            if (i + 1 >= argc || std::string(argv[i + 1]).empty() ||
                std::string(argv[i + 1]).rfind("--", 0) == 0) {
                throw std::runtime_error("Missing value for " + arg);
            }
            return argv[++i];
        };

        if (arg == "--aa") {
            gUseAA = true;
        } else if (arg == "--no-aa") {
            gUseAA = false;
        } else if (arg == "--refl") {
            gUseReflection = true;
        } else if (arg == "--no-refl") {
            gUseReflection = false;
        } else if (arg == "--bvh") {
            gUseBVH = true;
        } else if (arg == "--no-bvh") {
            gUseBVH = false;
        } else if (arg == "--samples") {
            gSamplesPerPixel = positiveInteger(nextValue(), arg);
        } else if (arg == "--scene") {
            sceneFilename = nextValue();
        } else if (arg == "--texture") {
            textureFilename = nextValue();
        } else if (arg == "--width") {
            widthOverride = positiveInteger(nextValue(), arg);
        } else if (arg == "--height") {
            heightOverride = positiveInteger(nextValue(), arg);
        } else if (arg == "--output") {
            gOutputFilename = nextValue();
        } else if (arg == "--softshadow") {
            gUseSoftShadow = true;
        } else if (arg == "--no-softshadow") {
            gUseSoftShadow = false;
        } else if (arg == "--shadow-samples") {
            gShadowSamples = positiveInteger(nextValue(), arg);
        } else if (arg == "--light-radius") {
            gLightRadius = finiteNumber(nextValue(), arg);
        } else if (arg == "--glossy") {
            gUseGlossy = true;
        } else if (arg == "--no-glossy") {
            gUseGlossy = false;
        } else if (arg == "--glossy-samples") {
            gGlossySamples = positiveInteger(nextValue(), arg);
        } else if (arg == "--glossy-spread") {
            gGlossySpread = finiteNumber(nextValue(), arg);
        } else if (arg == "--dof") {
            gUseDOF = true;
        } else if (arg == "--no-dof") {
            gUseDOF = false;
        } else if (arg == "--aperture") {
            gApertureRadius = finiteNumber(nextValue(), arg);
        } else if (arg == "--focus-dist") {
            gFocusDist = finiteNumber(nextValue(), arg, true);
        } else if (arg == "--motionblur") {
            gUseMotionBlur = true;
        } else if (arg == "--no-motionblur") {
            gUseMotionBlur = false;
        } else if (arg == "--motion-amount") {
            gMotionAmount = finiteNumber(nextValue(), arg);
        } else if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        } else {
            throw std::runtime_error("Unknown option: " + arg + ". Use --help for usage.");
        }
    }

    const int requestedGrid = static_cast<int>(std::sqrt(static_cast<double>(gSamplesPerPixel)));
    if (static_cast<long long>(requestedGrid) * requestedGrid != gSamplesPerPixel) {
        throw std::runtime_error("--samples must be a perfect square: 1, 4, 9, 16...");
    }
    if ((gUseDOF || gUseMotionBlur) && (!gUseAA || gSamplesPerPixel < 4)) {
        throw std::runtime_error("Depth of field and motion blur require --aa and --samples >= 4");
    }

    // ------- 2) 加载场景 -------
    Scene scene;
    if (!loadScene(sceneFilename, scene)) {
        std::cerr << "Failed to load scene\n";
        return 1;
    }
    if (widthOverride > 0) scene.camera.resX = widthOverride;
    if (heightOverride > 0) scene.camera.resY = heightOverride;

    const std::filesystem::path outputParent = std::filesystem::path(gOutputFilename).parent_path();
    if (!outputParent.empty()) std::filesystem::create_directories(outputParent);

    // 如果你本地已经在 Scene 里加了 buildBVH() / bvhRoot：
    // 这里用开关控制是否真正构建 BVH
    if (gUseBVH) {
        scene.buildBVH();
        std::cout << "BVH built for " << scene.objects.size() << " objects\n";
    } else {
        std::cout << "BVH disabled (linear traversal)\n";
    }

    // ------- 3) 打印标志和相机信息 -------
    std::cout << "Flags:\n";
    std::cout << "  AntiAliasing: " << (gUseAA ? "ON" : "OFF") << "\n";
    std::cout << "  Reflection:   " << (gUseReflection ? "ON" : "OFF") << "\n";
    std::cout << "  BVH:          " << (gUseBVH ? "ON" : "OFF") << "\n";
    std::cout << "  Samples:      " << gSamplesPerPixel << "\n";
    std::cout << "  SoftShadow:  " << (gUseSoftShadow ? "ON" : "OFF") << "\n";
    std::cout << "  ShadowSpp:   " << gShadowSamples << "\n";
    std::cout << "  Glossy:      " << (gUseGlossy ? "ON" : "OFF") << "\n";
    std::cout << "  DOF:         " << (gUseDOF ? "ON" : "OFF") << "  (aperture=" << gApertureRadius
              << ", focusDist=" << gFocusDist << ")\n";
    std::cout << "  MotionBlur:  " << (gUseMotionBlur ? "ON" : "OFF")
              << "  (amount=" << gMotionAmount << ")\n";

    std::cout << "Camera pos: "
              << scene.camera.position.x << ", "
              << scene.camera.position.y << ", "
              << scene.camera.position.z << "\n";

    std::cout << "Camera rot: "
              << scene.camera.rotation.x << ", "
              << scene.camera.rotation.y << ", "
              << scene.camera.rotation.z << "\n";

    std::cout << "Objects: " << scene.objects.size() << "\n";
    std::cout << "Lights:  " << scene.lights.size() << "\n";

    // ------- 相机坐标系，用于 DOF 和 Motion Blur -------
    Vec3 camForward = normalize(rotateEulerXYZ(scene.camera.rotation, Vec3(0, 0, -1)));
    Vec3 camUp      = normalize(rotateEulerXYZ(scene.camera.rotation, Vec3(0, 1,  0)));
    Vec3 camRight   = normalize(cross(camForward, camUp));
    // 重新正交一次，确保 three orthonormal
    camUp = normalize(cross(camRight, camForward));

    Image img(scene.camera.resX, scene.camera.resY);
    Image stoneTex(textureFilename);

    std::cout << "Loaded stone texture: "
              << stoneTex.width << " x " << stoneTex.height << std::endl;

    // ------- 4) 渲染循环 -------
    int spp = gUseAA ? std::max(1, gSamplesPerPixel) : 1;

    // 命令行已验证 samples 是完全平方数：1, 4, 9 ...
    int gridN = static_cast<int>(std::sqrt(static_cast<double>(spp)));
    int actualSamples = gridN * gridN;

    for (int y = 0; y < scene.camera.resY; y++) {
        for (int x = 0; x < scene.camera.resX; x++) {

            Vec3 sum(0, 0, 0);

            if (gUseAA && actualSamples > 1) {
                // N x N 网格抖动采样 + DOF + Motion Blur
                for (int sx = 0; sx < gridN; sx++) {
                    for (int sy = 0; sy < gridN; sy++) {
                        float dx = (sx + 0.5f) / gridN;
                        float dy = (sy + 0.5f) / gridN;

                        float pxSample = x + dx;
                        float pySample = y + dy;

                        // 用样本索引做一个 [0,1] 的“时间 / 随机数”
                        int   idx      = sy * gridN + sx;
                        float sample01 = (idx + 0.5f) / static_cast<float>(actualSamples);

                        // --- Motion Blur: 沿着 camRight 平移相机 ---
                        Vec3 camPos = scene.camera.position;
                        if (gUseMotionBlur && gMotionAmount != 0.0f) {
                            float offset = (sample01 - 0.5f) * gMotionAmount;
                            camPos = camPos + camRight * offset;
                        }

                        Ray primaryRay;

                        if (!gUseDOF || gApertureRadius <= 0.0f) {
                            // 普通 pinhole 相机
                            primaryRay = generateRayWithCamPos(scene.camera, camPos, pxSample, pySample);
                        } else {
                            // DOF：thin-lens 模型
                            // 1) 先从相机中心算一条“理想焦点”的射线
                            Ray pinholeRay = generateRayWithCamPos(scene.camera,
                                                                   scene.camera.position,
                                                                   pxSample,
                                                                   pySample);
                            Vec3 focalPoint = pinholeRay.origin
                                            + normalize(pinholeRay.direction) * gFocusDist;

                            // 2) 在镜头平面（right/up）上采样一个圆盘点
                            float rLens     = gApertureRadius * std::sqrt(sample01);
                            float thetaLens = 2.0f * PI_F * sample01;
                            float dxLens    = rLens * std::cos(thetaLens);
                            float dyLens    = rLens * std::sin(thetaLens);

                            Vec3 lensOffset = camRight * dxLens + camUp * dyLens;
                            Vec3 lensPos    = camPos + lensOffset;

                            primaryRay.origin    = lensPos;
                            primaryRay.direction = normalize(focalPoint - lensPos);
                        }

                        sum += trace(primaryRay, scene, &stoneTex, 0);
                    }
                }
                sum = sum * (1.0f / static_cast<float>(actualSamples));
            } else {
                // 不抗锯齿：像素中心 1 个样本，同样套 DOF + Motion Blur
                float pxSample = x + 0.5f;
                float pySample = y + 0.5f;

                float sample01 = 0.5f;  // 单个样本，就取固定 0.5

                Vec3 camPos = scene.camera.position;
                if (gUseMotionBlur && gMotionAmount != 0.0f) {
                    float offset = (sample01 - 0.5f) * gMotionAmount;
                    camPos = camPos + camRight * offset;
                }

                Ray primaryRay;
                if (!gUseDOF || gApertureRadius <= 0.0f) {
                    primaryRay = generateRayWithCamPos(scene.camera, camPos, pxSample, pySample);
                } else {
                    Ray pinholeRay = generateRayWithCamPos(scene.camera,
                                                           scene.camera.position,
                                                           pxSample,
                                                           pySample);
                    Vec3 focalPoint = pinholeRay.origin
                                    + normalize(pinholeRay.direction) * gFocusDist;

                    float rLens     = gApertureRadius * std::sqrt(sample01);
                    float thetaLens = 2.0f * PI_F * sample01;
                    float dxLens    = rLens * std::cos(thetaLens);
                    float dyLens    = rLens * std::sin(thetaLens);

                    Vec3 lensOffset = camRight * dxLens + camUp * dyLens;
                    Vec3 lensPos    = camPos + lensOffset;

                    primaryRay.origin    = lensPos;
                    primaryRay.direction = normalize(focalPoint - lensPos);
                }

                sum = trace(primaryRay, scene, &stoneTex, 0);
            }

            Vec3 c = sum;
            c.x = std::clamp(c.x, 0.0f, 1.0f);
            c.y = std::clamp(c.y, 0.0f, 1.0f);
            c.z = std::clamp(c.z, 0.0f, 1.0f);

            Pixel color;
            color.r = static_cast<unsigned char>(c.x * 255.0f);
            color.g = static_cast<unsigned char>(c.y * 255.0f);
            color.b = static_cast<unsigned char>(c.z * 255.0f);

            img.setPixel(x, y, color);
        }
    }

    img.save(gOutputFilename);
    std::cout << "Rendered " << gOutputFilename << "\n";
    return 0;
}

int main(int argc, char** argv) {
    try {
        return runRenderer(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}
