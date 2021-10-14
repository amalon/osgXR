// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr_platform.h>

#include "Compositor.h"
#include "Session.h"
#include "GraphicsBinding.h"

#include <osg/Notify>

#include <osgViewer/api/X11/GraphicsWindowX11>

#include <GL/glx.h>

#include <vector>

using namespace osgXR::OpenXR;

Session::Session(System *system,
                 osgViewer::GraphicsWindow *window) :
    _window(window),
    _instance(system->getInstance()),
    _system(system),
    _session(XR_NULL_HANDLE),
    _viewConfiguration(nullptr),
    _state(XR_SESSION_STATE_UNKNOWN),
    _running(false),
    _exiting(false),
    _readSwapchainFormats(false)
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
    if (check(xrCreateSession(getXrInstance(), &createInfo, &_session),
              "Failed to create OpenXR session"))
    {
        _instance->registerSession(this);
    }
}

Session::~Session()
{
    if (_session != XR_NULL_HANDLE)
    {
        _instance->unregisterSession(this);
        _localSpace = nullptr;
        // GL context must not be bound in another thread
        check(xrDestroySession(_session),
              "Failed to destroy OpenXR session");
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
    // FIXME ugly hack, X11 specific
    const auto *window = dynamic_cast<const osgViewer::GraphicsWindowX11*>(_window.get());
    return glXGetCurrentContext() == window->getContext();
}

void Session::makeCurrent() const
{
    _window->makeCurrentImplementation();
}

void Session::releaseContext() const
{
    _window->releaseContextImplementation();
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
    osg::ref_ptr<Frame> frame;

    XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
    XrFrameState frameState;
    frameState.type = XR_TYPE_FRAME_STATE;
    frameState.next = nullptr;
    if (check(xrWaitFrame(_session, &frameWaitInfo, &frameState),
              "Failed to wait for OpenXR frame"))
    {
        frame = new Frame(this, &frameState);
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

    bool currentSet = _session->checkCurrent();
    bool ret = check(xrEndFrame(_session->getXrSession(), &frameEndInfo),
                 "Failed to end OpenXR frame");

    // TODO: should not be necessary, but is for SteamVR 1.16.4 (but not 1.15.x)
    if (currentSet)
        _session->makeCurrent();

    return ret;
}
