// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/Settings>

#include <openxr/openxr.h>

#include "OpenXR/Instance.h"

using namespace osgXR;

Settings::Settings() :
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

