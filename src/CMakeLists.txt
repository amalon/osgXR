find_package(OpenGL REQUIRED)
find_package(OpenSceneGraph REQUIRED osgViewer)
find_package(OpenXR REQUIRED)

set(osgXR_SRCS
    OpenXR/Compositor.cpp
    OpenXR/Instance.cpp
    OpenXR/Session.cpp
    OpenXR/Swapchain.cpp
    OpenXR/System.cpp
    OpenXRDisplay.cpp
)

add_library(osgXR
    ${osgXR_SRCS}
)

target_include_directories(osgXR
    PRIVATE
        ../include/
        ${OPENGL_INCLUDE_DIR}
        ${OpenXR_INCLUDE_DIR}
    PUBLIC
        ${OPENSCENEGRAPH_INCLUDE_DIRS}
)
target_link_libraries(osgXR
    PRIVATE
        ${OPENGL_LIBRARIES}
    PUBLIC
        ${OPENSCENEGRAPH_LIBRARIES}
        OpenXR::openxr_loader
)
