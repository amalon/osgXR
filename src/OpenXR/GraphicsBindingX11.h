// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_GRAPHICS_BINDING_X11
#define OSGXR_OPENXR_GRAPHICS_BINDING_X11 1

#ifdef OSGXR_USE_X11

#include "GraphicsBinding.h"

#include <osgViewer/api/X11/GraphicsWindowX11>

#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_PLATFORM_XLIB
#include <openxr/openxr_platform.h>

namespace osgXR {

namespace OpenXR {

typedef GraphicsBindingImpl<osgViewer::GraphicsWindowX11, XrGraphicsBindingOpenGLXlibKHR> GraphicsBindingX11;

} // osgXR::OpenXR

} // osgXR

#endif // OSGXR_USE_X11

#endif
