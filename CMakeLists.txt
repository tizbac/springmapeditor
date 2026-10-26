cmake_minimum_required(VERSION 3.16)
project(SpringMapEditor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(Freetype REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

add_subdirectory(ftgl)

add_executable(editor
    engine.cxx
    float3.cpp
    light.cxx
    main.cc
    shader.cxx
    Map/ReadMap.cpp
    Map/SMF/BFGroundTextures.cpp
    Map/SMF/SmfReadMap.cpp
)

target_include_directories(editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Map
)

target_link_libraries(editor PRIVATE
    ftglS
    OpenGL::GL
    OpenGL::GLU
    GLEW::GLEW
    Freetype::Freetype
    SDL3::SDL3
)

# Shaders are loaded relative to the working directory at runtime.
add_custom_command(TARGET editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/groundshader.vert
            ${CMAKE_CURRENT_SOURCE_DIR}/groundshader.frag
            $<TARGET_FILE_DIR:editor>
)
