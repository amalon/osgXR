# Top level CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(osgXR
        VERSION     0.1
        DESCRIPTION "OpenXR integration for OpenSceneGraph"
)

# Build options
option(BUILD_SHARED_LIBS "Whether to build as a shared library" ON)

# Source files in src/
add_subdirectory(src)

add_subdirectory(examples)

# Installation
include(GNUInstallDirs)
install(TARGETS osgXR
        LIBRARY       DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/osgXR)
install(FILES ${CMAKE_BINARY_DIR}/src/osgXR.pc
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)
