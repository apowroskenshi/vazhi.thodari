#include "scene/scene.h"
#include <cstdio>

namespace vt {

Scene::~Scene() = default;

void Scene::build() {
    bvh = std::make_unique<AccelerationBvh>(shapes);

    if (!m_areaLight.empty()) {
        m_areaLight.build();
        printf("Area light built: %zu emissive triangles, total area: %.4f\n",
               m_areaLight.triangleCount(), m_areaLight.totalArea());
    }
}

} // namespace vt
