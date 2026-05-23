cmake_minimum_required(VERSION 3.28)
project(game_tests VERSION 1.0.0 LANGUAGES CXX)

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

include(FetchContent)
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(Catch2)

include(FetchContent)
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(nlohmann_json)

enable_testing()

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/ai/GameStateObserver.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/combat/SwordWeapon.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/combat/ShotgunWeapon.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/entities/Player.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/entities/Projectile.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/entities/Enemy.cpp"
)

add_executable(${PROJECT_NAME} ${TEST_SOURCES} ${GAME_SOURCES})

target_include_directories(${PROJECT_NAME}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        SYSTEM
        ${nlohmann_json_SOURCE_DIR}/single_include
)

target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2WithMain SFML::Graphics SFML::System project_options)

include(CTest)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})