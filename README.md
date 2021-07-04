osgXR: Virtual Reality with OpenXR and OpenSceneGraph
=====================================================

This library is to allow Virtual Reality support to be added to [OpenSceneGraph](http://www.openscenegraph.org/) applications, using the [OpenXR](https://www.khronos.org/OpenXR/) standard.

Status:
 * Very early development, contributions welcome.
 * Allows OpenSceneGraph demos to be run on VR.
 * Has serious performance issues and plenty of work to do.
 * Currently only X11/Xlib bindings are implemented (tested on Linux).

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

The API is currently considered unstable. Look in the include/ directory.

If you use ``osgXR::setupViewerDefaults`` from the osgXR/osgXR header, you can enable VR using environment variables:
 * ``OSGXR=1``                  enables VR.
 * ``OSGXR_UNITS_PER_METER=10`` allows the scale of the environment to be controlled.
