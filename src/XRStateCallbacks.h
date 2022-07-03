// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_XRSTATE_CALLBACKS
#define OSGXR_XRSTATE_CALLBACKS 1

#include "XRState.h"

#include <osg/Camera>
#include <osg/GraphicsContext>
#include <osg/View>

#include <osgUtil/SceneView>

namespace osgXR {

class SlaveCamsUpdateSlaveCallback : public osg::View::Slave::UpdateSlaveCallback
{
    public:

        SlaveCamsUpdateSlaveCallback(uint32_t viewIndex,
                                     XRState *xrState,
                                     osg::MatrixTransform *visMaskTransform) :
            _viewIndex(viewIndex),
            _xrState(xrState),
            _visMaskTransform(visMaskTransform)
        {
        }

        void updateSlave(osg::View& view, osg::View::Slave& slave) override
        {
            _xrState->updateSlave(_viewIndex, view, slave);
            if (_visMaskTransform.valid())
                _xrState->updateVisibilityMaskTransform(slave._camera,
                                                        _visMaskTransform.get());
        }

    protected:

        uint32_t _viewIndex;
        osg::observer_ptr<XRState> _xrState;
        osg::observer_ptr<osg::MatrixTransform> _visMaskTransform;
};

class SceneViewUpdateSlaveCallback : public osg::View::Slave::UpdateSlaveCallback
{
    public:

        SceneViewUpdateSlaveCallback(osg::ref_ptr<XRState> xrState,
                                     osg::ref_ptr<osg::MatrixTransform> visMaskTransform) :
            _xrState(xrState),
            _visMaskTransform(visMaskTransform)
        {
        }

        void updateSlave(osg::View& view, osg::View::Slave& slave) override
        {
            if (_visMaskTransform.valid())
                _xrState->updateVisibilityMaskTransform(slave._camera,
                                                        _visMaskTransform.get());
        }

    protected:

        osg::observer_ptr<XRState> _xrState;
        osg::observer_ptr<osg::MatrixTransform> _visMaskTransform;
};

class ComputeStereoMatricesCallback : public osgUtil::SceneView::ComputeStereoMatricesCallback
{
    public:

        ComputeStereoMatricesCallback(XRState *xrState,
                                      osgUtil::SceneView *sceneView) :
            _xrState(xrState),
            _sceneView(sceneView)
        {
        }

        osg::Matrixd computeLeftEyeProjection(const osg::Matrixd& projection) const override
        {
            return _xrState->getEyeProjection(_sceneView->getFrameStamp(),
                                              0, projection);
        }

        osg::Matrixd computeLeftEyeView(const osg::Matrixd& view) const override
        {
            return _xrState->getEyeView(_sceneView->getFrameStamp(),
                                        0, view);
        }

        osg::Matrixd computeRightEyeProjection(const osg::Matrixd& projection) const override
        {
            return _xrState->getEyeProjection(_sceneView->getFrameStamp(),
                                              1, projection);
        }

        osg::Matrixd computeRightEyeView(const osg::Matrixd& view) const override
        {
            return _xrState->getEyeView(_sceneView->getFrameStamp(),
                                        1, view);
        }

    protected:

        osg::observer_ptr<XRState> _xrState;
        osg::observer_ptr<osgUtil::SceneView> _sceneView;
};

class InitialDrawCallback : public osg::Camera::DrawCallback
{
    public:

        InitialDrawCallback(osg::ref_ptr<XRState> xrState) :
            _xrState(xrState)
        {
        }

        void operator()(osg::RenderInfo& renderInfo) const override
        {
            _xrState->initialDrawCallback(renderInfo);
        }

        void releaseGLObjects(osg::State* state) const override
        {
            _xrState->releaseGLObjects(state);
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

        void operator()(osg::RenderInfo& renderInfo) const override
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

        void operator()(osg::RenderInfo& renderInfo) const override
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
