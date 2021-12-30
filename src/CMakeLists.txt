# Dependencies
find_package(OpenGL REQUIRED)
find_package(OpenSceneGraph REQUIRED COMPONENTS osgViewer osgUtil)
find_package(OpenXR REQUIRED)

# Public header files
set(osgXR_HEADERS
    include/osgXR/Action
    include/osgXR/ActionSet
    include/osgXR/InteractionProfile
    include/osgXR/Manager
    include/osgXR/Mirror
    include/osgXR/MirrorSettings
    include/osgXR/OpenXRDisplay
    include/osgXR/Settings
    include/osgXR/Subaction
    include/osgXR/View
    include/osgXR/osgXR
)

# Source files
set(osgXR_SRCS
    OpenXR/Action.cpp
    OpenXR/ActionSet.cpp
    OpenXR/Compositor.cpp
    OpenXR/EventHandler.cpp
    OpenXR/GraphicsBinding.cpp
    OpenXR/Instance.cpp
    OpenXR/InteractionProfile.cpp
    OpenXR/Path.cpp
    OpenXR/Session.cpp
    OpenXR/Space.cpp
    OpenXR/Swapchain.cpp
    OpenXR/SwapchainGroup.cpp
    OpenXR/System.cpp
    XRFramebuffer.cpp
    XRState.cpp
    XRRealizeOperation.cpp
    Action.cpp
    ActionSet.cpp
    FrameStore.cpp
    InteractionProfile.cpp
    Manager.cpp
    Mirror.cpp
    MirrorSettings.cpp
    OpenXRDisplay.cpp
    Settings.cpp
    Subaction.cpp
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

# Generate a "generated/Version.h" header
set(osgXR_VERSION_HEADER "${PROJECT_BINARY_DIR}/include/generated/Version.h")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Version.h.in"
               "${osgXR_VERSION_HEADER}")

target_include_directories(osgXR
    PRIVATE
        ${PROJECT_BINARY_DIR}/include
        ${PROJECT_SOURCE_DIR}/include
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
        SOVERSION       ${osgXR_SOVERSION}
        PUBLIC_HEADER   "${osgXR_HEADERS}"
        INTERFACE_osgXR_MAJOR_VERSION   ${osgXR_MAJOR_VERSION}
        INTERFACE_osgXR_MINOR_VERSION   ${osgXR_MINOR_VERSION}
)
set_property(TARGET osgXR APPEND PROPERTY
    COMPATIBLE_INTERFACE_STRING     osgXR_MAJOR_VERSION osgXR_MINOR_VERSION
)
