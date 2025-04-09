// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/Manager>
#include <osgXR/Mirror>

#include "Extension.h"
#include "XRState.h"
#include "XRRealizeOperation.h"

using namespace osgXR;

Manager::Manager() :
    _settings(Settings::instance()),
    _destroying(false),
    _state(new XRState(_settings, const_cast<Manager *>(this)))
{
}

Manager::~Manager()
{
}

void Manager::setVisibilityMaskNodeMasks(osg::Node::NodeMask left,
                                         osg::Node::NodeMask right) const
{
    _state->setVisibilityMaskNodeMasks(left, right);
}

void Manager::configure(osgViewer::View &view) const
{
    osgViewer::ViewerBase *viewer = _viewer;
    if (!viewer)
        viewer = dynamic_cast<osgViewer::ViewerBase *>(&view);
    if (!viewer)
        return;

    _state->setViewer(viewer);

    // Its rather inconvenient that ViewConfig expects a const configure()
    // Just cheat and cast away the constness here
    osg::ref_ptr<XRRealizeOperation> realizeOp = new XRRealizeOperation(_state, &view);
    viewer->setRealizeOperation(realizeOp);
    if (viewer->isRealized())
    {
        osgViewer::ViewerBase::Contexts contexts;
        viewer->getContexts(contexts, true);
        if (contexts.size() > 0)
            (*realizeOp)(contexts[0]);
    }
}

void Manager::update()
{
    _state->update();
}

bool Manager::checkAndResetStateChanged()
{
    return _state->checkAndResetStateChanged();
}

bool Manager::getPresent() const
{
    return _state->getUpState() >= XRState::VRSTATE_SYSTEM;
}

bool Manager::getEnabled() const
{
    return _state->getUpState() == XRState::VRSTATE_ACTIONS;
}

void Manager::setEnabled(bool enabled)
{
    // Avoid needlessly discarding of the instance
    // SteamVR 1.15 and 1.16 have issues with xrDestroySession() hanging
    if (enabled)
    {
        _destroying = false;
        _state->setProbing(true);
    }
    else if (_destroying)
    {
        _state->setProbing(false);
    }

    _state->setDestState(enabled ? XRState::VRSTATE_ACTIONS
                                 : _state->getProbingState());
}

void Manager::destroyAndWait()
{
    _destroying = true;
    setEnabled(false);
    while (_state->isStateUpdateNeeded())
        _state->update();
}

bool Manager::isDestroying() const
{
    return _destroying;
}

bool Manager::isRunning() const
{
    return _state->isRunning();
}

void Manager::syncSettings()
{
    _state->syncSettings();
}

void Manager::syncActionSetup()
{
    _state->syncActionSetup();
}

bool Manager::hasValidationLayer() const
{
    return _state->hasValidationLayer();
}

bool Manager::hasDepthInfoExtension() const
{
    return _state->hasDepthInfoExtension();
}

bool Manager::hasVisibilityMaskExtension() const
{
    return _state->hasVisibilityMaskExtension();
}

osg::ref_ptr<Extension> Manager::getExtension(const std::string &name)
{
    return new Extension(this, name);
}

std::vector<std::string> Manager::getExtensionNames()
{
    return _state->getExtensionNames();
}

void Manager::enableExtension(const Extension *extension)
{
    _state->enableExtension(Extension::Private::get(extension));
}

void Manager::disableExtension(const Extension *extension)
{
    _state->disableExtension(Extension::Private::get(extension));
}

Version Manager::getApiVersion() const
{
    XrVersion apiVersion = _state->getApiVersion();
    return Version(XR_VERSION_MAJOR(apiVersion), XR_VERSION_MINOR(apiVersion));
}

const char *Manager::getRuntimeName() const
{
    return _state->getRuntimeName();
}

Version Manager::getRuntimeVersion() const
{
    return _state->getRuntimeVersion();
}

const char *Manager::getSystemName() const
{
    return _state->getSystemName();
}

const char *Manager::getStateString() const
{
    return _state->getStateString();
}

void Manager::onRunning()
{
}

void Manager::onStopped()
{
}

void Manager::onFocus()
{
}

void Manager::onUnfocus()
{
}

void Manager::addMirror(Mirror *mirror)
{
    if (!_state->valid())
    {
        // handle this later, _state may not be created yet
        _mirrorQueue.push_back(mirror);
    }
    else
    {
        // init the mirror right away
        mirror->_init();
    }
}

void Manager::setupMirrorCamera(osg::Camera *camera)
{
    addMirror(new Mirror(this, camera));
}

bool Manager::recenter()
{
    return _state->recenterLocalSpace();
}

void Manager::_setupMirrors()
{
    // init each mirror in the queue
    while (!_mirrorQueue.empty())
    {
        _mirrorQueue.front()->_init();
        _mirrorQueue.pop_front();
    }
}
