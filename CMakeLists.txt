cmake_minimum_required(VERSION 2.6)
project ( SpringMapEditor )
SUBDIRS( ftgl )
IF (MINGW)
	if (NOT COMPLETE_BUILD)
		SET(MINGWLIBS ${CMAKE_SOURCE_DIR}/mingwlibs CACHE PATH "Location of the mingwlibs (you can't build windows executables without them)")
		
		### add mingwlibs to link- and include-directories
		INCLUDE_DIRECTORIES(${MINGWLIBS}/include ${MINGWLIBS}/include/SDL ${MINGWLIBS}/include/gtk-2.0 ${MINGWLIBS}/include/glib-2.0 
		${MINGWLIBS}/include/atk-1.0 ${MINGWLIBS}/lib/glib-2.0/include ${MINGWLIBS}/include/pango-1.0 ${MINGWLIBS}/lib/gtk-2.0/include ${MINGWLIBS}/include/cairo ./ftgl)
		LINK_DIRECTORIES(${MINGWLIBS}/lib ${MINGWLIBS}/dll ${MINGWLIBS}/bin)
		#LIST(APPEND winlibs glu32 ilu devil )
	endif (NOT COMPLETE_BUILD)
ELSE (MINGW)
FIND_PACKAGE(Freetype REQUIRED)
ENDIF (MINGW)
INCLUDE_DIRECTORIES(. Map /usr/include/freetype2)
LINK_DIRECTORIES ( . ./ftgl )
ADD_EXECUTABLE ( editor 3doobject.cpp  engine.cxx  font.cxx  light.cxx  main.cc  matset.cxx  shader.cxx  texture.cxx  wavefront.cxx Map/MetalMap.cpp Map/ReadMap.cpp
Map/SMF/BFGroundTextures.cpp glFont.h glFont.cpp
Map/SMF/SmfReadMap.cpp fbo.cc float3.cpp nv_dds.cpp 2dstuff.cpp)
IF (MINGW)
TARGET_LINK_LIBRARIES ( editor ftglS SDL glew32 glu32 opengl32 freetype)
ELSE (MINGW)
TARGET_LINK_LIBRARIES ( editor ftglS SDL GL GLU GLEW freetype)
ENDIF (MINGW)
