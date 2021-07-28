// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/Settings>

#include <openxr/openxr.h>

#include "OpenXR/Instance.h"

using namespace osgXR;

Settings::Settings() :
    _probed(false),
    _appName("osgXR"),
    _appVersion(1),
    _validationLayer(false),
    _depthInfo(false),
    _formFactor(HEAD_MOUNTED_DISPLAY),
    _preferredEnvBlendModeMask(0),
    _allowedEnvBlendModeMask(0),
    _vrMode(VRMODE_AUTOMATIC),
    _swapchainMode(SWAPCHAIN_AUTOMATIC),
    _unitsPerMeter(1.0f)
{
}

Settings::~Settings()
{
}

Settings *Settings::instance()
{
    static osg::ref_ptr<Settings> settings = new Settings();
    return settings;
}

void Settings::probe() const
{
    _hasValidationLayer = OpenXR::Instance::hasLayer(XR_APILAYER_LUNARG_core_validation);
    _hasDepthInfoExtension = OpenXR::Instance::hasExtension(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);

    _probed = true;
}

bool Settings::hasValidationLayer() const
{
    if (!_probed)
        probe();
    return _hasValidationLayer;
}

bool Settings::hasDepthInfoExtension() const
{
    if (!_probed)
        probe();
    return _hasDepthInfoExtension;
}

bool Settings::present() const
{
    OpenXR::Instance *instance = OpenXR::Instance::instance();
    return instance->valid();
}

const char *Settings::runtimeName() const
{
    OpenXR::Instance *instance = OpenXR::Instance::instance();
    if (instance->valid())
        return instance->getRuntimeName();
    else
        return "";
}
