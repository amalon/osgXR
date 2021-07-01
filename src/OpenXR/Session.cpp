#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr_platform.h>

#include "Compositor.h"
#include "Session.h"

#include <osg/Notify>

using namespace osgViewer;
using namespace OpenXR;

Session::Session(System *system,
                 osgViewer::GraphicsWindow *window) :
    _window(window),
    _instance(system->getInstance()),
    _system(system),
    _session(XR_NULL_HANDLE),
    _viewConfiguration(nullptr),
    _state(XR_SESSION_STATE_UNKNOWN),
    _ready(false),
    _begun(false),
    _shouldEnd(false),
    _shouldDestroy(false),
    _readSwapchainFormats(false),
    _createdLocalSpace(false),
    _localSpace(XR_NULL_HANDLE)
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

    // FIXME implement graphics bindings

    if (check(xrCreateSession(getXrInstance(), &createInfo, &_session),
              "Failed to create OpenXR session"))
    {
        _instance->registerSession(this);
    }
}

Session::~Session()
{
    if (_begun)
        end();

    if (_session != XR_NULL_HANDLE)
    {
        _instance->unregisterSession(this);
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

XrSpace Session::getLocalSpace() const
{
    if (!_createdLocalSpace) {
        static XrPosef poseIdentity = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } };

        XrReferenceSpaceCreateInfo createInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
        createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        createInfo.poseInReferenceSpace = poseIdentity;

        check(xrCreateReferenceSpace(_session, &createInfo, &_localSpace),
              "Failed to create OpenXR local space");

        _createdLocalSpace = true;
    }

    return _localSpace;
}

void Session::handleEvent(const XrEventDataSessionStateChanged &event)
{
    _state = event.state;
    switch (_state)
    {
    case XR_SESSION_STATE_READY:
        OSG_WARN << "OpenXR session ready" << std::endl;
        _ready = true;
        break;
    case XR_SESSION_STATE_EXITING:
    case XR_SESSION_STATE_LOSS_PENDING:
        _shouldDestroy = true;
        _ready = false;
        OSG_WARN << "OpenXR session should be destroyed" << std::endl;
        break;
    case XR_SESSION_STATE_STOPPING:
        _shouldEnd = true;
        _ready = false;
        OSG_WARN << "OpenXR session should end" << std::endl;
        break;
    default:
        break;
    }
}

void Session::makeCurrent() const
{
    _window->makeCurrentImplementation();
}

bool Session::begin(const System::ViewConfiguration &viewConfiguration)
{
    _viewConfiguration = &viewConfiguration;

    XrSessionBeginInfo beginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
    beginInfo.primaryViewConfigurationType = viewConfiguration.getType();
    if (check(xrBeginSession(_session, &beginInfo),
              "Failed to begin OpenXR session"))
    {
        _begun = true;
        return true;
    }
    return false;
}

void Session::end()
{
    check(xrEndSession(_session),
          "Failed to end OpenXR session");
    _begun = false;
    _viewConfiguration = nullptr;
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
    locateInfo.space = _session->getLocalSpace();

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
    return check(xrEndFrame(_session->getXrSession(), &frameEndInfo),
                 "Failed to end OpenXR frame");
}
