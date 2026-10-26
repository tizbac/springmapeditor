cmake_minimum_required(VERSION 3.16)

project(ftgl LANGUAGES CXX)

find_package(Freetype REQUIRED)

add_library(ftglS STATIC
    FTBuffer.cpp
    FTCharmap.cpp
    FTContour.cpp
    FTFace.cpp
    FTFont/FTBitmapFont.cpp
    FTFont/FTBufferFont.cpp
    FTFont/FTExtrudeFont.cpp
    FTFont/FTFont.cpp
    FTFont/FTFontGlue.cpp
    FTFont/FTOutlineFont.cpp
    FTFont/FTPixmapFont.cpp
    FTFont/FTPolygonFont.cpp
    FTFont/FTTextureFont.cpp
    FTGlyph/FTBitmapGlyph.cpp
    FTGlyph/FTBufferGlyph.cpp
    FTGlyph/FTExtrudeGlyph.cpp
    FTGlyph/FTGlyph.cpp
    FTGlyph/FTGlyphGlue.cpp
    FTGlyph/FTOutlineGlyph.cpp
    FTGlyph/FTPixmapGlyph.cpp
    FTGlyph/FTPolygonGlyph.cpp
    FTGlyph/FTTextureGlyph.cpp
    FTGlyphContainer.cpp
    FTLayout/FTLayout.cpp
    FTLayout/FTLayoutGlue.cpp
    FTLayout/FTSimpleLayout.cpp
    FTLibrary.cpp
    FTPoint.cpp
    FTSize.cpp
    FTVectoriser.cpp
)

target_include_directories(ftglS
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(ftglS PUBLIC Freetype::Freetype)

target_compile_definitions(ftglS PRIVATE FTGL_LIBRARY_STATIC)
