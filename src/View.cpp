// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/View>

using namespace osgXR;

View::View(osgViewer::GraphicsWindow *window, osgViewer::View *osgView) :
    _window(window),
    _osgView(osgView)
{
}

View::~View()
{
}

void View::Callback::updateSubView(View *view,
                                   unsigned int subviewIndex,
                                   const SubView &subview)
{
}
