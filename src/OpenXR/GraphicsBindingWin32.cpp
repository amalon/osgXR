// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "GraphicsBindingWin32.h"

using namespace osgXR::OpenXR;

template <>
GraphicsBindingWin32::GraphicsBindingImpl(osgViewer::GraphicsWindowWin32 *window) :
    _binding{ XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR }
{
    _binding.hDC = window->getHDC();
    _binding.hGLRC = window->getWGLContext();
}
