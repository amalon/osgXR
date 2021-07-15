// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "Compositor.h"
#include "Swapchain.h"

using namespace osgXR;
using namespace OpenXR;

void CompositionLayerProjection::addView(osg::ref_ptr<Session::Frame> frame, uint32_t viewIndex,
                                         const Swapchain::SubImage &swapchainSubImage)
{
    // FIXME bit of a hack, how about some safety...
    if (_projViews.size() <= viewIndex)
        _projViews.resize(viewIndex + 1, {});

    XrCompositionLayerProjectionView projView{ XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
    projView.pose = frame->getViewPose(viewIndex);
    projView.fov = frame->getViewFov(viewIndex);
    swapchainSubImage.getXrSubImage(&projView.subImage);

    _projViews[viewIndex] = projView;
}

const XrCompositionLayerBaseHeader *CompositionLayerProjection::getXr() const
{
    for (const auto &view: _projViews)
    {
        if (view.type != XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW)
        {
            // Eek, some views have been omitted!
            // FIXME do something!
        }
    }
    _layer.layerFlags = _layerFlags;
    _layer.space = _space;
    _layer.viewCount = _projViews.size();
    _layer.views = _projViews.data();
    return reinterpret_cast<const XrCompositionLayerBaseHeader*>(&_layer);
}
