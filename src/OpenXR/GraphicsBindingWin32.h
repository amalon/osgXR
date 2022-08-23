// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_GRAPHICS_BINDING_WIN32
#define OSGXR_OPENXR_GRAPHICS_BINDING_WIN32 1

#ifdef OSGXR_USE_WIN32

#include "GraphicsBinding.h"

#include <osgViewer/api/Win32/GraphicsWindowWin32>
#include <unknwn.h>

#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_PLATFORM_WIN32
#include <openxr/openxr_platform.h>

namespace osgXR {

namespace OpenXR {

typedef GraphicsBindingImpl<osgViewer::GraphicsWindowWin32, XrGraphicsBindingOpenGLWin32KHR> GraphicsBindingWin32;

} // osgXR::OpenXR

} // osgXR

#endif // OSGXR_USE_WIN32

#endif
