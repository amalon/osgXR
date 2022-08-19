// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/MirrorSettings>
#include <osgXR/OpenXRDisplay>
#include <osgXR/Settings>
#include <osgXR/osgXR>

#include <osg/Notify>
#include <osg/os_utils>

using namespace osgXR;

void osgXR::setupViewerDefaults(osgViewer::Viewer *viewer,
                                const std::string &appName,
                                uint32_t appVersion)
{
    unsigned int vr = 0;
    osg::getEnvVar("OSGXR", vr);

    if (vr)
    {
        Settings *settings = Settings::instance();
        MirrorSettings *mirrorSettings = &settings->getMirrorSettings();
        std::string value;

        Settings::VRMode vrMode = Settings::VRMODE_AUTOMATIC;
        if (osg::getEnvVar("OSGXR_MODE", value))
        {
            if (value == "SLAVE_CAMERAS")
                vrMode = Settings::VRMODE_SLAVE_CAMERAS;
            else if (value == "SCENE_VIEW")
                vrMode = Settings::VRMODE_SCENE_VIEW;
        }

        Settings::SwapchainMode swapchainMode = Settings::SWAPCHAIN_AUTOMATIC;
        if (osg::getEnvVar("OSGXR_SWAPCHAIN", value))
        {
            if (value == "MULTIPLE")
                swapchainMode = Settings::SWAPCHAIN_MULTIPLE;
            else if (value == "SINGLE")
                swapchainMode = Settings::SWAPCHAIN_SINGLE;
        }

        float unitsPerMeter = 0.0f;
        osg::getEnvVar("OSGXR_UNITS_PER_METER", unitsPerMeter);

        int validationLayer = 0;
        osg::getEnvVar("OSGXR_VALIDATION_LAYER", validationLayer);

        int depthInfo = 0;
        osg::getEnvVar("OSGXR_DEPTH_INFO", depthInfo);

        MirrorSettings::MirrorMode mirrorMode = MirrorSettings::MIRROR_AUTOMATIC;
        int mirrorViewIndex = -1;
        if (osg::getEnvVar("OSGXR_MIRROR", value))
        {
            if (value == "NONE")
            {
                mirrorMode = MirrorSettings::MIRROR_NONE;
            }
            else if (value == "LEFT")
            {
                mirrorMode = MirrorSettings::MIRROR_SINGLE;
                mirrorViewIndex = 0;
            }
            else if (value == "RIGHT")
            {
                mirrorMode = MirrorSettings::MIRROR_SINGLE;
                mirrorViewIndex = 1;
            }
            else if (value == "LEFT_RIGHT")
            {
                mirrorMode = MirrorSettings::MIRROR_LEFT_RIGHT;
            }
        }

        settings->setApp(appName, appVersion);
        settings->setFormFactor(Settings::HEAD_MOUNTED_DISPLAY);
        settings->preferEnvBlendMode(Settings::BLEND_MODE_OPAQUE);
        if (unitsPerMeter > 0.0f)
            settings->setUnitsPerMeter(unitsPerMeter);
        settings->setVRMode(vrMode);
        settings->setSwapchainMode(swapchainMode);
        settings->setValidationLayer(!!validationLayer);
        settings->setDepthInfo(!!depthInfo);
        mirrorSettings->setMirror(mirrorMode, mirrorViewIndex);

        osg::ref_ptr<OpenXRDisplay> xr = new OpenXRDisplay(settings);
        viewer->apply(xr);

        OSG_WARN << "Setting up VR" << std::endl;
    }
}
