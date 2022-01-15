// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/Manager>
#include <osgXR/Mirror>

#include "XRState.h"

#include <osg/PolygonMode>

using namespace osgXR;

Mirror::Mirror(Manager *manager, osg::Camera *camera) :
    _manager(manager),
    _camera(camera),
    _mirrorSettings(manager->_getSettings()->getMirrorSettings())
{
}

Mirror::~Mirror()
{
}

void Mirror::_init()
{
    _camera->setAllowEventFocus(false);
    _camera->setViewMatrix(osg::Matrix::identity());
    _camera->setProjectionMatrix(osg::Matrix::ortho2D(0, 1, 0, 1));

    // Find the mirror settings
    MirrorSettings *mirrorSettings = &_mirrorSettings;
    // but fall back to the manager's mirror settings
    if (mirrorSettings->getMirrorMode() == MirrorSettings::MIRROR_AUTOMATIC)
        mirrorSettings = &_manager->_getSettings()->getMirrorSettings();
    switch (mirrorSettings->getMirrorMode())
    {
    case MirrorSettings::MIRROR_NONE:
        // Draw nothing, but still clear the viewport
        _camera->setClearMask(GL_COLOR_BUFFER_BIT);
        break;
    case MirrorSettings::MIRROR_AUTOMATIC:
        // Fall-through: Default to MIRROR_SINGLE
    case MirrorSettings::MIRROR_SINGLE:
        {
            int viewIndex = mirrorSettings->getMirrorViewIndex();
            if (viewIndex < 0)
                viewIndex = 0;
            setupQuad(viewIndex, 0.0f, 1.0f);
        }
        break;
    case MirrorSettings::MIRROR_LEFT_RIGHT:
        for (unsigned int viewIndex = 0; viewIndex < 2; ++viewIndex)
            setupQuad(viewIndex, 0.5f * viewIndex, 0.5f);
        break;
    }
}

namespace {

class MirrorPreDrawCallback : public osg::Camera::DrawCallback
{
    public:

        MirrorPreDrawCallback(osg::ref_ptr<XRState> xrState,
                              osg::ref_ptr<osg::StateSet> stateSet,
                              unsigned int viewIndex) :
            _xrState(xrState),
            _stateSet(stateSet),
            _viewIndex(viewIndex)
        {
        }

        void operator()(osg::RenderInfo& renderInfo) const override
        {
            const osg::FrameStamp *stamp = renderInfo.getState()->getFrameStamp();
            _stateSet->setTextureAttributeAndModes(0,
                                _xrState->getViewTexture(_viewIndex, stamp));
        }

    protected:

        osg::observer_ptr<XRState> _xrState;
        osg::ref_ptr<osg::StateSet> _stateSet;
        unsigned int _viewIndex;
};

class MirrorPostDrawCallback : public osg::Camera::DrawCallback
{
    public:

        MirrorPostDrawCallback(osg::ref_ptr<osg::StateSet> stateSet) :
            _stateSet(stateSet)
        {
        }

        void operator()(osg::RenderInfo& renderInfo) const override
        {
            _stateSet->removeTextureAttribute(0, osg::StateAttribute::Type::TEXTURE);
        }

    protected:

        osg::ref_ptr<osg::StateSet> _stateSet;
};

}

void Mirror::setupQuad(unsigned int viewIndex,
                       float x, float w)
{
    XRState *xrState = _manager->_getXrState();

    if (viewIndex >= xrState->getViewCount())
        return;

    // Build an always-visible quad to draw the view texture on
    osg::ref_ptr<osg::Geode> quad = new osg::Geode;
    quad->setCullingActive(false);

    XRState::TextureRect rect = xrState->getViewTextureRect(viewIndex);
    quad->addDrawable(osg::createTexturedQuadGeometry(
                                  osg::Vec3(x,    0.0f,  0.0f),
                                  osg::Vec3(w,    0.0f,  0.0f),
                                  osg::Vec3(0.0f, 1.0f,  0.0f),
                                  rect.x, rect.y,
                                  rect.x + rect.width, rect.y + rect.height));

    osg::ref_ptr<osg::StateSet> state = quad->getOrCreateStateSet();
    int forceOff = osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED;
    int forceOn = osg::StateAttribute::ON | osg::StateAttribute::PROTECTED;
    state->setMode(GL_LIGHTING, forceOff);
    state->setMode(GL_DEPTH_TEST, forceOff);
    state->setMode(GL_FRAMEBUFFER_SRGB, forceOn);

    _camera->addChild(quad);

    // Set a callback so we can switch the texture to the active swapchain image
    _camera->addPreDrawCallback(new MirrorPreDrawCallback(_manager->_getXrState(),
                                                          state, viewIndex));
    _camera->addPostDrawCallback(new MirrorPostDrawCallback(state));
}
