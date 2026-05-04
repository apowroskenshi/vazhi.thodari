#include "scene/assimp_loader.h"
#include "geometry/mesh_triangle.h"
#include "geometry/triangle.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/version.h>
#include <assimp/pbrmaterial.h>
#include <assimp/ProgressHandler.hpp>

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <unordered_set>

namespace fs = std::filesystem;

namespace vt {

namespace {

class LoadProgress : public Assimp::ProgressHandler {
public:
    bool Update(float percentage) override {
        printf("\rAssimp loading: %.1f%%    ", percentage * 100.0f);
        fflush(stdout);
        return true;
    }
};

std::shared_ptr<Texture> tryLoadTexture(const aiMaterial* mtl, aiTextureType type,
                                           const std::string& model_path,
                                           Scene& scene) {
    aiString tp;
    if (mtl->GetTexture(type, 0, &tp) != AI_SUCCESS || tp.length == 0)
        return nullptr;

    // Skip embedded textures
    if (tp.C_Str()[0] == '*') return nullptr;

    try {
        fs::path full_path = model_path;
        full_path.replace_filename(tp.C_Str());
        if (!fs::exists(full_path)) return nullptr;

        auto tex = std::make_shared<Texture>(full_path.string());
        return scene.addTexture(tex);
    } catch (const std::exception&) {
        return nullptr;
    }
}

Material* extractGltfMaterial(const aiMaterial* mtl, const std::string& model_path,
                                Material* fallback, Scene& scene) {
    aiString name;
    mtl->Get(AI_MATKEY_NAME, name);

    // Check emissive
    aiColor3D emit(0.f, 0.f, 0.f);
    mtl->Get(AI_MATKEY_COLOR_EMISSIVE, emit);
    bool has_emission = (emit.r + emit.g + emit.b) > 0.01f;

    aiString emissive_path;
    bool has_emissive_tex = (mtl->GetTexture(aiTextureType_EMISSIVE, 0, &emissive_path) == AI_SUCCESS);

    if (has_emission || has_emissive_tex) {
        auto mat = std::make_unique<Material>(Material::light(
            has_emission ? vec3(emit.r, emit.g, emit.b) : vec3(1.0f)));
        if (has_emissive_tex) {
            mat->tex_emissive = tryLoadTexture(mtl, aiTextureType_EMISSIVE, model_path, scene);
        }
        Material* raw = mat.get();
        scene.materials.push_back(std::move(mat));
        return raw;
    }

    // Base color
    aiColor4D base_color(1.f, 1.f, 1.f, 1.f);
    mtl->Get(AI_MATKEY_BASE_COLOR, base_color);
    vec3 Kd = vec3(base_color.r, base_color.g, base_color.b);

    float metallic = 0.0f, roughness = 0.5f, ior = 1.5f;
    mtl->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
    mtl->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
    mtl->Get(AI_MATKEY_REFRACTI, ior);

    float f0 = std::pow((ior - 1.0f) / (ior + 1.0f), 2.0f);
    vec3 Ks = vec3(f0);

    float alpha = 2.0f / (roughness * roughness) - 2.0f;
    alpha = glm::max(alpha, 1.0f);

    auto mat = std::make_unique<Material>(Material::diffuse(Kd, Ks, alpha));
    mat->roughness = glm::max(roughness, 0.01f);
    mat->metalness = metallic;
    mat->ior = ior;

    // Textures
    mat->tex = tryLoadTexture(mtl, aiTextureType_BASE_COLOR, model_path, scene);
    if (!mat->tex)
        mat->tex = tryLoadTexture(mtl, aiTextureType_DIFFUSE, model_path, scene);

    mat->tex_bump = tryLoadTexture(mtl, aiTextureType_NORMALS, model_path, scene);
    if (!mat->tex_bump)
        mat->tex_bump = tryLoadTexture(mtl, aiTextureType_HEIGHT, model_path, scene);

    mat->tex_metal_roughness = tryLoadTexture(mtl, aiTextureType_METALNESS, model_path, scene);
    if (!mat->tex_metal_roughness)
        mat->tex_metal_roughness = tryLoadTexture(mtl, aiTextureType_UNKNOWN, model_path, scene);

    mat->tex_opacity = tryLoadTexture(mtl, aiTextureType_OPACITY, model_path, scene);

    Material* raw = mat.get();
    scene.materials.push_back(std::move(mat));
    return raw;
}

Material* extractObjMaterial(const aiMaterial* mtl, const std::string& model_path,
                               Material* fallback, Scene& scene) {
    aiString name;
    mtl->Get(AI_MATKEY_NAME, name);

    aiColor3D emit(0.f, 0.f, 0.f);
    mtl->Get(AI_MATKEY_COLOR_EMISSIVE, emit);
    bool has_emission = (emit.r + emit.g + emit.b) > 0.01f;

    if (has_emission) {
        auto mat = std::make_unique<Material>(Material::light(vec3(emit.r, emit.g, emit.b)));
        Material* raw = mat.get();
        scene.materials.push_back(std::move(mat));
        return raw;
    }

    aiColor3D diff(0.f, 0.f, 0.f), spec(0.f, 0.f, 0.f);
    mtl->Get(AI_MATKEY_COLOR_DIFFUSE, diff);
    mtl->Get(AI_MATKEY_COLOR_SPECULAR, spec);

    float shininess = 50.0f;
    mtl->Get(AI_MATKEY_SHININESS, shininess);
    if (shininess <= 0.0f) shininess = 50.0f;

    aiColor3D ones(1, 1, 1);
    if (diff == ones && spec == ones) return fallback;

    bool has_diffuse = (diff.r + diff.g + diff.b) > 0.01f;
    vec3 Kd = has_diffuse ? vec3(diff.r, diff.g, diff.b) : vec3(0.5f);
    vec3 Ks = (spec.r + spec.g + spec.b > 0.001f)
        ? vec3(spec.r, spec.g, spec.b) : vec3(0.04f);

    float alpha = std::max(1.0f, shininess);

    auto mat = std::make_unique<Material>(Material::diffuse(Kd, Ks, alpha));

    // Transmission
    aiColor3D tf(0.f, 0.f, 0.f);
    mtl->Get(AI_MATKEY_COLOR_TRANSPARENT, tf);
    float ior = 1.5f;
    mtl->Get(AI_MATKEY_REFRACTI, ior);

    if (tf != aiColor3D(1.f, 1.f, 1.f) && tf != aiColor3D(0.f, 0.f, 0.f)) {
        mat->Kt = vec3(tf.r, tf.g, tf.b);
        mat->ior = ior;
    }

    // Textures
    mat->tex = tryLoadTexture(mtl, aiTextureType_DIFFUSE, model_path, scene);
    mat->tex_ks = tryLoadTexture(mtl, aiTextureType_SPECULAR, model_path, scene);
    mat->tex_alpha = tryLoadTexture(mtl, aiTextureType_SHININESS, model_path, scene);
    mat->tex_bump = tryLoadTexture(mtl, aiTextureType_NORMALS, model_path, scene);
    if (!mat->tex_bump)
        mat->tex_bump = tryLoadTexture(mtl, aiTextureType_HEIGHT, model_path, scene);
    mat->tex_opacity = tryLoadTexture(mtl, aiTextureType_OPACITY, model_path, scene);
    mat->tex_emissive = tryLoadTexture(mtl, aiTextureType_EMISSIVE, model_path, scene);

    // Emissive texture without emissive factor
    if (!has_emission && mat->tex_emissive) {
        auto light_mat = std::make_unique<Material>(Material::light(vec3(1.0f)));
        light_mat->tex_emissive = mat->tex_emissive;
        Material* raw = light_mat.get();
        scene.materials.push_back(std::move(light_mat));
        // mat goes out of scope and is freed automatically
        return raw;
    }

    Material* raw = mat.get();
    scene.materials.push_back(std::move(mat));
    return raw;
}

void recurseNodes(const aiScene* ai_scene, const aiNode* node,
                   const aiMatrix4x4& parent_tr,
                   const std::vector<Material*>& materials,
                   Scene& scene, bool use_mesh_materials)
{
    aiMatrix4x4 child_tr = parent_tr * node->mTransformation;
    aiMatrix3x3 normal_tr(child_tr);

    for (unsigned int m = 0; m < node->mNumMeshes; ++m) {
        aiMesh* mesh = ai_scene->mMeshes[node->mMeshes[m]];
        Material* mat = use_mesh_materials
            ? materials[mesh->mMaterialIndex]
            : scene.m_currentMat;

        if (mat && mat->skip_geometry) continue;

        bool has_normals = mesh->HasNormals();
        bool has_uvs = mesh->HasTextureCoords(0);

        // Pre-transform vertices
        std::vector<vec3> positions(mesh->mNumVertices);
        std::vector<vec3> normals(mesh->mNumVertices);
        std::vector<vec2> uvs(mesh->mNumVertices);

        for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
            aiVector3D p = child_tr * mesh->mVertices[v];
            positions[v] = vec3(p.x, p.y, p.z);

            if (has_normals) {
                aiVector3D n = normal_tr * mesh->mNormals[v];
                normals[v] = normalize(vec3(n.x, n.y, n.z));
            } else {
                normals[v] = vec3(0, 1, 0);
            }

            if (has_uvs) {
                uvs[v] = vec2(mesh->mTextureCoords[0][v].x,
                              mesh->mTextureCoords[0][v].y);
            }
        }

        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            aiFace* face = &mesh->mFaces[f];

            for (unsigned int i = 2; i < face->mNumIndices; i++) {
                unsigned int i0 = face->mIndices[0];
                unsigned int i1 = face->mIndices[i - 1];
                unsigned int i2 = face->mIndices[i];

                vec3 n0 = normals[i0], n1 = normals[i1], n2 = normals[i2];
                if (!has_normals) {
                    vec3 face_n = normalize(cross(positions[i1] - positions[i0],
                                                   positions[i2] - positions[i0]));
                    n0 = n1 = n2 = face_n;
                }

                if (use_mesh_materials) {
                    auto tri = std::make_unique<MeshTriangle>(
                        positions[i0], positions[i1], positions[i2],
                        n0, n1, n2,
                        uvs[i0], uvs[i1], uvs[i2],
                        mat);

                    if (mat && mat->isEmissive()) {
                        scene.m_areaLight.addTriangle(tri.get());
                    }
                    scene.shapes.push_back(std::move(tri));
                } else {
                    scene.shapes.push_back(std::make_unique<Triangle>(
                        positions[i0], positions[i1], positions[i2], mat));
                }
            }
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        recurseNodes(ai_scene, node->mChildren[i], child_tr,
                      materials, scene, use_mesh_materials);
    }
}

} // anonymous namespace

void loadAssimpFile(const std::string& path, const mat4& transform,
                    Scene& scene, bool use_mesh_materials) {
    std::string fullPath = kModelDir + "/" + path;

    std::ifstream test(fullPath);
    if (test.fail()) {
        throw std::runtime_error("Model file not found: " + fullPath);
    }

    printf("Assimp %d.%d reading: %s\n",
           aiGetVersionMajor(), aiGetVersionMinor(), fullPath.c_str());

    Assimp::Importer importer;
    importer.SetProgressHandler(new LoadProgress());

    unsigned int flags = aiProcess_Triangulate;
    if (!use_mesh_materials) flags |= aiProcess_GenSmoothNormals;

    const aiScene* ai_scene = importer.ReadFile(fullPath.c_str(), flags);
    if (!ai_scene || !ai_scene->mRootNode) {
        throw std::runtime_error("Assimp failed: " + std::string(importer.GetErrorString()));
    }

    printf("  Meshes: %d, Materials: %d\n", ai_scene->mNumMeshes, ai_scene->mNumMaterials);

    // Convert glm::mat4 to Assimp's row-major format
    const mat4& M = transform;
    aiMatrix4x4 model_tr(
        M[0][0], M[1][0], M[2][0], M[3][0],
        M[0][1], M[1][1], M[2][1], M[3][1],
        M[0][2], M[1][2], M[2][2], M[3][2],
        M[0][3], M[1][3], M[2][3], M[3][3]);

    // Extract materials
    auto fallback_uptr = std::make_unique<Material>(Material::diffuse(vec3(0.5f), vec3(0.04f), 10.0f));
    Material* fallback = fallback_uptr.get();
    scene.materials.push_back(std::move(fallback_uptr));

    fs::path file_path(fullPath);
    std::string ext = file_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    bool is_gltf = (ext == ".gltf" || ext == ".glb");

    std::vector<Material*> materials;
    materials.reserve(ai_scene->mNumMaterials);

    if (use_mesh_materials) {
        for (unsigned int i = 0; i < ai_scene->mNumMaterials; i++) {
            if (is_gltf) {
                materials.push_back(extractGltfMaterial(
                    ai_scene->mMaterials[i], fullPath, fallback, scene));
            } else {
                materials.push_back(extractObjMaterial(
                    ai_scene->mMaterials[i], fullPath, fallback, scene));
            }
        }
    } else {
        for (unsigned int i = 0; i < ai_scene->mNumMaterials; i++) {
            materials.push_back(scene.m_currentMat);
        }
    }

    size_t before = scene.shapes.size();
    recurseNodes(ai_scene, ai_scene->mRootNode, model_tr, materials, scene, use_mesh_materials);
    printf("  Added %zu shapes (%zu total)\n", scene.shapes.size() - before, scene.shapes.size());
}

} // namespace vt
