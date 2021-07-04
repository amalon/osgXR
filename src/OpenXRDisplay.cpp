// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/OpenXRDisplay>

#include <osg/ref_ptr>
#include <osgViewer/ViewerBase>

#include "XRState.h"
#include "XRRealizeOperation.h"

using namespace osgXR;

OpenXRDisplay::OpenXRDisplay()
{
}

OpenXRDisplay::OpenXRDisplay(const std::string &appName,
                             uint32_t appVersion,
                             enum FormFactor formFactor):
    _appName(appName),
    _appVersion(appVersion),
    _validationLayer(false),
    _formFactor(formFactor),
    _preferredEnvBlendModeMask(0),
    _allowedEnvBlendModeMask(0),
    _unitsPerMeter(1.0f)
{
}

OpenXRDisplay::OpenXRDisplay(const OpenXRDisplay& rhs,
                             const osg::CopyOp& copyop):
    ViewConfig(rhs,copyop),
    _appName(rhs._appName),
    _appVersion(rhs._appVersion),
    _validationLayer(rhs._validationLayer),
    _formFactor(rhs._formFactor),
    _preferredEnvBlendModeMask(rhs._preferredEnvBlendModeMask),
    _allowedEnvBlendModeMask(rhs._allowedEnvBlendModeMask),
    _unitsPerMeter(rhs._unitsPerMeter)
{
}

OpenXRDisplay::~OpenXRDisplay()
{
}

void OpenXRDisplay::configure(osgViewer::View &view) const
{
    osgViewer::ViewerBase *viewer = dynamic_cast<osgViewer::ViewerBase *>(&view);
    if (!viewer)
        return;

    _state = new XRState(this);
    viewer->setRealizeOperation(new XRRealizeOperation(_state, &view));
}
