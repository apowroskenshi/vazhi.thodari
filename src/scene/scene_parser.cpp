#include "scene/scene_parser.h"
#include "scene/assimp_loader.h"
#include "scene/scene_cache.h"
#include "geometry/sphere.h"
#include "geometry/box.h"
#include "geometry/cylinder.h"
#include "geometry/triangle.h"
#include "geometry/mesh_triangle.h"
#include "camera/camera.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <cstdio>

namespace vt {

namespace {

quat parseOrientation(int i,
                       const std::vector<std::string>& strings,
                       const std::vector<float>& f)
{
    quat q(1, 0, 0, 0);
    while (i < static_cast<int>(strings.size())) {
        std::string c = strings[i++];
        if (c == "x")
            q *= angleAxis(f[i++] * RADIANS, vec3(1, 0, 0));
        else if (c == "y")
            q *= angleAxis(f[i++] * RADIANS, vec3(0, 1, 0));
        else if (c == "z")
            q *= angleAxis(f[i++] * RADIANS, vec3(0, 0, 1));
        else if (c == "q") {
            q *= quat(f[i], f[i + 1], f[i + 2], f[i + 3]);
            i += 4;
        }
        else if (c == "a") {
            q *= angleAxis(f[i] * RADIANS, normalize(vec3(f[i + 1], f[i + 2], f[i + 3])));
            i += 4;
        }
    }
    return q;
}

void processCommand(const std::vector<std::string>& strings,
                    const std::vector<float>& f,
                    Scene& scene)
{
    if (strings.empty()) return;
    const std::string& cmd = strings[0];

    if (cmd == "screen") {
        scene.width = static_cast<int>(f[1]);
        scene.height = static_cast<int>(f[2]);
    }
    else if (cmd == "camera") {
        // Format: camera x y z ry yaw pitch
        // OR:     camera x y z ry <orientation spec>  (e.g. "y 30 x -10")
        // Detect which format by checking if f[5] is a named axis or a raw number
        quat orientation;
        if (strings.size() > 5 && (strings[5] == "x" || strings[5] == "y" ||
                                    strings[5] == "z" || strings[5] == "q" || strings[5] == "a")) {
            orientation = parseOrientation(5, strings, f);
        } else if (strings.size() >= 7) {
            // Raw yaw pitch (matches camera.txt hot-reload format)
            float yaw = f[5];
            float pitch = f[6];
            orientation = angleAxis(yaw * RADIANS, vec3(0, 1, 0))
                        * angleAxis(pitch * RADIANS, vec3(1, 0, 0));
        }
        scene.camera = std::make_unique<Camera>(
            vec3(f[1], f[2], f[3]),
            orientation,
            f[4]);
    }
    else if (cmd == "ambient") {
        // Ambient light (unused in full path tracing, kept for compatibility)
    }
    else if (cmd == "brdf") {
        std::unique_ptr<Material> mat;
        if (f.size() >= 12) {
            mat = std::make_unique<Material>(Material::withTransmission(
                vec3(f[1], f[2], f[3]), vec3(f[4], f[5], f[6]),
                f[7], vec3(f[8], f[9], f[10]), f[11]));
        } else {
            mat = std::make_unique<Material>(Material::diffuse(
                vec3(f[1], f[2], f[3]), vec3(f[4], f[5], f[6]), f[7]));
        }
        scene.m_currentMat = mat.get();
        scene.materials.push_back(std::move(mat));
    }
    else if (cmd == "light") {
        auto mat = std::make_unique<Material>(Material::light(vec3(f[1], f[2], f[3])));
        scene.m_currentMat = mat.get();
        scene.materials.push_back(std::move(mat));
    }
    else if (cmd == "sphere") {
        auto shape = std::make_unique<Sphere>(
            vec3(f[1], f[2], f[3]), f[4], scene.m_currentMat);

        if (scene.m_currentMat && scene.m_currentMat->isEmissive()) {
            scene.m_lightShape = shape.get();
            scene.m_lightPos = vec3(f[1], f[2], f[3]);
        }
        scene.shapes.push_back(std::move(shape));
    }
    else if (cmd == "box") {
        scene.shapes.push_back(std::make_unique<Box>(
            vec3(f[1], f[2], f[3]), vec3(f[4], f[5], f[6]), scene.m_currentMat));
    }
    else if (cmd == "cylinder") {
        scene.shapes.push_back(std::make_unique<Cylinder>(
            vec3(f[1], f[2], f[3]), vec3(f[4], f[5], f[6]), f[7], scene.m_currentMat));
    }
    else if (cmd == "mesh") {
        mat4 model_tr = translate(vec3(f[2], f[3], f[4]))
            * scale(vec3(f[5], f[5], f[5]))
            * toMat4(parseOrientation(6, strings, f));

        std::string fullPath = vt::kModelDir + "/" + strings[1];
        std::string cachePath = fullPath + ".cache";
        if (SceneCache::isValid(cachePath, fullPath)) {
            SceneCache::load(cachePath, scene.shapes, scene.materials,
                scene.textures, scene.m_areaLight, scene.m_areaLightEmitScale);
        } else {
            loadAssimpFile(strings[1], model_tr, scene, /*use_mesh_materials=*/false);
            SceneCache::save(cachePath, scene.shapes, scene.materials,
                scene.textures, fullPath);
        }
    }
    else if (cmd == "meshv2") {
        mat4 model_tr = translate(vec3(f[2], f[3], f[4]))
            * scale(vec3(f[5], f[5], f[5]))
            * toMat4(parseOrientation(6, strings, f));

        std::string fullPath = vt::kModelDir + "/" + strings[1];
        std::string cachePath = fullPath + ".cache";
        if (SceneCache::isValid(cachePath, fullPath)) {
            SceneCache::load(cachePath, scene.shapes, scene.materials,
                scene.textures, scene.m_areaLight, scene.m_areaLightEmitScale);
        } else {
            loadAssimpFile(strings[1], model_tr, scene, /*use_mesh_materials=*/true);
            SceneCache::save(cachePath, scene.shapes, scene.materials,
                scene.textures, fullPath);
        }
    }
    else if (cmd == "ibl") {
        scene.m_envMapManager.add("kloofendal_48d_partly_cloudy_puresky_4k.hdr", "Cloud Noon");
        scene.m_envMapManager.add("noon_grass_4k.hdr", "Afternoon");
        scene.m_envMapManager.add("umhlanga_sunrise_4k.hdr", "Interior - Morning");
        scene.m_envMapManager.add("golden_bay_4k.hdr", "Interior - Night");

        scene.m_envMapManager.loadAll();
        scene.m_activeEnvMap = scene.m_envMapManager.get(0);

        if (scene.m_activeEnvMap) {
            scene.m_activeEnvMap->rotation_offset = f[1] * PI / 180.0f;
        }
        scene.m_iblIntensity = f[2];
        scene.m_useIbl = (scene.m_activeEnvMap != nullptr);
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd.c_str());
    }
}

} // anonymous namespace

void parseSceneFile(const std::string& path, Scene& scene) {
    std::ifstream input(path);
    if (input.fail()) {
        throw std::runtime_error("Scene file not found: " + path);
    }

    for (std::string line; std::getline(input, line);) {
        std::vector<std::string> strings;
        std::vector<float> floats;

        std::stringstream ss(line);
        for (std::string s; ss >> s;) {
            float f;
            if (!(std::stringstream(s) >> f)) f = std::nanf("");
            floats.push_back(f);
            strings.push_back(s);
        }

        if (strings.empty()) continue;
        if (strings[0][0] == '#') continue;

        processCommand(strings, floats, scene);
    }
}

} // namespace vt
