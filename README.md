# வழி தொடரி — Vazhi Thodari

A physically-based Monte Carlo path tracer built with C++20. Features full global illumination with PBR materials, importance-sampled IBL, area lights with MIS, BVH acceleration, and multi-threaded rendering via OpenMP.

## Features

- **Path Tracing** with Russian roulette termination and multiple importance sampling
- **PBR Materials** - GGX microfacet BRDF with metallic-roughness workflow
- **Transmission** - Refraction with Snell's law, thin-shell and volumetric modes
- **Image-Based Lighting** - Importance-sampled equirectangular HDR environment maps
- **Area Lights** - CDF-weighted emissive triangle sampling
- **Normal Mapping** - Tangent-space perturbation with Gram-Schmidt orthogonalization
- **Texture Support** - Diffuse, specular, roughness, normal, opacity, emissive, metallic-roughness
- **BVH Acceleration** - SAH-optimized bounding volume hierarchy
- **Multi-Format Loading** - OBJ/MTL and glTF via Assimp
- **Multi-Threaded** - OpenMP parallel rendering
- **Camera Animation** - Keyframe paths with Catmull-Rom interpolation
- **HDR Output** - RGBE and tonemapped PNG with ACES

## Architecture

```
main
 |
 +-- SceneParser        Reads .scn scene description files
 +-- AssimpLoader        Loads OBJ/glTF models with materials
 |
 +-- Scene              Owns shapes, materials, textures, lights
 |    +-- AccelerationBvh    SAH BVH for ray-scene intersection
 |    +-- AreaLight          CDF-sampled emissive triangles
 |    +-- EnvironmentMap     Importance-sampled HDR IBL
 |
 +-- Renderer           Monte Carlo path tracing
 |    +-- tracePath()        Per-ray path integration
 |    |    +-- Explicit light sampling (area + IBL)
 |    |    +-- BRDF sampling (diffuse + specular + transmission)
 |    |    +-- MIS weighting (power heuristic)
 |    +-- traceImage()       Per-pass parallel rendering
 |
 +-- ImageBuffer        Accumulation with variance tracking
 +-- HdrWriter          RGBE output
 +-- PngWriter          Tonemapped PNG (ACES)
```

### BRDF

Cook-Torrance microfacet model with three lobes:

| Lobe | Model | Description |
|------|-------|-------------|
| Diffuse | Lambertian | Fresnel-attenuated, energy-conserving |
| Specular | GGX | Microfacet distribution with Smith geometry |
| Transmission | Snell + GGX | Refraction with total internal reflection |

Lobe selection is probabilistic, weighted by material properties. MIS balances explicit light sampling against BRDF sampling using the power heuristic.

### Geometry

| Primitive | Intersection |
|-----------|-------------|
| Sphere | Analytic quadratic |
| Box | Slab intersection |
| Cylinder | Rotated revolution surface |
| Triangle | Moller-Trumbore |
| MeshTriangle | Smooth normals, UVs, tangents, alpha testing |

All primitives implement `Shape` interface with `intersect()` and `boundingBox()`.

## Source Layout

```
src/
  main.cpp              Entry point, render loop, output
  sphere.cpp            Sphere intersection
  box.cpp               AABB slab intersection
  cylinder.cpp          Cylinder intersection
  triangle.cpp          Flat triangle
  mesh_triangle.cpp     Smooth-shaded mesh triangle
  bvh_accel.cpp         BVH construction and traversal
  material.cpp          PBR material with texture resolve
  texture.cpp           Image loading and bilinear sampling
  area_light.cpp        CDF-based emissive triangle sampling
  environment_map.cpp   HDR IBL with 2D importance sampling
  environment_map_manager.cpp  Multi-map preloading
  renderer.cpp          Path tracing core
  brdf.cpp              GGX evaluation, sampling, PDF
  scene.cpp             Scene container and BVH build
  scene_parser.cpp      .scn file format parser
  assimp_loader.cpp     OBJ/glTF model import
  camera_path.cpp       Keyframe animation
  hdr_writer.cpp        RGBE file output
  png_writer.cpp        Tonemapped PNG output
  rgbe.cpp              RGBE codec
  stb_image.cpp         STB implementation unit

  include/              All headers (flat)

external/
  bvh/v2/              BVH library (madmann91, header-only)
  stb_image.h          Image decoding
  stb_image_write.h    PNG encoding
  rgbe.h               RGBE format
```

## Dependencies

Managed via [vcpkg](https://vcpkg.io):

| Library | Purpose |
|---------|---------|
| assimp | 3D model loading (OBJ, glTF) |
| glm | Mathematics (vectors, matrices, quaternions) |

Built-in (vendored in `external/`):
- **BVH v2** - Ray tracing acceleration structure
- **stb_image** - Image loading
- **stb_image_write** - PNG encoding

Optional:
- **OpenMP** - Parallel rendering (auto-detected)

## Building

### Visual Studio (recommended)

```
1. Ensure vcpkg is integrated: vcpkg integrate install
2. Open vazhi.thodari.slnx in Visual Studio 2022
3. Build x64 Release
4. Run from project root with scene file as argument
```

### CMake

```
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

## Usage

```
vazhi_thodari.exe scene.scn
```

### Scene File Format (.scn)

```
screen 1920 1080
camera 0 2 8  0.6
brdf 0.5 0.5 0.5  0.04 0.04 0.04  50
sphere 0 1 0  1.0
mesh sponza/sponza.obj  0 0 0  0.01
ibl 0 1.0
```

| Command | Parameters |
|---------|-----------|
| `screen` | width height |
| `camera` | x y z ry [rotations...] |
| `brdf` | Kd.rgb Ks.rgb alpha [Kt.rgb ior] |
| `light` | emission.rgb |
| `sphere` | x y z radius |
| `box` | x y z dx dy dz |
| `cylinder` | base.xyz axis.xyz radius |
| `mesh` | path tx ty tz scale [rotations...] |
| `ibl` | rotation_deg intensity |
