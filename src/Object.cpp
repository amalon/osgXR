// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#include "Object.h"

#include "XRState.h"

#include <cassert>

using namespace osgXR;

Object::~Object()
{
    assert(!_state.valid());
}

void Object::registerState(XRState *state)
{
    assert(!_state.valid());
    _state = state;
    state->addObject(this);
}

void Object::unregisterState()
{
    if (_state.valid())
    {
        _state->removeObject(this);
        _state = nullptr;
    }
}

void Object::setup(OpenXR::Session *session)
{
}

void Object::cleanupSession()
{
}
