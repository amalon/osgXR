// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/Manager>
#include <osgXR/Mirror>

#include "XRState.h"
#include "XRRealizeOperation.h"

using namespace osgXR;

Manager::Manager() :
    _probed(false),
    _settings(Settings::instance())
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

    // Its rather inconvenient that ViewConfig expects a const configure()
    // Just cheat and cast away the constness here
    _state = new XRState(_settings, const_cast<Manager *>(this));
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

bool Manager::getPresent() const
{
    return _state.valid() && _state->getPresent();
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
    if (_state.valid())
        return _state->getRuntimeName();
    else
        return "";
}

const char *Manager::getSystemName() const
{
    if (_state.valid())
        return _state->getSystemName();
    else
        return "";
}

void Manager::addMirror(Mirror *mirror)
{
    if (!_state.valid() || !_state->valid())
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
