// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/Manager>
#include <osgXR/Mirror>

#include "XRState.h"
#include "XRRealizeOperation.h"

using namespace osgXR;

Manager::Manager() :
    _probed(false),
    _settings(Settings::instance()),
    _state(new XRState(_settings, const_cast<Manager *>(this)))
{
}

Manager::~Manager()
{
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

void Manager::updateToDestState()
{
    while (_state->isStateUpdateNeeded())
        _state->update();
}

bool Manager::getPresent() const
{
    return _state->getUpState() >= XRState::VRSTATE_SYSTEM;
}

bool Manager::getEnabled() const
{
    return _state->getUpState() == XRState::VRSTATE_SESSION;
}

void Manager::setEnabled(bool enabled)
{
    // Avoid discarding of the instance
    // FIXME don't let this apply on shutdown
    if (enabled)
        _state->setProbing(true);

    _state->setDestState(enabled ? XRState::VRSTATE_SESSION
                                 : _state->getProbingState());
}

bool Manager::isRunning() const
{
    return _state->isRunning();
}

void Manager::syncSettings()
{
    _state->syncSettings();
}

void Manager::probe() const
{
    _hasValidationLayer = OpenXR::Instance::hasLayer(XR_APILAYER_LUNARG_core_validation);
    _hasDepthInfoExtension = OpenXR::Instance::hasExtension(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);

    _probed = true;
}

bool Manager::hasValidationLayer() const
{
    if (!_probed)
        probe();
    return _hasValidationLayer;
}

bool Manager::hasDepthInfoExtension() const
{
    if (!_probed)
        probe();
    return _hasDepthInfoExtension;
}

const char *Manager::getRuntimeName() const
{
    return _state->getRuntimeName();
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

void Manager::_setupMirrors()
{
    // init each mirror in the queue
    while (!_mirrorQueue.empty())
    {
        _mirrorQueue.front()->_init();
        _mirrorQueue.pop_front();
    }
}
