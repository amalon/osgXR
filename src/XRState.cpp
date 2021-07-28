// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "XRState.h"
#include "XRStateCallbacks.h"
#include "projection.h"

#include <osgXR/Manager>

#include <osg/Camera>
#include <osg/DisplaySettings>
#include <osg/Notify>
#include <osg/RenderInfo>
#include <osg/View>

#include <osgUtil/SceneView>

#include <osgViewer/GraphicsWindow>
#include <osgViewer/Renderer>
#include <osgViewer/View>

using namespace osgXR;

XRState::XRState(Settings *settings, Manager *manager) :
    _settings(settings),
    _manager(manager),
    _vrMode(settings->getVRMode()),
    _swapchainMode(settings->getSwapchainMode()),
    _formFactor(XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY),
    _chosenViewConfig(nullptr),
    _chosenEnvBlendMode(XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM),
    _unitsPerMeter(settings->getUnitsPerMeter()),
    _passesPerView(1),
    _useDepthInfo(settings->getDepthInfo())
{
    // Create OpenXR instance

    // Decide on the algorithm to use
    if (_vrMode == VRMode::VRMODE_AUTOMATIC)
        _vrMode = VRMode::VRMODE_SCENE_VIEW;

    // SceneView mode requires a single swapchain
    if (_vrMode == VRMode::VRMODE_SCENE_VIEW)
    {
        if (_swapchainMode != SwapchainMode::SWAPCHAIN_AUTOMATIC &&
            _swapchainMode != SwapchainMode::SWAPCHAIN_SINGLE)
        {
            OSG_WARN << "osgXR: Overriding VR swapchain mode to SINGLE for VR mode SCENE_VIEW" << std::endl;
        }
        _swapchainMode = SwapchainMode::SWAPCHAIN_SINGLE;
    }

    // Decide on a swapchain mode to use
    if (_swapchainMode == SwapchainMode::SWAPCHAIN_AUTOMATIC)
        _swapchainMode = SwapchainMode::SWAPCHAIN_MULTIPLE;

    _instance = OpenXR::Instance::instance();
    _instance->setValidationLayer(settings->getValidationLayer());
    _instance->setDepthInfo(settings->getDepthInfo());
    if (!_instance->init(settings->getAppName().c_str(),
                         settings->getAppVersion()))
        return;
    if (_useDepthInfo && !_instance->supportsCompositionLayerDepth())
    {
        OSG_WARN << "osgXR: CompositionLayerDepth extension not supported, depth info will be disabled" << std::endl;
        _useDepthInfo = false;
    }

    // Get OpenXR system for chosen form factor

    switch (settings->getFormFactor())
    {
        case Settings::HEAD_MOUNTED_DISPLAY:
            _formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
            break;
        case Settings::HANDHELD_DISPLAY:
            _formFactor = XR_FORM_FACTOR_HANDHELD_DISPLAY;
            break;
    }
    _system = _instance->getSystem(_formFactor);
    if (!_system)
        return;

    // Choose the first supported view configuration

    for (const auto &viewConfig: _system->getViewConfigurations())
    {
        switch (viewConfig.getType())
        {
            case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
            case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
                _chosenViewConfig = &viewConfig;
                break;
            default:
                break;
        }
        if (_chosenViewConfig)
            break;
    }
    if (!_chosenViewConfig)
    {
        OSG_WARN << "OpenXRDisplay::configure(): No supported view configuration" << std::endl;
        return;
    }

    // Choose an environment blend mode

    for (XrEnvironmentBlendMode envBlendMode: _chosenViewConfig->getEnvBlendModes())
    {
        if ((unsigned int)envBlendMode > 31)
            continue;
        uint32_t mask = (1u << (unsigned int)envBlendMode);
        if (settings->getPreferredEnvBlendModeMask() & mask)
        {
            _chosenEnvBlendMode = envBlendMode;
            break;
        }
        if (_chosenEnvBlendMode != XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM &&
            settings->getAllowedEnvBlendModeMask() & mask)
        {
            _chosenEnvBlendMode = envBlendMode;
        }
    }
    if (_chosenEnvBlendMode == XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM)
    {
        OSG_WARN << "OpenXRDisplay::configure(): No supported environment blend mode" << std::endl;
        return;
    }
}

XRState::XRSwapchain::XRSwapchain(XRState *state,
                                  osg::ref_ptr<OpenXR::Session> session,
                                  const OpenXR::System::ViewConfiguration::View &view,
                                  int64_t chosenSwapchainFormat,
                                  int64_t chosenDepthSwapchainFormat) :
    OpenXR::SwapchainGroup(session, view,
                           XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
                           chosenSwapchainFormat,
                           XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                           chosenDepthSwapchainFormat),
    _state(state),
    _currentImage(-1),
    _numDrawPasses(0),
    _drawPassesDone(0)
{
    if (valid())
    {
        // Create framebuffer objects for each image in swapchain
        auto &textures = getImageTextures();
        const ImageTextures *depthTextures = nullptr;
        if (depthValid())
        {
            depthTextures = &getDepthImageTextures();
            if (textures.size() != depthTextures->size())
                OSG_WARN << "Depth swapchain image count mismatch, expected " << textures.size() << ", got " << depthTextures->size() << std::endl;
        }

        _imageFramebuffers.reserve(textures.size());
        for (unsigned int i = 0; i < textures.size(); ++i)
        {
            GLuint texture = textures[i];
            GLuint depthTexture = 0;
            if (depthTextures)
                depthTexture = (*depthTextures)[i];
            XRFramebuffer *fb = new XRFramebuffer(getWidth(),
                                                  getHeight(),
                                                  texture, depthTexture);
            _imageFramebuffers.push_back(fb);
        }
    }
}

void XRState::XRSwapchain::preDrawCallback(osg::RenderInfo &renderInfo)
{
    bool firstPass = (_currentImage < 0);
    if (firstPass)
    {
        // Acquire a swapchain image
        _currentImage = acquireImages();
        if (_currentImage < 0 || (unsigned int)_currentImage >= _imageFramebuffers.size())
        {
            OSG_WARN << "XRView::preDrawCallback(): Failure to acquire OpenXR swapchain image (got image index " << _currentImage << ")" << std::endl;
            _currentImage = -1;
            return;
        }
        _drawPassesDone = 0;
    }
    const auto &fbo = _imageFramebuffers[_currentImage];

    // Bind the framebuffer
    osg::State &state = *renderInfo.getState();
    fbo->bind(state);

    if (firstPass)
    {
        // Wait for the image to be ready to render into
        if (!waitImages(100e6 /* 100ms */))
        {
            OSG_WARN << "XRView::preDrawCallback(): Failure to wait for OpenXR swapchain image " << _currentImage << std::endl;

            // Unclear what the best course of action is here...
            fbo->unbind(state);
            return;
        }
    }
}

void XRState::XRSwapchain::postDrawCallback(osg::RenderInfo &renderInfo)
{
    if (_currentImage < 0)
        return;
    const auto &fbo = _imageFramebuffers[_currentImage];

    // Unbind the framebuffer
    osg::State& state = *renderInfo.getState();
    fbo->unbind(state);

    if (++_drawPassesDone == _numDrawPasses*_state->getPassesPerView())
    {
        // Done rendering. release the swapchain image
        releaseImages();
        _currentImage = -1;
    }
}

XRState::XRView::XRView(XRState *state,
                        uint32_t viewIndex,
                        osg::ref_ptr<XRSwapchain> swapchain) :
    _state(state),
    _swapchainSubImage(swapchain),
    _viewIndex(viewIndex)
{
    swapchain->incNumDrawPasses();
}

XRState::XRView::XRView(XRState *state,
                        uint32_t viewIndex,
                        osg::ref_ptr<XRSwapchain> swapchain,
                        const OpenXR::System::ViewConfiguration::View::Viewport &viewport) :
    _state(state),
    _swapchainSubImage(swapchain, viewport),
    _viewIndex(viewIndex)
{
    swapchain->incNumDrawPasses();
}

XRState::XRView::~XRView()
{
    getSwapchain()->decNumDrawPasses();
}

osg::ref_ptr<osg::Camera> XRState::XRView::createCamera(osg::ref_ptr<osg::GraphicsContext> gc,
                                                        osg::ref_ptr<osg::Camera> mainCamera)
{
    // Inspired by osgopenvrviewer
    osg::ref_ptr<osg::Camera> camera = new osg::Camera();
    camera->setClearColor(mainCamera->getClearColor());
    camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    // FIXME necessary I expect...
    //camera->setRenderOrder(osg::Camera::PRE_RENDER, eye);
    //camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
    camera->setAllowEventFocus(false);
    camera->setReferenceFrame(osg::Camera::RELATIVE_RF);
    //camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
    //camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF_INHERIT_VIEWPOINT);
    camera->setViewport(_swapchainSubImage.getX(),
                        _swapchainSubImage.getY(),
                        _swapchainSubImage.getWidth(),
                        _swapchainSubImage.getHeight());
    camera->setGraphicsContext(gc);

    // Here we avoid doing anything regarding OSG camera RTT attachment.
    // Ideally we would use automatic methods within OSG for handling RTT but in this
    // case it seemed simpler to handle FBO creation and selection within this class.

    // This initial draw callback is used to disable normal OSG camera setup which
    // would undo our RTT FBO configuration.
    camera->setInitialDrawCallback(new InitialDrawCallback(_state));

    camera->setPreDrawCallback(new PreDrawCallback(getSwapchain()));
    camera->setFinalDrawCallback(new PostDrawCallback(getSwapchain()));

    return camera;
}

void XRState::XRView::endFrame()
{
    if (_state->_frame != nullptr)
    {
        // Add view info to projection layer for compositor
        osg::ref_ptr<OpenXR::CompositionLayerProjection> proj = _state->getProjectionLayer();
        if (proj != nullptr)
        {
            proj->addView(_state->_frame, _viewIndex, _swapchainSubImage,
                          _state->_useDepthInfo ? &_state->_depthInfo : nullptr);
        }
        else
        {
            OSG_WARN << "No projection layer" << std::endl;
        }
    }
    else
    {
        OSG_WARN << "No frame" << std::endl;
    }
}

void XRState::init(osgViewer::GraphicsWindow *window,
                   osgViewer::View *view)
{
    // Create session using the GraphicsWindow

    _session = new OpenXR::Session(_system, window);
    if (!_session)
    {
        OSG_WARN << "XRState::init(): No suitable GraphicsWindow to create an OpenXR session" << std::endl;
        return;
    }

    // Choose a swapchain format

    int64_t chosenSwapchainFormat = 0;
    int64_t chosenDepthSwapchainFormat = 0;
    for (int64_t format: _session->getSwapchainFormats())
    {
        switch (format)
        {
            case GL_RGBA16:
                // FIXME This one will do for now...
                if (!chosenSwapchainFormat)
                    chosenSwapchainFormat = format;
                break;
            case GL_DEPTH_COMPONENT16:
            case GL_DEPTH_COMPONENT24:
            case GL_DEPTH_COMPONENT32:
                if (_useDepthInfo && !chosenDepthSwapchainFormat)
                    chosenDepthSwapchainFormat = format;
                break;
            default:
                break;
        }
    }
    if (!chosenSwapchainFormat)
    {
        OSG_WARN << "XRState::init(): No supported swapchain format found" << std::endl;
        return;
    }
    if (_useDepthInfo && !chosenDepthSwapchainFormat)
    {
        OSG_WARN << "XRState::init(): No supported depth swapchain format found" << std::endl;
        _useDepthInfo = false;
    }

    // Set up swapchains & viewports
    switch (_swapchainMode)
    {
        case SwapchainMode::SWAPCHAIN_SINGLE:
            if (!setupSingleSwapchain(chosenSwapchainFormat,
                                      chosenDepthSwapchainFormat))
                return;
            break;

        case SwapchainMode::SWAPCHAIN_AUTOMATIC:
            // Should already have been handled by the constructor
        case SwapchainMode::SWAPCHAIN_MULTIPLE:
            if (!setupMultipleSwapchains(chosenSwapchainFormat,
                                         chosenDepthSwapchainFormat))
                return;
            break;
    }

    // Set up cameras
    switch (_vrMode)
    {
        case VRMode::VRMODE_SLAVE_CAMERAS:
            setupSlaveCameras(window, view);
            break;

        case VRMode::VRMODE_AUTOMATIC:
            // Should already have been handled by the constructor
        case VRMode::VRMODE_SCENE_VIEW:
            setupSceneViewCameras(window, view);
            break;
    }

    // Attach a callback to detect swap
    osg::ref_ptr<osg::GraphicsContext> gc = window;
    osg::ref_ptr<SwapCallback> swapCallback = new SwapCallback(this);
    gc->setSwapCallback(swapCallback);
}

bool XRState::setupSingleSwapchain(int64_t format, int64_t depthFormat)
{
    const auto &views = _chosenViewConfig->getViews();
    _views.reserve(views.size());

    // Arrange viewports on a single swapchain image
    OpenXR::System::ViewConfiguration::View singleView(0, 0);
    std::vector<OpenXR::System::ViewConfiguration::View::Viewport> viewports;
    viewports.resize(views.size());
    for (uint32_t i = 0; i < views.size(); ++i)
        viewports[i] = singleView.tileHorizontally(views[i]);

    // Create a single swapchain
    osg::ref_ptr<XRSwapchain> xrSwapchain = new XRSwapchain(this, _session,
                                                            singleView, format,
                                                            depthFormat);
    // And the views
    _views.reserve(views.size());
    for (uint32_t i = 0; i < views.size(); ++i)
    {
        osg::ref_ptr<XRView> xrView = new XRView(this, i, xrSwapchain,
                                                 viewports[i]);
        if (!xrView.valid())
            return false; // failure
        _views.push_back(xrView);
    }

    return true;
}

bool XRState::setupMultipleSwapchains(int64_t format, int64_t depthFormat)
{
    const auto &views = _chosenViewConfig->getViews();
    _views.reserve(views.size());

    for (uint32_t i = 0; i < views.size(); ++i)
    {
        const auto &vcView = views[i];
        osg::ref_ptr<XRSwapchain> xrSwapchain = new XRSwapchain(this, _session,
                                                                vcView, format,
                                                                depthFormat);
        osg::ref_ptr<XRView> xrView = new XRView(this, i, xrSwapchain);
        if (!xrView.valid())
            return false; // failure
        _views.push_back(xrView);
    }

    return true;
}

void XRState::setupSlaveCameras(osgViewer::GraphicsWindow *window,
                                osgViewer::View *view)
{
    osg::ref_ptr<osg::GraphicsContext> gc = window;
    osg::ref_ptr<osg::Camera> camera = view->getCamera();
    //camera->setName("Main");

    for (uint32_t i = 0; i < _views.size(); ++i)
    {
        osg::ref_ptr<osg::Camera> cam = _views[i]->createCamera(gc, camera);

        // Add view camera as slave. We'll find the position once the
        // session is ready.
        if (!view->addSlave(cam.get(), osg::Matrix::identity(), osg::Matrix::identity(), true))
        {
            OSG_WARN << "XRState::init(): Couldn't add slave camera" << std::endl;
        } else {
            view->getSlave(i)._updateSlaveCallback = new UpdateSlaveCallback(i, this);
        }
    }

    // Disable rendering of main camera since its being overwritten by the swap texture anyway
    camera->setGraphicsContext(nullptr);
}

void XRState::setupSceneViewCameras(osgViewer::GraphicsWindow *window,
                                    osgViewer::View *view)
{
    // If the main camera is for rendering, set up that
    osg::ref_ptr<osg::Camera> camera = view->getCamera();
    if (camera->getGraphicsContext() != nullptr)
    {
        setupSceneViewCamera(camera);
    }
    else
    {
        // Otherwise, we'll have to go and poke about in the slave cameras
        _passesPerView = 0;
        unsigned int numSlaves = view->getNumSlaves();
        for (unsigned int i = 0; i < numSlaves; ++i)
        {
            osg::ref_ptr<osg::Camera> slaveCam = view->getSlave(i)._camera;
            if (slaveCam->getRenderTargetImplementation() == osg::Camera::FRAME_BUFFER)
            {
                OSG_WARN << "XRState::setupSceneViewCameras(): slave " << slaveCam->getName() << std::endl;
                setupSceneViewCamera(slaveCam);
                ++_passesPerView;
            }
        }

        if (!_passesPerView)
        {
            OSG_WARN << "XRState::setupSceneViewCameras(): Failed to find suitable slave camera" << std::endl;
            return;
        }
    }

    osg::DisplaySettings* ds = osg::DisplaySettings::instance().get();
    ds->setStereo(true);
    ds->setStereoMode(osg::DisplaySettings::HORIZONTAL_SPLIT);
    ds->setSplitStereoHorizontalEyeMapping(osg::DisplaySettings::LEFT_EYE_LEFT_VIEWPORT);
    ds->setUseSceneViewForStereoHint(true);
}

void XRState::setupSceneViewCamera(osg::ref_ptr<osg::Camera> camera)
{
    camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    camera->setDrawBuffer(GL_COLOR_ATTACHMENT0);
    camera->setReadBuffer(GL_COLOR_ATTACHMENT0);

    // Here we avoid doing anything regarding OSG camera RTT attachment.
    // Ideally we would use automatic methods within OSG for handling RTT but in this
    // case it seemed simpler to handle FBO creation and selection within this class.

    // This initial draw callback is used to disable normal OSG camera setup which
    // would undo our RTT FBO configuration.
    camera->setInitialDrawCallback(new InitialDrawCallback(this));

    camera->setPreDrawCallback(new PreDrawCallback(_views[0]->getSwapchain()));
    camera->setFinalDrawCallback(new PostDrawCallback(_views[0]->getSwapchain()));

    // Set the viewport (seems to need redoing!)
    camera->setViewport(0, 0,
                        _views[0]->getSwapchain()->getWidth(),
                        _views[0]->getSwapchain()->getHeight());

    // Set the stereo matrices callback on each SceneView
    osgViewer::Renderer *renderer = static_cast<osgViewer::Renderer *>(camera->getRenderer());
    osg::ref_ptr<ComputeStereoMatricesCallback> stereoCallback = new ComputeStereoMatricesCallback(this);
    for (unsigned int i = 0; i < 2; ++i)
    {
        osgUtil::SceneView *sceneView = renderer->getSceneView(i);
        sceneView->setComputeStereoMatricesCallback(stereoCallback);
    }
}

void XRState::startFrame()
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);

    if (_frame)
        return;

    _instance->handleEvents();
    if (!_session->isReady())
    {
        OSG_WARN << "OpenXR session not ready for frames yet" << std::endl;
        return;
    }
    if (!_session->hasBegun())
    {
        if (!_session->begin(*_chosenViewConfig))
            return;
    }

    _frame = _session->waitFrame();
}

void XRState::startRendering()
{
    startFrame();
    if (_frame != nullptr && !_frame->hasBegun()) {
        _frame->begin();
        _projectionLayer = new OpenXR::CompositionLayerProjection(_views.size());
        _projectionLayer->setLayerFlags(XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT);
        _projectionLayer->setSpace(_session->getLocalSpace());
    }
}

void XRState::endFrame()
{
    if (_frame == nullptr)
    {
        OSG_WARN << "OpenXR frame not waited for" << std::endl;
        return;
    }
    if (!_frame->hasBegun())
    {
        OSG_WARN << "OpenXR frame not begun" << std::endl;
        return;
    }
    for (auto &view: _views)
        view->endFrame();
    _frame->setEnvBlendMode(_chosenEnvBlendMode);
    _frame->addLayer(_projectionLayer.get());
    _frame->end();
    _frame = nullptr;
}

void XRState::updateSlave(uint32_t viewIndex, osg::View& view,
                          osg::View::Slave& slave)
{
    bool setProjection = false;
    osg::Matrix projectionMatrix;

    startFrame();
    if (_frame != nullptr) {
        if (_frame->isPositionValid() && _frame->isOrientationValid()) {
            const auto &pose = _frame->getViewPose(viewIndex);
            osg::Vec3 position(pose.position.x,
                               pose.position.y,
                               pose.position.z);
            osg::Quat orientation(pose.orientation.x,
                                  pose.orientation.y,
                                  pose.orientation.z,
                                  pose.orientation.w);

            osg::Matrix viewOffset;
            viewOffset.setTrans(viewOffset.getTrans() + position * _unitsPerMeter);
            viewOffset.preMultRotate(orientation);
            viewOffset = osg::Matrix::inverse(viewOffset);
            slave._viewOffset = viewOffset;

            double left, right, bottom, top, zNear, zFar;
            if (view.getCamera()->getProjectionMatrixAsFrustum(left, right,
                                                               bottom, top,
                                                               zNear, zFar))
            {
                const auto &fov = _frame->getViewFov(viewIndex);
                createProjectionFov(projectionMatrix, fov, zNear, zFar);
                setProjection = true;
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

osg::Matrixd XRState::getEyeProjection(uint32_t viewIndex, const osg::Matrixd& projection)
{
    startFrame();
    if (_frame != nullptr) {
        double left, right, bottom, top, zNear, zFar;
        if (projection.getFrustum(left, right,
                                  bottom, top,
                                  zNear, zFar))
        {
            const auto &fov = _frame->getViewFov(viewIndex);
            osg::Matrix projectionMatrix;
            createProjectionFov(projectionMatrix, fov, zNear, zFar);
            return projectionMatrix;
        }
    }
    return projection;
}

osg::Matrixd XRState::getEyeView(uint32_t viewIndex, const osg::Matrixd& view)
{
    startFrame();
    if (_frame != nullptr) {
        if (_frame->isPositionValid() && _frame->isOrientationValid()) {
            const auto &pose = _frame->getViewPose(viewIndex);
            osg::Vec3 position(pose.position.x,
                               pose.position.y,
                               pose.position.z);
            osg::Quat orientation(pose.orientation.x,
                                  pose.orientation.y,
                                  pose.orientation.z,
                                  pose.orientation.w);

            osg::Matrix viewOffset;
            viewOffset.setTrans(viewOffset.getTrans() + position * _unitsPerMeter);
            viewOffset.preMultRotate(orientation);
            viewOffset = osg::Matrix::inverse(viewOffset);
            return view * viewOffset;
        }
    }
    return view;
}

void XRState::initialDrawCallback(osg::RenderInfo &renderInfo)
{
    osg::GraphicsOperation *graphicsOperation = renderInfo.getCurrentCamera()->getRenderer();
    osgViewer::Renderer *renderer = dynamic_cast<osgViewer::Renderer*>(graphicsOperation);
    if (renderer != nullptr)
    {
        // Disable normal OSG FBO camera setup because it will undo the MSAA FBO configuration.
        renderer->setCameraRequiresSetUp(false);
    }

    startRendering();

    // Get up to date depth info from camera's projection matrix
    _depthInfo.setZRangeFromProjection(renderInfo.getCurrentCamera()->getProjectionMatrix());
}

void XRState::swapBuffersImplementation(osg::GraphicsContext* gc)
{
    // Submit rendered frame to compositor
    //m_device->submitFrame();

    endFrame();

    // Blit mirror texture to backbuffer
    //m_device->blitMirrorTexture(gc);

    // Run the default system swapBufferImplementation
    //gc->swapBuffersImplementation();
}
