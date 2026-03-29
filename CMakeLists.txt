cmake_minimum_required(VERSION 3.25)

# 1. Tự động nhận diện Geode SDK trên GitHub Actions
if(DEFINED ENV{GEODE_SDK})
    file(TO_CMAKE_PATH "$ENV{GEODE_SDK}" GEODE_SDK_PATH)
    set(Geode_DIR "${GEODE_SDK_PATH}")
    list(APPEND CMAKE_PREFIX_PATH "${GEODE_SDK_PATH}")
endif()

# 2. Tên Project (Nên để giống ID trong mod.json)
project(osu_death_gd)

# 3. Tìm Geode
find_package(Geode REQUIRED)

# 4. Tạo Mod
add_geode_mod(
    ${PROJECT_NAME}
    SOURCES 
        src/main.cpp
)
