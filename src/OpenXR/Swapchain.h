// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_SWAPCHAIN
#define OSGXR_OPENXR_SWAPCHAIN 1

#include "Session.h"
#include "System.h"

#include <osg/Referenced>
#include <osg/Texture2D>
#include <osg/ref_ptr>

#include <cinttypes>
#include <openxr/openxr.h>

namespace osgXR {

namespace OpenXR {

class Swapchain : public osg::Referenced
{
    public:

        // GL context must not be bound in another thread
        Swapchain(osg::ref_ptr<Session> session,
                  const System::ViewConfiguration::View &view,
                  XrSwapchainUsageFlags usageFlags,
                  int64_t format);
        // GL context must not be bound in another thread
        virtual ~Swapchain();

        // Error checking

        inline bool valid() const
        {
            return _swapchain != XR_NULL_HANDLE;
        }

        /// Find whether the swapchain has a released image.
        inline bool released() const
        {
            return _released;
        }

        inline bool check(XrResult result, const char *actionMsg) const
        {
            return _session->check(result, actionMsg);
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

        inline int64_t getFormat() const
        {
            return _format;
        }

        // Queries

        typedef std::vector<GLuint> ImageTextures;
        // GL context must not be bound in another thread
        const ImageTextures &getImageTextures() const;

        osg::ref_ptr<osg::Texture2D> getImageOsgTexture(unsigned int index) const;

        // Operations

        // GL context must not be bound in another thread
        int acquireImage() const;
        // GL context must not be bound in another thread
        bool waitImage(XrDuration timeoutNs) const;
        // GL context must not be bound in another thread
        void releaseImage() const;

    protected:

        // Session data
        osg::ref_ptr<Session> _session;
        XrSwapchain _swapchain;
        uint32_t _width;
        uint32_t _height;
        uint32_t _samples;
        int64_t _format;

        // Image OpenGL textures
        mutable bool _readImageTextures;
        mutable ImageTextures _imageTextures;
        mutable std::vector<osg::ref_ptr<osg::Texture2D>> _imageOsgTextures;

        mutable bool _released;
};

} // osgXR::OpenXR

} // osgXR

#endif
