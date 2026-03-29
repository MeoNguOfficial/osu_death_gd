cmake_minimum_required(VERSION 3.25)

# Fix triệt để bằng cách trỏ thẳng vào thư mục SDK từ log của Mèo
set(Geode_DIR "D:/a/osu_death_gd/osu_death_gd/geode-sdk")
list(APPEND CMAKE_PREFIX_PATH "D:/a/osu_death_gd/osu_death_gd/geode-sdk")

project(OsuDeathMod)

find_package(Geode REQUIRED)

add_geode_mod(
    ${PROJECT_NAME}
    SOURCES 
        src/main.cpp
)
