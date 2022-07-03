// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "EventHandler.h"
#include "Instance.h"
#include "Session.h"
#include "System.h"
#include "generated/Version.h"

#include <osg/Notify>
#include <osg/Version>
#include <osg/ref_ptr>

#include <cstring>
#include <vector>

#define ENGINE_NAME     "osgXR"
#define ENGINE_VERSION  (OSGXR_MAJOR_VERSION << 16 | \
                         OSGXR_MINOR_VERSION <<  8 | \
                         OSGXR_PATCH_VERSION)
#define API_VERSION     XR_MAKE_VERSION(1, 0, 0)

using namespace osgXR::OpenXR;

static std::vector<XrApiLayerProperties> layers;
static std::vector<XrExtensionProperties> extensions;

static bool enumerateLayers(bool invalidate = false)
{
    static bool layersEnumerated = false;
    if (invalidate)
    {
        layers.resize(0);
        layersEnumerated = false;
        return false;
    }
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

static bool enumerateExtensions(bool invalidate = false)
{
    static bool extensionsEnumerated = false;
    if (invalidate)
    {
        extensions.resize(0);
        extensionsEnumerated = false;
        return false;
    }
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

void Instance::invalidateLayers()
{
    enumerateLayers(true);
}

void Instance::invalidateExtensions()
{
    enumerateExtensions(true);
}

bool Instance::hasLayer(const char *name)
{
    enumerateLayers();

    for (auto &layer: layers)
    {
        if (!strncmp(name, layer.layerName, XR_MAX_API_LAYER_NAME_SIZE))
        {
            return true;
        }
    }
    return false;
}

bool Instance::hasExtension(const char *name)
{
    enumerateExtensions();

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
    _depthInfo(false),
    _visibilityMask(true),
    _instance(XR_NULL_HANDLE),
    _lost(false)
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

Instance::InitResult Instance::init(const char *appName, uint32_t appVersion)
{
    if (_instance != XR_NULL_HANDLE)
    {
        return INIT_SUCCESS;
    }

    std::vector<const char *> layerNames;
    std::vector<const char *> extensionNames;

    // Enable validation layer if selected
    if (_layerValidation && hasLayer(XR_APILAYER_LUNARG_core_validation))
    {
        layerNames.push_back(XR_APILAYER_LUNARG_core_validation);
    }

    // We need OpenGL support
    if (!hasExtension(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME))
    {
        OSG_WARN << "OpenXR runtime doesn't support XR_KHR_opengl_enable extension" << std::endl;
        return INIT_FAIL;
    }
    extensionNames.push_back(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);

    // Enable depth composition layer support if supported
    _supportsCompositionLayerDepth = hasExtension(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
    if (_depthInfo)
    {
        if (_supportsCompositionLayerDepth)
            extensionNames.push_back(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
        else
            _depthInfo = false;
    }

    // Enable visibility mask support if supported
    _supportsVisibilityMask = hasExtension(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME);
    if (_visibilityMask)
    {
        if (_supportsVisibilityMask)
            extensionNames.push_back(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME);
        else
            _visibilityMask = false;
    }

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
    if (XR_FAILED(res))
    {
        OSG_WARN << "Failed to create OpenXR instance: " << res << std::endl;
        switch (res)
        {
        case XR_ERROR_RUNTIME_UNAVAILABLE:
        case XR_ERROR_RUNTIME_FAILURE: // Monado returns this when not running
            return INIT_LATER;

        default:
            return INIT_FAIL;
        }
    }

    // Log the runtime properties
    _properties.type = XR_TYPE_INSTANCE_PROPERTIES;
    _properties.next = nullptr;

    if (XR_SUCCEEDED(xrGetInstanceProperties(_instance, &_properties)))
    {
        OSG_INFO << "OpenXR Runtime: \"" << _properties.runtimeName
                 << "\" version " << XR_VERSION_MAJOR(_properties.runtimeVersion)
                 << "." << XR_VERSION_MINOR(_properties.runtimeVersion)
                 << "." << XR_VERSION_PATCH(_properties.runtimeVersion) << std::endl;
        _quirks.probe(this);
    }

    // Get extension functions
    _xrGetOpenGLGraphicsRequirementsKHR = (PFN_xrGetOpenGLGraphicsRequirementsKHR)getProcAddr("xrGetOpenGLGraphicsRequirementsKHR");
    if (_visibilityMask)
        _xrGetVisibilityMaskKHR = (PFN_xrGetVisibilityMaskKHR)getProcAddr("xrGetVisibilityMaskKHR");

    return INIT_SUCCESS;
}

bool Instance::check(XrResult result, const char *warnMsg) const
{
    if (XR_FAILED(result))
    {
        if (result == XR_ERROR_INSTANCE_LOST)
            _lost = true;

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

System *Instance::getSystem(XrFormFactor formFactor, bool *supported)
{
    unsigned long ffId = formFactor - 1;
    if (ffId < _systems.size() && _systems[ffId])
    {
        if (supported)
            *supported = true;
        return _systems[ffId];
    }

    XrSystemGetInfo getInfo{ XR_TYPE_SYSTEM_GET_INFO };
    getInfo.formFactor = formFactor;

    XrSystemId systemId;
    XrResult res = xrGetSystem(_instance, &getInfo, &systemId);
    if (res == XR_ERROR_FORM_FACTOR_UNAVAILABLE)
    {
        // The system is only *TEMPORARILY* unavailable
        if (supported)
            *supported = true;
        return nullptr;
    }
    else if (check(res, "Failed to get OpenXR system"))
    {
        if (ffId >= _systems.size())
            _systems.resize(ffId+1, nullptr);

        if (supported)
            *supported = true;
        return _systems[ffId] = new System(this, systemId);
    }

    if (supported)
        *supported = false;
    return nullptr;
}

void Instance::invalidateSystem(XrFormFactor formFactor)
{
    unsigned long ffId = formFactor - 1;
    if (ffId < _systems.size())
    {
        delete _systems[ffId];
        _systems[ffId] = nullptr;
    }
}

void Instance::registerSession(Session *session)
{
    _sessions[session->getXrSession()] = session;
}

void Instance::unregisterSession(Session *session)
{
    _sessions.erase(session->getXrSession());
}

Session *Instance::getSession(XrSession xrSession)
{
    auto it = _sessions.find(xrSession);
    if (it == _sessions.end())
        return nullptr;
    return (*it).second;
}

void Instance::pollEvents(EventHandler *handler)
{
    for (;;)
    {
        XrEventDataBuffer event;
        event.type = XR_TYPE_EVENT_DATA_BUFFER;
        event.next = nullptr;

        XrResult res = xrPollEvent(_instance, &event);
        if (XR_FAILED(res))
            break;
        if (res == XR_EVENT_UNAVAILABLE)
            break;

        handler->onEvent(this, &event);
    }
}
