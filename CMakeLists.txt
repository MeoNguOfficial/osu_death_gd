cmake_minimum_required(VERSION 3.25)

# Thêm dòng này để CMake biết tìm Geode ở đâu trong môi trường GitHub
set(GEODE_DIR $ENV{GEODE_SDK})

project(OsuDeathMod)

find_package(Geode REQUIRED)

add_geode_mod(
    ${PROJECT_NAME}
    SOURCES src/main.cpp
)
