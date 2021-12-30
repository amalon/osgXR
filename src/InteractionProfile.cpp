// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "Action.h"
#include "InteractionProfile.h"

#include "OpenXR/InteractionProfile.h"
#include "OpenXR/Path.h"
#include "OpenXR/Session.h"

#include <osgXR/Manager>

#include "XRState.h"

using namespace osgXR;

// Internal API

InteractionProfile::Private::Private(InteractionProfile *pub,
                                     XRState *state,
                                     const std::string &vendor,
                                     const std::string &type) :
    _pub(pub),
    _state(state),
    _vendor(vendor),
    _type(type),
    _updated(true)
{
    state->addInteractionProfile(this);
}

InteractionProfile::Private::~Private()
{
    XRState *state = _state.get();
    if (state)
        state->removeInteractionProfile(this);
}

void InteractionProfile::Private::suggestBinding(Action *action,
                                                 const std::string &binding)
{
    _bindings.push_back({action, binding});
    _updated = true;
}

bool InteractionProfile::Private::setup(OpenXR::Instance *instance)
{
    // Recreate every time, as actions may have been altered and recreated
    _profile = new OpenXR::InteractionProfile(instance, _vendor.c_str(),
                                              _type.c_str());

    for (Binding &binding: _bindings)
    {
        // ensure action is set up
        OpenXR::Action *action = Action::Private::get(binding.action)->setup(instance);
        if (action)
            _profile->addBinding(action, binding.binding);
    }

    bool ret = _profile->suggestBindings();
    if (ret)
        _updated = false;
    return ret;
}

void InteractionProfile::Private::cleanupInstance()
{
    _profile = nullptr;
}

OpenXR::Path InteractionProfile::Private::getPath() const
{
    if (_profile.valid())
        return _profile->getPath();
    else
        return OpenXR::Path();
}

// Public API

InteractionProfile::InteractionProfile(Manager *manager,
                                       const std::string &vendor,
                                       const std::string &type) :
    _private(new Private(this, manager->_getXrState(), vendor, type))
{
}

InteractionProfile::~InteractionProfile()
{
}

const std::string &InteractionProfile::getVendor() const
{
    return _private->getVendor();
}

const std::string &InteractionProfile::getType() const
{
    return _private->getType();
}

void InteractionProfile::suggestBinding(Action *action,
                                        const std::string &binding)
{
    _private->suggestBinding(action, binding);
}
