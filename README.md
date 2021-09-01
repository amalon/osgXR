osgXR: Virtual Reality with OpenXR and OpenSceneGraph
=====================================================

This library is to allow Virtual Reality support to be added to
[OpenSceneGraph](http://www.openscenegraph.org/) applications, using the
[OpenXR](https://www.khronos.org/OpenXR/) standard.

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

To import osgXR into a CMake based project, you can use the included CMake
module, adding something like this to your CMakeLists.txt:
```cmake
find_package(osgXR 0.3 REQUIRED)

target_include_directories(target
        ...
        ${osgXR_INCLUDE_DIR}
)

target_link_libraries(target
        ..
        osgXR::osgXR
)
```

If you have installed osgXR outside of the system prefix (CMake's default prefix
on UNIX systems is ``/usr/local``), you may need to tell CMake where to find it
when you configure the project. You can do this by defining ``osgXR_DIR`` when
invoking cmake, e.g. with the argument ``-DosgXR_DIR=$PREFIX/lib/cmake/osgXR``
where ``$PREFIX`` is osgXR's install prefix (``CMAKE_INSTALL_PREFIX``).


The Public API
--------------

See the [API documentation](docs/API.md) for details of the API.
