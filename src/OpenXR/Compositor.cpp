// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "Compositor.h"
#include "DepthInfo.h"
#include "Space.h"
#include "SwapchainGroupSubImage.h"

#include <cassert>

using namespace osgXR::OpenXR;

// CompositionLayerProjection

void CompositionLayerProjection::addView(osg::ref_ptr<Session::Frame> frame, uint32_t viewIndex,
                                         const SwapchainGroup::SubImage &subImage,
                                         const DepthInfo *depthInfo)
{
    assert(viewIndex < _projViews.size());

    XrCompositionLayerProjectionView &projView = _projViews[viewIndex];
    projView = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
    projView.pose = frame->getViewPose(viewIndex);
    projView.fov = frame->getViewFov(viewIndex);
    subImage.getXrSubImage(&projView.subImage);

    if (depthInfo && subImage.depthValid())
    {
        // depth info
        XrCompositionLayerDepthInfoKHR &xrDepthInfo = _depthInfos[viewIndex];
        xrDepthInfo = { XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR };
        subImage.getDepthXrSubImage(&xrDepthInfo.subImage);
        xrDepthInfo.minDepth = depthInfo->getMinDepth();
        xrDepthInfo.maxDepth = depthInfo->getMaxDepth();
        xrDepthInfo.nearZ    = depthInfo->getNearZ();
        xrDepthInfo.farZ     = depthInfo->getFarZ();

        // add depth info to projection view chain
        projView.next = &xrDepthInfo;
    }
}

const XrCompositionLayerBaseHeader *CompositionLayerProjection::getXr()
{
    unsigned int validDepthInfos = 0;
    for (unsigned int i = 0; i < _projViews.size(); ++i)
    {
        auto &view = _projViews[i];
        auto &depthInfo = _depthInfos[i];
        if (view.type != XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW)
        {
            // Eek, some views have been omitted!
            OSG_WARN << "Partial projection views!" << std::endl;
        }

        if (depthInfo.type == XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR)
            ++validDepthInfos;
    }

    // Sanity check that depth info is entirely missing or complete
    if (validDepthInfos > 0 && validDepthInfos < _projViews.size())
    {
        OSG_WARN << "Partial projection depth info, disabling depth information" << std::endl;
        for (auto &view: _projViews)
            view.next = nullptr;
    }

    _layer.layerFlags = _layerFlags;
    _layer.space = _space->getXrSpace();
    _layer.viewCount = _projViews.size();
    _layer.views = _projViews.data();
    return reinterpret_cast<const XrCompositionLayerBaseHeader*>(&_layer);
}

// CompositionLayerQuad

void CompositionLayerQuad::setSubImage(const SwapchainGroup::SubImage &subImage)
{
    subImage.getXrSubImage(&_layer.subImage);
}

const XrCompositionLayerBaseHeader *CompositionLayerQuad::getXr()
{
    _layer.layerFlags = _layerFlags;
    _layer.space = _space->getXrSpace();
    return reinterpret_cast<const XrCompositionLayerBaseHeader*>(&_layer);
}
