#include "lighting/environment_map_manager.h"
#include <cstdio>

namespace vt {

void EnvironmentMapManager::add(const std::string& filename, const std::string& name) {
    MapEntry entry;
    entry.path = m_baseDir + "/" + filename;
    entry.name = name.empty() ? filename : name;
    m_maps.push_back(std::move(entry));
}

int EnvironmentMapManager::loadAll() {
    int loaded = 0;
    printf("Loading %zu environment maps...\n", m_maps.size());

    for (size_t i = 0; i < m_maps.size(); i++) {
        printf("  [%zu] %s ... ", i, m_maps[i].name.c_str());
        fflush(stdout);

        if (m_maps[i].map.load(m_maps[i].path)) {
            m_maps[i].map.build();
            loaded++;
            printf("OK\n");
        } else {
            printf("FAILED\n");
        }
    }

    printf("Environment maps loaded: %d / %zu\n", loaded, m_maps.size());
    return loaded;
}

EnvironmentMap* EnvironmentMapManager::get(int index) {
    if (index < 0 || index >= static_cast<int>(m_maps.size())) {
        fprintf(stderr, "EnvMap index %d out of range [0, %zu)\n",
                index, m_maps.size());
        return nullptr;
    }
    if (!m_maps[index].map.isLoaded()) {
        fprintf(stderr, "EnvMap index %d failed to load: %s\n",
                index, m_maps[index].name.c_str());
        return nullptr;
    }
    return &m_maps[index].map;
}

} // namespace vt
