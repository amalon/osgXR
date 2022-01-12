cmake_minimum_required(VERSION 3.11)
project(osgXR::examples)

find_package(OpenGL REQUIRED)
find_package(OpenSceneGraph REQUIRED COMPONENTS osgDB osgViewer)

if(CMAKE_PROJECT_NAME STREQUAL osgXR)
    # If we're building from osgXR source tree, take a shortcut
    set(osgXR_INCLUDE_DIR "../include")
    set(osgXR_LIBRARY osgXR)
else()
    # Otherwise, we'd normally use osgXR::osgXR, but osgXR_LIBRARY will do here
    find_package(osgXR REQUIRED)
endif()

add_executable(osgteapot osgteapot.cpp)

target_include_directories(osgteapot
    PRIVATE
        ${OPENGL_INCLUDE_DIR}
        ${OPENSCENEGRAPH_INCLUDE_DIRS}
)

target_link_libraries(osgteapot
    PUBLIC
        ${OPENGL_LIBRARIES}
        ${OPENSCENEGRAPH_LIBRARIES}
        ${osgXR_LIBRARY}
)
