// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#include "AppViewSlaveCams.h"

#include "XRStateCallbacks.h"
#include "projection.h"

#include <osg/MatrixTransform>

using namespace osgXR;

class AppViewSlaveCams::UpdateSlaveCallback : public osg::View::Slave::UpdateSlaveCallback
{
    public:

        UpdateSlaveCallback(AppViewSlaveCams *appView,
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

        osg::observer_ptr<AppViewSlaveCams> _appView;
        osg::observer_ptr<osg::MatrixTransform> _visMaskTransform;
};

AppViewSlaveCams::AppViewSlaveCams(XRState *state,
                                   uint32_t viewIndex,
                                   osgViewer::GraphicsWindow *window,
                                   osgViewer::View *osgView) :
    AppView(state, window, osgView),
    _viewIndex(viewIndex)
{
    // Record how big MVR intermediate buffers should be
    XRState::XRView *xrView = _state->getView(_viewIndex);
    setMVRSize(xrView->getSubImage().getWidth(),
               xrView->getSubImage().getHeight());

    // Record how per-view data should be indexed (not at all)
    setMVRViews(1, "", "0", "0", "0");
}

void AppViewSlaveCams::addSlave(osg::Camera *slaveCamera)
{
    setupCamera(slaveCamera);

    XRState::XRView *xrView = _state->getView(_viewIndex);
    xrView->getSwapchain()->incNumDrawPasses();

    osg::ref_ptr<osg::MatrixTransform> visMaskTransform;
    // Set up visibility mask for this slave camera
    // We'll keep track of the transform in the slave callback so it can be
    // positioned at the appropriate range
    if (_state->needsVisibilityMask(slaveCamera))
        _state->setupVisibilityMask(slaveCamera, _viewIndex, visMaskTransform);

    osg::View::Slave *slave = _osgView->findSlaveForCamera(slaveCamera);
    slave->_updateSlaveCallback = new UpdateSlaveCallback(this, visMaskTransform.get());
}

void AppViewSlaveCams::removeSlave(osg::Camera *slaveCamera)
{
    XRState::XRView *xrView = _state->getView(_viewIndex);
    xrView->getSwapchain()->decNumDrawPasses();
}

void AppViewSlaveCams::setupCamera(osg::Camera *camera)
{
    camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    // FIXME necessary I expect...
    //camera->setRenderOrder(osg::Camera::PRE_RENDER, eye);
    //camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
    camera->setAllowEventFocus(false);
    camera->setReferenceFrame(osg::Camera::RELATIVE_RF);
    //camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
    //camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF_INHERIT_VIEWPOINT);

    XRState::XRView *xrView = _state->getView(_viewIndex);
    auto *swapchain = xrView->getSwapchain().get();
    auto &subImage = xrView->getSubImage();
    camera->setViewport(subImage.getX(),
                        subImage.getY(),
                        subImage.getWidth(),
                        subImage.getHeight());

    // Here we avoid doing anything regarding OSG camera RTT attachment.
    // Ideally we would use automatic methods within OSG for handling RTT but in
    // this case it seemed simpler to handle FBO creation and selection within
    // this class.

    // This initial draw callback is used to disable normal OSG camera setup which
    // would undo our RTT FBO configuration.
    camera->setInitialDrawCallback(new InitialDrawCallback(_state));

    camera->setPreDrawCallback(new PreDrawCallback(swapchain));
    camera->setFinalDrawCallback(new PostDrawCallback(swapchain));
}

void AppViewSlaveCams::updateSlave(osg::View &view, osg::View::Slave &slave)
{
    bool setProjection = false;
    osg::Matrix projectionMatrix;

    osg::ref_ptr<OpenXR::Session::Frame> frame = _state->getFrame(view.getFrameStamp());
    if (frame.valid())
    {
        if (frame->isPositionValid() && frame->isOrientationValid())
        {
            const auto &pose = frame->getViewPose(_viewIndex);
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
            // Used by updateSlaveImplementation() to update view matrix
            slave._viewOffset = viewOffsetInv;

            double left, right, bottom, top, zNear, zFar;
            if (view.getCamera()->getProjectionMatrixAsFrustum(left, right,
                                                               bottom, top,
                                                               zNear, zFar))
            {
                const auto &fov = frame->getViewFov(_viewIndex);
                createProjectionFov(projectionMatrix, fov, zNear, zFar);
                setProjection = true;

                View::Callback *cb = getCallback();
                if (cb)
                {
                    XRState::AppSubView subview(_state->getView(_viewIndex), viewOffsetInv, projectionMatrix);
                    cb->updateSubView(this, 0, subview);
                }
            }
        }
    }

    //slave._camera->setViewMatrix(view.getCamera()->getViewMatrix() * slave._viewOffset);
    slave.updateSlaveImplementation(view);
    if (setProjection)
    {
        slave._camera->setProjectionMatrix(projectionMatrix);
    }
}
