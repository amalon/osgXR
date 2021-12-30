// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "XRState.h"
#include "XRStateCallbacks.h"
#include "ActionSet.h"
#include "InteractionProfile.h"
#include "Subaction.h"
#include "projection.h"

#include <osgXR/Manager>

#include <osg/Camera>
#include <osg/ColorMask>
#include <osg/Depth>
#include <osg/DisplaySettings>
#include <osg/FrameBufferObject>
#include <osg/Notify>
#include <osg/MatrixTransform>
#include <osg/RenderInfo>
#include <osg/View>

#include <osgUtil/SceneView>

#include <osgViewer/GraphicsWindow>
#include <osgViewer/Renderer>
#include <osgViewer/View>

#include <cassert>
#include <climits>
#include <cmath>
#include <sstream>

using namespace osgXR;

XRState::XRState(Settings *settings, Manager *manager) :
    _settings(settings),
    _settingsCopy(*settings),
    _manager(manager),
    _visibilityMaskLeft(0),
    _visibilityMaskRight(0),
    _actionsUpdated(false),
    _currentState(VRSTATE_DISABLED),
    _downState(VRSTATE_MAX),
    _upState(VRSTATE_DISABLED),
    _upDelay(0),
    _probing(false),
    _stateChanged(false),
    _probed(false),
    _useDepthInfo(false),
    _useVisibilityMask(false),
    _formFactor(XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY),
    _system(nullptr),
    _chosenViewConfig(nullptr),
    _chosenEnvBlendMode(XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM),
    _vrMode(VRMode::VRMODE_AUTOMATIC),
    _swapchainMode(SwapchainMode::SWAPCHAIN_AUTOMATIC)
{
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
    _numDrawPasses(0),
    _drawPassesDone(0),
    _imagesReady(false)
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

XRState::XRSwapchain::~XRSwapchain()
{
    osg::State *state = _state->_window->getState();
    // FIXME window has no state on shutdown...
    if (!state)
        return;
    // Explicitly release FBOs etc
    // GL context must be current
    for (unsigned int i = 0; i < _imageFramebuffers.size(); ++i)
        _imageFramebuffers[i]->releaseGLObjects(*state);
}

void XRState::XRSwapchain::setupImage(const osg::FrameStamp *stamp)
{
    auto opt_fbo = _imageFramebuffers[stamp];
    bool firstPass = !opt_fbo.has_value();
    int imageIndex;
    if (firstPass)
    {
        // Acquire a swapchain image
        imageIndex = acquireImages();
        if (imageIndex < 0 || (unsigned int)imageIndex >= _imageFramebuffers.size())
        {
            OSG_WARN << "XRView::preDrawCallback(): Failure to acquire OpenXR swapchain image (got image index " << imageIndex << ")" << std::endl;
            return;
        }
        _imageFramebuffers.setStamp(imageIndex, stamp);
        opt_fbo.emplace(_imageFramebuffers[imageIndex]);
        _drawPassesDone = 0;
        // Images aren't ready until we've waited for them to be so
        _imagesReady = false;
    }
}

void XRState::XRSwapchain::preDrawCallback(osg::RenderInfo &renderInfo)
{
    const osg::FrameStamp *stamp = renderInfo.getState()->getFrameStamp();
    setupImage(stamp);

    auto opt_fbo = _imageFramebuffers[stamp];
    if (!opt_fbo.has_value())
        return;

    const auto &fbo = opt_fbo.value();

    // Bind the framebuffer
    osg::State &state = *renderInfo.getState();
    fbo->bind(state);

    if (!_imagesReady)
    {
        // Wait for the image to be ready to render into
        if (!waitImages(100e6 /* 100ms */))
        {
            OSG_WARN << "XRView::preDrawCallback(): Failure to wait for OpenXR swapchain image" << std::endl;

            // Unclear what the best course of action is here...
            fbo->unbind(state);
            return;
        }

        _imagesReady = true;
    }
}

void XRState::XRSwapchain::postDrawCallback(osg::RenderInfo &renderInfo)
{
    const osg::FrameStamp *stamp = renderInfo.getState()->getFrameStamp();
    auto opt_fbo = _imageFramebuffers[stamp];
    if (!opt_fbo.has_value())
        return;
    const auto &fbo = opt_fbo.value();

    // Unbind the framebuffer
    osg::State& state = *renderInfo.getState();
    fbo->unbind(state);

    if (++_drawPassesDone == _numDrawPasses && _imagesReady)
    {
        // Done rendering. release the swapchain image
        releaseImages();

        _imagesReady = false;
    }
}

void XRState::XRSwapchain::endFrame()
{
    // Double check images are released
    if (_imagesReady)
    {
        releaseImages();
        _imagesReady = false;
    }
}

osg::ref_ptr<osg::Texture2D> XRState::XRSwapchain::getOsgTexture(const osg::FrameStamp *stamp)
{
    int index = _imageFramebuffers.findStamp(stamp);
    if (index < 0)
        return nullptr;
    return getSwapchain()->getImageOsgTexture(index);
}

XRState::XRView::XRView(XRState *state,
                        uint32_t viewIndex,
                        osg::ref_ptr<XRSwapchain> swapchain) :
    _state(state),
    _swapchainSubImage(swapchain),
    _viewIndex(viewIndex)
{
}

XRState::XRView::XRView(XRState *state,
                        uint32_t viewIndex,
                        osg::ref_ptr<XRSwapchain> swapchain,
                        const OpenXR::System::ViewConfiguration::View::Viewport &viewport) :
    _state(state),
    _swapchainSubImage(swapchain, viewport),
    _viewIndex(viewIndex)
{
}

XRState::XRView::~XRView()
{
}

void XRState::XRView::setupCamera(osg::ref_ptr<osg::Camera> camera)
{
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

    // Here we avoid doing anything regarding OSG camera RTT attachment.
    // Ideally we would use automatic methods within OSG for handling RTT but in this
    // case it seemed simpler to handle FBO creation and selection within this class.

    // This initial draw callback is used to disable normal OSG camera setup which
    // would undo our RTT FBO configuration.
    camera->setInitialDrawCallback(new InitialDrawCallback(_state));

    camera->setPreDrawCallback(new PreDrawCallback(getSwapchain()));
    camera->setFinalDrawCallback(new PostDrawCallback(getSwapchain()));
}

void XRState::XRView::endFrame(OpenXR::Session::Frame *frame)
{
    // Double check images are released
    getSwapchain()->endFrame();

    // Add view info to projection layer for compositor
    osg::ref_ptr<OpenXR::CompositionLayerProjection> proj = _state->getProjectionLayer();
    if (proj != nullptr)
    {
        proj->addView(frame, _viewIndex, _swapchainSubImage,
                      _state->_useDepthInfo ? &_state->_depthInfo : nullptr);
    }
    else
    {
        OSG_WARN << "No projection layer" << std::endl;
    }
}

XRState::AppView::AppView(XRState *state,
                          osgViewer::GraphicsWindow *window,
                          osgViewer::View *osgView) :
    View(window, osgView),
    _valid(false),
    _state(state)
{
}

void XRState::AppView::init()
{
    // Notify app to create a new view
    if (_state->_manager.valid())
        _state->_manager->doCreateView(this);
    _valid = true;
}

XRState::AppView::~AppView()
{
    destroy();
}

void XRState::AppView::destroy()
{
    // Notify app to destroy this view
    if (_valid && _state->_manager.valid())
        _state->_manager->doDestroyView(this);
    _valid = false;
}

XRState::SlaveCamsAppView::SlaveCamsAppView(XRState *state,
                                            uint32_t viewIndex,
                                            osgViewer::GraphicsWindow *window,
                                            osgViewer::View *osgView) :
    AppView(state, window, osgView),
    _viewIndex(viewIndex)
{
}

void XRState::SlaveCamsAppView::addSlave(osg::Camera *slaveCamera)
{
    XRView *xrView = _state->_xrViews[_viewIndex];
    xrView->setupCamera(slaveCamera);
    xrView->getSwapchain()->incNumDrawPasses();

    osg::ref_ptr<osg::MatrixTransform> visMaskTransform;
    // Set up visibility mask for this slave camera
    // We'll keep track of the transform in the slave callback so it can be
    // positioned at the appropriate range
    if (_state->needsVisibilityMask(slaveCamera))
        _state->setupVisibilityMask(slaveCamera, _viewIndex, visMaskTransform);

    osg::View::Slave *slave = _osgView->findSlaveForCamera(slaveCamera);
    slave->_updateSlaveCallback = new SlaveCamsUpdateSlaveCallback(_viewIndex, _state, visMaskTransform.get());
}

void XRState::SlaveCamsAppView::removeSlave(osg::Camera *slaveCamera)
{
    XRView *xrView = _state->_xrViews[_viewIndex];
    xrView->getSwapchain()->decNumDrawPasses();
}

XRState::SceneViewAppView::SceneViewAppView(XRState *state,
                                            osgViewer::GraphicsWindow *window,
                                            osgViewer::View *osgView) :
    AppView(state, window, osgView)
{
}

void XRState::SceneViewAppView::addSlave(osg::Camera *slaveCamera)
{
    _state->setupSceneViewCamera(slaveCamera);
    _state->_xrViews[0]->getSwapchain()->incNumDrawPasses(2);

    osg::ref_ptr<osg::MatrixTransform> visMaskTransform;
    // Set up visibility masks for this slave camera
    // We'll keep track of the transform in the slave callback so it can be
    // positioned at the appropriate range
    if (_state->needsVisibilityMask(slaveCamera))
        _state->setupSceneViewVisibilityMasks(slaveCamera, visMaskTransform);

    if (visMaskTransform.valid())
    {
        osg::View::Slave *slave = _osgView->findSlaveForCamera(slaveCamera);
        slave->_updateSlaveCallback = new SceneViewUpdateSlaveCallback(_state, visMaskTransform.get());
    }
}

void XRState::SceneViewAppView::removeSlave(osg::Camera *slaveCamera)
{
    _state->_xrViews[0]->getSwapchain()->decNumDrawPasses(2);
}

std::shared_ptr<Subaction::Private> XRState::getSubaction(const std::string &path)
{
    auto it = _subactions.find(path);
    if (it != _subactions.end())
    {
        auto ret = (*it).second.lock();
        if (ret)
            return ret;
    }

    auto subaction = std::make_shared<Subaction::Private>(this, path);
    _subactions[path] = subaction;
    return subaction;
}

InteractionProfile *XRState::getCurrentInteractionProfile(const OpenXR::Path &subactionPath) const
{
    if (_session.valid())
    {
        // Find the path of the current profile
        OpenXR::Path profilePath = _session->getCurrentInteractionProfile(subactionPath);
        if (!profilePath.valid())
            return nullptr;

        // Compare against the paths of known interaction profiles
        for (auto *profile: _interactionProfiles)
            if (profile->getPath() == profilePath)
                return profile->getPublic();
    }
    return nullptr;
}

const char *XRState::getStateString() const
{
    static const char *vrStateNames[VRSTATE_MAX] = {
        "disabled",
        "inactive",
        "detected",
        "session",
        "actions",
    };
    static const char *sessionStateNames[] = {
        "unknown",
        "idle",
        "starting",
        "invisible",
        "visible unfocused",
        "focused",
        "stopping",
        "loss pending",
        "ending"
    };
    static const char *vrStateChangeNames[VRSTATE_MAX + 1][VRSTATE_MAX] = {
        {   // down = VRSTATE_DISABLED
            "disabling",                // up = VRSTATE_DISABLED
            "reinitialising",           // up = VRSTATE_INSTANCE
            "reinitialising & probing", // up = VRSTATE_SYSTEM
            "restarting session",       // up = VRSTATE_SESSION
            "restarting"                // up = VRSTATE_ACTIONS
        },
        {   // down = VRSTATE_INSTANCE
            nullptr,                    // up = VRSTATE_DISABLED
            "deactivating",             // up = VRSTATE_INSTANCE
            "reprobing",                // up = VRSTATE_SYSTEM
            "reprobing session",        // up = VRSTATE_SESSION
            "reprobing session"         // up = VRSTATE_ACTIONS
        },
        {   // down = VRSTATE_SYSTEM
            nullptr,                    // up = VRSTATE_DISABLED
            nullptr,                    // up = VRSTATE_INSTANCE
            "ending session",           // up = VRSTATE_SYSTEM
            "restarting session",       // up = VRSTATE_SESSION
            "restarting"                // up = VRSTATE_ACTIONS
        },
        {   // down = VRSTATE_SESSION
            nullptr,                    // up = VRSTATE_DISABLED
            nullptr,                    // up = VRSTATE_INSTANCE
            nullptr,                    // up = VRSTATE_SYSTEM
            nullptr,                    // up = VRSTATE_SESSION
            "attaching actions"         // up = VRSTATE_ACTIONS
        },
        {   // down = VRSTATE_ACTIONS
            nullptr,                    // up = VRSTATE_DISABLED
            nullptr,                    // up = VRSTATE_INSTANCE
            nullptr,                    // up = VRSTATE_SYSTEM
            nullptr,                    // up = VRSTATE_SESSION
            nullptr,                    // up = VRSTATE_ACTIONS
        },
        {   // down = VRSTATE_MAX
            nullptr,                    // up = VRSTATE_DISABLED
            "initialising",             // up = VRSTATE_INSTANCE
            "probing",                  // up = VRSTATE_SYSTEM
            "starting session",         // up = VRSTATE_SESSION
            "attaching actions"         // up = VRSTATE_ACTIONS
        },
    };

    std::string out = vrStateNames[_currentState];
    if (_currentState >= VRSTATE_SESSION)
    {
        out += " ";
        out += sessionStateNames[_session->getState()];
    }
    if (isStateUpdateNeeded())
    {
        const char *str = vrStateChangeNames[_downState][_upState];
        if (str)
        {
            out += " (";
            out += str;
            out += ")";
        }
    }

    _stateString = out;
    return _stateString.c_str();
}

bool XRState::hasValidationLayer() const
{
    if (!_probed)
        probe();
    return _hasValidationLayer;
}

bool XRState::hasDepthInfoExtension() const
{
    if (!_probed)
        probe();
    return _hasDepthInfoExtension;
}

bool XRState::hasVisibilityMaskExtension() const
{
    if (!_probed)
        probe();
    return _hasVisibilityMaskExtension;
}

void XRState::syncSettings()
{
    unsigned int diff = _settingsCopy._diff(*_settings.get());
    if (diff & (Settings::DIFF_APP_INFO |
                Settings::DIFF_VALIDATION_LAYER))
        // Recreate instance
        setDownState(VRSTATE_DISABLED);
    else if (diff & (Settings::DIFF_FORM_FACTOR |
                     Settings::DIFF_BLEND_MODE))
        // Reread system
        setDownState(VRSTATE_INSTANCE);
    else if (diff & (Settings::DIFF_DEPTH_INFO |
                     Settings::DIFF_VISIBILITY_MASK |
                     Settings::DIFF_VR_MODE |
                     Settings::DIFF_SWAPCHAIN_MODE))
        // Recreate session
        setDownState(VRSTATE_SYSTEM);
}

bool XRState::getActionsUpdated() const
{
    // Have action sets or interaction profiles been added or removed?
    if (_actionsUpdated)
        return true;

    // Have action sets or their actions been altered?
    for (auto *actionSet: _actionSets)
        if (actionSet->getUpdated())
            return true;

    // Have interaction profile bindings been altered?
    for (auto *interactionProfile: _interactionProfiles)
        if (interactionProfile->getUpdated())
            return true;

    return false;
}

void XRState::syncActionSetup()
{
    // Nothing is required if actions haven't been attached yet
    if (_currentState < VRSTATE_ACTIONS)
        return;

    // Restart session if actions have been updated
    if (getActionsUpdated())
        setDownState(VRSTATE_SYSTEM);
}

bool XRState::checkAndResetStateChanged()
{
    bool ret = _stateChanged;
    _stateChanged = false;
    return ret;
}

void XRState::update()
{
    assert(_manager.valid());

    typedef UpResult (XRState::*UpHandler)();
    static UpHandler upStateHandlers[VRSTATE_MAX - 1] = {
        &XRState::upInstance,
        &XRState::upSystem,
        &XRState::upSession,
        &XRState::upActions,
    };
    typedef DownResult (XRState::*DownHandler)();
    static DownHandler downStateHandlers[VRSTATE_MAX - 1] = {
        &XRState::downInstance,
        &XRState::downSystem,
        &XRState::downSession,
        &XRState::downActions,
    };

    bool pollNeeded = true;
    for (;;)
    {
        // Poll first
        if (pollNeeded && _instance.valid() && _instance->valid())
        {
            // Poll for events
            _instance->pollEvents(this);

            // Sync actions
            if (_session.valid())
                _session->syncActions();

            // Check for instance lost
            if (_instance->lost())
                setDownState(VRSTATE_DISABLED);

            pollNeeded = false;
        }
        // Then down transitions
        else if (_downState < _currentState)
        {
            DownResult res = (this->*downStateHandlers[_currentState-1])();
            if (res == DOWN_SUCCESS)
            {
                _currentState = (VRState)((int)_currentState - 1);
                if (_currentState == _downState)
                    _downState = VRSTATE_MAX;
                _stateChanged = true;
            }
            else // DOWN_SOON
            {
                break;
            }
        }
        // Then up transitions
        else if (_upState > _currentState)
        {
            if (_upDelay > 0)
            {
                // try again soon
                --_upDelay;
                break;
            }
            UpResult res = (this->*upStateHandlers[_currentState])();
            if (res == UP_SUCCESS)
            {
                if (_currentState <= _downState)
                    _downState = VRSTATE_MAX;
                _currentState = (VRState)((int)_currentState + 1);
                // Poll events again after bringing up session
                if (_currentState >= VRSTATE_SESSION)
                    pollNeeded = true;
                _stateChanged = true;
            }
            else if (res == UP_ABORT)
            {
                VRState probingState = getProbingState();
                if (probingState < _currentState)
                    // Drop down to probing state
                    setDestState(getProbingState());
                else
                    // Go up no further
                    _upState = _currentState;
                _stateChanged = true;
            }
            else // UP_SOON or UP_LATER
            {
                if (res == UP_LATER)
                {
                    // Don't poll incessantly
                    _upDelay = 100;
                }
                break;
            }
        }
        else
        {
            _upDelay = 0;
            break;
        }
    }

    // Restart threading in case we had to disable it to prevent the GL context
    // being bound in another thread during certain OpenXR calls.
    if (_viewer.valid())
        _viewer->startThreading();
}

void XRState::onInstanceLossPending(OpenXR::Instance *instance,
                                    const XrEventDataInstanceLossPending *event)
{
    // Reinitialize instance
    setDownState(VRSTATE_DISABLED);
    // FIXME use event.lossTime?
    _upDelay = 100;
}

void XRState::onInteractionProfileChanged(OpenXR::Session *session,
                                          const XrEventDataInteractionProfileChanged *event)
{
    // notify subactions so they can invalidate their cached current profile
    for (auto &pair: _subactions)
    {
        auto subaction = pair.second.lock();
        if (subaction)
            subaction->onInteractionProfileChanged(session);
    }
}

void XRState::onSessionStateChanged(OpenXR::Session *session,
                                    const XrEventDataSessionStateChanged *event)
{
    OpenXR::EventHandler::onSessionStateChanged(session, event);
    _stateChanged = true;
}

void XRState::onSessionStateStart(OpenXR::Session *session)
{
}

void XRState::onSessionStateEnd(OpenXR::Session *session, bool retry)
{
    if (!session->isExiting())
    {
        // If the exit wasn't requested, drop back to a safe state
        if (retry)
            setDownState(VRSTATE_INSTANCE);
        else
            setDestState(getProbingState());
    }
}

void XRState::onSessionStateReady(OpenXR::Session *session)
{
    assert(session == _session);
    if (!session->begin(*_chosenViewConfig))
    {
        // This should normally have succeeded
        setDestState(getProbingState());
        return;
    }

    // Set up cameras
    switch (_vrMode)
    {
        case VRMode::VRMODE_SLAVE_CAMERAS:
            setupSlaveCameras();
            break;

        case VRMode::VRMODE_AUTOMATIC:
            // Should already have been handled by upSession()
        case VRMode::VRMODE_SCENE_VIEW:
            setupSceneViewCameras();
            break;
    }

    // Attach a callback to detect swap
    osg::ref_ptr<osg::GraphicsContext> gc = _window.get();
    osg::ref_ptr<SwapCallback> swapCallback = new SwapCallback(this);
    gc->setSwapCallback(swapCallback);

    // Finally set up any mirrors that may be queued in the manager
    if (_manager.valid())
    {
        // FIXME consider
        _manager->_setupMirrors();
        _manager->onRunning();
    }
}

void XRState::onSessionStateStopping(OpenXR::Session *session, bool loss)
{
    // check no frame in progress

    // clean up appViews
    for (auto appView: _appViews)
        appView->destroy();
    _appViews.resize(0);

    osg::ref_ptr<osg::GraphicsContext> gc = _window.get();
    gc->setSwapCallback(nullptr);

    if (!loss)
        session->end();

    if (_manager.valid())
        _manager->onStopped();
}

void XRState::onSessionStateFocus(OpenXR::Session *session)
{
    if (_manager.valid())
        _manager->onFocus();
}

void XRState::onSessionStateUnfocus(OpenXR::Session *session)
{
    if (_manager.valid())
        _manager->onUnfocus();
}

void XRState::probe() const
{
    _hasValidationLayer = OpenXR::Instance::hasLayer(XR_APILAYER_LUNARG_core_validation);
    _hasDepthInfoExtension = OpenXR::Instance::hasExtension(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
    _hasVisibilityMaskExtension = OpenXR::Instance::hasExtension(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME);

    _probed = true;
}

void XRState::unprobe() const
{
    OpenXR::Instance::invalidateLayers();
    OpenXR::Instance::invalidateExtensions();

    _probed = false;
}

XRState::UpResult XRState::upInstance()
{
    assert(!_instance.valid());

    // Create OpenXR instance

    // Update needed settings that may have changed
    _settingsCopy.setApp(_settings->getAppName(), _settings->getAppVersion());
    _settingsCopy.setValidationLayer(_settings->getValidationLayer());

    _instance = new OpenXR::Instance();
    _instance->setValidationLayer(_settingsCopy.getValidationLayer());
    _instance->setDepthInfo(true);
    _instance->setVisibilityMask(true);
    switch (_instance->init(_settingsCopy.getAppName().c_str(),
                            _settingsCopy.getAppVersion()))
    {
    case OpenXR::Instance::INIT_SUCCESS:
        break;
    case OpenXR::Instance::INIT_LATER:
        _instance = nullptr;
        return UP_LATER;
    case OpenXR::Instance::INIT_FAIL:
        _instance = nullptr;
        return UP_ABORT;
    }

    return UP_SUCCESS;
}

XRState::DownResult XRState::downInstance()
{
    assert(_instance.valid());

    // This should destroy actions and action sets
    for (auto *profile: _interactionProfiles)
        profile->cleanupInstance();
    for (auto *actionSet: _actionSets)
        actionSet->cleanupInstance();

    for (auto &pair: _subactions)
    {
        auto subaction = pair.second.lock();
        if (subaction)
            subaction->cleanupInstance();
    }

    if (_probed)
        unprobe();

    osg::observer_ptr<OpenXR::Instance> oldInstance = _instance;
    _instance = nullptr;
    assert(!oldInstance.valid());

    return DOWN_SUCCESS;
}

XRState::UpResult XRState::upSystem()
{
    assert(!_system);

    // Update needed settings that may have changed
    _settingsCopy.setFormFactor(_settings->getFormFactor());
    _settingsCopy.setPreferredEnvBlendModeMask(_settings->getPreferredEnvBlendModeMask());
    _settingsCopy.setAllowedEnvBlendModeMask(_settings->getAllowedEnvBlendModeMask());

    // Get OpenXR system for chosen form factor

    switch (_settingsCopy.getFormFactor())
    {
        case Settings::HEAD_MOUNTED_DISPLAY:
            _formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
            break;
        case Settings::HANDHELD_DISPLAY:
            _formFactor = XR_FORM_FACTOR_HANDHELD_DISPLAY;
            break;
    }
    bool supported;
    _system = _instance->getSystem(_formFactor, &supported);
    if (!_system)
        return supported ? UP_LATER : UP_ABORT;

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
        OSG_WARN << "XRState::XRState(): No supported view configuration" << std::endl;
        _system = nullptr;
        return UP_ABORT;
    }

    // Choose an environment blend mode

    for (XrEnvironmentBlendMode envBlendMode: _chosenViewConfig->getEnvBlendModes())
    {
        if ((unsigned int)envBlendMode > 31)
            continue;
        uint32_t mask = (1u << (unsigned int)envBlendMode);
        if (_settingsCopy.getPreferredEnvBlendModeMask() & mask)
        {
            _chosenEnvBlendMode = envBlendMode;
            break;
        }
        if (_chosenEnvBlendMode != XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM &&
            _settingsCopy.getAllowedEnvBlendModeMask() & mask)
        {
            _chosenEnvBlendMode = envBlendMode;
        }
    }
    if (_chosenEnvBlendMode == XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM)
    {
        OSG_WARN << "XRState::XRState(): No supported environment blend mode" << std::endl;
        _system = nullptr;
        return UP_ABORT;
    }

    return UP_SUCCESS;
}

XRState::DownResult XRState::downSystem()
{
    _system = nullptr;
    _instance->invalidateSystem(_formFactor);
    return DOWN_SUCCESS;
}

XRState::UpResult XRState::upSession()
{
    assert(_system);
    assert(!_session.valid());

    if (!_window.valid() || !_view.valid())
        // Maybe window & view haven't been initialized yet
        return UP_SOON;

    // Update needed settings that may have changed
    _settingsCopy.setDepthInfo(_settings->getDepthInfo());
    _settingsCopy.setVisibilityMask(_settings->getVisibilityMask());
    _settingsCopy.setVRMode(_settings->getVRMode());
    _settingsCopy.setSwapchainMode(_settings->getSwapchainMode());
    _useDepthInfo = _settingsCopy.getDepthInfo();
    _useVisibilityMask = _settingsCopy.getVisibilityMask();
    _vrMode = _settingsCopy.getVRMode();
    _swapchainMode = _settingsCopy.getSwapchainMode();

    if (_useDepthInfo && !_instance->supportsCompositionLayerDepth())
    {
        OSG_WARN << "osgXR: CompositionLayerDepth extension not supported, depth info will be disabled" << std::endl;
        _useDepthInfo = false;
    }
    if (_useVisibilityMask && !_instance->supportsVisibilityMask())
    {
        OSG_WARN << "osgXR: VisibilityMask extension not supported, visibility masking will be disabled" << std::endl;
        _useVisibilityMask = false;
    }

    // Decide on the algorithm to use. SceneView mode is faster.
    if (_vrMode == VRMode::VRMODE_AUTOMATIC)
        _vrMode = VRMode::VRMODE_SCENE_VIEW;

    // SceneView mode only works with a stereo view config
    if (_vrMode == VRMode::VRMODE_SCENE_VIEW &&
        _chosenViewConfig->getType() != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
    {
        _vrMode = VRMode::VRMODE_SLAVE_CAMERAS;
        if (_settingsCopy.getVRMode() == VRMode::VRMODE_SCENE_VIEW)
            OSG_WARN << "osgXR: No stereo view config for VR mode SCENE_VIEW, falling back to SLAVE_CAMERAS" << std::endl;
    }

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

    // Stop threading to prevent the GL context being bound in another thread
    // during certain OpenXR calls (session & swapchain handling).
    if (_viewer.valid())
        _viewer->stopThreading();

    // Create session using the GraphicsWindow
    _session = new OpenXR::Session(_system, _window.get());
    if (!_session)
    {
        OSG_WARN << "XRState::init(): No suitable GraphicsWindow to create an OpenXR session" << std::endl;
        return UP_ABORT;
    }

    // Decide on ideal depth bits
    unsigned int bestDepthBits = 24;
    auto *traits = _window->getTraits();
    if (traits)
        bestDepthBits = traits->depth;

    // Choose a swapchain format
    int64_t chosenSwapchainFormat = 0;
    int64_t chosenDepthSwapchainFormat = 0;
    unsigned int chosenDepthBits = 0;
    for (int64_t format: _session->getSwapchainFormats())
    {
        unsigned int thisDepthBits = 0;
        switch (format)
        {
            case GL_RGBA16:
            case GL_RGB10_A2:
            case GL_RGBA8:
                // Choose the first supported format suggested by the runtime
                if (!chosenSwapchainFormat)
                    chosenSwapchainFormat = format;
                break;
            case GL_DEPTH_COMPONENT16:
                thisDepthBits = 16;
                goto handle_depth;
            case GL_DEPTH_COMPONENT24:
                thisDepthBits = 24;
                goto handle_depth;
            case GL_DEPTH_COMPONENT32:
                thisDepthBits = 32;
                // fall through
            handle_depth:
                if (_useDepthInfo)
                {
                    if (// Anything is better than nothing
                        !chosenDepthSwapchainFormat ||
                        // A higher number of bits is better than not enough
                        (thisDepthBits > chosenDepthBits && chosenDepthBits < bestDepthBits) ||
                        // A lower number of bits may still be enough
                        (bestDepthBits < thisDepthBits && thisDepthBits < chosenDepthBits))
                    {
                        chosenDepthSwapchainFormat = format;
                        chosenDepthBits = thisDepthBits;
                    }
                }
                break;
            default:
                break;
        }
    }
    if (!chosenSwapchainFormat)
    {
        std::stringstream formats;
        formats << std::hex;
        for (int64_t format: _session->getSwapchainFormats())
            formats << " 0x" << format;
        OSG_WARN << "XRState::init(): No supported swapchain format found in ["
                 << formats.str() << " ]" << std::endl;
        _session = nullptr;
        return UP_ABORT;
    }
    if (_useDepthInfo && !chosenDepthSwapchainFormat)
    {
        std::stringstream formats;
        formats << std::hex;
        for (int64_t format: _session->getSwapchainFormats())
            formats << " 0x" << format;
        OSG_WARN << "XRState::init(): No supported depth swapchain format found in ["
                 << formats.str() << " ]" << std::endl;
        _useDepthInfo = false;
    }

    // Set up swapchains & viewports
    switch (_swapchainMode)
    {
        case SwapchainMode::SWAPCHAIN_SINGLE:
            if (!setupSingleSwapchain(chosenSwapchainFormat,
                                      chosenDepthSwapchainFormat))
            {
                _session = nullptr;
                return UP_ABORT;
            }
            break;

        case SwapchainMode::SWAPCHAIN_AUTOMATIC:
            // Should already have been handled by upSession()
        case SwapchainMode::SWAPCHAIN_MULTIPLE:
            if (!setupMultipleSwapchains(chosenSwapchainFormat,
                                         chosenDepthSwapchainFormat))
            {
                _session = nullptr;
                return UP_ABORT;
            }
            break;
    }

    return UP_SUCCESS;
}

XRState::DownResult XRState::downSession()
{
    assert(_session.valid());

    if (_session->isRunning())
    {
        if (!_session->isExiting())
            _session->requestExit();
        return DOWN_SOON;
    }

    // no frames should be in progress

    // Stop threading to prevent the GL context being bound in another thread
    // during certain OpenXR calls (session & swapchain destruction).
    if (_viewer.valid())
        _viewer->stopThreading();

    // Ensure the GL context is active for destruction of FBOs in XRFramebuffer
    _session->makeCurrent();
    _xrViews.resize(0);
    _session->releaseContext();

    // this will destroy the session
    for (auto *actionSet: _actionSets)
        actionSet->cleanupSession();
    for (auto &pair: _subactions)
    {
        auto subaction = pair.second.lock();
        if (subaction)
            subaction->cleanupSession();
    }
    osg::observer_ptr<OpenXR::Session> oldSession = _session;
    _session = nullptr;
    assert(!oldSession.valid());

    return DOWN_SUCCESS;
}

XRState::UpResult XRState::upActions()
{
    // Wait until the app has set up action sets and interaction profiles
    if (_actionSets.empty() || _interactionProfiles.empty())
        return UP_SOON;

    // Set up anything needed for interaction profiles
    for (auto *profile: _interactionProfiles)
        profile->setup(_instance);

    // Attach action sets to the session
    for (auto *actionSet: _actionSets)
        actionSet->setup(_session);
    if (_session->attachActionSets())
        _actionsUpdated = false;
    // Treat attach fail as success, as VR can still continue without input
    return UP_SUCCESS;
}

XRState::DownResult XRState::downActions()
{
    // Action setup cannot be undone
    return DOWN_SUCCESS;
}

bool XRState::setupSingleSwapchain(int64_t format, int64_t depthFormat)
{
    const auto &views = _chosenViewConfig->getViews();
    _xrViews.reserve(views.size());

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
    _xrViews.reserve(views.size());
    for (uint32_t i = 0; i < views.size(); ++i)
    {
        osg::ref_ptr<XRView> xrView = new XRView(this, i, xrSwapchain,
                                                 viewports[i]);
        if (!xrView.valid())
        {
            _xrViews.resize(0);
            return false; // failure
        }
        _xrViews.push_back(xrView);
    }

    return true;
}

bool XRState::setupMultipleSwapchains(int64_t format, int64_t depthFormat)
{
    const auto &views = _chosenViewConfig->getViews();
    _xrViews.reserve(views.size());

    for (uint32_t i = 0; i < views.size(); ++i)
    {
        const auto &vcView = views[i];
        osg::ref_ptr<XRSwapchain> xrSwapchain = new XRSwapchain(this, _session,
                                                                vcView, format,
                                                                depthFormat);
        osg::ref_ptr<XRView> xrView = new XRView(this, i, xrSwapchain);
        if (!xrView.valid())
        {
            _xrViews.resize(0);
            return false; // failure
        }
        _xrViews.push_back(xrView);
    }

    return true;
}

void XRState::setupSlaveCameras()
{
    osg::ref_ptr<osg::GraphicsContext> gc = _window.get();
    osg::Camera *camera = _view.valid() ? _view->getCamera() : nullptr;
    //camera->setName("Main");

    _appViews.resize(_xrViews.size());
    for (uint32_t i = 0; i < _xrViews.size(); ++i)
    {
        SlaveCamsAppView *appView = new SlaveCamsAppView(this, i, _window.get(),
                                                         _view.get());
        appView->init();
        _appViews[i] = appView;

        if (camera && !_manager.valid())
        {
            // The app isn't using a manager class, so create the new slave
            // camera ourselves
            osg::ref_ptr<osg::Camera> cam = new osg::Camera();
            cam->setClearColor(camera->getClearColor());
            cam->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            cam->setGraphicsContext(gc);

            // Add as a slave to the OSG view
            if (!_view->addSlave(cam.get(), osg::Matrix::identity(),
                                 osg::Matrix::identity(), true))
            {
                OSG_WARN << "XRState::init(): Couldn't add slave camera" << std::endl;
                continue;
            }

            // And ensure it gets configured for VR
            appView->addSlave(cam.get());
        }
    }

    if (camera && !_manager.valid())
    {
        // Disable rendering of main camera since its being overwritten by the swap texture anyway
        camera->setGraphicsContext(nullptr);
    }
}

void XRState::setupSceneViewCameras()
{
    _stereoDisplaySettings = new osg::DisplaySettings(*osg::DisplaySettings::instance().get());
    _stereoDisplaySettings->setStereo(true);
    _stereoDisplaySettings->setStereoMode(osg::DisplaySettings::HORIZONTAL_SPLIT);
    _stereoDisplaySettings->setSplitStereoHorizontalEyeMapping(osg::DisplaySettings::LEFT_EYE_LEFT_VIEWPORT);
    _stereoDisplaySettings->setUseSceneViewForStereoHint(true);

    _appViews.resize(1);
    SceneViewAppView *appView = new SceneViewAppView(this, _window.get(),
                                                     _view.get());
    appView->init();
    _appViews[0] = appView;

    if (_view.valid() && !_manager.valid())
    {
        // If the main camera is for rendering, set up that
        osg::ref_ptr<osg::Camera> camera = _view->getCamera();
        if (camera->getGraphicsContext() != nullptr)
        {
            _appViews[0]->addSlave(camera);
        }
        else
        {
            // Otherwise, we'll have to go and poke about in the slave cameras
            unsigned int numSlaves = _view->getNumSlaves();
            for (unsigned int i = 0; i < numSlaves; ++i)
            {
                osg::ref_ptr<osg::Camera> slaveCam = _view->getSlave(i)._camera;
                if (slaveCam->getRenderTargetImplementation() == osg::Camera::FRAME_BUFFER)
                {
                    OSG_WARN << "XRState::setupSceneViewCameras(): slave " << slaveCam->getName() << std::endl;
                    _appViews[0]->addSlave(slaveCam);
                }
            }

            if (!_xrViews[0]->getSwapchain()->getNumDrawPasses())
            {
                OSG_WARN << "XRState::setupSceneViewCameras(): Failed to find suitable slave camera" << std::endl;
                return;
            }
        }
    }
}

void XRState::setupSceneViewCamera(osg::Camera *camera)
{
    camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    camera->setDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    camera->setReadBuffer(GL_COLOR_ATTACHMENT0_EXT);

    // Here we avoid doing anything regarding OSG camera RTT attachment.
    // Ideally we would use automatic methods within OSG for handling RTT but in this
    // case it seemed simpler to handle FBO creation and selection within this class.

    // This initial draw callback is used to disable normal OSG camera setup which
    // would undo our RTT FBO configuration.
    camera->setInitialDrawCallback(new InitialDrawCallback(this));

    camera->setPreDrawCallback(new PreDrawCallback(_xrViews[0]->getSwapchain()));
    camera->setFinalDrawCallback(new PostDrawCallback(_xrViews[0]->getSwapchain()));

    // Set the viewport (seems to need redoing!)
    camera->setViewport(0, 0,
                        _xrViews[0]->getSwapchain()->getWidth(),
                        _xrViews[0]->getSwapchain()->getHeight());

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

void XRState::setupSceneViewVisibilityMasks(osg::Camera *camera,
                                            osg::ref_ptr<osg::MatrixTransform> &transform)
{
    for (uint32_t i = 0; i < _xrViews.size(); ++i)
    {
        osg::ref_ptr<osg::Geode> geode = setupVisibilityMask(camera, i, transform);
        if (geode.valid())
        {
            if (i == 0)
                geode->setNodeMask(_visibilityMaskLeft);
            else
                geode->setNodeMask(_visibilityMaskRight);
        }
    }
}

osg::ref_ptr<osg::Geode> XRState::setupVisibilityMask(osg::Camera *camera, uint32_t viewIndex,
                                                      osg::ref_ptr<osg::MatrixTransform> &transform)
{
    osg::ref_ptr<osg::Geometry> geometry;
    geometry = _session->getVisibilityMask(viewIndex,
                                           XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR);
    if (!geometry.valid())
        return nullptr;

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->setCullingActive(false);
    geode->addDrawable(geometry);

    osg::ref_ptr<osg::StateSet> state = geode->getOrCreateStateSet();
    int forceOff = osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED;
    state->setMode(GL_LIGHTING, forceOff);
    state->setAttribute(new osg::ColorMask(false, false, false, false),
                        osg::StateAttribute::OVERRIDE);
    state->setAttribute(new osg::Depth(osg::Depth::ALWAYS, 0.0f, 0.0f, true),
                        osg::StateAttribute::OVERRIDE);
    state->setRenderBinDetails(INT_MIN, "RenderBin");

    if (!transform.valid())
    {
        transform = new osg::MatrixTransform;
        transform->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
    }
    transform->addChild(geode);

    camera->addChild(transform);

    return geode;
}

osg::ref_ptr<OpenXR::Session::Frame> XRState::getFrame(osg::FrameStamp *stamp)
{
    // Fast path
    osg::ref_ptr<OpenXR::Session::Frame> frame = _frames.getFrame(stamp);
    if (frame.valid())
        return frame;

    if (!_session->isRunning())
        return nullptr;

    // Slow path
    return _frames.getFrame(stamp, _session);
}

void XRState::startRendering(osg::FrameStamp *stamp)
{
    osg::ref_ptr<OpenXR::Session::Frame> frame = getFrame(stamp);
    if (frame.valid() && !frame->hasBegun())
    {
        frame->begin();
        _projectionLayer = new OpenXR::CompositionLayerProjection(_xrViews.size());
        _projectionLayer->setLayerFlags(XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT);
        _projectionLayer->setSpace(_session->getLocalSpace());
    }
}

void XRState::endFrame(osg::FrameStamp *stamp)
{
    osg::ref_ptr<OpenXR::Session::Frame> frame = _frames.getFrame(stamp);
    if (!frame.valid())
    {
        OSG_WARN << "OpenXR frame not waited for" << std::endl;
        return;
    }
    if (!frame->hasBegun())
    {
        OSG_WARN << "OpenXR frame not begun" << std::endl;
        return;
    }
    for (auto &view: _xrViews)
        view->endFrame(frame);
    frame->setEnvBlendMode(_chosenEnvBlendMode);
    frame->addLayer(_projectionLayer.get());
    _frames.endFrame(stamp);
}

void XRState::updateSlave(uint32_t viewIndex, osg::View& view,
                          osg::View::Slave& slave)
{
    bool setProjection = false;
    osg::Matrix projectionMatrix;

    osg::ref_ptr<OpenXR::Session::Frame> frame = getFrame(view.getFrameStamp());
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
            viewOffset.setTrans(viewOffset.getTrans() + position * _settings->getUnitsPerMeter());
            viewOffset.preMultRotate(orientation);
            viewOffset = osg::Matrix::inverse(viewOffset);
            slave._viewOffset = viewOffset;

            double left, right, bottom, top, zNear, zFar;
            if (view.getCamera()->getProjectionMatrixAsFrustum(left, right,
                                                               bottom, top,
                                                               zNear, zFar))
            {
                const auto &fov = frame->getViewFov(viewIndex);
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

void XRState::updateVisibilityMaskTransform(osg::Camera *camera,
                                            osg::MatrixTransform *transform)
{
    float scale = 1.0f;
    double left, right, bottom, top, zNear, zFar;
    if (camera->getProjectionMatrixAsFrustum(left, right,
                                             bottom, top,
                                             zNear, zFar))
    {
        if (isinf(zFar))
            scale = zNear * 1.1;
        else
            scale = (zNear + zFar) / 2;
    }
    transform->setMatrix(osg::Matrix::translate(0, 0, -1));
    transform->postMult(osg::Matrix::scale(scale, scale, scale));
}

osg::Matrixd XRState::getEyeProjection(osg::FrameStamp *stamp,
                                       uint32_t viewIndex,
                                       const osg::Matrixd& projection)
{
    osg::ref_ptr<OpenXR::Session::Frame> frame = getFrame(stamp);
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

osg::Matrixd XRState::getEyeView(osg::FrameStamp *stamp, uint32_t viewIndex,
                                 const osg::Matrixd& view)
{
    osg::ref_ptr<OpenXR::Session::Frame> frame = getFrame(stamp);
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
            viewOffset.setTrans(viewOffset.getTrans() + position * _settings->getUnitsPerMeter());
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

    startRendering(renderInfo.getState()->getFrameStamp());

    // Get up to date depth info from camera's projection matrix
    _depthInfo.setZRangeFromProjection(renderInfo.getCurrentCamera()->getProjectionMatrix());
}

void XRState::swapBuffersImplementation(osg::GraphicsContext* gc)
{
    // Submit rendered frame to compositor
    //m_device->submitFrame();

    endFrame(gc->getState()->getFrameStamp());

    // Blit mirror texture to backbuffer
    //m_device->blitMirrorTexture(gc);

    // Run the default system swapBufferImplementation
    gc->swapBuffersImplementation();
}
