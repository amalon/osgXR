// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "XRRealizeOperation.h"

#include "XRState.h"

#include <osgViewer/GraphicsWindow>

using namespace osgXR;

XRRealizeOperation::XRRealizeOperation(osg::ref_ptr<XRState> state,
                                       osgViewer::View *view) :
    osg::GraphicsOperation("XRRealizeOperation", false),
    _state(state),
    _view(view),
    _realized(false)
{
}

void XRRealizeOperation::operator () (osg::GraphicsContext *gc)
{
    if (!_realized)
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);
        gc->makeCurrent();

        auto *window = dynamic_cast<osgViewer::GraphicsWindow *>(gc);
        if (window)
        {
            _state->init(window, _view);
            _realized = true;
        }
    }
}
