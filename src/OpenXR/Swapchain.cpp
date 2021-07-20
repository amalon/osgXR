// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <openxr/openxr.h>

#include "Swapchain.h"

using namespace osgXR;
using namespace OpenXR;

Swapchain::Swapchain(osg::ref_ptr<Session> session,
                     const System::ViewConfiguration::View &view,
                     XrSwapchainUsageFlags usageFlags,
                     int64_t format) :
    _session(session),
    _swapchain(XR_NULL_HANDLE),
    _width(view.getRecommendedWidth()),
    _height(view.getRecommendedHeight()),
    _samples(view.getRecommendedSamples()),
    _readImageTextures(false)
{
    XrSwapchainCreateInfo createInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
    createInfo.usageFlags = usageFlags;
    createInfo.format = format;
    createInfo.sampleCount = _samples;
    createInfo.width = _width;
    createInfo.height = _height;
    createInfo.faceCount = 1;
    createInfo.arraySize = 1;
    createInfo.mipCount = 1;
    check(xrCreateSwapchain(getXrSession(), &createInfo, &_swapchain),
          "Failed to create OpenXR swapchain");

    // TODO: should not be necessary, but is for SteamVR 1.16.4 (but not 1.15.x)
    _session->makeCurrent();
}

Swapchain::~Swapchain()
{
    if (valid())
    {
        check(xrDestroySwapchain(_swapchain),
              "Failed to destroy OpenXR swapchain");
    }
}

const Swapchain::ImageTextures &Swapchain::getImageTextures() const
{
    if (!_readImageTextures) {
        // Enumerate the images
        uint32_t imageCount;
        if (check(xrEnumerateSwapchainImages(_swapchain, 0, &imageCount, nullptr),
                  "Failed to count OpenXR swapchain images"))
        {
            if (imageCount)
            {
                std::vector<XrSwapchainImageOpenGLKHR> images(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
                if (check(xrEnumerateSwapchainImages(_swapchain, images.size(), &imageCount,
                                                     (XrSwapchainImageBaseHeader *)images.data()),
                          "Failed to enumerate OpenXR swapchain images"))
                {
                    for (auto image: images)
                        _imageTextures.push_back(image.image);
                }
            }
        }

        _readImageTextures = true;
    }

    return _imageTextures;
}

int Swapchain::acquireImage() const
{
    // Acquire a swapchain image
    uint32_t imageIndex;
    if (check(xrAcquireSwapchainImage(_swapchain, nullptr, &imageIndex),
              "Failed to acquire swapchain image"))
    {
        // TODO: should not be necessary, but is for SteamVR 1.16.4 (but not 1.15.x)
        _session->makeCurrent();

        return imageIndex;
    }

    return -1;
}

bool Swapchain::waitImage(XrDuration timeoutNs) const
{
    // Wait on the swapchain image
    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    waitInfo.timeout = timeoutNs; // 100ms
    bool ret = check(xrWaitSwapchainImage(_swapchain, &waitInfo),
                     "Failed to wait for swapchain image");

    // TODO: should not be necessary, but is for SteamVR 1.16.4 (but not 1.15.x)
    _session->makeCurrent();

    return ret;
}

void Swapchain::releaseImage() const
{
    // Release the swapchain image
    check(xrReleaseSwapchainImage(_swapchain, nullptr),
          "Failed to release OpenXR swapchain image");

    // TODO: should not be necessary, but is for SteamVR 1.16.4 (but not 1.15.x)
    _session->makeCurrent();
}
