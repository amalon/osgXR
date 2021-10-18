// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "GraphicsBindingX11.h"

using namespace osgXR::OpenXR;

namespace {

/// Class to spy on protected members of GraphicsWindowX11.
class GraphicsWindowX11Spy : public osgViewer::GraphicsWindowX11
{
public:
    const XVisualInfo *getVisualInfo() const
    {
        return _visualInfo;
    }

    const GLXFBConfig &getFBConfig() const
    {
        return _fbConfig;
    }
};

}

template <>
GraphicsBindingX11::GraphicsBindingImpl(osgViewer::GraphicsWindowX11 *window) :
    _binding{ XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR }
{
    // window isn't actually of type GraphicsWindowX11Spy, but this allows us to
    // spy on protected members that don't have public accessors.
    auto spyWindow = static_cast<GraphicsWindowX11Spy *>(window);

    _binding.xDisplay = window->getDisplay();
    _binding.visualid = spyWindow->getVisualInfo()->visualid;
    _binding.glxFBConfig = spyWindow->getFBConfig();
    _binding.glxDrawable = window->getWindow();
    _binding.glxContext = window->getContext();
}
