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
    _visibilityMask(true),
    _formFactor(HEAD_MOUNTED_DISPLAY),
    _preferredEnvBlendModeMask(0),
    _allowedEnvBlendModeMask(0),
    _vrMode(VRMODE_AUTOMATIC),
    _swapchainMode(SWAPCHAIN_AUTOMATIC),
    _preferredRGBEncodingMask(0),
    _allowedRGBEncodingMask(0),
    _preferredDepthEncodingMask(0),
    _allowedDepthEncodingMask(0),
    _rgbBits(-1),
    _alphaBits(-1),
    _depthBits(-1),
    _stencilBits(-1),
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

unsigned int Settings::_diff(const Settings &other) const
{
    unsigned int ret = DIFF_NONE;
    if (_appName != other._appName ||
        _appVersion != other._appVersion)
        ret |= DIFF_APP_INFO;
    if (_validationLayer != other._validationLayer)
        ret |= DIFF_VALIDATION_LAYER;
    if (_depthInfo != other._depthInfo)
        ret |= DIFF_DEPTH_INFO;
    if (_visibilityMask != other._visibilityMask)
        ret |= DIFF_VISIBILITY_MASK;
    if (_formFactor != other._formFactor)
        ret |= DIFF_FORM_FACTOR;
    if (_preferredEnvBlendModeMask != other._preferredEnvBlendModeMask ||
        _allowedEnvBlendModeMask != other._allowedEnvBlendModeMask)
        ret |= DIFF_BLEND_MODE;
    if (_vrMode != other._vrMode)
        ret |= DIFF_VR_MODE;
    if (_swapchainMode != other._swapchainMode)
        ret |= DIFF_SWAPCHAIN_MODE;
    if (_preferredRGBEncodingMask != other._preferredRGBEncodingMask ||
        _allowedRGBEncodingMask != other._allowedRGBEncodingMask)
        ret |= DIFF_RGB_ENCODING;
    if (_preferredDepthEncodingMask != other._preferredDepthEncodingMask ||
        _allowedDepthEncodingMask != other._allowedDepthEncodingMask)
        ret |= DIFF_DEPTH_ENCODING;
    if (_rgbBits != other._rgbBits)
        ret |= DIFF_RGB_BITS;
    if (_alphaBits != other._alphaBits)
        ret |= DIFF_ALPHA_BITS;
    if (_depthBits != other._depthBits)
        ret |= DIFF_DEPTH_BITS;
    if (_stencilBits != other._stencilBits)
        ret |= DIFF_STENCIL_BITS;
    if (_mirrorSettings != other._mirrorSettings)
        ret |= DIFF_MIRROR;
    if (_unitsPerMeter != other._unitsPerMeter)
        ret |= DIFF_SCALE;
    return ret;
}
