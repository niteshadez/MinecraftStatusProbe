cmake_minimum_required(VERSION 3.20)
project(MinecraftStatusProbe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
            nlohmann_json
            URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_executable(MinecraftStatusProbeV1 WIN32
        src/main.cpp
        src/target.cpp
        src/slp.cpp
        src/gui.cpp
        resource.rc
)

target_include_directories(MinecraftStatusProbeV1 PRIVATE include)
target_link_libraries(MinecraftStatusProbeV1 PRIVATE nlohmann_json::nlohmann_json)

if(WIN32)
    target_link_libraries(MinecraftStatusProbeV1 PRIVATE ws2_32 comctl32 gdi32 user32)
endif()