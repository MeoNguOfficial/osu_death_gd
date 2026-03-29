cmake_minimum_required(VERSION 3.25)

project(osu_death_gd)

find_package(Geode REQUIRED)

add_geode_mod(
    ${PROJECT_NAME}
    SOURCES 
        src/main.cpp
)
