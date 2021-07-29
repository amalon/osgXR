# Dependencies
find_package(OpenGL REQUIRED)
find_package(OpenSceneGraph REQUIRED COMPONENTS osgViewer)
find_package(OpenXR REQUIRED)

# Public header files
set(osgXR_HEADERS
    include/osgXR/Manager
    include/osgXR/Mirror
    include/osgXR/MirrorSettings
    include/osgXR/OpenXRDisplay
    include/osgXR/Settings
    include/osgXR/View
    include/osgXR/osgXR
)

# Source files
set(osgXR_SRCS
    OpenXR/Compositor.cpp
    OpenXR/GraphicsBinding.cpp
    OpenXR/Instance.cpp
    OpenXR/Session.cpp
    OpenXR/Swapchain.cpp
    OpenXR/SwapchainGroup.cpp
    OpenXR/System.cpp
    XRFramebuffer.cpp
    XRState.cpp
    XRRealizeOperation.cpp
    FrameStore.cpp
    Manager.cpp
    Mirror.cpp
    MirrorSettings.cpp
    OpenXRDisplay.cpp
    Settings.cpp
    View.cpp
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
        ${OPENSCENEGRAPH_INCLUDE_DIRS}
        ${OpenXR_INCLUDE_DIR}
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
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
        INTERFACE_osgXR_MAJOR_VERSION   0
)
set_property(TARGET osgXR APPEND PROPERTY
    COMPATIBLE_INTERFACE_STRING     osgXR_MAJOR_VERSION
)
