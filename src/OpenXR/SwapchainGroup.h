// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_SWAPCHAIN_GROUP
#define OSGXR_OPENXR_SWAPCHAIN_GROUP 1

#include "Swapchain.h"

#include <osg/Referenced>
#include <osg/ref_ptr>

namespace osgXR {

namespace OpenXR {

class SwapchainGroupSubImage;

/// Groups colour and depth swapchains together
class SwapchainGroup : public osg::Referenced
{
    public:

        typedef SwapchainGroupSubImage SubImage;

        // GL context must not be bound in another thread
        SwapchainGroup(osg::ref_ptr<Session> session,
                       const System::ViewConfiguration::View &view,
                       XrSwapchainUsageFlags usageFlags,
                       int64_t format,
                       XrSwapchainUsageFlags depthUsageFlags = 0,
                       int64_t depthFormat = 0);
        // GL context must not be bound in another thread
        virtual ~SwapchainGroup();

        // Error checking

        inline bool valid() const
        {
            return _swapchain->valid();
        }

        inline bool released() const
        {
            return _swapchain->released();
        }

        inline bool depthValid() const
        {
            return _depthSwapchain.valid() && _depthSwapchain->valid();
        }

        // Accessors

        inline osg::ref_ptr<Swapchain> getSwapchain() const
        {
            return _swapchain;
        }

        inline osg::ref_ptr<Swapchain> getDepthSwapchain() const
        {
            return _depthSwapchain;
        }

        inline XrSwapchain getXrSwapchain() const
        {
            return _swapchain->getXrSwapchain();
        }

        inline XrSwapchain getDepthXrSwapchain() const
        {
            if (_depthSwapchain.valid())
                return _depthSwapchain->getXrSwapchain();
            else
                return XR_NULL_HANDLE;
        }

        inline uint32_t getWidth() const
        {
            return _swapchain->getWidth();
        }

        inline uint32_t getHeight() const
        {
            return _swapchain->getHeight();
        }

        inline uint32_t getSamples() const
        {
            return _swapchain->getSamples();
        }

        // Queries

        typedef Swapchain::ImageTextures ImageTextures;
        const ImageTextures &getImageTextures() const
        {
            return _swapchain->getImageTextures();
        }
        const ImageTextures &getDepthImageTextures() const
        {
            return _depthSwapchain->getImageTextures();
        }

        // Operations

        int acquireImages() const;
        bool waitImages(XrDuration timeoutNs) const;
        void releaseImages() const;

    protected:

        osg::ref_ptr<Swapchain> _swapchain;
        osg::ref_ptr<Swapchain> _depthSwapchain;
};

}

}

#endif
