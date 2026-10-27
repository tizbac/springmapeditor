cmake_minimum_required(VERSION 3.16) project(SpringMapEditor LANGUAGES C CXX)

    set(CMAKE_CXX_STANDARD 17) set(CMAKE_CXX_STANDARD_REQUIRED ON) set(CMAKE_CXX_EXTENSIONS OFF)

        if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES) set(
            CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE) endif()

            find_package(OpenGL REQUIRED) find_package(GLEW REQUIRED) find_package(
                Freetype REQUIRED) find_package(SDL3 CONFIG REQUIRED)

#libarchive is used to import and export.sd7(7z) map archives.
                find_library(ARCHIVE_LIBRARY archive) if (NOT ARCHIVE_LIBRARY) message(
                    FATAL_ERROR "libarchive is required (libarchive-dev)") endif()

#Image loading for the texture browser.FreeImage is used when available;
#without it only BMP files can be read(through SDL).
                    find_library(FREEIMAGE_LIBRARY freeimage) if (
                        FREEIMAGE_LIBRARY) add_definitions(-DHAVE_FREEIMAGE) endif()

                        add_executable(editor

                                           dxt1.cxx engine.cxx float3.cpp font.cxx main.cc shader
                                               .cxx terrain.cxx texturebrowser.cxx ui.cxx Map /
                                       ReadMap.cpp Map / SMF / BFGroundTextures.cpp Map / SMF /
                                       SmfReadMap.cpp)

                            target_include_directories(editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${
                                                           CMAKE_CURRENT_SOURCE_DIR} /
                                                       Map)

                                target_link_libraries(editor PRIVATE OpenGL::GL GLEW::GLEW
                                                          Freetype::Freetype SDL3::SDL3 ${
                                                              ARCHIVE_LIBRARY})

                                    if (FREEIMAGE_LIBRARY) target_link_libraries(editor PRIVATE ${
                                        FREEIMAGE_LIBRARY}) endif()

#The shaders are loaded from the working directory at runtime.
                                        add_custom_command(
                                            TARGET editor POST_BUILD COMMAND ${CMAKE_COMMAND} -
                                                E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR} /
                                                    ground.vert ${CMAKE_CURRENT_SOURCE_DIR} /
                                                    ground.frag ${CMAKE_CURRENT_SOURCE_DIR} /
                                                    flat.vert ${CMAKE_CURRENT_SOURCE_DIR} /
                                                    flat.frag $ <
                                            TARGET_FILE_DIR : editor >)
