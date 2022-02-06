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

class CompositionLayerQuad : public CompositionLayer
{
    public:

        CompositionLayerQuad() :
            _layer{ XR_TYPE_COMPOSITION_LAYER_QUAD }
        {
            _layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            _layer.subImage.swapchain = XR_NULL_HANDLE;
            _layer.pose.orientation = { 0.0f, 0.0f, 0.0f, 1.0f };
        }

        virtual ~CompositionLayerQuad()
        {
        }

        inline XrEyeVisibility getEyeVisibility() const
        {
            return _layer.eyeVisibility;
        }
        inline void setEyeVisibility(XrEyeVisibility eyeVisibility)
        {
            _layer.eyeVisibility = eyeVisibility;
        }

        void setSubImage(const SwapchainGroup::SubImage &subImage);

        inline osg::Quat getOrientation() const
        {
            return osg::Quat(_layer.pose.orientation.x,
                             _layer.pose.orientation.y,
                             _layer.pose.orientation.z,
                             _layer.pose.orientation.w);
        }
        inline void setOrientation(const osg::Quat &quat)
        {
            _layer.pose.orientation.x = quat.x();
            _layer.pose.orientation.y = quat.y();
            _layer.pose.orientation.z = quat.z();
            _layer.pose.orientation.w = quat.w();
        }

        inline osg::Vec3f getPosition() const
        {
            return osg::Vec3f(_layer.pose.position.x,
                              _layer.pose.position.y,
                              _layer.pose.position.z);
        }
        inline void setPosition(const osg::Vec3f &pos)
        {
            _layer.pose.position.x = pos.x();
            _layer.pose.position.y = pos.y();
            _layer.pose.position.z = pos.z();
        }

        inline osg::Vec2f getSize() const
        {
            return osg::Vec2f(_layer.size.width,
                              _layer.size.height);
        }
        inline void setSize(const osg::Vec2f &size)
        {
            _layer.size.width = size.x();
            _layer.size.height = size.y();
        }

        const XrCompositionLayerBaseHeader *getXr() override;

    protected:

        mutable XrCompositionLayerQuad _layer;
};

} // osgXR::OpenXR

} // osgXR

#endif
