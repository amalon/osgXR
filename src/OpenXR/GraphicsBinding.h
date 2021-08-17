// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_GRAPHICS_BINDING
#define OSGXR_OPENXR_GRAPHICS_BINDING 1

#include <osg/Referenced>
#include <osg/ref_ptr>
#include <osgViewer/GraphicsWindow>

namespace osgXR {

namespace OpenXR {

class GraphicsBinding : public osg::Referenced
{
    public:
        virtual ~GraphicsBinding() { }

        virtual void *getXrGraphicsBinding() = 0;
};

template <typename GRAPHICS_WINDOW, typename XR_BINDING>
class GraphicsBindingImpl : public GraphicsBinding
{
    public:
        typedef GRAPHICS_WINDOW GraphicsWindow;

        GraphicsBindingImpl(GraphicsWindow *window);
        virtual ~GraphicsBindingImpl() {}

        void *getXrGraphicsBinding() override
        {
            return &_binding;
        }

    protected:

        XR_BINDING _binding;
};

osg::ref_ptr<GraphicsBinding> createGraphicsBinding(osgViewer::GraphicsWindow *window);

} // osgXR::OpenXR

} // osgXR

#endif
