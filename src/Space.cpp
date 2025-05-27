// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2025 James Hogan <james@albanarts.com>

#include "Space.h"

#include <osgXR/Manager>

#include "OpenXR/Space.h"

#include "XRState.h"

using namespace osgXR;

// Internal API

Space::Private::Private(XRState *state, XrReferenceSpaceType type) :
    _state(state),
    _refType(type)
{
    state->addSpace(this);
}

Space::Private::Private(XRState *state, XrReferenceSpaceType type,
                        const osg::Quat &orientationInRef,
                        const osg::Vec3f &positionInRef) :
    _state(state),
    _refType(type),
    _poseInRef(0, orientationInRef, positionInRef)
{
    state->addSpace(this);
}

Space::Private::~Private()
{
    XRState *state = _state.get();
    if (state)
        state->removeSpace(this);
}

void Space::Private::cleanupSession()
{
    _space = nullptr;
}

bool Space::Private::locate(Pose &pose)
{
    XRState *state = _state.get();
    if (!state) {
        pose = Pose();
        return false;
    }

    // set up space if not already set up
    if (!_space && state->isRunning()) {
        _space = new OpenXR::Space(state->getSession(), _refType, _poseInRef);
    }

    if (!_space) {
        pose = Pose();
        return false;
    }

    OpenXR::Session *session = _space->getSession();
    OpenXR::Space::Location loc;
    XrTime time = session->getLastDisplayTime();
    bool ret = _space->locate(session->getLocalSpace(time), time,
                              loc);
    pose = Pose((Pose::Flags)loc.getFlags(),
                loc.getOrientation(),
                loc.getPosition());
    return ret;
}

// Public API

Space::Space(Private *priv) :
    _private(priv)
{
}

Space::~Space()
{
}

Pose Space::locate()
{
    Pose pose;
    Private::get(this)->locate(pose);
    return pose;
}

// RefSpaceView

RefSpaceView::RefSpaceView(Manager *manager) :
    Space(new Space::Private(manager->_getXrState(),
                             XR_REFERENCE_SPACE_TYPE_VIEW))
{
}

RefSpaceView::RefSpaceView(Manager *manager, const Pose &poseInRef) :
    Space(new Space::Private(manager->_getXrState(),
                             XR_REFERENCE_SPACE_TYPE_VIEW,
                             poseInRef.getOrientation(),
                             poseInRef.getPosition()))
{
}
