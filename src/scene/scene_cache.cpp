#include "scene/scene_cache.h"
#include "scene/scene.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

namespace vt {

namespace {

constexpr uint32_t kCacheMagic   = 0x56544348; // "VTCH"
constexpr uint32_t kCacheVersion = 1;
constexpr uint32_t kNoTexture    = 0xFFFFFFFF;

struct CacheHeader {
	uint32_t magic;
	uint32_t version;
	uint64_t modelFileSize;
	uint64_t modelModTime;
	uint32_t numTriangles;
	uint32_t numMaterials;
	uint32_t numTextures;
};

struct CachedTextureHeader {
	int32_t  width;
	int32_t  height;
	int32_t  depth;
	uint32_t pathLength;
	// Followed by: pathLength bytes of path string
	// Followed by: width * height * 4 bytes of RGBA pixel data
};

struct CachedMaterial {
	vec3     Kd, Ks, Kt;
	float    alpha;
	float    ior;
	float    roughness;
	float    metalness;
	bool     emissive;
	bool     thin_shell;
	uint32_t texIndex;
	uint32_t texKsIndex;
	uint32_t texAlphaIndex;
	uint32_t texBumpIndex;
	uint32_t texOpacityIndex;
	uint32_t texEmissiveIndex;
	uint32_t texMetalRoughnessIndex;
};

struct CachedTriangle {
	vec3     v0, v1, v2;
	vec3     n0, n1, n2;
	vec2     uv0, uv1, uv2;
	vec3     t0, t1, t2;
	uint32_t materialIndex;
};

} // namespace

// --- Validation ---

bool SceneCache::isValid(const std::string& cachePath, const std::string& modelPath)
{
	if (!fs::exists(cachePath) || !fs::exists(modelPath))
		return false;

	FILE* f = fopen(cachePath.c_str(), "rb");
	if (!f) return false;

	CacheHeader header;
	if (fread(&header, sizeof(CacheHeader), 1, f) != 1) {
		fclose(f);
		return false;
	}
	fclose(f);

	if (header.magic != kCacheMagic || header.version != kCacheVersion) {
		printf("Cache version mismatch — rebuilding\n");
		return false;
	}

	auto modelSize = fs::file_size(modelPath);
	auto modelTime = fs::last_write_time(modelPath).time_since_epoch().count();

	if (header.modelFileSize != static_cast<uint64_t>(modelSize) ||
		header.modelModTime != static_cast<uint64_t>(modelTime)) {
		printf("Model file changed — rebuilding cache\n");
		return false;
	}

	printf("Cache valid: %s\n", cachePath.c_str());
	return true;
}

// --- Save ---

bool SceneCache::save(const std::string& cachePath,
	const std::vector<std::unique_ptr<Shape>>& shapes,
	const std::vector<std::unique_ptr<Material>>& materials,
	const std::vector<std::shared_ptr<Texture>>& textures,
	const std::string& modelPath)
{
	printf("Saving scene cache: %s\n", cachePath.c_str());

	FILE* f = fopen(cachePath.c_str(), "wb");
	if (!f) {
		fprintf(stderr, "Failed to open cache for writing: %s\n", cachePath.c_str());
		return false;
	}

	// Build deduplicated texture index map
	std::unordered_map<const Texture*, uint32_t> texIndexMap;
	for (uint32_t i = 0; i < textures.size(); i++) {
		texIndexMap[textures[i].get()] = i;
	}

	// Build material index map
	std::unordered_map<const Material*, uint32_t> matIndexMap;
	for (uint32_t i = 0; i < materials.size(); i++) {
		matIndexMap[materials[i].get()] = i;
	}

	// Count mesh triangles
	uint32_t triCount = 0;
	for (const auto& shape : shapes) {
		if (dynamic_cast<const MeshTriangle*>(shape.get())) triCount++;
	}

	// Write header
	CacheHeader header{};
	header.magic = kCacheMagic;
	header.version = kCacheVersion;
	header.modelFileSize = static_cast<uint64_t>(fs::file_size(modelPath));
	header.modelModTime = static_cast<uint64_t>(
		fs::last_write_time(modelPath).time_since_epoch().count());
	header.numTriangles = triCount;
	header.numMaterials = static_cast<uint32_t>(materials.size());
	header.numTextures = static_cast<uint32_t>(textures.size());
	fwrite(&header, sizeof(CacheHeader), 1, f);

	// Write textures
	for (const auto& tex : textures) {
		CachedTextureHeader th{};
		th.width = tex->width();
		th.height = tex->height();
		th.depth = 4;
		th.pathLength = static_cast<uint32_t>(tex->path().size());

		fwrite(&th, sizeof(CachedTextureHeader), 1, f);

		if (th.pathLength > 0) {
			fwrite(tex->path().c_str(), 1, th.pathLength, f);
		}

		fwrite(tex->data(), 1, tex->dataSize(), f);
	}

	// Write materials
	auto getTexIdx = [&](const std::shared_ptr<Texture>& tex) -> uint32_t {
		if (!tex) return kNoTexture;
		auto it = texIndexMap.find(tex.get());
		return (it != texIndexMap.end()) ? it->second : kNoTexture;
	};

	for (const auto& matPtr : materials) {
		const Material* mat = matPtr.get();
		CachedMaterial cm{};
		cm.Kd = mat->Kd;
		cm.Ks = mat->Ks;
		cm.Kt = mat->Kt;
		cm.alpha = mat->alpha;
		cm.ior = mat->ior;
		cm.roughness = mat->roughness;
		cm.metalness = mat->metalness;
		cm.emissive = mat->emissive;
		cm.thin_shell = mat->thin_shell;

		cm.texIndex = getTexIdx(mat->tex);
		cm.texKsIndex = getTexIdx(mat->tex_ks);
		cm.texAlphaIndex = getTexIdx(mat->tex_alpha);
		cm.texBumpIndex = getTexIdx(mat->tex_bump);
		cm.texOpacityIndex = getTexIdx(mat->tex_opacity);
		cm.texEmissiveIndex = getTexIdx(mat->tex_emissive);
		cm.texMetalRoughnessIndex = getTexIdx(mat->tex_metal_roughness);

		fwrite(&cm, sizeof(CachedMaterial), 1, f);
	}

	// Write triangles
	for (const auto& shape : shapes) {
		const auto* tri = dynamic_cast<const MeshTriangle*>(shape.get());
		if (!tri) continue;

		CachedTriangle ct{};
		ct.v0 = tri->v0; ct.v1 = tri->v1; ct.v2 = tri->v2;
		ct.n0 = tri->n0; ct.n1 = tri->n1; ct.n2 = tri->n2;
		ct.uv0 = tri->uv0; ct.uv1 = tri->uv1; ct.uv2 = tri->uv2;
		ct.t0 = tri->t0; ct.t1 = tri->t1; ct.t2 = tri->t2;

		auto it = matIndexMap.find(tri->mat);
		ct.materialIndex = (it != matIndexMap.end()) ? it->second : 0;

		fwrite(&ct, sizeof(CachedTriangle), 1, f);
	}

	fclose(f);

	auto cacheSize = fs::file_size(cachePath);
	printf("Cache saved: %u triangles, %u materials, %u textures (%.1f MB)\n",
		triCount, header.numMaterials, header.numTextures,
		static_cast<float>(cacheSize) / (1024.0f * 1024.0f));

	return true;
}

// --- Load ---

bool SceneCache::load(const std::string& cachePath,
	std::vector<std::unique_ptr<Shape>>& shapes,
	std::vector<std::unique_ptr<Material>>& materials,
	std::vector<std::shared_ptr<Texture>>& textures,
	AreaLight& areaLight,
	float emitScale)
{
	printf("Loading scene cache: %s\n", cachePath.c_str());

	FILE* f = fopen(cachePath.c_str(), "rb");
	if (!f) {
		fprintf(stderr, "Failed to open cache: %s\n", cachePath.c_str());
		return false;
	}

	// Read header
	CacheHeader header;
	fread(&header, sizeof(CacheHeader), 1, f);

	printf("  Triangles: %u\n", header.numTriangles);
	printf("  Materials: %u\n", header.numMaterials);
	printf("  Textures:  %u\n", header.numTextures);

	// Read textures
	textures.clear();
	textures.reserve(header.numTextures);

	for (uint32_t i = 0; i < header.numTextures; i++) {
		CachedTextureHeader th;
		if (fread(&th, sizeof(CachedTextureHeader), 1, f) != 1) {
			fprintf(stderr, "Cache read error: texture header %u\n", i);
			fclose(f);
			return false;
		}

		std::string texPath;
		if (th.pathLength > 0) {
			texPath.resize(th.pathLength);
			fread(&texPath[0], 1, th.pathLength, f);
		}

		uint32_t pixelBytes = th.width * th.height * 4;
		std::vector<uint8_t> pixelData(pixelBytes);

		if (fread(pixelData.data(), 1, pixelBytes, f) != pixelBytes) {
			fprintf(stderr, "Cache read error: texture pixels %u (%s)\n", i, texPath.c_str());
			fclose(f);
			return false;
		}

		auto tex = std::make_shared<Texture>();
		tex->loadFromMemory(th.width, th.height, th.depth,
			pixelData.data(), pixelBytes, texPath);
		textures.push_back(std::move(tex));

		printf("  Loaded texture [%u]: %dx%d %s\n", i, th.width, th.height, texPath.c_str());
	}

	// Read materials
	materials.clear();
	materials.reserve(header.numMaterials);

	auto getTexPtr = [&](uint32_t idx) -> std::shared_ptr<Texture> {
		if (idx == kNoTexture || idx >= textures.size()) return nullptr;
		return textures[idx];
	};

	for (uint32_t i = 0; i < header.numMaterials; i++) {
		CachedMaterial cm;
		fread(&cm, sizeof(CachedMaterial), 1, f);

		auto mat = std::make_unique<Material>();
		mat->Kd = cm.Kd;
		mat->Ks = cm.Ks;
		mat->Kt = cm.Kt;
		mat->alpha = cm.alpha;
		mat->ior = cm.ior;
		mat->roughness = cm.roughness;
		mat->metalness = cm.metalness;
		mat->emissive = cm.emissive;
		mat->thin_shell = cm.thin_shell;

		mat->tex = getTexPtr(cm.texIndex);
		mat->tex_ks = getTexPtr(cm.texKsIndex);
		mat->tex_alpha = getTexPtr(cm.texAlphaIndex);
		mat->tex_bump = getTexPtr(cm.texBumpIndex);
		mat->tex_opacity = getTexPtr(cm.texOpacityIndex);
		mat->tex_emissive = getTexPtr(cm.texEmissiveIndex);
		mat->tex_metal_roughness = getTexPtr(cm.texMetalRoughnessIndex);

		materials.push_back(std::move(mat));
	}

	// Read triangles
	std::vector<CachedTriangle> triData(header.numTriangles);
	size_t read = fread(triData.data(), sizeof(CachedTriangle), header.numTriangles, f);
	fclose(f);

	if (read != header.numTriangles) {
		fprintf(stderr, "Cache read error: expected %u triangles, got %zu\n",
			header.numTriangles, read);
		return false;
	}

	shapes.reserve(shapes.size() + header.numTriangles);

	for (uint32_t i = 0; i < header.numTriangles; i++) {
		const CachedTriangle& ct = triData[i];
		const Material* mat = (ct.materialIndex < materials.size())
			? materials[ct.materialIndex].get()
			: materials[0].get();

		auto tri = std::make_unique<MeshTriangle>(
			ct.v0, ct.v1, ct.v2,
			ct.n0, ct.n1, ct.n2,
			ct.uv0, ct.uv1, ct.uv2,
			ct.t0, ct.t1, ct.t2,
			mat);

		if (mat->isEmissive()) {
			areaLight.addTriangle(tri.get());
		}

		shapes.push_back(std::move(tri));
	}

	if (!areaLight.empty()) {
		areaLight.build();
	}

	printf("Cache loaded: %u triangles, %u materials, %u textures\n",
		header.numTriangles, static_cast<uint32_t>(materials.size()),
		static_cast<uint32_t>(textures.size()));

	return true;
}

} // namespace vt
