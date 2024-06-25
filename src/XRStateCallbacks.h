// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_XRSTATE_CALLBACKS
#define OSGXR_XRSTATE_CALLBACKS 1

#include "XRState.h"

#include <osgXR/View>

#include <osg/Camera>
#include <osg/GraphicsContext>

namespace osgXR {

class InitialDrawCallback : public osg::Camera::DrawCallback
{
    public:

        InitialDrawCallback(osg::ref_ptr<XRState> xrState, View::Flags flags) :
            _xrState(xrState),
            _flags(flags)
        {
        }

        void operator()(osg::RenderInfo& renderInfo) const override
        {
            _xrState->initialDrawCallback(renderInfo, _flags);
        }

        void releaseGLObjects(osg::State* state) const override
        {
            _xrState->releaseGLObjects(state);
        }

    protected:

        osg::observer_ptr<XRState> _xrState;
        View::Flags _flags;
};

class PreDrawCallback : public osg::Camera::DrawCallback
{
    public:

        PreDrawCallback(osg::ref_ptr<XRState::XRSwapchain> xrSwapchain,
                        unsigned int arrayIndex = 0) :
            _xrSwapchain(xrSwapchain),
            _arrayIndex(arrayIndex)
        {
        }

        void operator()(osg::RenderInfo& renderInfo) const override
        {
            _xrSwapchain->preDrawCallback(renderInfo, _arrayIndex);
        }

    protected:

        osg::observer_ptr<XRState::XRSwapchain> _xrSwapchain;
        unsigned int _arrayIndex;
};

class PostDrawCallback : public osg::Camera::DrawCallback
{
    public:

        PostDrawCallback(osg::ref_ptr<XRState::XRSwapchain> xrSwapchain,
                         unsigned int arrayIndex = 0) :
            _xrSwapchain(xrSwapchain),
            _arrayIndex(arrayIndex)
        {
        }

        void operator()(osg::RenderInfo& renderInfo) const override
        {
            _xrSwapchain->postDrawCallback(renderInfo, _arrayIndex);
        }

    protected:

        osg::observer_ptr<XRState::XRSwapchain> _xrSwapchain;
        unsigned int _arrayIndex;
};

class SwapCallback : public osg::GraphicsContext::SwapCallback
{
    public:

        explicit SwapCallback(osg::ref_ptr<XRState> xrState) :
            _xrState(xrState),
            _frameIndex(0)
        {
        }

        void swapBuffersImplementation(osg::GraphicsContext* gc)
        {
            _xrState->swapBuffersImplementation(gc);
        }

        int frameIndex() const
        {
            return _frameIndex;
        }

    private:

        osg::observer_ptr<XRState> _xrState;
        int _frameIndex;
};

}

#endif
