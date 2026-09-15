cmake_minimum_required(VERSION 3.6)

project(harbour-sailvideo)

include(GNUInstallDirs)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Core Gui Qml Quick Multimedia Network Concurrent REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SAILFISHAPP REQUIRED sailfishapp)
pkg_check_modules(SAILFISHSECRETS REQUIRED sailfishsecrets)

option(SAILVIDEO_BUNDLE_LIBSMB2 "Build the app-private bundled libsmb2 from third_party/libsmb2" ON)
option(SAILVIDEO_ALLOW_MISSING_LIBSMB2 "Allow building without bundled libsmb2 source; NAS/SMB pages will be disabled at runtime" OFF)
set(SAILVIDEO_PRIVATE_LIBDIR "${CMAKE_INSTALL_LIBDIR}/harbour-sailvideo")
set(SAILVIDEO_HAS_BUNDLED_LIBSMB2 FALSE)

if(SAILVIDEO_BUNDLE_LIBSMB2)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsmb2/CMakeLists.txt")
        message(STATUS "Building bundled app-private libsmb2 from third_party/libsmb2")
        set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
        set(ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(ENABLE_LIBKRB5 OFF CACHE BOOL "" FORCE)
        set(ENABLE_GSSAPI OFF CACHE BOOL "" FORCE)
        set(_SAILVIDEO_ORIGINAL_LIB_SUFFIX "${LIB_SUFFIX}")
        if(_SAILVIDEO_ORIGINAL_LIB_SUFFIX STREQUAL "")
            set(_SAILVIDEO_ORIGINAL_LIB_SUFFIX "${CMAKE_INSTALL_LIBDIR}")
            string(REGEX REPLACE "^lib" "" _SAILVIDEO_ORIGINAL_LIB_SUFFIX "${_SAILVIDEO_ORIGINAL_LIB_SUFFIX}")
        endif()

        # libsmb2's own install rule installs the shared target to
        # lib${LIB_SUFFIX}, so temporarily redirect LIB_SUFFIX while
        # configuring the bundled subproject. This keeps libsmb2 app-private
        # under /usr/lib*/harbour-sailvideo instead of installing it globally.
        set(LIB_SUFFIX "${_SAILVIDEO_ORIGINAL_LIB_SUFFIX}/harbour-sailvideo" CACHE STRING "" FORCE)
        set(INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}" CACHE PATH "" FORCE)
        set(INSTALL_INC_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/include" CACHE PATH "" FORCE)
        set(INSTALL_PKGCONFIG_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/pkgconfig" CACHE PATH "" FORCE)
        set(INSTALL_CMAKE_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/cmake/libsmb2" CACHE PATH "" FORCE)

        add_subdirectory(third_party/libsmb2)

        set(LIB_SUFFIX "${_SAILVIDEO_ORIGINAL_LIB_SUFFIX}" CACHE STRING "" FORCE)
        set(SAILVIDEO_HAS_BUNDLED_LIBSMB2 TRUE)
    elseif(NOT SAILVIDEO_ALLOW_MISSING_LIBSMB2)
        message(FATAL_ERROR
            "SMB/NAS support is enabled, but third_party/libsmb2/CMakeLists.txt is missing.\n"
            "Run: sh tools/import-libsmb2.sh\n"
            "Then remove the old CMake cache and rebuild.\n"
            "For a local-only developer build, configure with -DSAILVIDEO_ALLOW_MISSING_LIBSMB2=ON.")
    else()
        message(WARNING "Building without bundled libsmb2 source; NAS/SMB pages will be disabled at runtime")
    endif()
else()
    message(WARNING "SAILVIDEO_BUNDLE_LIBSMB2 is OFF; NAS/SMB pages require a system libsmb2 at runtime")
endif()

add_executable(harbour-sailvideo
    src/main.cpp
    src/application/appsettings.cpp
    src/application/appsettings.h
    src/media/playbackhistorymodel.cpp
    src/media/playbackhistorymodel.h
    src/media/localvideomodel.cpp
    src/media/localvideomodel.h
    src/media/localvideocategorymodel.cpp
    src/media/localvideocategorymodel.h
    src/media/localvideofoldermodel.cpp
    src/media/localvideofoldermodel.h
    src/sources/networksourcemodel.cpp
    src/sources/networksourcemodel.h
    src/sources/networkdiscoverymodel.cpp
    src/sources/networkdiscoverymodel.h
    src/sources/nassourcemodel.cpp
    src/sources/nassourcemodel.h
    src/smb/smbbackend.cpp
    src/smb/smbbackend.h
    src/smb/smbcredentialstore.cpp
    src/smb/smbcredentialstore.h
    src/system/systemaudiocontroller.cpp
    src/system/systemaudiocontroller.h
    src/stream/localfilestreamserver.cpp
    src/stream/localfilestreamserver.h
    src/stream/urldownloadcache.cpp
    src/stream/urldownloadcache.h
)

target_include_directories(harbour-sailvideo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SAILFISHAPP_INCLUDE_DIRS}
    ${SAILFISHSECRETS_INCLUDE_DIRS}
)

target_compile_options(harbour-sailvideo PRIVATE
    ${SAILFISHAPP_CFLAGS_OTHER}
    ${SAILFISHSECRETS_CFLAGS_OTHER}
)

target_compile_definitions(harbour-sailvideo PRIVATE
    SAILVIDEO_PRIVATE_LIB_DIR="${CMAKE_INSTALL_PREFIX}/${SAILVIDEO_PRIVATE_LIBDIR}"
)

if(SAILVIDEO_HAS_BUNDLED_LIBSMB2)
    target_compile_definitions(harbour-sailvideo PRIVATE SAILVIDEO_BUILT_WITH_BUNDLED_LIBSMB2=1)
else()
    target_compile_definitions(harbour-sailvideo PRIVATE SAILVIDEO_BUILT_WITH_BUNDLED_LIBSMB2=0)
endif()

link_directories(
    ${SAILFISHAPP_LIBRARY_DIRS}
    ${SAILFISHSECRETS_LIBRARY_DIRS}
)

target_link_libraries(harbour-sailvideo
    Qt5::Core
    Qt5::Gui
    Qt5::Qml
    Qt5::Quick
    Qt5::Multimedia
    Qt5::Network
    Qt5::Concurrent
    ${SAILFISHAPP_LIBRARIES}
    ${SAILFISHSECRETS_LIBRARIES}
)

install(TARGETS harbour-sailvideo
    RUNTIME DESTINATION bin
)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/empty-private-libdir")
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/empty-private-libdir/"
    DESTINATION ${SAILVIDEO_PRIVATE_LIBDIR}
)

if(SAILVIDEO_HAS_BUNDLED_LIBSMB2)
    install(FILES
        third_party/libsmb2/COPYING
        third_party/libsmb2/LICENCE-LGPL-2.1.txt
        DESTINATION ${SAILVIDEO_PRIVATE_LIBDIR}/licenses
    )
endif()

install(FILES harbour-sailvideo.desktop
    DESTINATION share/applications
)

install(FILES icons/harbour-sailvideo.png
    DESTINATION share/icons/hicolor/172x172/apps
)

install(DIRECTORY qml
    DESTINATION share/harbour-sailvideo
)
