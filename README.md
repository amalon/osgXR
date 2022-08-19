osgXR: Virtual Reality with OpenXR and OpenSceneGraph
=====================================================

This library is to allow Virtual Reality support to be added to
[OpenSceneGraph](http://www.openscenegraph.org/) applications, using the
[OpenXR](https://www.khronos.org/OpenXR/) standard.

Status:
 * Still in development, contributions welcome. Plenty of work to do.
 * APIs to support OpenXR VR display output in OpenSceneGraph apps.
 * APIs to support OpenXR's action based input and haptic output for controller
   interaction.
 * OpenGL graphics bindings for Linux (X11/Xlib) and Windows (note that WMR
   only supports DirectX bindings).

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
find_package(osgXR 0.5.0 REQUIRED)

target_link_libraries(target
        ..
        osgXR::osgXR
)
```

osgXR can also optionally be built as a subproject. Consider using ``git
subtree`` to import osgXR, then use something like this:
```cmake
add_subdirectory(osgXR)

target_link_libraries(target
        ..
        osgXR
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
