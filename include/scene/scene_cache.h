#pragma once

#include "core/math_types.h"
#include "geometry/mesh_triangle.h"
#include "material/material.h"
#include "material/texture.h"
#include "lighting/area_light.h"

#include <string>
#include <vector>
#include <memory>

namespace vt {

class Scene;

class SceneCache {
public:
	// Check if a valid cache exists for the given model file.
	static bool isValid(const std::string& cachePath, const std::string& modelPath);

	// Save the current scene's mesh triangles, materials, and textures to a binary cache.
	static bool save(const std::string& cachePath,
		const std::vector<std::unique_ptr<Shape>>& shapes,
		const std::vector<std::unique_ptr<Material>>& materials,
		const std::vector<std::shared_ptr<Texture>>& textures,
		const std::string& modelPath);

	// Load scene data from a binary cache.
	// Returns true on success, populating shapes/materials/textures/areaLight.
	static bool load(const std::string& cachePath,
		std::vector<std::unique_ptr<Shape>>& shapes,
		std::vector<std::unique_ptr<Material>>& materials,
		std::vector<std::shared_ptr<Texture>>& textures,
		AreaLight& areaLight,
		float emitScale = 1.0f);
};

} // namespace vt
