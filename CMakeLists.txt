cmake_minimum_required(VERSION 3.25)

project(OsuDeathMod)

# Không cần set Geode_DIR ở đây nữa vì đã có ở file build.yml
find_package(Geode REQUIRED)

add_geode_mod(
    ${PROJECT_NAME}
    SOURCES src/main.cpp
)
