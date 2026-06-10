# Project overlay of vcpkg's community x64-mingw-static triplet.
#
# Only difference from the upstream community triplet: VCPKG_BUILD_TYPE=release,
# which tells vcpkg to build ONLY the release variant of each dependency, not
# the debug + release pair. The debug halves are unoptimized, full-symbol copies
# of the third-party libs (libwebp, zlib, libpng, opencv deps, ...) meant for
# stepping INTO those libraries. visSele only ever links the release variants and
# you debug visSele's own code -- so the debug builds just doubled dependency
# build time (the ~30 min "Building x64-mingw-static-dbg" pass) for no benefit.
#
# This is an OVERLAY (VCPKG_OVERLAY_TRIPLETS -> this dir), so it lives in the repo
# and survives vcpkg updates instead of editing C:/vcpkg/triplets/community/.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_ENV_PASSTHROUGH PATH)

set(VCPKG_CMAKE_SYSTEM_NAME MinGW)

set(VCPKG_BUILD_TYPE release)
