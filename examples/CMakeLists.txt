find_package(OpenGL REQUIRED)
find_package(OpenSceneGraph REQUIRED osgDB osgViewer)

add_executable(osgteapot osgteapot.cpp)

target_include_directories(osgteapot
    PRIVATE
        ../include/
        ${OPENSCENEGRAPH_INCLUDE_DIRS}
)

target_link_libraries(osgteapot
    PUBLIC
        ${OPENGL_LIBRARIES}
        ${OPENSCENEGRAPH_LIBRARIES}
)
