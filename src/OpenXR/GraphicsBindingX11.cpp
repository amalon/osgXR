// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "GraphicsBindingX11.h"

using namespace osgXR;
using namespace OpenXR;

template <>
GraphicsBindingX11::GraphicsBindingImpl(osgViewer::GraphicsWindowX11 *window) :
    _binding{ XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR }
{
    _binding.xDisplay = window->getDisplay();
    //_binding.visualid = window->_visualInfo->visualid;
    //_binding.glxFBConfig = window->_fbConfig;
    _binding.glxDrawable = window->getWindow();
    _binding.glxContext = window->getContext();
}
