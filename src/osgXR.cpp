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
        float unitsPerMeter = 0.0f;
        osg::getEnvVar("OSGXR_UNITS_PER_METER", unitsPerMeter);

        osg::ref_ptr<OpenXRDisplay> xr = new OpenXRDisplay(appName, appVersion,
                                                           OpenXRDisplay::HEAD_MOUNTED_DISPLAY);
        xr->preferEnvBlendMode(OpenXRDisplay::OPAQUE);
        if (unitsPerMeter > 0.0f)
            xr->setUnitsPerMeter(unitsPerMeter);
        xr->setValidationLayer(true);
        viewer->apply(xr);

        // Force single threaded to make sure that no other thread can use the GL context
        viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);

        OSG_WARN << "Setting up VR" << std::endl;
    }
}
