// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_SWAPCHAIN
#define OSGXR_OPENXR_SWAPCHAIN 1

#include "Session.h"
#include "System.h"

#include <osg/Referenced>
#include <osg/ref_ptr>

#include <cinttypes>
#include <openxr/openxr.h>

namespace osgViewer {

namespace OpenXR {

class Swapchain : public osg::Referenced
{
    public:

        Swapchain(osg::ref_ptr<Session> session,
                  const System::ViewConfiguration::View &view,
                  XrSwapchainUsageFlags usageFlags,
                  int64_t format);
        virtual ~Swapchain();

        // Error checking

        inline bool valid() const
        {
            return _swapchain != XR_NULL_HANDLE;
        }

        inline bool check(XrResult result, const char *warnMsg) const
        {
            return _session->check(result, warnMsg);
        }

        // Conversions

        inline XrSession getXrSession() const
        {
            return _session->getXrSession();
        }

        inline XrSwapchain getXrSwapchain() const
        {
            return _swapchain;
        }

        // Accessors

        inline uint32_t getWidth() const
        {
            return _width;
        }

        inline uint32_t getHeight() const
        {
            return _height;
        }

        inline uint32_t getSamples() const
        {
            return _samples;
        }

        // Queries

        typedef std::vector<GLuint> ImageTextures;
        const ImageTextures &getImageTextures() const;

        void getSubImage(uint32_t imageIndex, XrSwapchainSubImage *out) const
        {
            out->swapchain = _swapchain;
            out->imageRect.offset = { 0, 0 };
            out->imageRect.extent = { (int32_t)_width, (int32_t)_height };
            out->imageArrayIndex = imageIndex;
        }

        // Operations

        int acquireImage() const;
        bool waitImage(XrDuration timeoutNs) const;
        void releaseImage() const;

    protected:

        // Session data
        osg::ref_ptr<Session> _session;
        XrSwapchain _swapchain;
        uint32_t _width;
        uint32_t _height;
        uint32_t _samples;

        // Image OpenGL textures
        mutable bool _readImageTextures;
        mutable ImageTextures _imageTextures;
};

} // osgViewer::OpenXR

} // osgViewer

#endif
