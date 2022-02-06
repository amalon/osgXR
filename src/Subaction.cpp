// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "Subaction.h"
#include "XRState.h"

#include <osgXR/Manager>

using namespace osgXR;

// Internal API

Subaction::Private::Private(XRState *state,
                            const std::string &path) :
    _state(state),
    _pathString(path)
{
}

void Subaction::Private::registerPublic(Subaction *subaction)
{
    _publics.insert(subaction);
}

void Subaction::Private::unregisterPublic(Subaction *subaction)
{
    _publics.erase(subaction);
}

InteractionProfile *Subaction::Private::getCurrentProfile()
{
    if (!_currentProfile.valid())
    {
        if (_path.valid())
            _currentProfile = _state->getCurrentInteractionProfile(_path);
    }

    return _currentProfile.get();
}

void Subaction::Private::onInteractionProfileChanged(OpenXR::Session *session)
{
    // Ensure path is set up
    setup(session->getInstance());

    // Find whether this subaction's current interaction profile has changed
    InteractionProfile *prevProfile = _currentProfile.get();
    _currentProfile = nullptr;
    InteractionProfile *newProfile = getCurrentProfile();
    if (newProfile != prevProfile)
    {
        // Notify any derived Subaction classes from the app
        for (auto *pub: _publics)
            pub->onProfileChanged(newProfile);
    }
}

const OpenXR::Path &Subaction::Private::setup(OpenXR::Instance *instance)
{
    if (!_path.valid())
        _path = OpenXR::Path(instance, _pathString);
    return _path;
}

void Subaction::Private::cleanupSession()
{
    bool hadProfile = _currentProfile.valid();
    _currentProfile = nullptr;
    if (hadProfile)
    {
        // Notify any derived Subaction classes from the app
        for (auto *pub: _publics)
            pub->onProfileChanged(nullptr);
    }
}

void Subaction::Private::cleanupInstance()
{
    _path = OpenXR::Path();
}

// Public API

Subaction::Subaction(Manager *manager,
                     const std::string &path) :
    _private(manager->_getXrState()->getSubaction(path))
{
    _private->registerPublic(this);
}

Subaction::~Subaction()
{
    _private->unregisterPublic(this);
}

const std::string &Subaction::getPath() const
{
    return _private->getPathString();
}

InteractionProfile *Subaction::getCurrentProfile()
{
    return _private->getCurrentProfile();
}

void Subaction::onProfileChanged(InteractionProfile *newProfile)
{
    // This is for derived classes to implement to their own ends
}
