// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_COMPOSITOR
#define OSGXR_OPENXR_COMPOSITOR 1

#include "Session.h"
#include "SwapchainGroup.h"

#include <osg/Referenced>
#include <osg/ref_ptr>

namespace osgXR {

namespace OpenXR {

class DepthInfo;

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

        virtual const XrCompositionLayerBaseHeader *getXr() = 0;

    protected:

        XrCompositionLayerFlags _layerFlags;
        XrSpace                 _space;
};

class CompositionLayerProjection : public CompositionLayer
{
    public:

        CompositionLayerProjection(unsigned int viewCount)
        {
            _layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
            _layer.next = nullptr;
            _projViews.resize(viewCount);
            _depthInfos.resize(viewCount);
        }

        virtual ~CompositionLayerProjection()
        {
        }

        void addView(osg::ref_ptr<Session::Frame> frame, uint32_t viewIndex,
                     const SwapchainGroup::SubImage &subImage,
                     const DepthInfo *depthInfo = nullptr);

        virtual const XrCompositionLayerBaseHeader *getXr();

    protected:

        mutable XrCompositionLayerProjection _layer;
        std::vector<XrCompositionLayerProjectionView> _projViews;
        std::vector<XrCompositionLayerDepthInfoKHR> _depthInfos;
};

} // osgXR::OpenXR

} // osgXR

#endif
