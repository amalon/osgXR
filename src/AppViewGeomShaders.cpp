// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#include "AppViewGeomShaders.h"

#include "XRStateCallbacks.h"
#include "projection.h"

using namespace osgXR;

class AppViewGeomShaders::UpdateSlaveCallback : public osg::View::Slave::UpdateSlaveCallback
{
    public:

        UpdateSlaveCallback(AppViewGeomShaders *appView,
                            View::Flags flags) :
            _appView(appView),
            _flags(flags)
        {
        }

        void updateSlave(osg::View &view, osg::View::Slave &slave) override
        {
            _appView->updateSlave(view, slave, _flags);
        }

    protected:

        osg::observer_ptr<AppViewGeomShaders> _appView;
        View::Flags _flags;
};

AppViewGeomShaders::AppViewGeomShaders(XRState *state,
                                       const std::vector<uint32_t>& viewIndices,
                                       osgViewer::GraphicsWindow *window,
                                       osgViewer::View *osgView) :
    AppView(state, window, osgView),
    _viewIndices(viewIndices),
    _multiView(MultiView::create(state->getSession())),
    _lastUpdate(0)
{
    // Record how big MVR buffers should be
    XRState::XRView *xrView = _state->getView(_viewIndices[0]);
    auto swapchainGroup = xrView->getSubImage().getSwapchainGroup();
    setMVRSize(swapchainGroup->getWidth(),
               swapchainGroup->getHeight());

    // Record how per-view data should be indexed
    setMVRViews(_viewIndices.size(), "",
                "0", // Undefined from vertex shader
                "gl_InvocationID\n#extension GL_ARB_gpu_shader5 : enable",
                "gl_ViewportIndex\n#extension GL_ARB_fragment_layer_viewport : enable");

    // Record how many layers to use for MVR buffers
    if (_state->getSwapchainMode() == Settings::SwapchainMode::SWAPCHAIN_LAYERED)
        setMVRLayers(_viewIndices.size(), XRFramebuffer::ARRAY_INDEX_GEOMETRY,
                     "0", // Undefined from vertex shader
                     "gl_Layer",
                     "gl_Layer\n#extension GL_ARB_fragment_layer_viewport : enable");
    else
        setMVRCells(_viewIndices.size());
}

void AppViewGeomShaders::addSlave(osg::Camera *slaveCamera,
                                  View::Flags flags)
{
    setCamFlags(slaveCamera, flags);

    setupCamera(slaveCamera, flags);
    if (flags & View::CAM_TOXR_BIT)
    {
        XRState::XRView *xrView = _state->getView(_viewIndices[0]);
        xrView->getSwapchain()->incNumDrawPasses();
    }

    if (flags & View::CAM_MVR_SCENE_BIT)
    {
        osg::View::Slave *slave = _osgView->findSlaveForCamera(slaveCamera);
        // calls updateSlave() on update
        slave->_updateSlaveCallback = new UpdateSlaveCallback(this, flags);
    }
}

void AppViewGeomShaders::removeSlave(osg::Camera *slaveCamera)
{
    View::Flags flags = getCamFlagsAndDrop(slaveCamera);
    if (flags & View::CAM_TOXR_BIT)
    {
        XRState::XRView *xrView = _state->getView(_viewIndices[0]);
        xrView->getSwapchain()->decNumDrawPasses();
    }
}

void AppViewGeomShaders::setupCamera(osg::Camera *camera, View::Flags flags)
{
    XRState::XRView *xrView = _state->getView(_viewIndices[0]);
    uint32_t width, height;
    if (flags & View::CAM_TOXR_BIT)
    {
        camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        camera->setDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
        camera->setReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
        width = xrView->getSwapchain()->getWidth();
        height = xrView->getSwapchain()->getHeight();
        camera->setViewport(0, 0, width, height);

        // Here we avoid doing anything regarding OSG camera RTT attachment.
        // Ideally we would use automatic methods within OSG for handling RTT
        // but in this case it seemed simpler to handle FBO creation and
        // selection within this class.

        camera->setPreDrawCallback(new PreDrawCallback(xrView->getSwapchain()));
        camera->setFinalDrawCallback(new PostDrawCallback(xrView->getSwapchain()));
    }
    else
    {
        width = camera->getViewport()->width();
        height = camera->getViewport()->height();
    }

    // This initial draw callback is used to disable normal OSG camera setup
    // which would undo our RTT FBO configuration, and start the frame.
    camera->setInitialDrawCallback(new InitialDrawCallback(_state, flags));

    if (flags & (View::CAM_MVR_SCENE_BIT))
        camera->setReferenceFrame(osg::Camera::RELATIVE_RF);

    if (flags & (View::CAM_MVR_SCENE_BIT | View::CAM_MVR_SHADING_BIT))
    {
        osg::ref_ptr<osg::StateSet> stateSet = camera->getOrCreateStateSet();

        // Indicates geometry shaders should be used for MVR
        stateSet->setDefine("OSGXR_GEOM");

        std::string strViews = std::to_string(_viewIndices.size());
        std::string strGeomFragUniforms, strGeomUniforms;

        if (flags & View::CAM_MVR_SCENE_BIT)
        {
            // Vertex shader definitions
            stateSet->setDefine("OSGXR_VERT_TRANSFORM(POS)", "(osg_ModelViewMatrix * (POS))");

            // Geometry shader definitions
            strGeomUniforms += "uniform mat4 osgxr_transforms[" + strViews + "];"
                               "uniform mat4 osgxr_view_matrices[" + strViews + "];"
                               "uniform mat3 osgxr_normal_matrices[" + strViews + "];";
            stateSet->setDefine("OSGXR_GEOM_TRANSFORM(POS)", "(osgxr_transforms[gl_InvocationID] * (POS))");
            stateSet->setDefine("OSGXR_GEOM_VIEW_MATRIX",    "osgxr_view_matrices[gl_InvocationID]");
            stateSet->setDefine("OSGXR_GEOM_NORMAL_MATRIX",  "osgxr_normal_matrices[gl_InvocationID]");
        }

        if (flags & View::CAM_MVR_SHADING_BIT)
        {
            strGeomFragUniforms += "uniform vec2 osgxr_viewport_offsets[" + strViews + "];"
                                   "uniform vec2 osgxr_viewport_scales[" + strViews + "];";

            // Geometry shader definitions
            stateSet->setDefine("OSGXR_GEOM_MVR_TEXCOORD(UV)",
                                "(osgxr_viewport_offsets[gl_InvocationID] + (UV) * osgxr_viewport_scales[gl_InvocationID])");
            if (_state->getSwapchainMode() == Settings::SwapchainMode::SWAPCHAIN_SINGLE)
                stateSet->setDefine("OSGXR_GEOM_MVB_TEXCOORD(UV)",
                                    "((vec2(gl_InvocationID, 0) + (UV)) / vec2(" + strViews + ", 1))");
            else
                stateSet->setDefine("OSGXR_GEOM_MVB_TEXCOORD(UV)", "UV");

            // Fragment shader definitions
            stateSet->setDefine("OSGXR_FRAG_GLOBAL", strGeomFragUniforms);
            stateSet->setDefine("OSGXR_FRAG_MVR_TEXCOORD(UV)",
                                "(osgxr_viewport_offsets[gl_ViewportIndex] + (UV) * osgxr_viewport_scales[gl_ViewportIndex])"
                                "\n#extension GL_ARB_fragment_layer_viewport : enable");
            if (_state->getSwapchainMode() == Settings::SwapchainMode::SWAPCHAIN_SINGLE)
                stateSet->setDefine("OSGXR_FRAG_MVB_TEXCOORD(UV)",
                                    "((vec2(gl_ViewportIndex, 0) + (UV)) / vec2(" + strViews + ", 1))"
                                    "\n#extension GL_ARB_fragment_layer_viewport : enable");
        }

        // Geometry shader definitions

        std::string strGeomLayout = "layout (invocations = " + strViews + ") in;";
        std::string strGeomExtensions = "#extension GL_ARB_gpu_shader5 : enable\n"
                                        "#extension GL_ARB_viewport_array : enable";
        stateSet->setDefine("OSGXR_GEOM_GLOBAL", strGeomLayout + strGeomFragUniforms + strGeomUniforms +
                                                 "\n" + strGeomExtensions);

        std::string strGeomPrepareVertex = "gl_ViewportIndex = gl_InvocationID;";
        if (_state->getSwapchainMode() == Settings::SwapchainMode::SWAPCHAIN_LAYERED)
            strGeomPrepareVertex += "gl_Layer = gl_InvocationID;";
        stateSet->setDefine("OSGXR_GEOM_PREPARE_VERTEX", "do {" + strGeomPrepareVertex + "} while (false)");

        // Set up the indexed viewports
        setupIndexedViewports(stateSet, _viewIndices, width, height, flags);

        // Set up uniforms for the geometry shader, to be set on update by
        // updateSlave().
        if (!_uniformTransforms.valid())
        {
            _uniformTransforms = new osg::Uniform(osg::Uniform::FLOAT_MAT4,
                                                  "osgxr_transforms",
                                                  _viewIndices.size());
            _uniformViewMatrices = new osg::Uniform(osg::Uniform::FLOAT_MAT4,
                                                    "osgxr_view_matrices",
                                                    _viewIndices.size());
            _uniformNormalMatrices = new osg::Uniform(osg::Uniform::FLOAT_MAT3,
                                                      "osgxr_normal_matrices",
                                                      _viewIndices.size());
            _uniformViewportOffsets = new osg::Uniform(osg::Uniform::FLOAT_VEC2,
                                                       "osgxr_viewport_offsets",
                                                       _viewIndices.size());
            _uniformViewportScales = new osg::Uniform(osg::Uniform::FLOAT_VEC2,
                                                      "osgxr_viewport_scales",
                                                      _viewIndices.size());
            for (uint32_t i = 0; i < _viewIndices.size(); ++i)
            {
                XRState::XRView *xrView = _state->getView(_viewIndices[i]);
                _uniformTransforms->setElement(i, osg::Matrix::identity());
                _uniformViewMatrices->setElement(i, osg::Matrix::identity());
                _uniformNormalMatrices->setElement(i, osg::Matrix3(1.0, 0.0, 0.0,
                                                                   0.0, 1.0, 0.0,
                                                                   0.0, 0.0, 1.0));
                _uniformViewportOffsets->setElement(i,
                    osg::Vec2((float)xrView->getSubImage().getX() / xrView->getSwapchain()->getWidth(),
                              (float)xrView->getSubImage().getY() / xrView->getSwapchain()->getHeight()));
                _uniformViewportScales->setElement(i,
                    osg::Vec2((float)xrView->getSubImage().getWidth() / xrView->getSwapchain()->getWidth(),
                              (float)xrView->getSubImage().getHeight() / xrView->getSwapchain()->getHeight()));
            }
        }
        stateSet->addUniform(_uniformTransforms);
        stateSet->addUniform(_uniformViewMatrices);
        stateSet->addUniform(_uniformNormalMatrices);
        stateSet->addUniform(_uniformViewportOffsets);
        stateSet->addUniform(_uniformViewportScales);
    }
}

void AppViewGeomShaders::updateSlave(osg::View &view,
                                     osg::View::Slave &slave,
                                     View::Flags flags)
{
    // Find if we've already handled this frame
    unsigned int frameNumber = view.getFrameStamp()->getFrameNumber();
    bool newFrame = (_lastUpdate != frameNumber);
    _lastUpdate = frameNumber;

    bool setProjection = false;
    osg::Matrix projectionMatrix;

    osg::ref_ptr<OpenXR::Session::Frame> frame = _state->getFrame(view.getFrameStamp());
    if (frame.valid())
    {
        // Analyse frame
        if (newFrame && _multiView.valid())
            _multiView->loadFrame(frame);

        if (frame->isPositionValid() && frame->isOrientationValid())
        {
            double left, right, bottom, top, zNear, zFar;
            bool validProj = view.getCamera()->getProjectionMatrixAsFrustum(
                                                    left, right,
                                                    bottom, top,
                                                    zNear, zFar);

            osg::Matrix sharedViewInv;
            MultiView::SharedView sharedView;
            if (_multiView.valid() && _multiView->getSharedView(sharedView))
            {
                osg::Vec3 position(sharedView.pose.position.x,
                                   sharedView.pose.position.y,
                                   sharedView.pose.position.z);
                osg::Quat orientation(sharedView.pose.orientation.x,
                                      sharedView.pose.orientation.y,
                                      sharedView.pose.orientation.z,
                                      sharedView.pose.orientation.w);
                float zoffset = sharedView.zoffset * _state->getUnitsPerMeter();
                osg::Vec3 sharedViewVec = position * _state->getUnitsPerMeter();
                osg::Matrix sharedViewMatrix;
                sharedViewMatrix.setTrans(sharedViewVec);
                sharedViewMatrix.preMultRotate(orientation);
                sharedViewInv = osg::Matrix::inverse(sharedViewMatrix);

                // Used by updateSlaveImplementation() to update view matrix
                if (flags & View::CAM_MVR_SCENE_BIT)
                    slave._viewOffset = sharedViewInv;

                if (validProj)
                {
                    createProjectionFov(projectionMatrix, sharedView.fov,
                                        zNear + zoffset, zFar + zoffset);
                    setProjection = true;
                }
            }
            for (uint32_t i = 0; i < _viewIndices.size(); ++i)
            {
                uint32_t viewIndex = _viewIndices[i];
                const auto &pose = frame->getViewPose(viewIndex);
                osg::Vec3 position(pose.position.x,
                                   pose.position.y,
                                   pose.position.z);
                osg::Quat orientation(pose.orientation.x,
                                      pose.orientation.y,
                                      pose.orientation.z,
                                      pose.orientation.w);

                osg::Matrix viewOffset;
                osg::Vec3 viewOffsetVec = position * _state->getUnitsPerMeter();
                viewOffset.setTrans(viewOffsetVec);
                viewOffset.preMultRotate(orientation);
                osg::Matrix masterViewOffsetInv = osg::Matrix::inverse(viewOffset);
                viewOffset.postMult(sharedViewInv);
                osg::Matrix viewOffsetInv = osg::Matrix::inverse(viewOffset);

                _uniformViewMatrices->setElement(i, viewOffsetInv);
                osg::Matrix3 normalMatrix(viewOffset(0,0), viewOffset(1, 0), viewOffset(2, 0),
                                          viewOffset(0,1), viewOffset(1, 1), viewOffset(2, 1),
                                          viewOffset(0,2), viewOffset(1, 2), viewOffset(2, 2));
                _uniformNormalMatrices->setElement(i, normalMatrix);

                if (validProj)
                {
                    const auto &fov = frame->getViewFov(viewIndex);
                    osg::Matrix projMat, offsetProjMat;
                    createProjectionFov(projMat, fov, zNear, zFar);
                    offsetProjMat = viewOffsetInv * projMat;
                    _uniformTransforms->setElement(i, offsetProjMat);

                    View::Callback *cb = getCallback();
                    if (cb)
                    {
                        XRState::XRView *xrView = _state->getView(viewIndex);
                        XRState::AppSubView subview(xrView, masterViewOffsetInv, projMat);
                        cb->updateSubView(this, i, subview);
                    }
                }
            }
        }
    }

    slave.updateSlaveImplementation(view);
    if (setProjection && (flags & View::CAM_MVR_SCENE_BIT))
        slave._camera->setProjectionMatrix(projectionMatrix);
}
