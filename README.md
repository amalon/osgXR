osgXR: Virtual Reality with OpenXR and OpenSceneGraph
=====================================================

This library is to allow Virtual Reality support to be added to [OpenSceneGraph](http://www.openscenegraph.org/) applications, using the [OpenXR](https://www.khronos.org/OpenXR/) standard.

Status:
 * Very early development, contributions welcome.
 * Allows OpenSceneGraph demos to be run on VR.
 * Plenty of work to do.
 * Currently only X11/Xlib bindings are implemented (tested on Linux).
 * Currently no automatic mirror to normal display.

License: LGPL 2.1

Dependencies: OpenSceneGraph, OpenXR

Links:
 * Matrix room: [#osgxr:hoganfam.uk](https://matrix.to/#/#osgxr:hoganfam.uk?via=hoganfam.uk)
 * [OpenXR specifications](https://www.khronos.org/registry/OpenXR/#apispecs)


Installation
------------

Something like this:
```shell
mkdir build
cd build
cmake ..
make
make install
```


Getting Started
---------------

To import osgXR into a CMake based project, you can use the included CMake module, adding something like this to your CMakeLists.txt:
```cmake
find_package(osgXR REQUIRED)

target_include_directories(target
        ...
        ${osgXR_INCLUDE_DIR}
)

target_link_libraries(target
        ..
        osgXR::osgXR
)
```

The API is currently considered unstable. Look in the include/ directory.

If you use ``osgXR::setupViewerDefaults`` from the osgXR/osgXR header, you can enable VR using environment variables:
 * ``OSGXR=1``                  enables VR.
 * ``OSGXR_MODE=SLAVE_CAMERAS`` forces the use of separate slave cameras per view.
 * ``OSGXR_MODE=SCENE_VIEW``    forces the use of OpenSceneGraph's SceneView stereo (default).
 * ``OSGXR_SWAPCHAIN=MULTIPLE`` forces the use of separate swapchains per view.
 * ``OSGXR_SWAPCHAIN=SINGLE``   forces the use of a single swapchain containing all views.
 * ``OSGXR_UNITS_PER_METER=10`` allows the scale of the environment to be controlled.
 * ``OSGXR_VALIDATION_LAYER=1`` enables the OpenXR validation layer (off by default).
 * ``OSGXR_DEPTH_INFO=1``       enables passing of depth information to OpenXR (off by default).
 * ``OSGXR_MIRROR=NONE``        use a blank screen as the default mirror.
 * ``OSGXR_MIRROR=LEFT``        use OpenXR view 0 (left) as the default mirror.
 * ``OSGXR_MIRROR=RIGHT``       use OpenXR view 1 (right) as the default mirror.
 * ``OSGXR_MIRROR=LEFT_RIGHT``  use both left and right views side by side as the default mirror.
