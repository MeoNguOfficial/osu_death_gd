cmake_minimum_required(VERSION 3.25)

# Force CMake to find Geode by manually pointing to the SDK path
# On GitHub Actions, the SDK is installed to the GEODE_SDK environment variable
set(Geode_DIR "$ENV{GEODE_SDK}")
list(APPEND CMAKE_PREFIX_PATH "$ENV{GEODE_SDK}")

project(OsuDeathMod)

# This line is where it usually fails, but now Geode_DIR is explicitly set above
find_package(Geode REQUIRED)

add_geode_mod(
    ${PROJECT_NAME}
    SOURCES src/main.cpp
)
