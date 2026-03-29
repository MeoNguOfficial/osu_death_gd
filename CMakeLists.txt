cmake_minimum_required(VERSION 3.25)
project(OsuDeathMod)

# Allow the user to provide Geode via Geode_DIR or CMAKE_PREFIX_PATH.
# If a local geode-sdk directory exists, prefer it for easier local builds.
if(NOT DEFINED Geode_DIR)
    if(EXISTS "${CMAKE_SOURCE_DIR}/geode-sdk")
        set(Geode_DIR "${CMAKE_SOURCE_DIR}/geode-sdk" CACHE PATH "Geode SDK directory")
    endif()
endif()

find_package(Geode REQUIRED)

add_geode_mod(
    ${PROJECT_NAME}
    SOURCES src/main.cpp
)
