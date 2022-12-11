// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/OpenXRDisplay>

#include <osg/ref_ptr>
#include <osgViewer/ViewerBase>

#include "XRState.h"
#include "XRRealizeOperation.h"
#include "XRUpdateOperation.h"

using namespace osgXR;

OpenXRDisplay::OpenXRDisplay()
{
}

OpenXRDisplay::OpenXRDisplay(Settings *settings):
    _settings(settings)
{
}

OpenXRDisplay::OpenXRDisplay(const OpenXRDisplay& rhs,
                             const osg::CopyOp& copyop):
    ViewConfig(rhs,copyop),
    _settings(rhs._settings)
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

    _state = new XRState(_settings);
    _state->setViewer(viewer);
    _state->setDestState(XRState::VRSTATE_ACTIONS);
    viewer->setRealizeOperation(new XRRealizeOperation(_state, &view));
    viewer->addUpdateOperation(new XRUpdateOperation(_state));
}
