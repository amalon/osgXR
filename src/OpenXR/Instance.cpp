// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "Instance.h"
#include "Session.h"
#include "System.h"

#include <osg/Notify>
#include <osg/Version>
#include <osg/ref_ptr>

#include <cstring>
#include <vector>

#define ENGINE_NAME     "OpenSceneGraph"
#define ENGINE_VERSION  (OPENSCENEGRAPH_MAJOR_VERSION << 16 | \
                         OPENSCENEGRAPH_MINOR_VERSION <<  8 | \
                         OPENSCENEGRAPH_PATCH_VERSION)
#define API_VERSION     XR_MAKE_VERSION(1, 0, 0)

#define XR_APILAYER_LUNARG_core_validation  "XR_APILAYER_LUNARG_core_validation"

using namespace osgViewer;
using namespace OpenXR;

static std::vector<XrApiLayerProperties> layers;
static std::vector<XrExtensionProperties> extensions;

static bool enumerateLayers()
{
    static bool layersEnumerated = false;
    if (layersEnumerated)
    {
        return true;
    }

    // Count layers
    uint32_t layerCount = 0;
    XrResult res = xrEnumerateApiLayerProperties(0, &layerCount, nullptr);
    if (XR_FAILED(res))
    {
        OSG_WARN << "Failed to count OpenXR API layers: " << res << std::endl;
        return false;
    }

    if (layerCount)
    {
        // Allocate memory
        layers.resize(layerCount);
        for (auto &layer: layers)
        {
            layer.type = XR_TYPE_API_LAYER_PROPERTIES;
            layer.next = nullptr;
        }

        // Enumerate layers
        res = xrEnumerateApiLayerProperties(layers.size(), &layerCount, layers.data());
        if (XR_FAILED(res))
        {
            OSG_WARN << "Failed to enumerate " << layerCount
                     << " OpenXR API layers: " << res << std::endl;
            return false;
        }

        // Layers may change at any time
        layers.resize(layerCount);
    }

    layersEnumerated = true;
    return true;
}

static bool enumerateExtensions()
{
    static bool extensionsEnumerated = false;
    if (extensionsEnumerated)
    {
        return true;
    }

    // Count extensions
    uint32_t extensionCount;
    XrResult res = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);
    if (XR_FAILED(res))
    {
        OSG_WARN << "Failed to count OpenXR instance extensions: " << res << std::endl;
        return false;
    }

    if (extensionCount)
    {
        // Allocate memory
        extensions.resize(extensionCount);
        for (auto &extension: extensions)
        {
            extension.type = XR_TYPE_EXTENSION_PROPERTIES;
            extension.next = nullptr;
        }

        // Enumerate extensions
        res = xrEnumerateInstanceExtensionProperties(nullptr, extensions.size(),
                                                     &extensionCount, extensions.data());
        if (XR_FAILED(res))
        {
            OSG_WARN << "Failed to enumerate " << extensionCount
                     << " OpenXR instance extensions: " << res << std::endl;
            return false;
        }

        // Extensions may change (?)
        extensions.resize(extensionCount);
    }

    extensionsEnumerated = true;
    return true;
}

static bool hasLayer(const char *name)
{
  for (auto &layer: layers)
  {
    if (!strncmp(name, layer.layerName, XR_MAX_API_LAYER_NAME_SIZE))
    {
      return true;
    }
  }
  return false;
}

static bool hasExtension(const char *name)
{
  for (auto &extension: extensions)
  {
    if (!strncmp(name, extension.extensionName, XR_MAX_EXTENSION_NAME_SIZE))
    {
      return true;
    }
  }
  return false;
}

Instance *Instance::instance()
{
    static osg::ref_ptr<Instance> s_instance = new Instance();
    return s_instance;
}

Instance::Instance(): 
    _layerValidation(false),
    _instance(XR_NULL_HANDLE)
{
}

Instance::~Instance()
{
    if (_instance != XR_NULL_HANDLE)
    {
        // Delete the systems
        for (System *system: _systems)
        {
            delete system;
        }

        // Destroy the OpenXR instance
        XrResult res = xrDestroyInstance(_instance);
        if (XR_FAILED(res))
        {
            OSG_WARN << "Failed to destroy OpenXR instance" << std::endl;
        }
    }
}

bool Instance::init(const char *appName, uint32_t appVersion)
{
    if (_instance != XR_NULL_HANDLE)
    {
        return true;
    }

    std::vector<const char *> layerNames;
    std::vector<const char *> extensionNames;

    enumerateLayers();
    enumerateExtensions();

    // Enable validation layer if selected
    if (_layerValidation && hasLayer(XR_APILAYER_LUNARG_core_validation))
    {
        layerNames.push_back(XR_APILAYER_LUNARG_core_validation);
    }

    // We need OpenGL support
    if (!hasExtension(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME))
    {
        OSG_WARN << "OpenXR runtime doesn't support XR_KHR_opengl_enable extension" << std::endl;
        return false;
    }
    extensionNames.push_back(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);

    // Create the instance
    XrInstanceCreateInfo info{ XR_TYPE_INSTANCE_CREATE_INFO };
    strncpy(info.applicationInfo.applicationName, appName,
            XR_MAX_APPLICATION_NAME_SIZE - 1);
    info.applicationInfo.applicationVersion = appVersion;
    strncpy(info.applicationInfo.engineName, ENGINE_NAME,
            XR_MAX_ENGINE_NAME_SIZE - 1);
    info.applicationInfo.engineVersion = ENGINE_VERSION;
    info.applicationInfo.apiVersion = API_VERSION;
    info.enabledApiLayerCount = layerNames.size();
    info.enabledApiLayerNames = layerNames.data();
    info.enabledExtensionCount = extensionNames.size();
    info.enabledExtensionNames = extensionNames.data();

    XrResult res = xrCreateInstance(&info, &_instance);
    if (XR_FAILED(res)) {
        OSG_WARN << "Failed to create OpenXR instance: " << res << std::endl;
        return false;
    }

    // Log the runtime properties
    XrInstanceProperties properties;
    properties.type = XR_TYPE_INSTANCE_PROPERTIES;
    properties.next = nullptr;

    if (XR_SUCCEEDED(xrGetInstanceProperties(_instance, &properties)))
    {
        OSG_INFO << "OpenXR Runtime: \"" << properties.runtimeName
                 << "\" version " << XR_VERSION_MAJOR(properties.runtimeVersion)
                 << "." << XR_VERSION_MINOR(properties.runtimeVersion)
                 << "." << XR_VERSION_PATCH(properties.runtimeVersion) << std::endl;
    }

    // Get extension functions
    _xrGetOpenGLGraphicsRequirementsKHR = (PFN_xrGetOpenGLGraphicsRequirementsKHR)getProcAddr("xrGetOpenGLGraphicsRequirementsKHR");

    return true;
}

bool Instance::check(XrResult result, const char *warnMsg) const
{
    if (XR_FAILED(result))
    {
        char resultName[XR_MAX_RESULT_STRING_SIZE];
        if (XR_FAILED(xrResultToString(_instance, result, resultName)))
        {
            OSG_WARN << warnMsg << ": " << result << std::endl;
        }
        else
        {
            OSG_WARN << warnMsg << ": " << resultName << std::endl;
        }
        return false;
    }
    return true;
}

PFN_xrVoidFunction Instance::getProcAddr(const char *name) const
{
    PFN_xrVoidFunction ret = nullptr;
    check(xrGetInstanceProcAddr(_instance, name, &ret),
          "Failed to get OpenXR procedure address");
    return ret;
}

System *Instance::getSystem(XrFormFactor formFactor)
{
    unsigned long ffId = formFactor - 1;
    if (ffId < _systems.size() && _systems[ffId])
        return _systems[ffId];

    XrSystemGetInfo getInfo{ XR_TYPE_SYSTEM_GET_INFO };
    getInfo.formFactor = formFactor;

    XrSystemId systemId;
    if (check(xrGetSystem(_instance, &getInfo, &systemId),
              "Failed to get OpenXR system"))
    {
        if (ffId >= _systems.size())
            _systems.resize(ffId+1, nullptr);

        return _systems[ffId] = new System(this, systemId);
    }

    return nullptr;
}

void Instance::registerSession(Session *session)
{
    _sessions[session->getXrSession()] = session;
}

void Instance::unregisterSession(Session *session)
{
    _sessions.erase(session->getXrSession());
}

void Instance::handleEvent(const XrEventDataBuffer &event)
{
    switch (event.type)
    {
    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
        {
            auto *stateEvent = reinterpret_cast<const XrEventDataSessionStateChanged *>(&event);
            auto it = _sessions.find(stateEvent->session);
            if (it != _sessions.end())
                (*it).second->handleEvent(*stateEvent);

            break;
        }
    default:
        OSG_WARN << "Unhandled OpenXR Event: " << event.type << std::endl;
        break;
    }
}

void Instance::handleEvents()
{
    for (;;) {
        XrEventDataBuffer event;
        event.type = XR_TYPE_EVENT_DATA_BUFFER;
        event.next = nullptr;

        XrResult res = xrPollEvent(_instance, &event);
        if (XR_FAILED(res))
            break;
        if (res == XR_EVENT_UNAVAILABLE)
            break;

        handleEvent(event);
    }
}
