#pragma once

#include "core/math_types.h"
#include "renderer/renderer.h"

#include <string>
#include <vector>

namespace vt {

// ─── Scene description (read once at startup) ───────────────────

struct MeshEntry {
    std::string path;           // relative to kModelDir
    vec3 position{0.0f};
    float scale = 1.0f;
    float yaw = 0.0f;          // degrees
};

struct SphereEntry {
    vec3 position{0.0f};
    float radius = 1.0f;
    std::string material;       // name from [materials.*]
};

struct BoxEntry {
    vec3 corner{0.0f};
    vec3 diagonal{1.0f};
    std::string material;
};

struct CylinderEntry {
    vec3 base{0.0f};
    vec3 axis{0.0f, 1.0f, 0.0f};
    float radius = 1.0f;
    std::string material;
};

struct MaterialEntry {
    std::string name;
    bool is_light = false;

    // Light
    vec3 emission{0.0f};

    // BRDF
    vec3 Kd{0.5f};
    vec3 Ks{0.04f};
    vec3 Kt{0.0f};
    float alpha = 10.0f;
    float ior = 1.5f;
    float roughness = 0.5f;
    float metalness = 0.0f;
};

struct IblMapEntry {
    std::string path;
    std::string name;
};

// ─── Full config ────────────────────────────────────────────────

struct SceneConfig {
    // Scene setup
    int width = 1544;
    int height = 1080;

    std::vector<IblMapEntry> ibl_maps;
    std::vector<MaterialEntry> named_materials;
    std::vector<MeshEntry> meshes;
    std::vector<SphereEntry> spheres;
    std::vector<BoxEntry> boxes;
    std::vector<CylinderEntry> cylinders;

    // Camera (hot-reloadable)
    vec3  camera_position{0.0f};
    float camera_ry = 0.8f;
    float camera_yaw = 0.0f;
    float camera_pitch = 0.0f;

    // IBL (hot-reloadable)
    int   ibl_map_index = 0;
    float ibl_rotation = 0.0f;
    float ibl_intensity = 1.0f;

    // Area light (hot-reloadable)
    float area_light_emit_scale = 1.0f;

    // Render (hot-reloadable)
    RenderConfig render;
};

// Parse full config from TOML. Returns default on failure.
SceneConfig loadConfig(const std::string& path);

// Build scene geometry from config (meshes, shapes, IBL maps).
// Call once after loadConfig, before scene.build().
class Scene;
void buildScene(const SceneConfig& cfg, Scene& scene);

// Apply hot-reloadable params (camera, IBL, lighting) to scene.
void applyConfig(const SceneConfig& cfg, Scene& scene);

} // namespace vt
