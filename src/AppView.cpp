// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#include "AppView.h"

using namespace osgXR;

AppView::AppView(XRState *state,
                 osgViewer::GraphicsWindow *window,
                 osgViewer::View *osgView) :
    View(window, osgView),
    _valid(false),
    _state(state)
{
}

void AppView::init()
{
    _state->initAppView(this);
    _valid = true;
}

AppView::~AppView()
{
    destroy();
}

void AppView::destroy()
{
    if (_valid)
        _state->destroyAppView(this);
    _valid = false;
}
