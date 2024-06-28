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
            if (_visMaskTransform.valid())
                XRState::updateVisibilityMaskTransform(slave._camera,
                                                       _visMaskTransform.get());
        }

    protected:

        osg::observer_ptr<AppViewSceneView> _appView;
        osg::observer_ptr<osg::MatrixTransform> _visMaskTransform;
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
                                   osgViewer::GraphicsWindow *window,
                                   osgViewer::View *osgView) :
    AppView(state, window, osgView)
{
    // Create a DisplaySettings configured for side-by-side stereo
    _stereoDisplaySettings = new osg::DisplaySettings(*osg::DisplaySettings::instance().get());
    _stereoDisplaySettings->setStereo(true);
    _stereoDisplaySettings->setStereoMode(osg::DisplaySettings::HORIZONTAL_SPLIT);
    _stereoDisplaySettings->setSplitStereoHorizontalEyeMapping(osg::DisplaySettings::LEFT_EYE_LEFT_VIEWPORT);
    _stereoDisplaySettings->setUseSceneViewForStereoHint(true);
}

void AppViewSceneView::addSlave(osg::Camera *slaveCamera)
{
    setupCamera(slaveCamera);

    XRState::XRView *xrView = _state->getView(0);
    xrView->getSwapchain()->incNumDrawPasses(2);

    osg::ref_ptr<osg::MatrixTransform> visMaskTransform;
    // Set up visibility masks for this slave camera
    // We'll keep track of the transform in the slave callback so it can be
    // positioned at the appropriate range
    if (_state->needsVisibilityMask(slaveCamera))
        _state->setupSceneViewVisibilityMasks(slaveCamera, visMaskTransform);

    if (visMaskTransform.valid())
    {
        osg::View::Slave *slave = _osgView->findSlaveForCamera(slaveCamera);
        if (slave)
            slave->_updateSlaveCallback = new UpdateSlaveCallback(this, visMaskTransform.get());
    }
}

void AppViewSceneView::removeSlave(osg::Camera *slaveCamera)
{
    XRState::XRView *xrView = _state->getView(0);
    xrView->getSwapchain()->decNumDrawPasses(2);
}

void AppViewSceneView::setupCamera(osg::Camera *camera)
{
    XRState::XRView *xrView = _state->getView(0);

    camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    camera->setDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    camera->setReadBuffer(GL_COLOR_ATTACHMENT0_EXT);

    // Here we avoid doing anything regarding OSG camera RTT attachment.
    // Ideally we would use automatic methods within OSG for handling RTT
    // but in this case it seemed simpler to handle FBO creation and
    // selection within this class.

    // This initial draw callback is used to disable normal OSG camera setup which
    // would undo our RTT FBO configuration.
    camera->setInitialDrawCallback(new InitialDrawCallback(_state));

    camera->setPreDrawCallback(new PreDrawCallback(xrView->getSwapchain()));
    camera->setFinalDrawCallback(new PostDrawCallback(xrView->getSwapchain()));

    // Set the viewport (seems to need redoing!)
    camera->setViewport(0, 0,
                        xrView->getSwapchain()->getWidth(),
                        xrView->getSwapchain()->getHeight());

    // Set the stereo matrices callback on each SceneView
    osgViewer::Renderer *renderer = static_cast<osgViewer::Renderer *>(camera->getRenderer());
    for (unsigned int i = 0; i < 2; ++i)
    {
        osgUtil::SceneView *sceneView = renderer->getSceneView(i);
        sceneView->setComputeStereoMatricesCallback(
            new ComputeStereoMatricesCallback(this, sceneView));
    }

    camera->setDisplaySettings(_stereoDisplaySettings);
}

osg::Matrixd AppViewSceneView::getEyeProjection(osg::FrameStamp *stamp,
                                                uint32_t viewIndex,
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
            const auto &fov = frame->getViewFov(viewIndex);
            osg::Matrix projectionMatrix;
            createProjectionFov(projectionMatrix, fov, zNear, zFar);
            return projectionMatrix;
        }
    }
    return projection;
}

osg::Matrixd AppViewSceneView::getEyeView(osg::FrameStamp *stamp,
                                          uint32_t viewIndex,
                                          const osg::Matrixd &view)
{
    osg::ref_ptr<OpenXR::Session::Frame> frame = _state->getFrame(stamp);
    if (frame.valid())
    {
        if (frame->isPositionValid() && frame->isOrientationValid())
        {
            const auto &pose = frame->getViewPose(viewIndex);
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

