// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#include "Extension.h"
#include "XRState.h"

#include "OpenXR/Instance.h"

#include <osgXR/Manager>

using namespace osgXR;

// Internal API

Extension::Private::Private(XRState *state,
                            const std::string &name) :
    _state(state),
    _name(name),
    _enabled(false),
    _probed(false)
{
}

void Extension::Private::registerPublic(Extension *extension)
{
    _publics.insert(extension);
}

void Extension::Private::unregisterPublic(Extension *extension)
{
    _publics.erase(extension);
}

bool Extension::Private::getDependsOn(const std::shared_ptr<Private> &extension) const
{
    if (_dependencies.find(extension) != _dependencies.end())
        return true;
    for (auto &dep: _dependencies)
        if (dep->getDependsOn(extension))
            return true;
    return false;
}

void Extension::Private::addDependency(std::shared_ptr<Private> &dependency)
{
    _dependencies.insert(dependency);
}

bool Extension::Private::getAvailable() const
{
    if (!_probed)
        probe();
    return _available;
}

bool Extension::Private::getAvailable(uint32_t *outVersion) const
{
    if (!_probed)
        probe();
    if (outVersion)
        *outVersion = _version;
    return _available;
}

uint32_t Extension::Private::getVersion() const
{
    if (!_probed)
        probe();
    return _version;
}

bool Extension::Private::getEnabled() const
{
    return _enabled;
}

void Extension::Private::setup(OpenXR::Instance *instance)
{
    assert(getAvailable());
    // Enable dependencies
    for (auto &dep: _dependencies)
        dep->setup(instance);
    instance->enableExtension(_name);
    _enabled = true;
    notifyChanged();
}

void Extension::Private::cleanup()
{
    _probed = false;
    _enabled = false;
    notifyChanged();
}

void Extension::Private::notifyChanged() const
{
    for (auto pub: _publics)
        pub->onChanged();
}

void Extension::Private::probe() const
{
    _available = OpenXR::Instance::hasExtension(_name.c_str(), &_version);
    _probed = true;
}

// Public API

Extension::Extension(Manager *manager,
                     const std::string &name) :
    _private(manager->_getXrState()->getExtension(name))
{
    _private->registerPublic(this);
}

Extension::~Extension()
{
    _private->unregisterPublic(this);
}

void Extension::addDependency(Extension *dependency)
{
    // Avoid circular dependencies
    if (dependency != this && !dependency->_private->getDependsOn(_private))
        _private->addDependency(dependency->_private);
}

const std::string &Extension::getName() const
{
    return _private->getName();
}

bool Extension::getAvailable() const
{
    return _private->getAvailable();
}

bool Extension::getAvailable(uint32_t *outVersion) const
{
    return _private->getAvailable(outVersion);
}

uint32_t Extension::getVersion() const
{
    return _private->getVersion();
}

bool Extension::getEnabled() const
{
    return _private->getEnabled();
}

void Extension::onChanged()
{
}
