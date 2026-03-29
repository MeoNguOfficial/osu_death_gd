cmake_minimum_required(VERSION 3.25)

# Force CMake to find Geode in the environment path set by the Action
if(NOT DEFINED Geode_DIR AND DEFINED ENV{GEODE_SDK})
    set(Geode_DIR "$ENV{GEODE_SDK}")
endif()

project(OsuDeathMod)

find_package(Geode REQUIRED)

add_geode_mod(
    ${PROJECT_NAME}
    SOURCES src/main.cpp
)
