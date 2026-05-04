#pragma once

#include "core/math_types.h"
#include "material/material.h"
#include "material/texture.h"
#include "geometry/shape.h"
#include "geometry/bvh_accel.h"
#include "lighting/area_light.h"
#include "lighting/environment_map.h"
#include "lighting/environment_map_manager.h"
#include "camera/camera.h"

#include <vector>
#include <memory>

namespace vt {

class Scene {
public:
    int width = 800;
    int height = 600;

    std::vector<std::unique_ptr<Shape>> shapes;
    std::vector<std::unique_ptr<Material>> materials;
    std::vector<std::shared_ptr<Texture>> textures;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<AccelerationBvh> bvh;

    AreaLight m_areaLight;
    EnvironmentMapManager m_envMapManager;
    EnvironmentMap* m_activeEnvMap = nullptr;
    int m_activeEnvMapIndex = 0;
    bool m_useIbl = false;
    float m_iblIntensity = 1.0f;
    float m_areaLightEmitScale = 1.0f;

    vec3 m_lightPos{0.0f};
    const Shape* m_lightShape = nullptr;
    Material* m_currentMat = nullptr;

    Scene() = default;
    ~Scene();

    void build();

    std::shared_ptr<Texture> addTexture(std::shared_ptr<Texture> tex) {
        textures.push_back(tex);
        return tex;
    }
};

} // namespace vt
