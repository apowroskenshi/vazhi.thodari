#pragma once

#include "scene/scene.h"
#include <string>

namespace vt {

// Parse a .scn scene file and populate the Scene.
// Throws std::runtime_error on file-not-found or parse errors.
void parseSceneFile(const std::string& path, Scene& scene);

} // namespace vt
