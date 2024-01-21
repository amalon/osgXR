// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "SwapchainGroup.h"

#include <osg/Notify>

using namespace osgXR::OpenXR;

SwapchainGroup::SwapchainGroup(osg::ref_ptr<Session> session,
                               const System::ViewConfiguration::View &view,
                               XrSwapchainUsageFlags usageFlags,
                               int64_t format,
                               XrSwapchainUsageFlags depthUsageFlags,
                               int64_t depthFormat) :
    _swapchain(new Swapchain(session, view, usageFlags, format))
{
    if (depthFormat)
        _depthSwapchain = new Swapchain(session, view, depthUsageFlags, depthFormat);
}

SwapchainGroup::~SwapchainGroup()
{
}

int SwapchainGroup::acquireImages() const
{
    int imageIndex = _swapchain->acquireImage();
    if (depthValid())
    {
        int depthImageIndex = _depthSwapchain->acquireImage();
        if (imageIndex != depthImageIndex)
            OSG_WARN << "osgXR: Depth swapchain image mismatch, expected " << imageIndex
                     << ", got " << depthImageIndex << std::endl;
    }
    return imageIndex;
}

bool SwapchainGroup::waitImages(XrDuration timeoutNs) const
{
    bool ret = _swapchain->waitImage(timeoutNs);
    if (depthValid())
    {
        bool depthRet = _depthSwapchain->waitImage(timeoutNs);
        ret = ret && depthRet;
    }
    return ret;
}

void SwapchainGroup::releaseImages() const
{
    _swapchain->releaseImage();
    if (depthValid())
        _depthSwapchain->releaseImage();
}
