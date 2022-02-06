// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#ifndef OSGXR_SWAPCHAIN
#define OSGXR_SWAPCHAIN 1

#include <osgXR/SubImage>
#include <osgXR/Swapchain>

#include "OpenXR/Session.h"
#include "OpenXR/SwapchainGroupSubImage.h"

#include "XRState.h"

#include <osg/Camera>
#include <osg/observer_ptr>

namespace osgXR {

class XRState;

class Swapchain::Private
{
    public:

        static Private *get(Swapchain *pub)
        {
            return pub->_private.get();
        }

        Private(uint32_t width, uint32_t height);
        virtual ~Private();

        void attachToCamera(std::shared_ptr<Private> &self,
                            osg::Camera *camera);
        void attachToMirror(std::shared_ptr<Private> &self,
                            osg::StateSet *stateSet);

        // Accessors

        void preferRGBEncoding(Encoding encoding);
        void allowRGBEncoding(Encoding encoding);
        void setRGBBits(unsigned int rgbBits);
        unsigned int getRGBBits() const;
        void setAlphaBits(unsigned int alphaBits);
        unsigned int getAlphaBits() const;

        void setWidth(uint32_t width);
        uint32_t getWidth() const;
        void setHeight(uint32_t height);
        uint32_t getHeight() const;

        void setForcedAlpha(float alpha);
        void disableForcedAlpha();
        float getForcedAlpha() const;

        // Internal API

        /// Setup swapchain with an OpenXR session
        bool setup(XRState *state, OpenXR::Session *session);

        /// Synchronise any app changes, such as resizes
        bool sync();

        /// Clean up swapchain before an OpenXR session is destroyed
        void cleanupSession();

        /// Find whether the swapchain is valid for use.
        bool valid() const;

        void initialDrawCallback(osg::RenderInfo &renderInfo);
        void preDrawCallback(osg::RenderInfo &renderInfo);
        void postDrawCallback(osg::RenderInfo &renderInfo);

        void incNumDrawPasses()
        {
            ++_numDrawPasses;
        }
        void decNumDrawPasses()
        {
            --_numDrawPasses;
        }

        OpenXR::SwapchainGroupSubImage convertSubImage(const SubImage &subImage) const;

    protected:

        // Format requirements
        uint32_t _preferredRGBEncodingMask;
        uint32_t _allowedRGBEncodingMask;
        int _rgbBits;  // for linear RGB formats, per channel
        int _alphaBits;

        // Dimention requirements
        uint32_t _width;
        uint32_t _height;

        // Forced alpha
        float _forcedAlpha;

        unsigned int _numDrawPasses;
        bool _updated;

        // State sets to update
        std::list<osg::observer_ptr<osg::StateSet>> _stateSets;

        // State
        osg::observer_ptr<XRState> _state;
        osg::observer_ptr<OpenXR::Session> _session;
        osg::ref_ptr<XRState::XRSwapchain> _swapchain;
};

} // osgXR

#endif
