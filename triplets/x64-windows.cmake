set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

# Match _ITERATOR_DEBUG_LEVEL=0 used in project debug builds so STL container
# layouts are consistent across the DLL boundary (tomlplusplus, assimp).
set(VCPKG_CXX_FLAGS_DEBUG "/D_ITERATOR_DEBUG_LEVEL=0")
set(VCPKG_C_FLAGS_DEBUG   "/D_ITERATOR_DEBUG_LEVEL=0")
