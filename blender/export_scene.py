import bpy
import json
from pathlib import Path

data = {
    "cameras": [],
    "lights": [],
    "spheres": [],
    "cubes": [],
    "planes": []
}

for obj in bpy.context.scene.objects:
    if obj.type == 'CAMERA':
        cam = obj.data
        dof = cam.dof   # ★ 相机的 DOF 设置

        data["cameras"].append({
            "name": obj.name,
            "location": list(obj.location),
            "rotation_euler": list(obj.rotation_euler),
            "focal_length": cam.lens,
            "sensor_width": cam.sensor_width,
            "sensor_height": cam.sensor_height,
            "resolution_x": bpy.context.scene.render.resolution_x,
            "resolution_y": bpy.context.scene.render.resolution_y,

            # ★ 新增：把 Blender DOF 写进 JSON
            # focus_distance: 焦点距离（Blender DOF 面板里的值）
            "focus_distance": dof.focus_distance,
            # aperture_fstop: 光圈 f-stop（数值越小，Blender 里景深越强）
            "aperture_fstop": dof.aperture_fstop
        })

    elif obj.type == 'LIGHT' and obj.data.type == 'POINT':
        data["lights"].append({
            "name": obj.name,
            "location": list(obj.location),
            "intensity": obj.data.energy
        })

    elif obj.type == 'MESH':
        if "Sphere" in obj.name:
            data["spheres"].append({
                "name": obj.name,
                "location": list(obj.location),
                "radius": obj.scale[0]
            })
        elif "Cube" in obj.name:
            data["cubes"].append({
                "name": obj.name,
                "translation": list(obj.location),
                "rotation": list(obj.rotation_euler),
                "scale": obj.scale[0]
            })
        elif obj.type == 'MESH' and "Plane" in obj.name:
            # 使用世界矩阵，把局部坐标变成世界坐标（会带上 scale / rotation / location）
            verts = [list((obj.matrix_world @ v.co)) for v in obj.data.vertices]
            data["planes"].append({
                "name": obj.name,
                "corners": verts[:4]
            })

# wite json
output_path = Path(bpy.path.abspath("//demo.json"))
with output_path.open("w", encoding="utf-8") as f:
    json.dump(data, f, indent=4)
print(f"Exported scene to {output_path}")
