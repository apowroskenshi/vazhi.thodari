#pragma once

#include "lighting/environment_map.h"
#include <vector>
#include <string>

namespace vt {

// Preloads multiple HDR environment maps at startup for instant hot-swap.
class EnvironmentMapManager {
public:
    struct MapEntry {
        std::string path;
        std::string name;
        EnvironmentMap map;
    };

    EnvironmentMapManager() = default;

    void setBaseDir(const std::string& dir) { m_baseDir = dir; }
    void add(const std::string& filename, const std::string& name = "");
    int loadAll(); // Returns number successfully loaded
    EnvironmentMap* get(int index);
    int count() const { return static_cast<int>(m_maps.size()); }

    // Access entries for display names
    const std::vector<MapEntry>& entries() const { return m_maps; }

private:
    std::string m_baseDir = "assets/hdri";
    std::vector<MapEntry> m_maps;
};

} // namespace vt
