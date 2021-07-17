// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_XRSTATE_CALLBACKS
#define OSGXR_XRSTATE_CALLBACKS 1

#include "XRState.h"

#include <osg/Camera>
#include <osg/GraphicsContext>
#include <osg/View>

namespace osgXR {

class UpdateSlaveCallback : public osg::View::Slave::UpdateSlaveCallback
{
    public:

        UpdateSlaveCallback(uint32_t viewIndex, osg::ref_ptr<XRState> xrState) :
            _viewIndex(viewIndex),
            _xrState(xrState)
        {
        }

        virtual void updateSlave(osg::View& view, osg::View::Slave& slave)
        {
            _xrState->updateSlave(_viewIndex, view, slave);
        }

    protected:

        uint32_t _viewIndex;
        osg::observer_ptr<XRState> _xrState;
};

class InitialDrawCallback : public osg::Camera::DrawCallback
{
    public:

        InitialDrawCallback(osg::ref_ptr<XRState> xrState) :
            _xrState(xrState)
        {
        }

        virtual void operator()(osg::RenderInfo& renderInfo) const
        {
            _xrState->initialDrawCallback(renderInfo);
        }

    protected:

        osg::observer_ptr<XRState> _xrState;
};

class PreDrawCallback : public osg::Camera::DrawCallback
{
    public:

        PreDrawCallback(osg::ref_ptr<XRState::XRSwapchain> xrSwapchain) :
            _xrSwapchain(xrSwapchain)
        {
        }

        virtual void operator()(osg::RenderInfo& renderInfo) const
        {
            _xrSwapchain->preDrawCallback(renderInfo);
        }

    protected:

        osg::observer_ptr<XRState::XRSwapchain> _xrSwapchain;
};

class PostDrawCallback : public osg::Camera::DrawCallback
{
    public:

        PostDrawCallback(osg::ref_ptr<XRState::XRSwapchain> xrSwapchain) :
            _xrSwapchain(xrSwapchain)
        {
        }

        virtual void operator()(osg::RenderInfo& renderInfo) const
        {
            _xrSwapchain->postDrawCallback(renderInfo);
        }

    protected:

        osg::observer_ptr<XRState::XRSwapchain> _xrSwapchain;
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
