// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "ActionSet.h"
#include "Action.h"

#include "OpenXR/ActionSet.h"

#include <osgXR/Manager>

#include "XRState.h"

using namespace osgXR;

// Internal API

ActionSet::Private::Private(XRState *state) :
    _state(state),
    _priority(0),
    _updated(true)
{
    state->addActionSet(this);
}

ActionSet::Private::~Private()
{
    XRState *state = _state.get();
    if (state)
        state->removeActionSet(this);
}

void ActionSet::Private::setName(const std::string &name)
{
    _updated = true;
    _name = name;
}

const std::string &ActionSet::Private::getName() const
{
    return _name;
}

void ActionSet::Private::setLocalizedName(const std::string &localizedName)
{
    _updated = true;
    _localizedName = localizedName;
}

const std::string &ActionSet::Private::getLocalizedName() const
{
    return _localizedName;
}

void ActionSet::Private::setPriority(uint32_t priority)
{
    _updated = true;
    _priority = priority;
}

uint32_t ActionSet::Private::getPriority() const
{
    return _priority;
}

bool ActionSet::Private::getUpdated() const
{
    if (_updated)
        return true;
    for (Action::Private *action: _actions)
        if (action->getUpdated())
            return true;
    return false;
}

void ActionSet::Private::activate(std::shared_ptr<Subaction::Private> subaction)
{
    _activeSubactions.insert(subaction);

    if (_actionSet.valid() && _session.valid())
    {
        OpenXR::Path path;
        if (subaction)
            path = subaction->setup(_session->getInstance());
        _session->activateActionSet(_actionSet, path);
    }
}

void ActionSet::Private::deactivate(std::shared_ptr<Subaction::Private> subaction)
{
    _activeSubactions.erase(subaction);

    if (_actionSet.valid() && _session.valid())
    {
        OpenXR::Path path;
        if (subaction)
            path = subaction->setup(_session->getInstance());
        _session->deactivateActionSet(_actionSet, path);
    }
}

bool ActionSet::Private::isActive()
{
    return !_activeSubactions.empty();
}

void ActionSet::Private::registerAction(Action::Private *action)
{
    _actions.insert(action);
}

void ActionSet::Private::unregisterAction(Action::Private *action)
{
    _actions.erase(action);
}

OpenXR::ActionSet *ActionSet::Private::setup(OpenXR::Instance *instance)
{
    if (_updated)
    {
        _actionSet = new OpenXR::ActionSet(instance, _name, _localizedName,
                                           _priority);
        _updated = false;
    }
    return _actionSet;
}

bool ActionSet::Private::setup(OpenXR::Session *session)
{
    _session = session;
    if (_actionSet.valid())
    {
        session->addActionSet(_actionSet);
        // Init all the actions
        for (Action::Private *action: _actions)
        {
            OpenXR::Action *xrAction = action->setup(session->getInstance());
            if (xrAction)
                xrAction->init();
        }
        for (auto &subaction: _activeSubactions)
        {
            OpenXR::Path path;
            if (subaction)
                path = subaction->setup(session->getInstance());
            session->activateActionSet(_actionSet, path);
        }
        return true;
    }
    return false;
}

void ActionSet::Private::cleanupSession()
{
    for (auto *action: _actions)
        action->cleanupSession();
}

void ActionSet::Private::cleanupInstance()
{
    _updated = true;
    _actionSet = nullptr;
    for (auto *action: _actions)
        action->cleanupInstance();
}

// Public API

ActionSet::ActionSet(Manager *manager) :
    _private(new Private(manager->_getXrState()))
{
}

ActionSet::ActionSet(Manager *manager,
                     const std::string &name) :
    _private(new Private(manager->_getXrState()))
{
    setName(name, name);
}

ActionSet::ActionSet(Manager *manager,
                     const std::string &name,
                     const std::string &localizedName) :
    _private(new Private(manager->_getXrState()))
{
    setName(name, localizedName);
}

ActionSet::~ActionSet()
{
}

void ActionSet::setName(const std::string &name,
                        const std::string &localizedName)
{
    _private->setName(name);
    _private->setLocalizedName(localizedName);
}

void ActionSet::setName(const std::string &name)
{
    _private->setName(name);
}

const std::string &ActionSet::getName() const
{
    return _private->getName();
}

void ActionSet::setLocalizedName(const std::string &localizedName)
{
    _private->setLocalizedName(localizedName);
}

const std::string &ActionSet::getLocalizedName() const
{
    return _private->getLocalizedName();
}

void ActionSet::setPriority(uint32_t priority)
{
    _private->setPriority(priority);
}

uint32_t ActionSet::getPriority() const
{
    return _private->getPriority();
}

void ActionSet::activate(Subaction *subaction)
{
    _private->activate(Subaction::Private::get(subaction));
}

void ActionSet::deactivate(Subaction *subaction)
{
    _private->deactivate(Subaction::Private::get(subaction));
}

bool ActionSet::isActive()
{
    return _private->isActive();
}
