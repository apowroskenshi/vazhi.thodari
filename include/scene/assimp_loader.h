#pragma once

#include "scene/scene.h"
#include <string>

namespace vt {

inline const std::string kModelDir = "assets/models";

// Load a 3D model file via Assimp and add shapes/materials/textures to the scene.
// path is relative to kModelDir (e.g. "sponza/sponza.obj").
// Supports OBJ/MTL and glTF formats.
// If use_mesh_materials is false, all geometry uses scene.m_currentMat.
void loadAssimpFile(const std::string& path, const mat4& transform,
                    Scene& scene, bool use_mesh_materials);

} // namespace vt
