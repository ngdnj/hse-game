cmake_minimum_required(VERSION 3.28)
project(game VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_23)
target_compile_options(project_options INTERFACE
        -Wall -Wextra -Wpedantic -Werror
        $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics>
)

include(FetchContent)
FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 3.0.2
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(SFML)

FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.18.1
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(nlohmann_json)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

# Copy runtime assets next to the binary so relative paths like "assets/player.png"
# work regardless of the current working directory (e.g. when launching from the
# build tree or an IDE).
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/assets
                ${CMAKE_BINARY_DIR}/assets)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        SYSTEM
        ${httplib_SOURCE_DIR}/single_include
        ${nlohmann_json_SOURCE_DIR}/single_include
)

target_link_libraries(${PROJECT_NAME} PRIVATE SFML::Graphics SFML::Window SFML::System project_options)
