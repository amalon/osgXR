// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/Manager>

#include "XRState.h"
#include "XRRealizeOperation.h"

using namespace osgXR;

Manager::Manager() :
    _settings(Settings::instance())
{
}

Manager::~Manager()
{
}

void Manager::configure(osgViewer::View &view) const
{
    osgViewer::ViewerBase *viewer = dynamic_cast<osgViewer::ViewerBase *>(&view);
    if (!viewer)
        return;

    // Its rather inconvenient that ViewConfig expects a const configure()
    // Just cheat and cast away the constness here
    _state = new XRState(_settings, const_cast<Manager *>(this));
    osg::ref_ptr<XRRealizeOperation> realizeOp = new XRRealizeOperation(_state, &view);
    viewer->setRealizeOperation(realizeOp);
    if (viewer->isRealized())
    {
        osgViewer::ViewerBase::Contexts contexts;
        viewer->getContexts(contexts, true);
        if (contexts.size() > 0)
            (*realizeOp)(contexts[0]);
    }
}
