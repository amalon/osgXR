// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_COMPOSITOR
#define OSGXR_OPENXR_COMPOSITOR 1

#include "Session.h"
#include "Space.h"
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
            _layerFlags(0)
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

        inline Space *getSpace() const
        {
            return _space;
        }
        inline void setSpace(Space *space)
        {
            _space = space;
        }

        virtual const XrCompositionLayerBaseHeader *getXr() = 0;

    protected:

        XrCompositionLayerFlags _layerFlags;
        osg::ref_ptr<Space>     _space;
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

        const XrCompositionLayerBaseHeader *getXr() override;

    protected:

        mutable XrCompositionLayerProjection _layer;
        std::vector<XrCompositionLayerProjectionView> _projViews;
        std::vector<XrCompositionLayerDepthInfoKHR> _depthInfos;
};

} // osgXR::OpenXR

} // osgXR

#endif
