# Dependencies
find_package(OpenGL REQUIRED)
find_package(OpenSceneGraph REQUIRED osgViewer)
find_package(OpenXR REQUIRED)

# Public header files
set(osgXR_HEADERS
    include/osgXR/OpenXRDisplay
    include/osgXR/osgXR
)

# Source files
set(osgXR_SRCS
    OpenXR/Compositor.cpp
    OpenXR/GraphicsBinding.cpp
    OpenXR/Instance.cpp
    OpenXR/Session.cpp
    OpenXR/Swapchain.cpp
    OpenXR/System.cpp
    XRFramebuffer.cpp
    XRState.cpp
    XRRealizeOperation.cpp
    OpenXRDisplay.cpp
    osgXR.cpp
    projection.cpp
)

# X11 graphics binding
find_package(X11)
if(X11_FOUND)
    list(APPEND osgXR_SRCS
        OpenXR/GraphicsBindingX11.cpp
    )
    add_compile_definitions(OSGXR_USE_X11)
endif()


# Build osgXR as a library
add_library(osgXR ${osgXR_SRCS})

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

set_target_properties(osgXR
    PROPERTIES
        VERSION         ${PROJECT_VERSION}
        SOVERSION       1
        PUBLIC_HEADER   "${osgXR_HEADERS}"
)

# Preprocess pkgconfig file
configure_file(osgXR.pc.in osgXR.pc @ONLY)
