// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#include "XRUpdateOperation.h"

#include "XRState.h"

using namespace osgXR;

XRUpdateOperation::XRUpdateOperation(osg::ref_ptr<XRState> state) :
    osg::Operation("XRUpdateOperation", true),
    _state(state)
{
}

void XRUpdateOperation::operator () (osg::Object *obj)
{
    _state->update();
}
