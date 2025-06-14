// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "System.h"

#include <cstring>

using namespace osgXR::OpenXR;

void System::getProperties() const
{
    XrSystemProperties properties;
    properties.type = XR_TYPE_SYSTEM_PROPERTIES;
    properties.next = nullptr;

    XrSystemUserPresencePropertiesEXT userPresenceProperties;
    bool instanceUserPresence = _instance->isExtensionEnabled(XR_EXT_USER_PRESENCE_EXTENSION_NAME);
    if (instanceUserPresence)
    {
        userPresenceProperties.type = XR_TYPE_SYSTEM_USER_PRESENCE_PROPERTIES_EXT;
        userPresenceProperties.next = properties.next;
        properties.next = &userPresenceProperties;
    }

    if (check(xrGetSystemProperties(getXrInstance(), _systemId, &properties),
              "get OpenXR system properties"))
    {
        memcpy(_systemName, properties.systemName, sizeof(_systemName));
        _orientationTracking = properties.trackingProperties.orientationTracking;
        _positionTracking = properties.trackingProperties.positionTracking;
        if (instanceUserPresence)
            _userPresence = userPresenceProperties.supportsUserPresence;
    }

    _readProperties = true;
}

const System::ViewConfiguration::Views &System::ViewConfiguration::getViews() const
{
    if (!_readViews)
    {
        uint32_t viewCount = 0;
        if (check(xrEnumerateViewConfigurationViews(_system->getXrInstance(),
                                                    _system->getXrSystemId(),
                                                    _type,
                                                    0, &viewCount, nullptr),
                  "count OpenXR view configuration views"))
        {
            if (viewCount)
            {
                std::vector<XrViewConfigurationView> views(viewCount,
                                        { XR_TYPE_VIEW_CONFIGURATION_VIEW });
                if (check(xrEnumerateViewConfigurationViews(_system->getXrInstance(),
                                                            _system->getXrSystemId(),
                                                            _type,
                                                            views.size(), &viewCount,
                                                            views.data()),
                          "enumerate OpenXR view configuration views"))
                {
                    for (auto &view: views)
                        _views.push_back(View(view));
                }
            }
        }

        _readViews = true;
    }

    return _views;
}

const System::ViewConfiguration::EnvBlendModes &System::ViewConfiguration::getEnvBlendModes() const
{
    if (!_readEnvBlendModes)
    {
        uint32_t blendModeCount = 0;
        if (check(xrEnumerateEnvironmentBlendModes(_system->getXrInstance(),
                                                   _system->getXrSystemId(),
                                                   _type,
                                                   0, &blendModeCount, nullptr),
                  "count OpenXR environment blend modes"))
        {
            if (blendModeCount)
            {
                _envBlendModes.resize(blendModeCount);
                if (!check(xrEnumerateEnvironmentBlendModes(_system->getXrInstance(),
                                                           _system->getXrSystemId(),
                                                           _type,
                                                           _envBlendModes.size(),
                                                           &blendModeCount,
                                                           _envBlendModes.data()),
                          "enumerate OpenXR environment blend modes"))
                {
                    _envBlendModes.resize(0);
                }
            }
        }

        _readEnvBlendModes = true;
    }

    return _envBlendModes;
}

const System::ViewConfigurations &System::getViewConfigurations() const
{
    if (!_readViewConfigurations)
    {
        uint32_t viewConfigCount = 0;
        if (check(xrEnumerateViewConfigurations(getXrInstance(), getXrSystemId(),
                                                0, &viewConfigCount, nullptr),
                  "count OpenXR view configuration types"))
        {
            if (viewConfigCount)
            {
                std::vector<XrViewConfigurationType> types(viewConfigCount);
                if (check(xrEnumerateViewConfigurations(getXrInstance(), getXrSystemId(),
                                                        types.size(), &viewConfigCount,
                                                        types.data()),
                          "enumerate OpenXR view configuration types"))
                {
                    _viewConfigurations.reserve(viewConfigCount);
                    for (auto type: types)
                        _viewConfigurations.push_back(ViewConfiguration(this, type));
                }
            }
        }

        _readViewConfigurations = true;
    }

    return _viewConfigurations;
}
