// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "XRState.h"
#include "XRStateCallbacks.h"
#include "AppView.h"
#include "AppViewSlaveCams.h"
#include "AppViewSceneView.h"
#include "AppViewGeomShaders.h"
#include "AppViewOVRMultiview.h"
#include "ActionSet.h"
#include "CompositionLayer.h"
#include "DebugCallbackOsg.h"
#include "Extension.h"
#include "InteractionProfile.h"
#include "Space.h"
#include "Subaction.h"

#include <osgXR/Manager>

#include <osg/Camera>
#include <osg/ColorMask>
#include <osg/Depth>
#include <osg/FrameBufferObject>
#include <osg/GLExtensions>
#include <osg/Notify>
#include <osg/MatrixTransform>
#include <osg/Program>
#include <osg/RenderInfo>
#include <osg/Shader>
#include <osg/Texture>
#include <osg/View>

#include <osgUtil/SceneView>

#include <osgViewer/GraphicsWindow>
#include <osgViewer/Renderer>
#include <osgViewer/View>

#include <cassert>
#include <climits>
#include <cmath>
#include <sstream>

#ifndef GL_DEPTH32F_STENCIL8
#define GL_DEPTH32F_STENCIL8 0x8cad
#endif

using namespace osgXR;

XRState::XRState(Settings *settings, Manager *manager) :
    _settings(settings),
    _settingsCopy(*settings),
    _manager(manager),
    _visibilityMaskLeft(0),
    _visibilityMaskRight(0),
    _actionsUpdated(false),
    _compositionLayersUpdated(false),
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
                                  int64_t chosenRGBAFormat,
                                  int64_t chosenDepthFormat,
                                  GLenum fallbackDepthFormat,
                                  unsigned int fbPerLayer) :
    OpenXR::SwapchainGroup(session, view,
                           XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
                           chosenRGBAFormat,
                           XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                           chosenDepthFormat),
    _state(state),
    _forcedAlpha(-1.0f),
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
                OSG_WARN << "osgXR: Depth swapchain image count mismatch, expected " << textures.size() << ", got " << depthTextures->size() << std::endl;
        }

        _imageFramebuffers.reserve(textures.size());
        for (unsigned int i = 0; i < textures.size(); ++i)
        {
            GLuint texture = textures[i];
            GLuint depthTexture = 0;
            if (depthTextures)
                depthTexture = (*depthTextures)[i];
            // Construct a framebuffer for each layer in the swapchain image,
            // unless fbPerLayer is something like
            // XRFramebuffer::ARRAY_INDEX_GEOMETRY in which case only a single
            // FB is needed.
            FBVec& fbos = _imageFramebuffers.push_back(FBVec());
            unsigned int numFbs = fbPerLayer ? 1 : getArraySize();
            for (unsigned int layer = 0; layer < numFbs; ++layer)
            {
                XRFramebuffer *fb = new XRFramebuffer(getWidth(),
                                                      getHeight(),
                                                      getArraySize(),
                                                      fbPerLayer ? fbPerLayer : layer,
                                                      texture, depthTexture,
                                                      chosenRGBAFormat,
                                                      chosenDepthFormat);
                fb->setFallbackDepthFormat(fallbackDepthFormat);
                fbos.push_back(fb);
            }
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
        for (auto &fb: _imageFramebuffers[i])
            fb->releaseGLObjects(*state);
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
            OSG_WARN << "osgXR: Failure to acquire OpenXR swapchain image (got image index " << imageIndex << ")" << std::endl;
            return;
        }
        _imageFramebuffers.setStamp(imageIndex, stamp);
        _drawPassesDone = 0;
        // Images aren't ready until we've waited for them to be so
        _imagesReady = false;
    }
}

void XRState::XRSwapchain::preDrawCallback(osg::RenderInfo &renderInfo,
                                           unsigned int arrayIndex)
{
    const osg::FrameStamp *stamp = renderInfo.getState()->getFrameStamp();
    setupImage(stamp);

    auto opt_fbo = _imageFramebuffers[stamp];
    if (!opt_fbo.has_value())
        return;

    const auto &fbo = opt_fbo.value()[arrayIndex];

    // Bind the framebuffer
    osg::State &state = *renderInfo.getState();
    fbo->bind(state, _state->_instance);

    if (!_imagesReady)
    {
        // Wait for the image to be ready to render into
        if (!waitImages(100e6 /* 100ms */))
        {
            OSG_WARN << "osgXR: Failure to wait for OpenXR swapchain image" << std::endl;

            // Unclear what the best course of action is here...
            fbo->unbind(state);
            return;
        }

        _imagesReady = true;
    }
}

void XRState::XRSwapchain::postDrawCallback(osg::RenderInfo &renderInfo,
                                            unsigned int arrayIndex)
{
    const osg::FrameStamp *stamp = renderInfo.getState()->getFrameStamp();
    auto opt_fbo = _imageFramebuffers[stamp];
    if (!opt_fbo.has_value())
        return;
    const auto &fbo = opt_fbo.value()[arrayIndex];

    // Unbind the framebuffer
    osg::State& state = *renderInfo.getState();

    if (++_drawPassesDone == _numDrawPasses && _imagesReady)
    {
        if (_forcedAlpha >= 0)
        {
            // Hack the alpha to a particular value, unpremultiplied
            // FIXME this overwrites clear colour!
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
            glClearColor(0, 0, 0, _forcedAlpha);
            glClear(GL_COLOR_BUFFER_BIT);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glClearColor(0, 0, 0, 1);
        }

        fbo->unbind(state);

        // Done rendering. release the swapchain image
        releaseImages();

        _imagesReady = false;
    }
    else
    {
        fbo->unbind(state);
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

osg::ref_ptr<osg::Texture> XRState::XRSwapchain::getOsgTexture(const osg::FrameStamp *stamp)
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
        OSG_WARN << "osgXR: No projection layer" << std::endl;
    }
}

XRState::AppSubView::AppSubView(XRState::XRView *xrView,
                                const osg::Matrix &viewMatrix,
                                const osg::Matrix &projectionMatrix) :
    _xrView(xrView),
    _viewMatrix(viewMatrix),
    _projectionMatrix(projectionMatrix)
{
}

unsigned int XRState::AppSubView::getArrayIndex() const
{
    return _xrView->getSubImage().getArrayIndex();
}

View::SubView::Viewport XRState::AppSubView::getViewport() const
{
    return View::SubView::Viewport{
        (double)_xrView->getSubImage().getX(),
        (double)_xrView->getSubImage().getY(),
        (double)_xrView->getSubImage().getWidth(),
        (double)_xrView->getSubImage().getHeight()
    };
}

const osg::Matrix &XRState::AppSubView::getViewMatrix() const
{
    return _viewMatrix;
}

const osg::Matrix &XRState::AppSubView::getProjectionMatrix() const
{
    return _projectionMatrix;
}

std::shared_ptr<Extension::Private> XRState::getExtension(const std::string &name)
{
    auto it = _extensions.find(name);
    if (it != _extensions.end())
    {
        auto ret = (*it).second.lock();
        if (ret)
            return ret;
    }

    auto extension = std::make_shared<Extension::Private>(this, name);
    _extensions[name] = extension;
    return extension;
}

std::vector<std::string> XRState::getExtensionNames()
{
    return OpenXR::Instance::getExtensionNames();
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

    // Find last error
    OpenXR::Instance::Result error{};
    const char *errorType[] = {
        "Failed to ",
        "Previously failed to ",
    };
    const OpenXR::Instance::Result *errorp[2] = {
        &error,
        &_lastRunError,
    };
    if (_instance.valid())
        _instance->getError(error);
    else
        errorp[0] = &_lastError;

    // If there was a failure, report it
    for (unsigned int i = 0; i < 2; ++i)
    {
        auto *curError = errorp[i];
        if (curError->failed())
        {
            out += "\n  ";
            out += errorType[i];
            out += curError->action;
            out += " (";
            if (curError->resultName[0] == '\0')
                out += std::to_string(curError->result);
            else
                out += curError->resultName;
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

bool XRState::supportsUserPresence() const
{
    if (_currentState < VRSTATE_SYSTEM)
        return false;
    return _system->getUserPresence();
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
                     Settings::DIFF_SWAPCHAIN_MODE |
                     Settings::DIFF_VIEW_ALIGN_MASK |
                     Settings::DIFF_RGB_ENCODING |
                     Settings::DIFF_DEPTH_ENCODING |
                     Settings::DIFF_RGB_BITS |
                     Settings::DIFF_ALPHA_BITS |
                     Settings::DIFF_DEPTH_BITS |
                     Settings::DIFF_STENCIL_BITS))
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

void XRState::addCompositionLayer(CompositionLayer::Private *layer)
{
    _compositionLayers.push_back(layer);
    _compositionLayersUpdated = true;
}

void XRState::removeCompositionLayer(CompositionLayer::Private *layer)
{
    auto it = std::find(_compositionLayers.begin(), _compositionLayers.end(),
                        layer);
    if (it != _compositionLayers.end())
    {
        _compositionLayers.erase(it);
        _compositionLayersUpdated = true;
    }
}

bool XRState::checkAndResetStateChanged()
{
    bool ret = _stateChanged;
    _stateChanged = false;
    return ret;
}

void XRState::update()
{
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

    _wasThreading = _viewer.valid() && _viewer->areThreadsRunning();
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

            // Check for session lost
            if (_session.valid() && _session->isLost())
                setDownState(VRSTATE_INSTANCE);

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
                    _upDelay = 500;
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
    if (_viewer.valid() && _wasThreading)
        _viewer->startThreading();
}

bool XRState::recenterLocalSpace()
{
    if (!_session.valid())
        return false;

    return _session->recenterLocalSpace();
}

void XRState::onInstanceLossPending(OpenXR::Instance *instance,
                                    const XrEventDataInstanceLossPending *event)
{
    // Reinitialize instance
    setDownState(VRSTATE_DISABLED);
    // FIXME use event.lossTime?
    _upDelay = 500;
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

void XRState::onReferenceSpaceChangePending(OpenXR::Session *session,
                                            const XrEventDataReferenceSpaceChangePending *event)
{
    session->onReferenceSpaceChangePending(event);
}

void XRState::onSessionStateChanged(OpenXR::Session *session,
                                    const XrEventDataSessionStateChanged *event)
{
    OpenXR::EventHandler::onSessionStateChanged(session, event);
    _stateChanged = true;
}

void XRState::onUserPresenceChanged(OpenXR::Session *session,
                                    const XrEventDataUserPresenceChangedEXT *event)
{
    if (_manager.valid())
        _manager->onUserPresence(event->isUserPresent);
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

        case VRMode::VRMODE_GEOMETRY_SHADERS:
            setupGeomShadersCameras();
            break;

        case VRMode::VRMODE_OVR_MULTIVIEW:
            setupOVRMultiviewCameras();
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

    for (auto &extension: _extensions)
    {
        auto ret = extension.second.lock();
        if (ret)
            ret->cleanup();
    }

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

    auto severity = //XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                    XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                    XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    auto types = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                 XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                 XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                 XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
    _instance->setDefaultDebugCallback(new DebugCallbackOsg(severity, types));

    // Always try to enable these extensions
    _extDepthInfo = enableExtension(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
    _extDepthUtils = enableExtension(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
    _extUserPresence = enableExtension(XR_EXT_USER_PRESENCE_EXTENSION_NAME);
    _extVisibilityMask = enableExtension(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME);

    // Enable any enabled extensions that are supported
    for (auto &extension: _enabledExtensions)
        if (extension->getAvailable())
            extension->setup(_instance);

    switch (_instance->init(_settingsCopy.getAppName().c_str(),
                            _settingsCopy.getAppVersion()))
    {
    case OpenXR::Instance::INIT_SUCCESS:
        break;
    case OpenXR::Instance::INIT_LATER:
        _instance->getError(_lastError);
        _instance = nullptr;
        return UP_LATER;
    case OpenXR::Instance::INIT_FAIL:
        _instance->getError(_lastError);
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

    _instance->deinit();

    if (_probed)
        unprobe();

    osg::observer_ptr<OpenXR::Instance> oldInstance = _instance;
    _instance->getError(_lastRunError);
    _lastError = {};
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
        OSG_WARN << "osgXR: No supported view configuration" << std::endl;
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
        OSG_WARN << "osgXR: No supported environment blend mode" << std::endl;
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

    chooseMode(&_vrMode, &_swapchainMode);

    // Update needed settings that may have changed
    _settingsCopy.setDepthInfo(_settings->getDepthInfo());
    _settingsCopy.setVisibilityMask(_settings->getVisibilityMask());
    _settingsCopy.setPreferredVRModeMask(_settings->getPreferredVRModeMask());
    _settingsCopy.setAllowedVRModeMask(_settings->getAllowedVRModeMask());
    _settingsCopy.setPreferredSwapchainModeMask(_settings->getPreferredSwapchainModeMask());
    _settingsCopy.setAllowedSwapchainModeMask(_settings->getAllowedSwapchainModeMask());
    _settingsCopy.setPreferredRGBEncodingMask(_settings->getPreferredRGBEncodingMask());
    _settingsCopy.setAllowedRGBEncodingMask(_settings->getAllowedRGBEncodingMask());
    _settingsCopy.setPreferredDepthEncodingMask(_settings->getPreferredDepthEncodingMask());
    _settingsCopy.setAllowedDepthEncodingMask(_settings->getAllowedDepthEncodingMask());
    _settingsCopy.setRGBBits(_settings->getRGBBits());
    _settingsCopy.setAlphaBits(_settings->getAlphaBits());
    _settingsCopy.setDepthBits(_settings->getDepthBits());
    _settingsCopy.setStencilBits(_settings->getStencilBits());
    _useDepthInfo = _settingsCopy.getDepthInfo();
    _useVisibilityMask = _settingsCopy.getVisibilityMask();

    if (_useDepthInfo && !hasDepthInfoExtension())
    {
        OSG_WARN << "osgXR: CompositionLayerDepth extension not supported, depth info will be disabled" << std::endl;
        _useDepthInfo = false;
    }
    if (_useVisibilityMask && !hasVisibilityMaskExtension())
    {
        OSG_WARN << "osgXR: VisibilityMask extension not supported, visibility masking will be disabled" << std::endl;
        _useVisibilityMask = false;
    }

    // Stop threading to prevent the GL context being bound in another thread
    // during certain OpenXR calls (session & swapchain handling).
    if (_viewer.valid())
        _viewer->stopThreading();

    // Create session using the GraphicsWindow
    _session = new OpenXR::Session(_system, _window.get());
    if (!_session->valid())
    {
        _session = nullptr;
        return UP_ABORT;
    }

    // Decide on ideal bit depths
    unsigned int bestRGBBits = 24; // combined
    unsigned int bestAlphaBits = 0;
    unsigned int bestDepthBits = 16;
    unsigned int bestStencilBits = 0;
    // Use graphics window traits
    auto *traits = _window->getTraits();
    if (traits)
    {
        bestRGBBits = traits->red + traits->green + traits->blue;
        bestAlphaBits = traits->alpha;
        bestDepthBits = traits->depth;
        bestStencilBits = traits->stencil;
    }
    // Override from osgXR::Settings
    if (_settingsCopy.getRGBBits() >= 0)
        bestRGBBits = _settingsCopy.getRGBBits() * 3;
    if (_settingsCopy.getAlphaBits() >= 0)
        bestAlphaBits = _settingsCopy.getAlphaBits();
    if (_settingsCopy.getDepthBits() >= 0)
        bestDepthBits = _settingsCopy.getDepthBits();
    if (_settingsCopy.getStencilBits() >= 0)
        bestStencilBits = _settingsCopy.getStencilBits();

    int64_t chosenRGBAFormat;
    int64_t chosenDepthFormat = 0;
    GLenum fallbackDepthFormat;

    // Choose OpenXR RGBA swapchain format
    chosenRGBAFormat = chooseRGBAFormat(bestRGBBits,
                                        bestAlphaBits,
                                        _settingsCopy.getPreferredRGBEncodingMask(),
                                        _settingsCopy.getAllowedRGBEncodingMask());
    if (!chosenRGBAFormat)
    {
        std::stringstream formats;
        formats << std::hex;
        for (int64_t format: _session->getSwapchainFormats())
            formats << " 0x" << format;
        OSG_WARN << "osgXR: No supported projection swapchain format found in ["
                 << formats.str() << " ]" << std::endl;
        _session = nullptr;
        return UP_ABORT;
    }

    // Choose a fallback depth format in case we can't submit depth to OpenXR
    fallbackDepthFormat = chooseFallbackDepthFormat(bestDepthBits,
                                                    bestStencilBits,
                                                    _settingsCopy.getPreferredDepthEncodingMask(),
                                                    _settingsCopy.getAllowedDepthEncodingMask());

    // Choose OpenXR depth swapchain format
    if (_useDepthInfo)
    {
        chosenDepthFormat = chooseDepthFormat(bestDepthBits,
                                              bestStencilBits,
                                              _settingsCopy.getPreferredDepthEncodingMask(),
                                              _settingsCopy.getAllowedDepthEncodingMask());
        if (!chosenDepthFormat)
        {
            std::stringstream formats;
            formats << std::hex;
            for (int64_t format: _session->getSwapchainFormats())
                formats << " 0x" << format;
            OSG_WARN << "osgXR: No supported projection depth swapchain format found in ["
                << formats.str() << " ]" << std::endl;
            _useDepthInfo = false;
        }
    }

    // Set up swapchains & viewports
    switch (_swapchainMode)
    {
        case SwapchainMode::SWAPCHAIN_SINGLE:
            if (!setupSingleSwapchain(chosenRGBAFormat,
                                      chosenDepthFormat,
                                      fallbackDepthFormat))
            {
                dropSessionCheck();
                return UP_ABORT;
            }
            break;

        case SwapchainMode::SWAPCHAIN_LAYERED:
            if (!setupLayeredSwapchain(chosenRGBAFormat,
                                       chosenDepthFormat,
                                       fallbackDepthFormat))
            {
                dropSessionCheck();
                return UP_ABORT;
            }
            break;

        case SwapchainMode::SWAPCHAIN_AUTOMATIC:
            // Should already have been handled by upSession()
        case SwapchainMode::SWAPCHAIN_MULTIPLE:
            if (!setupMultipleSwapchains(chosenRGBAFormat,
                                         chosenDepthFormat,
                                         fallbackDepthFormat))
            {
                dropSessionCheck();
                return UP_ABORT;
            }
            break;
    }

    // Finally set up other composition layers
    // Ensure layers are sorted
    if (_compositionLayersUpdated)
    {
        _compositionLayersUpdated = false;
        _compositionLayers.sort(CompositionLayer::Private::compareOrder);
    }
    // Set up all layers
    for (auto *layer: _compositionLayers)
        layer->setup(_session);

    return UP_SUCCESS;
}

XRState::DownResult XRState::downSession()
{
    assert(_session.valid());

    if (_session->isLost())
    {
        XrSessionState curState = _session->getState();
        if (curState == XR_SESSION_STATE_FOCUSED)
            onSessionStateUnfocus(_session);
        if (_session->isRunning())
            onSessionStateStopping(_session, true);
        // Attempt restart
        onSessionStateEnd(_session, true);
    }
    else if (_session->isRunning())
    {
        if (!_session->isExiting())
            _session->requestExit();
        return DOWN_SOON;
    }

    // no frames should be in progress
    assert(!_frames.countFrames());

    // Stop threading to prevent the GL context being bound in another thread
    // during certain OpenXR calls (session & swapchain destruction).
    if (_viewer.valid())
        _viewer->stopThreading();

    // Ensure the GL context is active for destruction of FBOs in XRFramebuffer
    if (_wasThreading)
        _window->makeCurrent();
    _xrViews.resize(0);
    if (_wasThreading)
        _window->releaseContext();

    // Clean compilation layers
    for (auto *layer: _compositionLayers)
        layer->cleanupSession();

    // this will destroy the session
    for (auto *actionSet: _actionSets)
        actionSet->cleanupSession();
    for (auto &pair: _subactions)
    {
        auto subaction = pair.second.lock();
        if (subaction)
            subaction->cleanupSession();
    }
    for (auto *space: _spaces)
        space->cleanupSession();
    dropSessionCheck();

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

bool XRState::dropSessionCheck()
{
    osg::observer_ptr<OpenXR::Session> oldSession = _session;
    _session = nullptr;
    if (oldSession.valid()) {
        OSG_WARN << "osgXR: Session not cleaned up" << std::endl;
        return false;
    }
    return true;
}

bool XRState::validateMode(VRMode vrMode, SwapchainMode swapchainMode,
                           std::vector<const char *> &outErrors) const
{
    osg::State *state = _window->getState();
    unsigned int contextID = state->getContextID();

    outErrors.clear();

    if (vrMode == Settings::VRMODE_SLAVE_CAMERAS)
    {
        if (swapchainMode == Settings::SWAPCHAIN_LAYERED &&
            !XRFramebuffer::supportsSingleLayer(*state))
            outErrors.push_back("OpenGL: glFramebufferTextureLayer required");
    }
    else if (vrMode == Settings::VRMODE_SCENE_VIEW)
    {
        auto &views = _chosenViewConfig->getViews();
        if (_chosenViewConfig->getType() != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
            outErrors.push_back("OpenXR: XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO required");
        else if (views.size() != 2)
            outErrors.push_back("OpenXR: View count must be 2");
        else if (views[0].getRecommendedWidth() != views[1].getRecommendedWidth() ||
                 views[0].getRecommendedHeight() != views[1].getRecommendedHeight())
            outErrors.push_back("OpenXR: Views must have matching recommended widths and heights");
    }
    else if (vrMode == Settings::VRMODE_GEOMETRY_SHADERS)
    {
        if (!osg::isGLExtensionSupported(contextID, "GL_ARB_gpu_shader5"))
            outErrors.push_back("OpenGL: GL_ARB_gpu_shader5 required");
        if (!osg::isGLExtensionSupported(contextID, "GL_ARB_viewport_array"))
            outErrors.push_back("OpenGL: GL_ARB_viewport_array required");
        if (swapchainMode == Settings::SWAPCHAIN_LAYERED &&
            !XRFramebuffer::supportsGeomLayer(*state))
            outErrors.push_back("OpenGL: glFramebufferTexture required");
    }
    else if (vrMode == Settings::VRMODE_OVR_MULTIVIEW)
    {
        if (!XRFramebuffer::supportsMultiview(*state))
            outErrors.push_back("OpenSceneGraph: GL_OVR_multiview2 support required");
        if (!osg::isGLExtensionSupported(contextID, "GL_OVR_multiview2"))
            outErrors.push_back("OpenGL: GL_OVR_multiview2 required");
        if (!osg::isGLExtensionSupported(contextID, "GL_ARB_shader_viewport_layer_array"))
            outErrors.push_back("OpenGL: GL_ARB_shader_viewport_layer_array required");
    }

    return outErrors.empty();
}

namespace {

/// Encode mode, swapchain, preference into a single priority number.
struct ModePriority
{
    uint8_t priority;

    // Bit field positions in priority encoding
    // More significant bits (higher shifts) are higher priority
    enum {
        PRIORITY_SWAPCHAIN_SHIFT = 0,
        PRIORITY_SWAPCHAIN_MASK  = 0x3,
        PRIORITY_VRMODE_SHIFT    = PRIORITY_SWAPCHAIN_SHIFT + 2,
        PRIORITY_VRMODE_MASK     = 0x3,
        PRIORITY_PREF_SHIFT      = PRIORITY_VRMODE_SHIFT + 2,
        PRIORITY_PREF_MASK       = 0x3,
    };
    // Priority order, high to low
    typedef enum {
        PREF_1ST  = 0,
        PREF_2ND  = 1,
        PREF_NONE = 2,
    } Preference;
    // Priority order, high to low
    static constexpr Settings::VRMode vrMapping[4] = {
        Settings::VRMODE_OVR_MULTIVIEW,
        Settings::VRMODE_GEOMETRY_SHADERS,
        Settings::VRMODE_SCENE_VIEW,
        Settings::VRMODE_SLAVE_CAMERAS,
    };
    // Priority order, high to low
    static constexpr Settings::SwapchainMode swapchainMapping[4] = {
        Settings::SWAPCHAIN_MULTIPLE,
        Settings::SWAPCHAIN_LAYERED,
        Settings::SWAPCHAIN_SINGLE,
        Settings::SWAPCHAIN_AUTOMATIC
    };

    ModePriority(Settings::VRMode vrmode = Settings::VRMODE_SLAVE_CAMERAS,
                 Settings::SwapchainMode swapchainMode = Settings::SWAPCHAIN_MULTIPLE,
                 Preference pref = PREF_NONE) :
        priority(0)
    {
        setVRMode(vrmode);
        setSwapchainMode(swapchainMode);
        setPreference(pref);
    }

    void setVRMode(Settings::VRMode mode)
    {
        for (unsigned int i = 0; i <= PRIORITY_VRMODE_MASK; ++i) {
            if (vrMapping[i] == mode) {
                priority &= ~(PRIORITY_VRMODE_MASK << PRIORITY_VRMODE_SHIFT);
                priority |= i << PRIORITY_VRMODE_SHIFT;
                return;
            }
        }
    }
    Settings::VRMode getVRMode() const
    {
        return vrMapping[(priority >> PRIORITY_VRMODE_SHIFT) & PRIORITY_VRMODE_MASK];
    }

    void setSwapchainMode(Settings::SwapchainMode mode)
    {
        for (unsigned int i = 0; i <= PRIORITY_SWAPCHAIN_MASK; ++i) {
            if (swapchainMapping[i] == mode) {
                priority &= ~(PRIORITY_SWAPCHAIN_MASK << PRIORITY_SWAPCHAIN_SHIFT);
                priority |= i << PRIORITY_SWAPCHAIN_SHIFT;
                return;
            }
        }
    }
    Settings::SwapchainMode getSwapchainMode() const
    {
        return swapchainMapping[(priority >> PRIORITY_SWAPCHAIN_SHIFT) & PRIORITY_SWAPCHAIN_MASK];
    }

    void setPreference(Preference pref)
    {
        if ((unsigned int)pref <= PRIORITY_PREF_MASK) {
            priority &= ~(PRIORITY_PREF_MASK << PRIORITY_PREF_SHIFT);
            priority |= (unsigned int)pref << PRIORITY_PREF_SHIFT;
        }
    }
    Preference getPreference() const
    {
        return (Preference)((priority >> PRIORITY_PREF_SHIFT) & PRIORITY_PREF_MASK);
    }

    bool operator <(ModePriority other) const
    {
        return priority < other.priority;
    }

    friend std::ostream &operator << (std::ostream &ost, ModePriority rhs)
    {
        const char * vrmodeName = nullptr;
        switch (rhs.getVRMode()) {
        case Settings::VRMODE_SLAVE_CAMERAS:    vrmodeName = "slave"; break;
        case Settings::VRMODE_SCENE_VIEW:       vrmodeName = "osg";   break;
        case Settings::VRMODE_GEOMETRY_SHADERS: vrmodeName = "geom";  break;
        case Settings::VRMODE_OVR_MULTIVIEW:    vrmodeName = "ovr";   break;
        default:                                vrmodeName = "UNK";   break;
        }
        const char * swapchainName = nullptr;
        switch (rhs.getSwapchainMode()) {
        case Settings::SWAPCHAIN_MULTIPLE: swapchainName = "multiple"; break;
        case Settings::SWAPCHAIN_SINGLE:   swapchainName = "tiled";    break;
        case Settings::SWAPCHAIN_LAYERED:  swapchainName = "layered";  break;
        default:                           swapchainName = "UNK";      break;
        }
        const char * prefName = nullptr;
        switch (rhs.getPreference()) {
        case ModePriority::PREF_1ST:  prefName = " (1st preference)"; break;
        case ModePriority::PREF_2ND:  prefName = " (2nd preference)"; break;
        default:                      prefName = "";                  break;
        }
        return ost << vrmodeName << "/" << swapchainName << prefName << std::hex
                   << " [0x" << (unsigned int)rhs.priority << "]" << std::dec;
    }
};

} // anon

void XRState::chooseMode(VRMode *outVRMode,
                         SwapchainMode *outSwapchainMode) const
{
    // Determine modes preferred and allowed by the application
    uint32_t appModePrefMask = _settings->getPreferredVRModeMask();
    uint32_t appModeAllowMask = _settings->getAllowedVRModeMask();
    uint32_t appSwapchainPrefMask = _settings->getPreferredSwapchainModeMask();
    uint32_t appSwapchainAllowMask = _settings->getAllowedSwapchainModeMask();
    // Default allow masks
    if (!appModeAllowMask || appModeAllowMask == (1u << Settings::VRMODE_AUTOMATIC))
        appModeAllowMask |= (1u << Settings::VRMODE_SLAVE_CAMERAS) |
                            (1u << Settings::VRMODE_SCENE_VIEW);
    if (!appSwapchainAllowMask || appSwapchainAllowMask == (1u << Settings::SWAPCHAIN_AUTOMATIC))
        appSwapchainAllowMask = (1u << Settings::SWAPCHAIN_MULTIPLE) |
                                (1u << Settings::SWAPCHAIN_SINGLE);
    // Preferring automatic prefers all allowed masks
    if (appModePrefMask & (1u << Settings::VRMODE_AUTOMATIC))
        appModePrefMask |= appModeAllowMask;
    if (appSwapchainPrefMask & (1u << Settings::SWAPCHAIN_AUTOMATIC))
        appSwapchainPrefMask |= appSwapchainAllowMask;

    // A set is used to automatically sort modes by priority, with a fallback
    // always present
    std::set<ModePriority> modePriorities;
    modePriorities.insert(ModePriority(Settings::VRMODE_SLAVE_CAMERAS,
                                       Settings::SWAPCHAIN_MULTIPLE));
    // Insert prioritised modes into the priority set
    static const ModePriority modesValid[] = {
        ModePriority(Settings::VRMODE_SLAVE_CAMERAS, Settings::SWAPCHAIN_MULTIPLE),
        ModePriority(Settings::VRMODE_SLAVE_CAMERAS, Settings::SWAPCHAIN_LAYERED),
        ModePriority(Settings::VRMODE_SLAVE_CAMERAS, Settings::SWAPCHAIN_SINGLE),
        ModePriority(Settings::VRMODE_SCENE_VIEW, Settings::SWAPCHAIN_SINGLE),
        ModePriority(Settings::VRMODE_GEOMETRY_SHADERS, Settings::SWAPCHAIN_LAYERED),
        ModePriority(Settings::VRMODE_GEOMETRY_SHADERS, Settings::SWAPCHAIN_SINGLE),
        ModePriority(Settings::VRMODE_OVR_MULTIVIEW, Settings::SWAPCHAIN_LAYERED),
    };
    for (ModePriority mode : modesValid)
    {
        uint32_t modeMask = 1u << mode.getVRMode();
        uint32_t swapchainMask = 1u << mode.getSwapchainMode();
        if (appModeAllowMask & modeMask &&
            appSwapchainAllowMask & swapchainMask)
        {
            if (appModePrefMask & modeMask &&
                appSwapchainPrefMask & swapchainMask)
            {
                mode.setPreference(ModePriority::PREF_1ST);
            }
            else if (appModePrefMask & modeMask ||
                     appSwapchainPrefMask & swapchainMask)
            {
                mode.setPreference(ModePriority::PREF_2ND);
            }
            modePriorities.insert(mode);
        }
    }

    // Choose the first (highest priority) mode that validates
    ModePriority chosenMode;
    std::vector<const char*> errors;
    for (ModePriority mode : modePriorities)
    {
        if (validateMode(mode.getVRMode(), mode.getSwapchainMode(), errors))
        {
            // This is the one
            OSG_WARN << "osgXR: Mode " << mode << " chosen" << std::endl;
            chosenMode = mode;
            break;
        }
        else
        {
            // This mode can't be supported
            OSG_WARN << "osgXR: Mode " << mode << " rejected:" << std::endl;
            for (const char *error: errors)
                OSG_WARN << "    " << error << std::endl;
        }
    }

    *outVRMode = chosenMode.getVRMode();
    *outSwapchainMode = chosenMode.getSwapchainMode();
}

static void applyDefaultRGBEncoding(uint32_t &preferredRGBEncodingMask,
                                    uint32_t &allowedRGBEncodingMask)
{
    if (!allowedRGBEncodingMask)
    {
        // Play safe and default to preferring sRGB over linear/float RGB, since
        // this is what apps are normally tuned for. This avoids incorrect
        // behaviour in SteamVR (no gamma correction) and also correct but
        // potentially unexpected behaviour in Monado (extra gamma correction of
        // linear RGB framebuffer when app produces sRGBish images already).
        allowedRGBEncodingMask = 1u << Settings::ENCODING_SRGB;
    }
    if (!preferredRGBEncodingMask)
    {
        // If no preferred RGB encodings, mark all allowed ones as preferred.
        preferredRGBEncodingMask = allowedRGBEncodingMask;
    }
}

static void applyDefaultDepthEncoding(uint32_t &preferredDepthEncodingMask,
                                      uint32_t &allowedDepthEncodingMask)
{
    if (!allowedDepthEncodingMask)
    {
        // Default to allowing both discrete or floating point depth.
        allowedDepthEncodingMask = 1u << Settings::ENCODING_LINEAR |
                                   1u << Settings::ENCODING_FLOAT;
    }
    if (!preferredDepthEncodingMask)
    {
        // If no preferred depth encodings, mark all allowed ones as preferred.
        preferredDepthEncodingMask = allowedDepthEncodingMask;
    }
}

int64_t XRState::chooseRGBAFormat(unsigned int bestRGBBits,
                                  unsigned int bestAlphaBits,
                                  uint32_t preferredRGBEncodingMask,
                                  uint32_t allowedRGBEncodingMask) const
{
    applyDefaultRGBEncoding(preferredRGBEncodingMask,
                            allowedRGBEncodingMask);

    // Choose a swapchain format
    int64_t chosenRGBAFormat = 0;
    unsigned int chosenAlphaBits = 0;
    uint32_t chosenRGBSat = 0;
    for (int64_t format: _session->getSwapchainFormats())
    {
        auto thisEncoding = Settings::ENCODING_LINEAR;
        uint32_t encodingMask = 0;
        unsigned int thisRGBBits = 0;
        unsigned int thisAlphaBits = 0;
        unsigned int thisSat = 0;
        switch (format)
        {
            // Discrete linear RGB(A)
            case GL_RGBA16:
                thisRGBBits = 16 * 3;
                thisAlphaBits = 16;
                goto handleRGBA;
            case GL_RGB10_A2:
                thisRGBBits = 10 * 3;
                thisAlphaBits = 2;
                goto handleRGBA;
            case GL_RGBA8:
                thisRGBBits = 8 * 3;
                thisAlphaBits = 8;
                goto handleRGBA;
            // Linear floating point RGB(A)
            case GL_RGB16F_ARB:
                thisRGBBits = 16 * 3;
                thisEncoding = Settings::ENCODING_FLOAT;
                goto handleRGBA;
            case GL_RGBA16F_ARB:
                thisRGBBits = 16 * 3;
                thisEncoding = Settings::ENCODING_FLOAT;
                thisAlphaBits = 16;
                goto handleRGBA;
            // Discrete sRGB (linear A)
            case GL_SRGB8_ALPHA8:
                thisEncoding = Settings::ENCODING_SRGB;
                thisAlphaBits = 8;
                goto handleRGBA;
            case GL_SRGB8:
                thisEncoding = Settings::ENCODING_SRGB;
                goto handleRGBA;
            handleRGBA:
                // Don't even consider a disallowed RGB encoding
                encodingMask = (1u << (unsigned int)thisEncoding);
                if (!(allowedRGBEncodingMask & encodingMask))
                    break;

                // Consider whether our preferences are satisfied
                if (preferredRGBEncodingMask & encodingMask)
                    thisSat |= 0x1;
                if (thisEncoding == Settings::ENCODING_SRGB || thisRGBBits >= bestRGBBits)
                    thisSat |= 0x2;
                if (thisAlphaBits >= bestAlphaBits)
                    thisSat |= 0x4;

                // Skip formats that no longer satisfies some preference
                if (chosenRGBSat & ~thisSat)
                    break;

                // Decide whether to choose this format
                if (// Anything is better than nothing
                    !chosenRGBAFormat ||
                    // New preferences satisfied is always better
                    (~chosenRGBSat & thisSat) ||
                    // All else being equal, allow improved alpha bits
                    // A higher number of alpha bits is better than not enough
                    (thisAlphaBits > chosenAlphaBits && chosenAlphaBits < bestAlphaBits))
                {
                    chosenRGBAFormat = format;
                    chosenAlphaBits = thisAlphaBits;
                    chosenRGBSat = thisSat;
                }
                break;

            default:
                break;
        }
    }
    return chosenRGBAFormat;
}

GLenum XRState::chooseFallbackDepthFormat(unsigned int bestDepthBits,
                                          unsigned int bestStencilBits,
                                          uint32_t preferredDepthEncodingMask,
                                          uint32_t allowedDepthEncodingMask) const
{
    applyDefaultDepthEncoding(preferredDepthEncodingMask,
                              allowedDepthEncodingMask);

    if (preferredDepthEncodingMask & (1u << (unsigned int)Settings::ENCODING_LINEAR))
    {
        bool allowFloatDepth = allowedDepthEncodingMask & (1u << (unsigned int)Settings::ENCODING_FLOAT);
        if (bestDepthBits > 24 && allowFloatDepth)
            return bestStencilBits ? GL_DEPTH32F_STENCIL8
                                   : GL_DEPTH_COMPONENT32F;
        else if (bestStencilBits)
            return GL_DEPTH24_STENCIL8_EXT;
        else if (bestDepthBits > 16)
            return GL_DEPTH_COMPONENT24;
        else
            return GL_DEPTH_COMPONENT16;
    }
    else // preferredDepthEncodingMask & (1 << ENCODING_FLOAT)
    {
        if (bestStencilBits)
            return GL_DEPTH32F_STENCIL8;
        else
            return GL_DEPTH_COMPONENT32F;
    }
}

int64_t XRState::chooseDepthFormat(unsigned int bestDepthBits,
                                   unsigned int bestStencilBits,
                                   uint32_t preferredDepthEncodingMask,
                                   uint32_t allowedDepthEncodingMask) const
{
    applyDefaultDepthEncoding(preferredDepthEncodingMask,
                              allowedDepthEncodingMask);

    // Choose a swapchain format
    int64_t chosenDepthFormat = 0;
    unsigned int chosenDepthBits = 0;
    unsigned int chosenStencilBits = 0;
    uint32_t chosenDepthSat = 0;
    for (int64_t format: _session->getSwapchainFormats())
    {
        auto thisEncoding = Settings::ENCODING_LINEAR;
        uint32_t encodingMask = 0;
        unsigned int thisDepthBits = 0;
        unsigned int thisStencilBits = 0;
        unsigned int thisSat = 0;
        switch (format)
        {
            // Discrete depth (stencil)
            case GL_DEPTH_COMPONENT16:
                thisDepthBits = 16;
                goto handleDepth;
            case GL_DEPTH_COMPONENT24:
                thisDepthBits = 24;
                goto handleDepth;
#if 0 // crashes nvidia (495.46, with monado)
            case GL_DEPTH24_STENCIL8_EXT:
                thisDepthBits = 24;
                thisStencilBits = 8;
                goto handleDepth;
#endif
            case GL_DEPTH_COMPONENT32:
                thisDepthBits = 32;
                goto handleDepth;
            // Floating point depth, discrete stencil
            case GL_DEPTH_COMPONENT32F:
                thisEncoding = Settings::ENCODING_FLOAT;
                thisDepthBits = 32;
                goto handleDepth;
            case GL_DEPTH32F_STENCIL8:
                thisEncoding = Settings::ENCODING_FLOAT;
                thisDepthBits = 32;
                thisStencilBits = 8;
                goto handleDepth;

            handleDepth:
                // Don't even consider a disallowed depth encoding
                encodingMask = (1u << (unsigned int)thisEncoding);
                if (!(allowedDepthEncodingMask & encodingMask))
                    break;

                // Consider whether our preferences are satisfied
                if (preferredDepthEncodingMask & encodingMask)
                    thisSat |= 0x1;
                if (thisDepthBits >= bestDepthBits)
                    thisSat |= 0x2;
                if (thisStencilBits >= bestStencilBits)
                    thisSat |= 0x4;

                // Skip formats that no longer satisfies some preference
                if (chosenDepthSat & ~thisSat)
                    break;

                if (// Anything is better than nothing
                    !chosenDepthFormat ||
                    // New preferences satisfied is always better
                    (~chosenDepthSat & thisSat) ||
                    // A higher number of depth bits is better than not enough
                    (thisDepthBits > chosenDepthBits && chosenDepthBits < bestDepthBits) ||
                    // A higher number of stencil bits is better than not enough
                    // so long as depth bits are no worse or good enough
                    ((thisDepthBits >= chosenDepthBits || thisDepthBits >= bestDepthBits) &&
                     thisStencilBits > chosenStencilBits && chosenStencilBits < bestStencilBits) ||
                    // A lower number of depth bits may still be enough
                    // so long as stencil bits are no worse or good enough
                    ((thisStencilBits >= chosenStencilBits || thisStencilBits >= bestStencilBits) &&
                     bestDepthBits < thisDepthBits && thisDepthBits < chosenDepthBits))
                {
                    chosenDepthFormat = format;
                    chosenDepthBits = thisDepthBits;
                    chosenStencilBits = thisStencilBits;
                    chosenDepthSat = thisSat;
                }
                break;

            default:
                break;
        }
    }
    return chosenDepthFormat;
}

bool XRState::setupSingleSwapchain(int64_t format, int64_t depthFormat,
                                   GLenum fallbackDepthFormat)
{
    const auto &views = _chosenViewConfig->getViews();

    // Arrange viewports on a single swapchain image
    OpenXR::System::ViewConfiguration::View singleView;
    std::vector<OpenXR::System::ViewConfiguration::View::Viewport> viewports;
    viewports.resize(views.size());
    for (uint32_t i = 0; i < views.size(); ++i) {
        OpenXR::System::ViewConfiguration::View view = views[i];
        view.alignSize(_settings->getViewAlignmentMask());
        viewports[i] = singleView.tileHorizontally(view);
    }

    // Create a single swapchain
    osg::ref_ptr<XRSwapchain> xrSwapchain = new XRSwapchain(this, _session,
                                                            singleView, format,
                                                            depthFormat,
                                                            fallbackDepthFormat);
    if (!xrSwapchain->valid()) {
        OSG_WARN << "osgXR: Invalid single swapchain" << std::endl;
        return false; // failure
    }

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

bool XRState::setupLayeredSwapchain(int64_t format, int64_t depthFormat,
                                    GLenum fallbackDepthFormat)
{
    const auto &views = _chosenViewConfig->getViews();
    _xrViews.reserve(views.size());

    // Arrange viewports on a single layered swapchain image
    OpenXR::System::ViewConfiguration::View layeredView;
    std::vector<OpenXR::System::ViewConfiguration::View::Viewport> viewports;
    viewports.resize(views.size());
    for (uint32_t i = 0; i < views.size(); ++i) {
        OpenXR::System::ViewConfiguration::View view = views[i];
        view.alignSize(_settings->getViewAlignmentMask());
        viewports[i] = layeredView.tileLayered(view);
    }

    // Create a single swapchain
    unsigned int fbPerLayer = 0; // An FBO per layer per swapchain image
    if (_vrMode == VRMode::VRMODE_GEOMETRY_SHADERS)
    {
        // Single FBO per swapchain image, gl_Layer specified by geom shader
        fbPerLayer = XRFramebuffer::ARRAY_INDEX_GEOMETRY;
    }
    else if (_vrMode == VRMode::VRMODE_OVR_MULTIVIEW)
    {
        // Single FBO per swapchain image, gl_ViewID_OVR determines layer
        fbPerLayer = XRFramebuffer::ARRAY_INDEX_MULTIVIEW;
    }
    osg::ref_ptr<XRSwapchain> xrSwapchain = new XRSwapchain(this, _session,
                                                            layeredView, format,
                                                            depthFormat,
                                                            fallbackDepthFormat,
                                                            fbPerLayer);
    if (!xrSwapchain->valid()) {
        OSG_WARN << "osgXR: Invalid layered swapchain" << std::endl;
        return false; // failure
    }

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

bool XRState::setupMultipleSwapchains(int64_t format, int64_t depthFormat,
                                      GLenum fallbackDepthFormat)
{
    const auto &views = _chosenViewConfig->getViews();
    _xrViews.reserve(views.size());

    for (uint32_t i = 0; i < views.size(); ++i)
    {
        const auto &vcView = views[i];
        osg::ref_ptr<XRSwapchain> xrSwapchain = new XRSwapchain(this, _session,
                                                                vcView, format,
                                                                depthFormat,
                                                                fallbackDepthFormat);
        if (!xrSwapchain->valid()) {
            OSG_WARN << "osgXR: Invalid swapchain for view " << i << std::endl;
            _xrViews.resize(0);
            return false; // failure
        }
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

void XRState::initAppView(AppView *appView)
{
    // Notify app to create a new view
    if (_manager.valid())
        _manager->doCreateView(appView);
}

void XRState::destroyAppView(AppView *appView)
{
    // Notify app to destroy this view
    if (_manager.valid())
        _manager->doDestroyView(appView);
}

void XRState::setupSlaveCameras()
{
    osg::ref_ptr<osg::GraphicsContext> gc = _window.get();
    osg::Camera *camera = _view.valid() ? _view->getCamera() : nullptr;
    //camera->setName("Main");

    _appViews.resize(_xrViews.size());
    for (uint32_t i = 0; i < _xrViews.size(); ++i)
    {
        AppViewSlaveCams *appView = new AppViewSlaveCams(this, i, _window.get(),
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
                OSG_WARN << "osgXR: Couldn't add slave camera" << std::endl;
                continue;
            }

            // And ensure it gets configured for VR
            appView->addSlave(cam.get(), View::CAM_DEFAULT_BITS);
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
    // Put both XR views in a single SceneView AppView
    uint32_t viewIndices[2] = { 0, 1 };
    AppViewSceneView *appView = new AppViewSceneView(this, viewIndices,
                                                     _window.get(),
                                                     _view.get());
    appView->init();

    _appViews.resize(1);
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
                    OSG_WARN << "osgXR: slave " << slaveCam->getName() << std::endl;
                    _appViews[0]->addSlave(slaveCam);
                }
            }

            if (!_xrViews[0]->getSwapchain()->getNumDrawPasses())
            {
                OSG_WARN << "osgXR: Failed to find suitable slave camera" << std::endl;
                return;
            }
        }
    }
}

void XRState::setupGeomShadersCameras()
{
    // Put all XR views in a single geometry shaders AppView
    std::vector<uint32_t> viewIndices;
    viewIndices.reserve(_xrViews.size());
    for (uint32_t viewIndex = 0; viewIndex < _xrViews.size(); ++viewIndex)
        viewIndices.push_back(viewIndex);

    AppViewGeomShaders *appView = new AppViewGeomShaders(this, viewIndices,
                                                         _window.get(),
                                                         _view.get());
    appView->init();

    _appViews.resize(1);
    _appViews[0] = appView;
}

void XRState::setupOVRMultiviewCameras()
{
    // Put all XR views in a single OVR_multiview AppView
    std::vector<uint32_t> viewIndices;
    viewIndices.reserve(_xrViews.size());
    for (uint32_t viewIndex = 0; viewIndex < _xrViews.size(); ++viewIndex)
        viewIndices.push_back(viewIndex);

    AppViewOVRMultiview *appView = new AppViewOVRMultiview(this, viewIndices,
                                                           _window.get(),
                                                           _view.get());
    appView->init();

    _appViews.resize(1);
    _appViews[0] = appView;
}

void XRState::setupSceneViewVisibilityMasks(osg::Camera *camera,
                                            osg::ref_ptr<osg::MatrixTransform> &transform)
{
    if (!_visibilityMaskProgram.valid()) {
        const char* vertSrc =
            "#version 330\n"
            "void main()\n"
            "{\n"
            "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
            "}\n";
        const char* fragSrc =
            "#version 330\n"
            "void main()\n"
            "{\n"
            "}\n";
        auto* vertShader = new osg::Shader(osg::Shader::VERTEX, vertSrc);
        auto* fragShader = new osg::Shader(osg::Shader::FRAGMENT, fragSrc);
        auto* program = new osg::Program();
        program->addShader(vertShader);
        program->addShader(fragShader);
        program->setName("osgXR VisibilityMask");
        _visibilityMaskProgram = program;
    }
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

    char name[36];
    snprintf(name, sizeof(name), "osgXR VisibilityMask view#%u", viewIndex);
    geode->setName(name);

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

    auto gc = camera->getGraphicsContext();
    if (gc->getState()->getUseVertexAttributeAliasing())
        state->setAttribute(_visibilityMaskProgram);

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
        _projectionLayer->setSpace(frame->getLocalSpace());
    }
}

void XRState::endFrame(osg::FrameStamp *stamp)
{
    osg::ref_ptr<OpenXR::Session::Frame> frame = _frames.getFrame(stamp);
    if (!frame.valid())
    {
        OSG_WARN << "osgXR: OpenXR frame not waited for" << std::endl;
        return;
    }
    if (!frame->hasBegun())
    {
        OSG_WARN << "osgXR: OpenXR frame not begun" << std::endl;
        _frames.killFrame(stamp);
        return;
    }
    for (auto &view: _xrViews)
        view->endFrame(frame);
    frame->setEnvBlendMode(_chosenEnvBlendMode);
    for (auto *layer: _compositionLayers)
    {
        if (layer->getOrder() >= 0)
            break;
        if (layer->getVisible())
            layer->endFrame(frame);
    }
    frame->addLayer(_projectionLayer.get());
    for (auto *layer: _compositionLayers)
        if (layer->getOrder() >= 0 && layer->getVisible())
            layer->endFrame(frame);
    _frames.endFrame(stamp);
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

void XRState::initialDrawCallback(osg::RenderInfo &renderInfo,
                                  View::Flags flags)
{
    if (flags & View::CAM_TOXR_BIT)
    {
        osg::GraphicsOperation *graphicsOperation = renderInfo.getCurrentCamera()->getRenderer();
        osgViewer::Renderer *renderer = dynamic_cast<osgViewer::Renderer*>(graphicsOperation);
        if (renderer != nullptr)
        {
            // Disable normal OSG FBO camera setup because it will undo the MSAA FBO configuration.
            renderer->setCameraRequiresSetUp(false);
        }
    }

    if (flags & View::CAM_MVR_SCENE_BIT)
    {
        startRendering(renderInfo.getState()->getFrameStamp());

        // Get up to date depth info from camera's projection matrix
        _depthInfo.setZRangeFromProjection(renderInfo.getCurrentCamera()->getProjectionMatrix());
    }
}

void XRState::releaseGLObjects(osg::State *state)
{
    // Release GL objects managed by the OpenXR session before the GL context is
    // destroyed
    if (_currentState >= VRSTATE_SESSION)
        _session->releaseGLObjects(state);
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
