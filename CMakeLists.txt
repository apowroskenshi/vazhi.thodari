cmake_minimum_required(VERSION 3.21)

# Overlay triplet forces _ITERATOR_DEBUG_LEVEL=0 in vcpkg debug builds so
# DLL-boundary container layouts match our project's debug config.
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/triplets" CACHE STRING "")

# Evaluate VCPKG_ROOT before project() so the toolchain is set up early.
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()

project(vazhi.thodari CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_SUPPRESS_REGENERATION true)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakePredefinedTargets")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# MSVC debug: swap /Od for /O2 and drop /RTC1 so debug builds run at full
# speed (BVH traversal and path tracing are unusable without optimisation).
# IDL=0 mismatch with vcpkg is handled via the overlay triplet.
# Non-MSVC (Xcode/Clang): swap -O0 for -O2.
if(MSVC)
    string(REPLACE "/Od" "/O2" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Ob0" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
else()
    string(REPLACE "-O0" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O2")
endif()

# Whole Program Optimization for Release.
include(CheckIPOSupported)
check_ipo_supported(RESULT lto_ok OUTPUT lto_err)
if(lto_ok)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
else()
    message(STATUS "LTO disabled: ${lto_err}")
endif()

# Setup libomp on Mac, since Apple's Clang ships without OpenMP runtime headers/libs.
if(APPLE)
    foreach(prefix /opt/homebrew/opt/libomp /usr/local/opt/libomp)
        if(EXISTS "${prefix}")
            set(OpenMP_ROOT "${prefix}" CACHE PATH "libomp root")
            break()
        endif()
    endforeach()
endif()

find_package(assimp CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(OpenMP)

set(SOURCES
    src/core/main.cpp
    src/core/config.cpp
    src/geometry/sphere.cpp
    src/geometry/box.cpp
    src/geometry/cylinder.cpp
    src/geometry/triangle.cpp
    src/geometry/mesh_triangle.cpp
    src/geometry/bvh_accel.cpp
    src/material/material.cpp
    src/material/texture.cpp
    src/lighting/area_light.cpp
    src/lighting/environment_map.cpp
    src/lighting/environment_map_manager.cpp
    src/renderer/renderer.cpp
    src/renderer/brdf.cpp
    src/scene/scene.cpp
    src/scene/scene_parser.cpp
    src/scene/scene_cache.cpp
    src/scene/assimp_loader.cpp
    src/camera/camera_path.cpp
    src/output/hdr_writer.cpp
    src/output/png_writer.cpp
    src/output/rgbe.cpp
    src/external/stb_image.cpp
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    include/*.h
    external/*.h
)

set(RESOURCES
    config.toml    
    camera_presets.toml
    README.md
    vcpkg.json
    vcpkg-configuration.json
)

add_executable(vazhi.thodari ${SOURCES} ${HEADERS} ${RESOURCES})

# Resources are listed for the navigator only — exclude from compilation.
# .h files are ClInclude by default in the VS generator; no override needed.
set_source_files_properties(${RESOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)

# Mirror the on-disk folder layout in the IDE navigator.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS} ${RESOURCES})

target_include_directories(vazhi.thodari PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

target_link_libraries(vazhi.thodari PRIVATE
    assimp::assimp
    glm::glm
    tomlplusplus::tomlplusplus
)

if(OpenMP_CXX_FOUND)
    target_link_libraries(vazhi.thodari PRIVATE OpenMP::OpenMP_CXX)
else()
    message(WARNING "OpenMP not found — building without parallel rendering. "
                    "On macOS: brew install libomp")
endif()

if(MSVC)
    target_compile_definitions(vazhi.thodari PRIVATE
        _CRT_SECURE_NO_WARNINGS
        $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
        $<$<CONFIG:Release>:_CONSOLE>
    )
    target_compile_options(vazhi.thodari PRIVATE
        /W4
        /permissive-
        $<$<CONFIG:Release>:/sdl>
        $<$<CONFIG:Release>:/Gy>
        $<$<CONFIG:Release>:/Oi>
    )
else()
    target_compile_options(vazhi.thodari PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Run from the project root so relative asset paths resolve.
set_target_properties(vazhi.thodari PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    XCODE_GENERATE_SCHEME ON
    XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY
    VS_STARTUP_PROJECT "vazhi.thodari")
