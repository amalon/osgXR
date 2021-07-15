// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/OpenXRDisplay>
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
        std::string value;

        OpenXRDisplay::VRMode vrMode = OpenXRDisplay::VRMODE_AUTOMATIC;
        if (osg::getEnvVar("OSGXR_MODE", value))
        {
            if (value == "SLAVE_CAMERAS")
                vrMode = OpenXRDisplay::VRMODE_SLAVE_CAMERAS;
        }

        OpenXRDisplay::SwapchainMode swapchainMode = OpenXRDisplay::SWAPCHAIN_AUTOMATIC;
        if (osg::getEnvVar("OSGXR_SWAPCHAIN", value))
        {
            if (value == "MULTIPLE")
                swapchainMode = OpenXRDisplay::SWAPCHAIN_MULTIPLE;
            else if (value == "SINGLE")
                swapchainMode = OpenXRDisplay::SWAPCHAIN_SINGLE;
        }

        float unitsPerMeter = 0.0f;
        osg::getEnvVar("OSGXR_UNITS_PER_METER", unitsPerMeter);

        osg::ref_ptr<OpenXRDisplay> xr = new OpenXRDisplay(appName, appVersion,
                                                           OpenXRDisplay::HEAD_MOUNTED_DISPLAY);
        xr->preferEnvBlendMode(OpenXRDisplay::OPAQUE);
        if (unitsPerMeter > 0.0f)
            xr->setUnitsPerMeter(unitsPerMeter);
        xr->setVRMode(vrMode);
        xr->setSwapchainMode(swapchainMode);
        xr->setValidationLayer(true);
        viewer->apply(xr);

        OSG_WARN << "Setting up VR" << std::endl;
    }
}
