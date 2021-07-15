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

namespace osgXR {

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

        // Operations

        int acquireImage() const;
        bool waitImage(XrDuration timeoutNs) const;
        void releaseImage() const;

        // Sub images
        class SubImage
        {
            public:
                SubImage(Swapchain *swapchain) :
                    _swapchain(swapchain),
                    _x(0),
                    _y(0),
                    _width(swapchain->_width),
                    _height(swapchain->_height),
                    _arrayIndex(0)
                {
                }

                SubImage(Swapchain *swapchain,
                         const System::ViewConfiguration::View::Viewport &vp) :
                    _swapchain(swapchain),
                    _x(vp.x),
                    _y(vp.y),
                    _width(vp.width),
                    _height(vp.height),
                    _arrayIndex(vp.arrayIndex)
                {
                }

                bool valid() const
                {
                    return _swapchain->valid();
                }

                osg::ref_ptr<Swapchain> getSwapchain()
                {
                    return _swapchain;
                }

                uint32_t getX() const
                {
                    return _x;
                }

                uint32_t getY() const
                {
                    return _y;
                }

                uint32_t getWidth() const
                {
                    return _width;
                }

                uint32_t getHeight() const
                {
                    return _height;
                }

                uint32_t getArrayIndex() const
                {
                    return _arrayIndex;
                }

                void getXrSubImage(XrSwapchainSubImage *out) const
                {
                    out->swapchain = _swapchain->_swapchain;
                    out->imageRect.offset = { (int32_t)_x,
                                              (int32_t)_y };
                    out->imageRect.extent = { (int32_t)_width,
                                              (int32_t)_height };
                    out->imageArrayIndex = _arrayIndex;
                }

            protected:
                osg::ref_ptr<Swapchain> _swapchain;
                uint32_t _x;
                uint32_t _y;
                uint32_t _width;
                uint32_t _height;
                uint32_t _arrayIndex;
        };

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

        // Current image
        mutable int _currentImage;
};

} // osgXR::OpenXR

} // osgXR

#endif
