// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGVIEWER_OPENXR_COMPOSITOR
#define OSGVIEWER_OPENXR_COMPOSITOR 1

#include "Session.h"

#include <osg/Referenced>
#include <osg/ref_ptr>

namespace osgViewer {

namespace OpenXR {

class Swapchain;

class CompositionLayer : public osg::Referenced
{
    public:

        CompositionLayer() :
            _layerFlags(0),
            _space(XR_NULL_HANDLE)
        {
        }

        virtual ~CompositionLayer()
        {
        }

        inline XrCompositionLayerFlags getLayerFlags() const
        {
            return _layerFlags;
        }
        inline void setLayerFlags(XrCompositionLayerFlags layerFlags)
        {
            _layerFlags = layerFlags;
        }

        inline XrSpace getSpace() const
        {
            return _space;
        }
        inline void setSpace(XrSpace space)
        {
            _space = space;
        }

        virtual const XrCompositionLayerBaseHeader *getXr() const = 0;

    protected:

        XrCompositionLayerFlags _layerFlags;
        XrSpace                 _space;
};

class CompositionLayerProjection : public CompositionLayer
{
    public:

        CompositionLayerProjection()
        {
            _layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
            _layer.next = nullptr;
        }

        virtual ~CompositionLayerProjection()
        {
        }

        void addView(osg::ref_ptr<Session::Frame> frame, uint32_t viewIndex,
                     osg::ref_ptr<Swapchain> swapchain, uint32_t imageIndex);

        virtual const XrCompositionLayerBaseHeader *getXr() const;

    protected:

        mutable XrCompositionLayerProjection _layer;
        std::vector<XrCompositionLayerProjectionView> _projViews;
};

} // osgViewer::OpenXR

} // osgViewer

#endif
