#include "scene/scene.h"
#include "core/config.h"
#include "renderer/renderer.h"
#include "camera/camera.h"
#include "camera/camera_path.h"
#include "output/image_buffer.h"
#include "output/hdr_writer.h"
#include "output/png_writer.h"

#include <cstdio>
#include <chrono>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <conio.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
static bool _kbhit() {
    struct termios oldt, newt;
    int ch, oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if (ch != EOF) { ungetc(ch, stdin); return true; }
    return false;
}
static char _getch() { return static_cast<char>(getchar()); }
#endif

namespace fs = std::filesystem;

static const std::string kConfigPath = "config.toml";

static void renderStill(vt::Scene& scene, vt::RenderConfig config,
                         const std::string& output_dir) {
    vt::ImageBuffer buffer(scene.width, scene.height);
    auto renderer = std::make_unique<vt::Renderer>(scene, config);

    fs::create_directories(output_dir);

    printf("Press 'R' to reload config.toml, 'W' to write current image\n");

    for (int pass = 1; pass <= config.max_passes; pass++) {
        fprintf(stderr, "Pass %d/%d\n", pass, config.max_passes);

        // Check for keyboard input (non-blocking)
        if (_kbhit()) {
            char c = _getch();
            if (c == 'r' || c == 'R') {
                printf("\nReloading %s...\n", kConfigPath.c_str());
                auto cfg = vt::loadConfig(kConfigPath);
                vt::applyConfig(cfg, scene);
                config = cfg.render;
                renderer = std::make_unique<vt::Renderer>(scene, config);
                buffer = vt::ImageBuffer(scene.width, scene.height);

                printf("Camera: pos(%.1f, %.1f, %.1f) ry=%.1f yaw=%.1f pitch=%.1f\n",
                    cfg.camera_position.x, cfg.camera_position.y, cfg.camera_position.z,
                    cfg.camera_ry, cfg.camera_yaw, cfg.camera_pitch);
                printf("IBL: map=%d rot=%.1f intensity=%.1f | Emit scale: %.1f\n",
                    cfg.ibl_map_index, cfg.ibl_rotation, cfg.ibl_intensity,
                    cfg.area_light_emit_scale);

                pass = 0;
                continue;
            }
            else if (c == 'w' || c == 'W') {
                printf("\nWriting intermediate output at pass %d\n", pass);
                vt::writeHdrImage(output_dir + "/output_" + std::to_string(pass) + ".hdr", buffer);
                vt::writePngImage(output_dir + "/output_" + std::to_string(pass) + ".png", buffer);
            }
        }

        renderer->traceImage(buffer, pass);
        buffer.incrementPass();

        // Noise estimation
        if (pass % 10 == 0) {
            float noise = buffer.noiseEstimate();
            fprintf(stderr, "  noise = %.6f\n", noise);
            if (noise < config.noise_threshold) {
                fprintf(stderr, "  Converged at pass %d\n", pass);
                break;
            }
        }

        // Periodic output
        if (pass % config.write_interval == 0) {
            std::string hdr_path = output_dir + "/output_" + std::to_string(pass) + ".hdr";
            vt::writeHdrImage(hdr_path, buffer);
        }
    }

    // Final output
    std::string final_hdr = output_dir + "/output_final.hdr";
    std::string final_png = output_dir + "/output_final.png";
    vt::writeHdrImage(final_hdr, buffer);
    vt::writePngImage(final_png, buffer);
    printf("Render complete: %d passes\n", buffer.passCount());
}

static void renderMovie(vt::Scene& scene, const vt::RenderConfig& config,
                         const std::string& path_file, const std::string& output_dir,
                         int fps, int max_passes) {
    vt::CameraPath cam_path;
    if (!cam_path.load(path_file)) {
        fprintf(stderr, "Failed to load camera path: %s\n", path_file.c_str());
        return;
    }

    fs::create_directories(output_dir);

    int total_frames = cam_path.totalFrames(fps);
    printf("Movie render: %d frames at %d fps\n", total_frames, fps);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int frame = 0; frame < total_frames; frame++) {
        auto fp = cam_path.evaluate(frame, fps);

        printf("\n=== Frame %d/%d (t=%.2fs) ===\n", frame + 1, total_frames, fp.time);

        glm::quat q = glm::angleAxis(fp.yaw * vt::RADIANS, glm::vec3(0, 1, 0))
                     * glm::angleAxis(fp.pitch * vt::RADIANS, glm::vec3(1, 0, 0));
        scene.camera = std::make_unique<vt::Camera>(
            glm::vec3(fp.x, fp.y, fp.z), q, 1.0f);

        if (scene.m_activeEnvMap) {
            scene.m_activeEnvMap->rotation_offset = fp.ibl_rot * vt::PI / 180.0f;
        }
        scene.m_iblIntensity = fp.ibl_int;
        scene.m_areaLightEmitScale = fp.emit_scale;

        vt::ImageBuffer buffer(scene.width, scene.height);
        vt::Renderer renderer(scene, config);

        for (int pass = 1; pass <= max_passes; pass++) {
            renderer.traceImage(buffer, pass);
            buffer.incrementPass();

            if (pass % 25 == 0) {
                float noise = buffer.noiseEstimate();
                if (noise < config.noise_threshold) break;
            }
        }

        char filename[256];
        snprintf(filename, sizeof(filename), "%s/frame_%04d.hdr", output_dir.c_str(), frame);
        vt::writeHdrImage(std::string(filename), buffer);

        snprintf(filename, sizeof(filename), "%s/frame_%04d.png", output_dir.c_str(), frame);
        vt::writePngImage(std::string(filename), buffer);

        auto now = std::chrono::high_resolution_clock::now();
        float elapsed = std::chrono::duration<float>(now - start_time).count();
        float remaining = (elapsed / (frame + 1)) * (total_frames - frame - 1);
        printf("  Elapsed: %.1f min | Remaining: ~%.1f min\n",
               elapsed / 60.0f, remaining / 60.0f);
    }

    printf("\nMovie render complete: %d frames\n", total_frames);
}

int main(int argc, char** argv) {
    try {
        std::string config_file = (argc > 1) ? argv[1] : kConfigPath;

        printf("Loading config: %s\n", config_file.c_str());
        auto cfg = vt::loadConfig(config_file);

        vt::Scene scene;
        vt::buildScene(cfg, scene);
        vt::applyConfig(cfg, scene);

        printf("Scene loaded: %zu shapes, %zu materials, %zu textures\n",
            scene.shapes.size(), scene.materials.size(), scene.textures.size());

        printf("Building BVH...\n");
        scene.build();
        printf("BVH built. Starting render.\n");

        renderStill(scene, cfg.render, "outputs");

    } catch (const std::exception& e) {
        fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }

    return 0;
}
