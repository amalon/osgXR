// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#include "AppView.h"

using namespace osgXR;

AppView::AppView(XRState *state,
                 osgViewer::GraphicsWindow *window,
                 osgViewer::View *osgView) :
    View(window, osgView),
    _valid(false),
    _state(state),
    _mvrWidth(1024),
    _mvrHeight(768),
    _mvrViews(1),
    _mvrViewIdStr{"0", "0", "0"},
    _mvrCells(1)
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

void AppView::setCamFlags(osg::Camera* cam, View::Flags flags)
{
    auto it = _camFlags.find(cam);
    if (it == _camFlags.end())
        _camFlags.emplace(cam, flags);
    else
        it->second = (View::Flags)(it->second | flags);
}

View::Flags AppView::getCamFlagsAndDrop(osg::Camera* cam)
{
    auto it = _camFlags.find(cam);
    if (it == _camFlags.end())
        return View::CAM_NO_BITS;
    View::Flags ret = it->second;
    _camFlags.erase(it);
    return ret;
}
