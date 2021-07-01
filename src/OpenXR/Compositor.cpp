#include "Compositor.h"
#include "Swapchain.h"

using namespace osgViewer;
using namespace OpenXR;

void CompositionLayerProjection::addView(osg::ref_ptr<Session::Frame> frame, uint32_t viewIndex,
                                         osg::ref_ptr<Swapchain> swapchain, uint32_t imageIndex)
{
    // FIXME bit of a hack, how about some safety...
    if (_projViews.size() <= viewIndex)
        _projViews.resize(viewIndex + 1, {});

    XrCompositionLayerProjectionView projView{ XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
    projView.pose = frame->getViewPose(viewIndex);
    projView.fov = frame->getViewFov(viewIndex);
    swapchain->getSubImage(imageIndex, &projView.subImage);

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
