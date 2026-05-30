import bpy
from mathutils import Vector
import math

#SCALE = 0.016  # match the mesh scale= value in config.toml

SCALE = 1.0

cam = bpy.data.objects['Camera']
fwd = cam.matrix_world.to_3x3() @ Vector((0, 0, -1))

# Convert Blender Z-up to Y-up
# Blender (X, Y, Z) -> Raytracer (X, Z, -Y)
fx = fwd.x
fy = fwd.z
fz = -fwd.y

# Camera at yaw=0 looks along -Z
# So yaw is the angle FROM -Z axis, rotating around Y
# atan2(fx, fz) gives angle from +Z, so negate to get angle from -Z
yaw = math.degrees(math.atan2(-fx, -fz))
pitch = math.degrees(math.asin(fy))

pos = cam.location
print(f"Blender raw: {pos.x:.3f} {pos.y:.3f} {pos.z:.3f}")
print(f"Converted:   {pos.x * SCALE:.3f} {pos.z * SCALE:.3f} {-pos.y * SCALE:.3f}")

px = pos.x * SCALE
py = pos.z * SCALE
pz = -pos.y * SCALE

print(f"{px:.3f} {py:.3f} {pz:.3f} 0.8 {yaw:.1f} {pitch:.1f}")