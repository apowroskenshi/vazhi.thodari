#include "core/config.h"
#include "scene/scene.h"
#include "camera/camera.h"
#include "scene/assimp_loader.h"
#include "geometry/sphere.h"
#include "geometry/box.h"
#include "geometry/cylinder.h"

#include <toml++/toml.hpp>
#include <cstdio>
#include <unordered_map>

namespace vt {

// ─── Helper: read vec3 from a TOML node that should be an array [x, y, z] ──

static vec3 toVec3(const toml::node* node, vec3 fallback = vec3(0.0f)) {
    if (!node) return fallback;
    auto arr = node->as_array();
    if (!arr || arr->size() < 3) return fallback;
    return vec3((*arr)[0].value_or(0.0), (*arr)[1].value_or(0.0), (*arr)[2].value_or(0.0));
}

// ─── loadConfig ─────────────────────────────────────────────────

SceneConfig loadConfig(const std::string& path) {
    SceneConfig cfg;

    toml::table tbl;
    try {
        tbl = toml::parse_file(path);
    } catch (const toml::parse_error& err) {
        fprintf(stderr, "Config parse error (%s): %s\n", path.c_str(), err.what());
        return cfg;
    }

    // [scene]
    cfg.width  = tbl["scene"]["width"].value_or(cfg.width);
    cfg.height = tbl["scene"]["height"].value_or(cfg.height);

    // [[scene.ibl_maps]]
    if (auto iblArray = tbl["scene"]["ibl_maps"].as_array()) {
        for (const auto& node : *iblArray) {
            if (auto* iblTable = node.as_table()) {
                const auto& ibl = *iblTable;
                IblMapEntry entry;
                entry.path = ibl["path"].value_or(std::string{});
                entry.name = ibl["name"].value_or(entry.path);
                if (!entry.path.empty())
                    cfg.ibl_maps.push_back(std::move(entry));
            }
        }
    }

    // [materials.*]
    if (auto* materialsTable = tbl["materials"].as_table()) {
        for (auto& [key, val] : *materialsTable) {
            if (auto* matTable = val.as_table()) {
                const auto& mat = *matTable;
                MaterialEntry entry;
                entry.name = std::string(key.str());

                std::string type = mat["type"].value_or(std::string{"brdf"});
                if (type == "light") {
                    entry.is_light = true;
                    entry.emission = toVec3(matTable->get("emission"), vec3(1.0f));
                } else {
                    entry.Kd        = toVec3(matTable->get("Kd"), entry.Kd);
                    entry.Ks        = toVec3(matTable->get("Ks"), entry.Ks);
                    entry.Kt        = toVec3(matTable->get("Kt"), entry.Kt);
                    entry.alpha     = mat["alpha"].value_or(entry.alpha);
                    entry.ior       = mat["ior"].value_or(entry.ior);
                    entry.roughness = mat["roughness"].value_or(entry.roughness);
                    entry.metalness = mat["metalness"].value_or(entry.metalness);
                }

                cfg.named_materials.push_back(std::move(entry));
            }
        }
    }

    // [[mesh]]
    if (auto* meshArray = tbl["mesh"].as_array()) {
        for (const auto& node : *meshArray) {
            if (auto* meshTable = node.as_table()) {
                const auto& mesh = *meshTable;
                MeshEntry entry;
                entry.path     = mesh["path"].value_or(std::string{});
                entry.position = toVec3(meshTable->get("position"), entry.position);
                entry.scale    = mesh["scale"].value_or(entry.scale);
                entry.yaw      = mesh["yaw"].value_or(entry.yaw);
                entry.pitch    = mesh["pitch"].value_or(entry.pitch);
                if (!entry.path.empty())
                    cfg.meshes.push_back(std::move(entry));
            }
        }
    }

    // [[sphere]]
    if (auto* sphereArray = tbl["sphere"].as_array()) {
        for (const auto& node : *sphereArray) {
            if (auto* sphereTable = node.as_table()) {
                const auto& sphere = *sphereTable;
                SphereEntry entry;
                entry.position = toVec3(sphereTable->get("position"), entry.position);
                entry.radius   = sphere["radius"].value_or(entry.radius);
                entry.material = sphere["material"].value_or(std::string{});
                cfg.spheres.push_back(std::move(entry));
            }
        }
    }

    // [[box]]
    if (auto* boxArray = tbl["box"].as_array()) {
        for (const auto& node : *boxArray) {
            if (auto* boxTable = node.as_table()) {
                const auto& box = *boxTable;
                BoxEntry entry;
                entry.corner   = toVec3(boxTable->get("corner"), entry.corner);
                entry.diagonal = toVec3(boxTable->get("diagonal"), entry.diagonal);
                entry.material = box["material"].value_or(std::string{});
                cfg.boxes.push_back(std::move(entry));
            }
        }
    }

    // [[cylinder]]
    if (auto* cylinderArray = tbl["cylinder"].as_array()) {
        for (const auto& node : *cylinderArray) {
            if (auto* cylinderTable = node.as_table()) {
                const auto& cylinder = *cylinderTable;
                CylinderEntry entry;
                entry.base     = toVec3(cylinderTable->get("base"), entry.base);
                entry.axis     = toVec3(cylinderTable->get("axis"), entry.axis);
                entry.radius   = cylinder["radius"].value_or(entry.radius);
                entry.material = cylinder["material"].value_or(std::string{});
                cfg.cylinders.push_back(std::move(entry));
            }
        }
    }

    // [camera]
    cfg.camera_position.x = tbl["camera"]["position"]["x"].value_or(cfg.camera_position.x);
    cfg.camera_position.y = tbl["camera"]["position"]["y"].value_or(cfg.camera_position.y);
    cfg.camera_position.z = tbl["camera"]["position"]["z"].value_or(cfg.camera_position.z);
    cfg.camera_ry         = tbl["camera"]["ry"].value_or(cfg.camera_ry);
    cfg.camera_yaw        = tbl["camera"]["yaw"].value_or(cfg.camera_yaw);
    cfg.camera_pitch      = tbl["camera"]["pitch"].value_or(cfg.camera_pitch);

    // [ibl]
    cfg.ibl_map_index = tbl["ibl"]["map_index"].value_or(cfg.ibl_map_index);
    cfg.ibl_rotation  = tbl["ibl"]["rotation"].value_or(cfg.ibl_rotation);
    cfg.ibl_intensity = tbl["ibl"]["intensity"].value_or(cfg.ibl_intensity);

    // [area_light]
    cfg.area_light_emit_scale = tbl["area_light"]["emit_scale"].value_or(cfg.area_light_emit_scale);

    // [render]
    cfg.render.max_passes            = tbl["render"]["max_passes"].value_or(cfg.render.max_passes);
    cfg.render.write_interval        = tbl["render"]["write_interval"].value_or(cfg.render.write_interval);
    cfg.render.noise_threshold       = tbl["render"]["noise_threshold"].value_or(cfg.render.noise_threshold);
    cfg.render.russian_roulette_prob = tbl["render"]["russian_roulette"].value_or(cfg.render.russian_roulette_prob);
    cfg.render.clamp_lights          = tbl["render"]["clamp_lights"].value_or(cfg.render.clamp_lights);
    cfg.render.clamp_factor          = tbl["render"]["clamp_factor"].value_or(cfg.render.clamp_factor);
    cfg.render.clamp_weights         = tbl["render"]["clamp_weights"].value_or(cfg.render.clamp_weights);
    cfg.render.weight_clamp_factor   = tbl["render"]["weight_clamp_factor"].value_or(cfg.render.weight_clamp_factor);

    return cfg;
}

// ─── buildScene ─────────────────────────────────────────────────

void buildScene(const SceneConfig& cfg, Scene& scene) {
    scene.width = cfg.width;
    scene.height = cfg.height;

    // Register named materials and build lookup map
    std::unordered_map<std::string, const Material*> mat_map;

    for (const auto& entry : cfg.named_materials) {
        std::unique_ptr<Material> mat;
        if (entry.is_light) {
            mat = std::make_unique<Material>(Material::light(entry.emission));
        } else {
            mat = std::make_unique<Material>(Material::diffuse(entry.Kd, entry.Ks, entry.alpha));
            mat->Kt = entry.Kt;
            mat->ior = entry.ior;
            mat->roughness = entry.roughness;
            mat->metalness = entry.metalness;
        }
        const Material* raw = mat.get();
        scene.materials.push_back(std::move(mat));
        mat_map[entry.name] = raw;
    }

    // Helper: resolve material name to pointer
    auto resolveMat = [&](const std::string& name) -> const Material* {
        if (name.empty()) return nullptr;
        auto it = mat_map.find(name);
        if (it != mat_map.end()) return it->second;
        fprintf(stderr, "Warning: material '%s' not found\n", name.c_str());
        return nullptr;
    };

    // Load IBL maps
    if (!cfg.ibl_maps.empty()) {
        for (const auto& entry : cfg.ibl_maps) {
            scene.m_envMapManager.add(entry.path, entry.name);
        }
        scene.m_envMapManager.loadAll();
        scene.m_activeEnvMap = scene.m_envMapManager.get(0);
        scene.m_useIbl = (scene.m_activeEnvMap != nullptr);
    }

    // Load meshes via Assimp
    for (const auto& entry : cfg.meshes) {
        mat4 model_tr = translate(entry.position)
            * scale(vec3(entry.scale))
            * toMat4(angleAxis(entry.yaw   * RADIANS, vec3(0, 1, 0)))
            * toMat4(angleAxis(entry.pitch * RADIANS, vec3(1, 0, 0)));

        printf("Loading mesh: %s\n", entry.path.c_str());
        loadAssimpFile(entry.path, model_tr, scene, /*use_mesh_materials=*/true);
    }

    // Create primitive shapes
    for (const auto& entry : cfg.spheres) {
        const Material* mat = resolveMat(entry.material);
        if (!mat) continue;

        auto shape = std::make_unique<Sphere>(entry.position, entry.radius, mat);
        if (mat->isEmissive()) {
            scene.m_lightShape = shape.get();
            scene.m_lightPos = entry.position;
        }
        scene.shapes.push_back(std::move(shape));
    }

    for (const auto& entry : cfg.boxes) {
        const Material* mat = resolveMat(entry.material);
        if (!mat) continue;
        scene.shapes.push_back(std::make_unique<Box>(entry.corner, entry.diagonal, mat));
    }

    for (const auto& entry : cfg.cylinders) {
        const Material* mat = resolveMat(entry.material);
        if (!mat) continue;
        scene.shapes.push_back(std::make_unique<Cylinder>(entry.base, entry.axis, entry.radius, mat));
    }
}

// ─── applyConfig (hot-reload) ───────────────────────────────────

void applyConfig(const SceneConfig& cfg, Scene& scene) {
    // Camera
    quat q = angleAxis(cfg.camera_yaw * RADIANS, vec3(0, 1, 0))
           * angleAxis(cfg.camera_pitch * RADIANS, vec3(1, 0, 0));
    scene.camera = std::make_unique<Camera>(cfg.camera_position, q, cfg.camera_ry);

    // IBL
    if (cfg.ibl_map_index != scene.m_activeEnvMapIndex) {
        auto* newMap = scene.m_envMapManager.get(cfg.ibl_map_index);
        if (newMap) {
            scene.m_activeEnvMap = newMap;
            scene.m_activeEnvMapIndex = cfg.ibl_map_index;
        }
    }
    if (scene.m_activeEnvMap) {
        scene.m_activeEnvMap->rotation_offset = cfg.ibl_rotation * PI / 180.0f;
    }
    scene.m_iblIntensity = cfg.ibl_intensity;

    // Area light
    scene.m_areaLightEmitScale = cfg.area_light_emit_scale;
}

} // namespace vt
