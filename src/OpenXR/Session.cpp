// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr_platform.h>

#include "ActionSet.h"
#include "Compositor.h"
#include "Session.h"
#include "GraphicsBinding.h"

#include <osg/Notify>

#include <cassert>
#include <vector>

#ifdef OSGXR_USE_X11
#include <osgViewer/api/X11/GraphicsWindowX11>
#include <GL/glx.h>
#endif // OSGXR_USE_X11

using namespace osgXR::OpenXR;

Session::Session(System *system,
                 osgViewer::GraphicsWindow *window) :
    _window(window),
    _instance(system->getInstance()),
    _system(system),
    _session(XR_NULL_HANDLE),
    _viewConfiguration(nullptr),
    _actionSyncCount(0),
    _state(XR_SESSION_STATE_UNKNOWN),
    _running(false),
    _exiting(false),
    _readSwapchainFormats(false),
    _lastDisplayTime(0)
{
    XrSessionCreateInfo createInfo = { XR_TYPE_SESSION_CREATE_INFO };
    createInfo.systemId = getXrSystemId();

    // Get OpenGL graphics requirements
    XrGraphicsRequirementsOpenGLKHR req;
    req.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
    req.next = nullptr;
    check(_instance->getOpenGLGraphicsRequirements(getXrSystemId(), &req),
            "Failed to get OpenXR's OpenGL graphics requirements");
    // ... and pretty much ignore what it says

    osg::ref_ptr<GraphicsBinding> graphicsBinding = createGraphicsBinding(window);
    if (graphicsBinding == nullptr)
    {
        OSG_WARN << "Failed to get OpenXR graphics binding" << std::endl;
        return;
    }

    createInfo.next = graphicsBinding->getXrGraphicsBinding();

    // GL context must not be bound in another thread
    bool switchContext = shouldSwitchContext();
    if (switchContext)
        makeCurrent();
    if (check(xrCreateSession(getXrInstance(), &createInfo, &_session),
              "Failed to create OpenXR session"))
    {
        _instance->registerSession(this);
    }
    if (switchContext)
        releaseContext();
}

Session::~Session()
{
    releaseGLObjects();
}

void Session::releaseGLObjects(osg::State *state)
{
    if (_session != XR_NULL_HANDLE)
    {
        _instance->unregisterSession(this);
        _localSpace = nullptr;
        // GL context must not be bound in another thread
        check(xrDestroySession(_session),
              "Failed to destroy OpenXR session");
        _session = XR_NULL_HANDLE;
        _running = false;
    }
}

void Session::addActionSet(ActionSet *actionSet)
{
    assert(actionSet->getInstance() == getInstance());
    _actionSets.insert(actionSet);
}

bool Session::attachActionSets()
{
    assert(valid());
    if (_actionSets.empty())
        return false;

    // Construct vector of XrActionSets
    std::vector<XrActionSet> actionSets;
    actionSets.reserve(_actionSets.size());
    for (auto actionSet: _actionSets)
        actionSets.push_back(actionSet->getXrActionSet());

    XrSessionActionSetsAttachInfo attachInfo{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
    attachInfo.countActionSets = actionSets.size();
    attachInfo.actionSets = actionSets.data();

    return check(xrAttachSessionActionSets(_session, &attachInfo),
                 "Failed to attach action sets to OpenXR session");
}

Path Session::getCurrentInteractionProfile(const Path &subactionPath) const
{
    XrInteractionProfileState interactionProfile{ XR_TYPE_INTERACTION_PROFILE_STATE };

    if (check(xrGetCurrentInteractionProfile(_session, subactionPath.getXrPath(),
                                             &interactionProfile),
              "Failed to get OpenXR current interaction profile"))
    {
        return Path(getInstance(), interactionProfile.interactionProfile);
    }
    return Path();
}

bool Session::getActionBoundSources(Action *action,
                                    std::vector<XrPath> &sourcePaths) const
{
    if (!valid())
        return false;

    // Count bound sources
    XrBoundSourcesForActionEnumerateInfo enumerateInfo{ XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO };
    enumerateInfo.action = action->getXrAction();
    uint32_t count;
    if (check(xrEnumerateBoundSourcesForAction(_session, &enumerateInfo,
                                                0, &count, nullptr),
               "Failed to count OpenXR action bound sources"))
    {
        // Resize output buffer
        sourcePaths.resize(count);
        if (!count)
            return true;

        // Fill buffer
        if (check(xrEnumerateBoundSourcesForAction(_session, &enumerateInfo,
                                                   sourcePaths.size(),
                                                   &count,
                                                   sourcePaths.data()),
                  "Failed to enumerate OpenXR action bound sources"))
        {
            // Success!
            if (count < sourcePaths.size())
                sourcePaths.resize(count);
            return true;
        }
    }

    // Failure!
    return false;
}

std::string Session::getInputSourceLocalizedName(XrPath sourcePath,
                                                 XrInputSourceLocalizedNameFlags whichComponents) const
{
    if (!valid())
        return "";

    XrInputSourceLocalizedNameGetInfo getInfo{ XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO };
    getInfo.sourcePath = sourcePath;
    getInfo.whichComponents = whichComponents;

    uint32_t count;
    if (!check(xrGetInputSourceLocalizedName(_session, &getInfo,
                              0, &count, nullptr),
               "Failed to size OpenXR input source localized name string"))
        return "";
    std::vector<char> buffer(count);
    if (!check(xrGetInputSourceLocalizedName(_session, &getInfo,
                              buffer.size(), &count, buffer.data()),
               "Failed to get OpenXR input source localized name string"))
        return "";

    return buffer.data();
}

void Session::activateActionSet(ActionSet *actionSet, Path subactionPath)
{
    assert(_actionSets.count(actionSet));
    _activeActionSets.insert(ActionSetSubactionPair(actionSet, subactionPath.getXrPath()));
}

void Session::deactivateActionSet(ActionSet *actionSet, Path subactionPath)
{
    _activeActionSets.erase(ActionSetSubactionPair(actionSet, subactionPath.getXrPath()));
}

bool Session::syncActions()
{
    if (!valid())
        return false;

    XrActionsSyncInfo syncInfo{ XR_TYPE_ACTIONS_SYNC_INFO };
    std::vector<XrActiveActionSet> actionSets;
    if (!_activeActionSets.empty())
    {
        // Construct vector of XrActionSets
        actionSets.reserve(_activeActionSets.size());
        for (auto actionSet: _activeActionSets)
        {
            XrActiveActionSet activeActionSet;
            activeActionSet.actionSet = actionSet.first->getXrActionSet();
            activeActionSet.subactionPath = actionSet.second;
            actionSets.push_back(activeActionSet);
        }

        syncInfo.countActiveActionSets = actionSets.size();
        syncInfo.activeActionSets = actionSets.data();

        bool ret = check(xrSyncActions(_session, &syncInfo),
                         "Failed to sync action sets to OpenXR session");
        if (ret)
            ++_actionSyncCount;
        return ret;
    }
    else
    {
        return false;
    }
}

const Session::SwapchainFormats &Session::getSwapchainFormats() const
{
    if (!_readSwapchainFormats && valid())
    {
        uint32_t formatCount;
        if (check(xrEnumerateSwapchainFormats(_session, 0, &formatCount, nullptr),
                  "Failed to count OpenXR swapchain formats"))
        {
            if (formatCount)
            {
                _swapchainFormats.resize(formatCount);
                if (!check(xrEnumerateSwapchainFormats(_session, formatCount,
                                               &formatCount, _swapchainFormats.data()),
                           "Failed to enumerate OpenXR swapchain formats"))
                {
                    _swapchainFormats.resize(0);
                }
            }
        }

        _readSwapchainFormats = true;
    }

    return _swapchainFormats;
}

Space *Session::getLocalSpace()
{
    if (!_localSpace.valid())
        _localSpace = new Space(this, XR_REFERENCE_SPACE_TYPE_LOCAL);

    return _localSpace;
}

void Session::updateVisibilityMasks(XrViewConfigurationType viewConfigurationType,
                                    uint32_t viewIndex)
{
    // Session must be started ...
    if (!_viewConfiguration)
        return;
    // ... and with a matching view configuration
    if (viewConfigurationType != _viewConfiguration->getType())
        return;

    if (viewIndex >= _viewConfiguration->getViews().size())
        return;
    VisMaskGeometryView &visMaskView = _visMaskCache[viewIndex];

    // Regenerate cached visibility mask geometries for this viewIndex
    for (uint32_t visMaskType = 0; visMaskType < visMaskView.size(); ++visMaskType)
        if (visMaskView[visMaskType].valid())
            getVisibilityMask(viewIndex, static_cast<XrVisibilityMaskTypeKHR>(1 + visMaskType), true);
}

osg::ref_ptr<osg::Geometry> Session::getVisibilityMask(uint32_t viewIndex,
                                                       XrVisibilityMaskTypeKHR visibilityMaskType,
                                                       bool force)
{
    if (!_viewConfiguration)
        return nullptr;
    if (viewIndex >= _viewConfiguration->getViews().size())
        return nullptr;
    if (visibilityMaskType == 0 || visibilityMaskType > XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR)
        return nullptr;

    // Size cache to match number of views...
    if (_visMaskCache.size() == 0)
        _visMaskCache.resize(_viewConfiguration->getViews().size());
    // ... and number of vis mask types
    VisMaskGeometryView &visMaskView = _visMaskCache[viewIndex];
    if (visMaskView.size() == 0)
        visMaskView.resize(XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR);
    // Cache hit?
    VisMaskGeometry &visMaskGeometry = visMaskView[visibilityMaskType - 1];
    if (!force && visMaskGeometry.valid())
        return visMaskGeometry;

    // Get counts of visibility mask
    XrVisibilityMaskKHR visibilityMask{ XR_TYPE_VISIBILITY_MASK_KHR };
    XrResult res = xrGetVisibilityMask(*_viewConfiguration, viewIndex,
                                   visibilityMaskType, &visibilityMask);
    if (res != XR_ERROR_FUNCTION_UNSUPPORTED &&
        check(res, "Failed to size OpenXR visibility mask"))
    {
        osg::PrimitiveSet::Mode mode;
        switch (visibilityMaskType)
        {
        case XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR:
            // fall through
        case XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR:
            mode = osg::PrimitiveSet::TRIANGLES;
            break;
        case XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR:
            mode = osg::PrimitiveSet::LINE_LOOP;
            break;
        default:
            return nullptr;
        }

        // Allocate space for data
        osg::ref_ptr<osg::Vec2Array> vertices = new osg::Vec2Array(visibilityMask.vertexCountOutput);
        osg::ref_ptr<osg::DrawElementsUInt> indices = new osg::DrawElementsUInt(mode, visibilityMask.indexCountOutput);

        // Get the actual data
        static_assert(sizeof((*vertices)[0]) == sizeof(XrVector2f));
        static_assert(sizeof((*indices)[0]) == sizeof(uint32_t));
        visibilityMask.vertexCapacityInput = vertices->size();
        visibilityMask.vertices = reinterpret_cast<XrVector2f *>(&vertices->front());
        visibilityMask.indexCapacityInput = indices->size();
        visibilityMask.indices = reinterpret_cast<uint32_t *>(&indices->front());
        XrResult res = xrGetVisibilityMask(*_viewConfiguration, viewIndex,
                                           visibilityMaskType, &visibilityMask);
        if (check(res, "Failed to get OpenXR visibility mask"))
        {
            if (!visMaskGeometry.valid())
            {
                // Create a new geometry object
                osg::Geometry *geometry = new osg::Geometry();
                geometry->setVertexArray(vertices);
                geometry->addPrimitiveSet(indices);
                visMaskGeometry = geometry;
                return geometry;
            }
            else
            {
                // Update the existing geometry object
                osg::Geometry *geometry = visMaskGeometry.get();
                geometry->setVertexArray(vertices);
                geometry->setPrimitiveSet(0, indices);
                return geometry;
            }
        }
    }

    return nullptr;
}

bool Session::checkCurrent() const
{
#ifdef OSGXR_USE_X11
    // Ugly X11 specific hack
    const auto *window = dynamic_cast<const osgViewer::GraphicsWindowX11*>(_window.get());
    return glXGetCurrentContext() == window->getContext();
#else
    return true;
#endif
}

void Session::makeCurrent() const
{
#ifdef OSGXR_USE_X11
    _window->makeCurrentImplementation();
#endif
}

void Session::releaseContext() const
{
#ifdef OSGXR_USE_X11
    _window->releaseContextImplementation();
#endif
}

bool Session::begin(const System::ViewConfiguration &viewConfiguration)
{
    _viewConfiguration = &viewConfiguration;

    XrSessionBeginInfo beginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
    beginInfo.primaryViewConfigurationType = viewConfiguration.getType();
    if (check(xrBeginSession(_session, &beginInfo),
              "Failed to begin OpenXR session"))
    {
        _running = true;
        return true;
    }
    return false;
}

void Session::end()
{
    check(xrEndSession(_session),
          "Failed to end OpenXR session");
    _running = false;
    _viewConfiguration = nullptr;
    _visMaskCache.resize(0);
}

void Session::requestExit()
{
    _exiting = true;
    if (isRunning())
        check(xrRequestExitSession(_session),
              "Failed to request OpenXR exit");
}

osg::ref_ptr<Session::Frame> Session::waitFrame()
{
    if (_instance->lost())
        return nullptr;

    osg::ref_ptr<Frame> frame;

    XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
    XrFrameState frameState;
    frameState.type = XR_TYPE_FRAME_STATE;
    frameState.next = nullptr;
    if (check(xrWaitFrame(_session, &frameWaitInfo, &frameState),
              "Failed to wait for OpenXR frame"))
    {
        frame = new Frame(this, &frameState);
        _lastDisplayTime = frameState.predictedDisplayTime;
    }

    return frame;
}

Session::Frame::Frame(osg::ref_ptr<Session> session, XrFrameState *frameState) :
    _session(session),
    _time(frameState->predictedDisplayTime),
    _period(frameState->predictedDisplayPeriod),
    _shouldRender(frameState->shouldRender),
    _osgFrameNumber(0),
    _locatedViews(false),
    _begun(false),
    _envBlendMode(XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM)
{
}

Session::Frame::~Frame()
{
}

void Session::Frame::locateViews()
{
    // Get view locations
    XrViewLocateInfo locateInfo = { XR_TYPE_VIEW_LOCATE_INFO };
    locateInfo.viewConfigurationType = _session->getViewConfiguration()->getType();
    locateInfo.displayTime = _time;
    locateInfo.space = _session->getLocalSpace()->getXrSpace();

    _viewState = { XR_TYPE_VIEW_STATE };

    uint32_t viewCount;
    if (!check(xrLocateViews(_session->getXrSession(), &locateInfo, &_viewState, 0, &viewCount, nullptr),
               "Failed to count OpenXR views"))
    {
        return;
    }
    _views.resize(viewCount);
    for (auto &view: _views)
        view = { XR_TYPE_VIEW };
    if (!check(xrLocateViews(_session->getXrSession(), &locateInfo, &_viewState, _views.size(), &viewCount, _views.data()),
               "Failed to locate OpenXR views"))
    {
        return;
    }

    _locatedViews = true;
}

void Session::Frame::addLayer(osg::ref_ptr<CompositionLayer> layer)
{
    _layers.push_back(layer);
}

bool Session::Frame::begin()
{
    XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
    return _begun = check(xrBeginFrame(_session->getXrSession(), &frameBeginInfo),
                          "Failed to begin OpenXR frame");
}

bool Session::Frame::end()
{
    std::vector<const XrCompositionLayerBaseHeader *> layers;
    layers.reserve(_layers.size());
    for (auto &layer: _layers)
        layers.push_back(layer->getXr());

    XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
    frameEndInfo.displayTime = _time;
    frameEndInfo.environmentBlendMode = _envBlendMode;
    frameEndInfo.layerCount = layers.size();
    frameEndInfo.layers = layers.data();

    bool restoreContext = _session->shouldRestoreContext();
    bool ret = check(xrEndFrame(_session->getXrSession(), &frameEndInfo),
                 "Failed to end OpenXR frame");

    if (restoreContext)
        _session->makeCurrent();

    return ret;
}
