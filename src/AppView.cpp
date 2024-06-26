// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#include "AppView.h"

#include <osg/ViewportIndexed>

using namespace osgXR;

AppView::AppView(XRState *state,
                 osgViewer::GraphicsWindow *window,
                 osgViewer::View *osgView) :
    View(window, osgView),
    _valid(false),
    _state(state),
    _mvrWidth(1024),
    _mvrHeight(768),
    _mvrViews(1),
    _mvrViewIdStr{"0", "0", "0"},
    _mvrCells(1),
    _mvrLayers(1),
    _mvrAttachmentFace(0)
{
}

void AppView::init()
{
    _state->initAppView(this);
    _valid = true;
}

AppView::~AppView()
{
    destroy();
}

void AppView::destroy()
{
    if (_valid)
        _state->destroyAppView(this);
    _valid = false;
}

void AppView::setCamFlags(osg::Camera* cam, View::Flags flags)
{
    auto it = _camFlags.find(cam);
    if (it == _camFlags.end())
        _camFlags.emplace(cam, flags);
    else
        it->second = (View::Flags)(it->second | flags);
}

View::Flags AppView::getCamFlagsAndDrop(osg::Camera* cam)
{
    auto it = _camFlags.find(cam);
    if (it == _camFlags.end())
        return View::CAM_NO_BITS;
    View::Flags ret = it->second;
    _camFlags.erase(it);
    return ret;
}

void AppView::setupIndexedViewports(osg::StateSet *stateSet,
                                    const std::vector<uint32_t> &viewIndices,
                                    uint32_t width, uint32_t height,
                                    View::Flags flags)
{
    for (uint32_t i = 0; i < viewIndices.size(); ++i)
    {
        XRState::XRView *xrView = _state->getView(viewIndices[i]);
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t w = width;
        uint32_t h = height;
        if (flags & View::CAM_MVR_FIXED_WIDTH_BIT) {
            if (_state->getSwapchainMode() == Settings::SwapchainMode::SWAPCHAIN_SINGLE) {
                x = i * width / viewIndices.size();
                w = width / viewIndices.size();
            }
        } else {
            uint32_t swapchainWidth = xrView->getSwapchain()->getWidth();
            x = xrView->getSubImage().getX() * width / swapchainWidth;
            w = xrView->getSubImage().getWidth() * width / swapchainWidth;
        }
        if (flags & View::CAM_MVR_FIXED_HEIGHT_BIT) {
            if (_state->getSwapchainMode() == Settings::SwapchainMode::SWAPCHAIN_SINGLE) {
                y = 0;
                h = height;
            }
        } else {
            uint32_t swapchainHeight = xrView->getSwapchain()->getHeight();
            y = xrView->getSubImage().getY() * height / swapchainHeight;
            h = xrView->getSubImage().getHeight() * height / swapchainHeight;
        }
        stateSet->setAttribute(new osg::ViewportIndexed(i, x, y, w, h));
    }
}
