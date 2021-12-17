// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <openxr/openxr.h>

#include <cassert>

#include "Swapchain.h"

using namespace osgXR::OpenXR;

Swapchain::Swapchain(osg::ref_ptr<Session> session,
                     const System::ViewConfiguration::View &view,
                     XrSwapchainUsageFlags usageFlags,
                     int64_t format) :
    _session(session),
    _swapchain(XR_NULL_HANDLE),
    _width(view.getRecommendedWidth()),
    _height(view.getRecommendedHeight()),
    _samples(view.getRecommendedSamples()),
    _format(format),
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

    bool currentSet = _session->checkCurrent();
    // As of 2021-12-16 Monado expects the GL context to be current
    // See https://gitlab.freedesktop.org/monado/monado/-/issues/145
    if (!currentSet)
        _session->makeCurrent();

    // GL context must not be bound in another thread
    check(xrCreateSwapchain(getXrSession(), &createInfo, &_swapchain),
          "Failed to create OpenXR swapchain");

    if (currentSet)
        // SteamVR 1.16.4 (but not 1.15.x) change context then clear it
        _session->makeCurrent();
    else
        // SteamVR linux_v1.14 changes context without changing back
        // Monado doesn't change it at all (see above)
        _session->releaseContext();
}

Swapchain::~Swapchain()
{
    if (valid())
    {
        // GL context must not be bound in another thread
        check(xrDestroySwapchain(_swapchain),
              "Failed to destroy OpenXR swapchain");
    }
}

const Swapchain::ImageTextures &Swapchain::getImageTextures() const
{
    if (!_readImageTextures)
    {
        // Enumerate the images
        uint32_t imageCount;
        // GL context must not be bound in another thread
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

osg::ref_ptr<osg::Texture2D> Swapchain::getImageOsgTexture(unsigned int index) const
{
    if (_imageOsgTextures.empty())
    {
        getImageTextures();
        _imageOsgTextures.resize(_imageTextures.size());
    }

    assert(index < _imageOsgTextures.size());
    if (!_imageOsgTextures[index].valid())
    {
        // Create an OSG texture out of it
        osg::Texture2D *texture = new osg::Texture2D;
        texture->setTextureSize(getWidth(),
                                getHeight());
        texture->setInternalFormat(getFormat());
        unsigned int contextID = _session->getWindow()->getState()->getContextID();
        texture->setTextureObject(contextID, new osg::Texture::TextureObject(texture, _imageTextures[index], GL_TEXTURE_2D));

        _imageOsgTextures[index] = texture;
    }

    return _imageOsgTextures[index];
}

int Swapchain::acquireImage() const
{
    // Acquire a swapchain image
    uint32_t imageIndex;

    bool currentSet = _session->checkCurrent();
    // GL context must not be bound in another thread
    if (check(xrAcquireSwapchainImage(_swapchain, nullptr, &imageIndex),
              "Failed to acquire swapchain image"))
    {
        // TODO: should not be necessary, but is for SteamVR 1.16.4 (but not 1.15.x)
        if (currentSet)
            _session->makeCurrent();

        return imageIndex;
    }

    // TODO: should not be necessary, but is for SteamVR 1.16.4 (but not 1.15.x)
    if (currentSet)
        _session->makeCurrent();

    return -1;
}

bool Swapchain::waitImage(XrDuration timeoutNs) const
{
    // Wait on the swapchain image
    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    waitInfo.timeout = timeoutNs; // 100ms

    bool currentSet = _session->checkCurrent();
    // GL context must not be bound in another thread
    bool ret = check(xrWaitSwapchainImage(_swapchain, &waitInfo),
                     "Failed to wait for swapchain image");

    // TODO: should not be necessary, but is for SteamVR 1.16.4 (but not 1.15.x)
    if (currentSet)
        _session->makeCurrent();

    return ret;
}

void Swapchain::releaseImage() const
{
    // Release the swapchain image
    bool currentSet = _session->checkCurrent();
    // GL context must not be bound in another thread
    check(xrReleaseSwapchainImage(_swapchain, nullptr),
          "Failed to release OpenXR swapchain image");

    // TODO: should not be necessary, but is for SteamVR 1.16.4 (but not 1.15.x)
    if (currentSet)
        _session->makeCurrent();
}
