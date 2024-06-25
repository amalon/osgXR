// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#include "AppViewSceneView.h"

#include "XRStateCallbacks.h"
#include "projection.h"

#include <osg/MatrixTransform>
#include <osgUtil/SceneView>
#include <osgViewer/Renderer>

using namespace osgXR;

class AppViewSceneView::UpdateSlaveCallback : public osg::View::Slave::UpdateSlaveCallback
{
    public:

        UpdateSlaveCallback(AppViewSceneView *appView,
                            osg::MatrixTransform *visMaskTransform) :
            _appView(appView),
            _visMaskTransform(visMaskTransform)
        {
        }

        void updateSlave(osg::View &view, osg::View::Slave &slave) override
        {
            _appView->updateSlave(view, slave);
            if (_visMaskTransform.valid())
                XRState::updateVisibilityMaskTransform(slave._camera,
                                                       _visMaskTransform.get());
        }

    protected:

        osg::observer_ptr<AppViewSceneView> _appView;
        osg::observer_ptr<osg::MatrixTransform> _visMaskTransform;
};

class AppViewSceneView::InitialDrawCallback : public osg::Camera::DrawCallback
{
    public:

        InitialDrawCallback(AppViewSceneView *appView, View::Flags flags) :
            _appView(appView),
            _flags(flags)
        {
        }

        void operator()(osg::RenderInfo &renderInfo) const override
        {
            _appView->initialDraw(renderInfo, _flags);
            _appView->_state->initialDrawCallback(renderInfo, _flags);
        }

        void releaseGLObjects(osg::State *state) const override
        {
            _appView->_state->releaseGLObjects(state);
        }

    protected:

        osg::observer_ptr<AppViewSceneView> _appView;
        View::Flags _flags;
};

class AppViewSceneView::ComputeStereoMatricesCallbackNop : public osgUtil::SceneView::ComputeStereoMatricesCallback
{
    public:

        osg::Matrixd computeLeftEyeProjection(const osg::Matrixd &projection) const override
        {
            return projection;
        }

        osg::Matrixd computeLeftEyeView(const osg::Matrixd &view) const override
        {
            return view;
        }

        osg::Matrixd computeRightEyeProjection(const osg::Matrixd &projection) const override
        {
            return projection;
        }

        osg::Matrixd computeRightEyeView(const osg::Matrixd &view) const override
        {
            return view;
        }
};

class AppViewSceneView::ComputeStereoMatricesCallback : public osgUtil::SceneView::ComputeStereoMatricesCallback
{
    public:

        ComputeStereoMatricesCallback(AppViewSceneView *appView,
                                      osgUtil::SceneView *sceneView) :
            _appView(appView),
            _sceneView(sceneView)
        {
        }

        osg::Matrixd computeLeftEyeProjection(const osg::Matrixd &projection) const override
        {
            return _appView->getEyeProjection(_sceneView->getFrameStamp(),
                                              0, projection);
        }

        osg::Matrixd computeLeftEyeView(const osg::Matrixd &view) const override
        {
            return _appView->getEyeView(_sceneView->getFrameStamp(),
                                        0, view);
        }

        osg::Matrixd computeRightEyeProjection(const osg::Matrixd &projection) const override
        {
            return _appView->getEyeProjection(_sceneView->getFrameStamp(),
                                              1, projection);
        }

        osg::Matrixd computeRightEyeView(const osg::Matrixd &view) const override
        {
            return _appView->getEyeView(_sceneView->getFrameStamp(),
                                        1, view);
        }

    protected:

        osg::observer_ptr<AppViewSceneView> _appView;
        osg::observer_ptr<osgUtil::SceneView> _sceneView;
};

AppViewSceneView::AppViewSceneView(XRState *state,
                                   uint32_t viewIndices[2],
                                   osgViewer::GraphicsWindow *window,
                                   osgViewer::View *osgView) :
    AppView(state, window, osgView),
    _viewIndices{viewIndices[0], viewIndices[1]},
    _lastUpdate(0)
{
    // Create a DisplaySettings configured for side-by-side stereo
    _stereoDisplaySettings = new osg::DisplaySettings(*osg::DisplaySettings::instance().get());
    _stereoDisplaySettings->setStereo(true);
    _stereoDisplaySettings->setStereoMode(osg::DisplaySettings::HORIZONTAL_SPLIT);
    _stereoDisplaySettings->setSplitStereoHorizontalEyeMapping(osg::DisplaySettings::LEFT_EYE_LEFT_VIEWPORT);
    _stereoDisplaySettings->setUseSceneViewForStereoHint(true);

    // Record how big MVR buffers should be
    XRState::XRView *xrView = _state->getView(_viewIndices[0]);
    auto swapchainGroup = xrView->getSubImage().getSwapchainGroup();
    setMVRSize(swapchainGroup->getWidth(),
               swapchainGroup->getHeight());
    setMVRCells(2);

    // Record how per-view data should be indexed
    setMVRViews(2,
                "uniform int osgxr_ViewIndex;",
                "osgxr_ViewIndex",
                "osgxr_ViewIndex",
                "osgxr_ViewIndex");
}

void AppViewSceneView::addSlave(osg::Camera *slaveCamera,
                                View::Flags flags)
{
    setCamFlags(slaveCamera, flags);

    setupCamera(slaveCamera, flags);
    if (flags & View::CAM_TOXR_BIT)
    {
        XRState::XRView *xrView = _state->getView(_viewIndices[0]);
        unsigned int drawPasses = 1;
        if (flags & (View::CAM_MVR_SCENE_BIT | View::CAM_MVR_SHADING_BIT))
            ++drawPasses;
        xrView->getSwapchain()->incNumDrawPasses(drawPasses);
    }

    osg::ref_ptr<osg::MatrixTransform> visMaskTransform;
    if (flags & (View::CAM_MVR_SCENE_BIT | View::CAM_MVR_SHADING_BIT))
    {
        // Set up visibility masks for this slave camera
        // We'll keep track of the transform in the slave callback so it can be
        // positioned at the appropriate range
        if (_state->needsVisibilityMask(slaveCamera))
            _state->setupSceneViewVisibilityMasks(slaveCamera, visMaskTransform);
    }

    osg::View::Slave *slave = _osgView->findSlaveForCamera(slaveCamera);
    // Calls updateSlave() and updateVisibilityMaskTransform() on update
    if (slave)
        slave->_updateSlaveCallback = new UpdateSlaveCallback(this, visMaskTransform.get());
}

void AppViewSceneView::removeSlave(osg::Camera *slaveCamera)
{
    View::Flags flags = getCamFlagsAndDrop(slaveCamera);
    if (flags & View::CAM_TOXR_BIT)
    {
        unsigned int drawPasses = 1;
        if (flags & (View::CAM_MVR_SCENE_BIT | View::CAM_MVR_SHADING_BIT))
            ++drawPasses;
        XRState::XRView *xrView = _state->getView(_viewIndices[0]);
        xrView->getSwapchain()->decNumDrawPasses(drawPasses);
    }
}

void AppViewSceneView::setupCamera(osg::Camera *camera, View::Flags flags)
{
    if (flags & View::CAM_TOXR_BIT)
    {
        XRState::XRView *xrView = _state->getView(_viewIndices[0]);

        camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        camera->setDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
        camera->setReadBuffer(GL_COLOR_ATTACHMENT0_EXT);

        // Set the viewport (seems to need redoing!)
        camera->setViewport(0, 0,
                            xrView->getSwapchain()->getWidth(),
                            xrView->getSwapchain()->getHeight());

        // Here we avoid doing anything regarding OSG camera RTT attachment.
        // Ideally we would use automatic methods within OSG for handling RTT
        // but in this case it seemed simpler to handle FBO creation and
        // selection within this class.

        camera->setPreDrawCallback(new PreDrawCallback(xrView->getSwapchain()));
        camera->setFinalDrawCallback(new PostDrawCallback(xrView->getSwapchain()));
    }

    // This initial draw callback is used to disable normal OSG camera setup which
    // would undo our RTT FBO configuration.
    camera->setInitialDrawCallback(new InitialDrawCallback(this, flags));

    osg::ref_ptr<osg::StateSet> stateSet = camera->getOrCreateStateSet();
    if (flags & (View::CAM_MVR_SCENE_BIT | View::CAM_MVR_SHADING_BIT))
    {
        // Set the stereo matrices callback on each SceneView
        osgViewer::Renderer *renderer = static_cast<osgViewer::Renderer *>(camera->getRenderer());
        for (unsigned int i = 0; i < 2; ++i)
        {
            osgUtil::SceneView *sceneView = renderer->getSceneView(i);
            osgUtil::SceneView::ComputeStereoMatricesCallback *stereoCallback;
            if (flags & View::CAM_MVR_SCENE_BIT)
                stereoCallback = new ComputeStereoMatricesCallback(this, sceneView);
            else
                stereoCallback = new ComputeStereoMatricesCallbackNop();

            sceneView->setComputeStereoMatricesCallback(stereoCallback);
        }

        camera->setDisplaySettings(_stereoDisplaySettings);

        // Set up uniforms, to be set before draw by initialDraw()
        if (!_uniformViewIndex.valid())
            _uniformViewIndex = new osg::Uniform("osgxr_ViewIndex", (int)0);
        stateSet->addUniform(_uniformViewIndex);
    }

    // Set up texture coordinate transformation macros and uniforms
    if (flags & View::CAM_MVR_SHADING_BIT)
    {
        std::string strUniforms = "uniform int osgxr_ViewIndexPriv;"
                                  "uniform vec2 osgxr_viewport_offsets[2];"
                                  "uniform vec2 osgxr_viewport_scales[2];";

        // Defines for vertex shaders
        stateSet->setDefine("OSGXR_VERT_GLOBAL", strUniforms);
        stateSet->setDefine("OSGXR_VERT_MVR_TEXCOORD(UV)",
                            "(osgxr_viewport_offsets[osgxr_ViewIndexPriv] + (UV) * osgxr_viewport_scales[osgxr_ViewIndexPriv])");
        stateSet->setDefine("OSGXR_VERT_MVB_TEXCOORD(UV)",
                            "((vec2(osgxr_ViewIndexPriv, 0) + (UV)) / vec2(2, 1))");

        // Defines for fragment shaders
        stateSet->setDefine("OSGXR_FRAG_GLOBAL", strUniforms);
        stateSet->setDefine("OSGXR_FRAG_MVR_TEXCOORD(UV)",
                            "(osgxr_viewport_offsets[osgxr_ViewIndexPriv] + (UV) * osgxr_viewport_scales[osgxr_ViewIndexPriv])");
        stateSet->setDefine("OSGXR_FRAG_MVB_TEXCOORD(UV)",
                            "((vec2(osgxr_ViewIndexPriv, 0) + (UV)) / vec2(2, 1))");

        // Set up uniforms, to be set before draw by initialDraw()
        if (!_uniformViewIndexPriv.valid())
        {
            _uniformViewIndexPriv = new osg::Uniform("osgxr_ViewIndexPriv", (int)0);
            _uniformViewportOffsets = new osg::Uniform(osg::Uniform::FLOAT_VEC2,
                                            "osgxr_viewport_offsets",
                                            _state->getViewCount());
            _uniformViewportScales = new osg::Uniform(osg::Uniform::FLOAT_VEC2,
                                            "osgxr_viewport_scales",
                                            _state->getViewCount());
            for (uint32_t i = 0; i < 2; ++i)
            {
                XRState::XRView *xrView = _state->getView(_viewIndices[i]);
                _uniformViewportOffsets->setElement(i,
                    osg::Vec2((float)xrView->getSubImage().getX() / xrView->getSwapchain()->getWidth(),
                              (float)xrView->getSubImage().getY() / xrView->getSwapchain()->getHeight()));
                _uniformViewportScales->setElement(i,
                    osg::Vec2((float)xrView->getSubImage().getWidth() / xrView->getSwapchain()->getWidth(),
                              (float)xrView->getSubImage().getHeight() / xrView->getSwapchain()->getHeight()));
            }
        }
        stateSet->addUniform(_uniformViewIndexPriv);
        stateSet->addUniform(_uniformViewportOffsets);
        stateSet->addUniform(_uniformViewportScales);
    }
}

void AppViewSceneView::updateSlave(osg::View &view,
                                   osg::View::Slave &slave)
{
    // Don't do this repeatedly for the same frame!
    unsigned int frameNumber = view.getFrameStamp()->getFrameNumber();
    if (_lastUpdate == frameNumber)
    {
        slave.updateSlaveImplementation(view);
        return;
    }
    _lastUpdate = frameNumber;

    osg::ref_ptr<OpenXR::Session::Frame> frame = _state->getFrame(view.getFrameStamp());
    if (frame.valid())
    {
        if (frame->isPositionValid() && frame->isOrientationValid())
        {
            for (int eye = 0; eye < 2; ++eye)
            {
                uint32_t viewIndex = _viewIndices[eye];
                const auto &pose = frame->getViewPose(viewIndex);
                osg::Vec3 position(pose.position.x,
                                   pose.position.y,
                                   pose.position.z);
                osg::Quat orientation(pose.orientation.x,
                                      pose.orientation.y,
                                      pose.orientation.z,
                                      pose.orientation.w);

                osg::Vec3 viewOffsetVec = position * _state->getUnitsPerMeter();

                osg::Matrix viewOffset;
                viewOffset.setTrans(viewOffset.getTrans() + viewOffsetVec);
                viewOffset.preMultRotate(orientation);
                osg::Matrix viewOffsetInv = osg::Matrix::inverse(viewOffset);

                double left, right, bottom, top, zNear, zFar;
                if (view.getCamera()->getProjectionMatrixAsFrustum(left, right,
                                                                   bottom, top,
                                                                   zNear, zFar))
                {
                    const auto &fov = frame->getViewFov(viewIndex);
                    osg::Matrix projMat;
                    createProjectionFov(projMat, fov, zNear, zFar);

                    View::Callback *cb = getCallback();
                    if (cb)
                    {
                        XRState::AppSubView subview(_state->getView(viewIndex), viewOffsetInv, projMat);
                        cb->updateSubView(this, eye, subview);
                    }
                }
            }
        }
    }

    slave.updateSlaveImplementation(view);
}

void AppViewSceneView::initialDraw(osg::RenderInfo &renderInfo,
                                   View::Flags flags)
{
    if (flags & (View::CAM_MVR_SCENE_BIT | View::CAM_MVR_SHADING_BIT))
    {
        // Determine whether this is the left or right view
        // We do this by matching renderInfo against the SceneView, then
        // matching the viewport state object.
        osg::GraphicsOperation *graphicsOperation = renderInfo.getCurrentCamera()->getRenderer();
        osgViewer::Renderer *renderer = dynamic_cast<osgViewer::Renderer*>(graphicsOperation);
        int subViewId = 0;
        for (unsigned int i = 0; i < 2; ++i)
        {
            osgUtil::SceneView *sceneView = renderer->getSceneView(i);
            if (&sceneView->getRenderInfo() != &renderInfo)
                continue;

            auto *state = sceneView->getLocalStateSet();
            auto *vp = static_cast<const osg::Viewport*>(state->getAttribute(osg::StateAttribute::VIEWPORT));

            auto *stageLeft = sceneView->getRenderStageLeft();
            auto *stageRight = sceneView->getRenderStageRight();
            if (stageLeft && vp == stageLeft->getViewport())
            {
                subViewId = 0;
                break;
            }
            else if (stageRight && vp == stageRight->getViewport())
            {
                subViewId = 1;
                break;
            }
        }

        // Update the view index uniform accordingly
        _uniformViewIndex->set(subViewId);
        if (_uniformViewIndexPriv.valid())
            _uniformViewIndexPriv->set(subViewId);
    }
}

osg::Matrixd AppViewSceneView::getEyeProjection(osg::FrameStamp *stamp, int eye,
                                                const osg::Matrixd &projection)
{
    osg::ref_ptr<OpenXR::Session::Frame> frame = _state->getFrame(stamp);
    if (frame.valid())
    {
        double left, right, bottom, top, zNear, zFar;
        if (projection.getFrustum(left, right,
                                  bottom, top,
                                  zNear, zFar))
        {
            const auto &fov = frame->getViewFov(_viewIndices[eye]);
            osg::Matrix projectionMatrix;
            createProjectionFov(projectionMatrix, fov, zNear, zFar);
            return projectionMatrix;
        }
    }
    return projection;
}

osg::Matrixd AppViewSceneView::getEyeView(osg::FrameStamp *stamp, int eye,
                                          const osg::Matrixd &view)
{
    osg::ref_ptr<OpenXR::Session::Frame> frame = _state->getFrame(stamp);
    if (frame.valid())
    {
        if (frame->isPositionValid() && frame->isOrientationValid())
        {
            const auto &pose = frame->getViewPose(_viewIndices[eye]);
            osg::Vec3 position(pose.position.x,
                               pose.position.y,
                               pose.position.z);
            osg::Quat orientation(pose.orientation.x,
                                  pose.orientation.y,
                                  pose.orientation.z,
                                  pose.orientation.w);

            osg::Matrix viewOffset;
            viewOffset.setTrans(viewOffset.getTrans() + position * _state->getUnitsPerMeter());
            viewOffset.preMultRotate(orientation);
            viewOffset = osg::Matrix::inverse(viewOffset);
            return view * viewOffset;
        }
    }
    return view;
}

