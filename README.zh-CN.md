# Whitted 光线追踪器

爱丁堡大学 Computer Graphics 课程的最终 C++17 项目，包含透视相机、球体/旋转立方体/有限平面求交、Blinn–Phong 光照、递归反射、纹理、BVH 和多种采样特效。

[English README](README.md) · [效果对比](docs/gallery.md) · [实现说明](docs/architecture.md)

![基础渲染](docs/images/base.png)

## 编译与运行

C++ 部分只依赖标准库。在仓库根目录运行：

```bash
make
./build/ray --width 640 --height 360 --output output/base.ppm
```

也可以使用 CMake：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
python3 tests/smoke_test.py build/ray
```

默认读取 `scenes/demo.json` 和 `assets/stone.ppm`，开启 BVH、反射及每像素 4 次采样。输出目录会自动创建。不指定分辨率时使用场景的 1920 × 1080。

## 特效示例

```bash
# 软阴影
./build/ray --softshadow --shadow-samples 16 --light-radius 0.5 \
  --output output/soft.ppm

# 模糊反射
./build/ray --glossy --glossy-samples 4 --glossy-spread 0.12 \
  --output output/glossy.ppm

# 景深 / 相机运动模糊
./build/ray --aa --samples 9 --dof --aperture 0.05 --focus-dist 36 \
  --output output/dof.ppm
./build/ray --aa --samples 9 --motionblur --motion-amount 0.5 \
  --output output/motion.ppm

# 一次生成整组对比图
python3 scripts/render_examples.py
```

`--samples` 使用 1、4、9、16 等完全平方数。景深和运动模糊需要多次相机采样，建议同时开启 `--aa --samples 4` 或更高。运动模糊模拟相机横向移动。

输入和分辨率可通过 `--scene`、`--texture`、`--width`、`--height` 指定，完整参数见 `./build/ray --help`。

PPM 转 PNG 仅需要可选依赖 Pillow：

```bash
python3 -m pip install Pillow
python3 scripts/convert_image.py output/base.ppm output/base.png
```

## 内容与边界

- `src/`：最终版渲染器源码；`scenes/` 和 `assets/`：可直接运行的示例输入。
- `blender/`：可用于自建 Blender 场景的导出器，已去除个人电脑绝对路径；示例场景以 JSON 提供。
- `docs/`：原始课程渲染图、架构和实现边界说明。
- `tests/`：小分辨率渲染、BVH 一致性、特效开关和错误处理验证。

JSON 读取器依赖导出脚本的 `indent=4` 排版，支持预设图元，不支持任意网格或通用材质导入。阴影求交仍为线性遍历。原课程目录保持原样，仓库只收录最终项目及运行、展示所需文件。
