// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#include "CompositionLayer.h"

#include "XRState.h"

using namespace osgXR;

// Internal API

CompositionLayer::Private::Private(XRState *state) :
    _state(state),
    _visible(true),
    _order(1), // in front of perspective layer
    _alphaMode(BLEND_NONE)
{
    state->addCompositionLayer(this);
}

CompositionLayer::Private::~Private()
{
    XRState *state = _state.get();
    if (state)
        state->removeCompositionLayer(this);
}

void CompositionLayer::Private::setVisible(bool visible)
{
    _visible = visible;
}

bool CompositionLayer::Private::getVisible() const
{
    return _visible;
}

void CompositionLayer::Private::setOrder(int order)
{
    _order = order;
    // FIXME reorder
}

int CompositionLayer::Private::getOrder() const
{
    return _order;
}

void CompositionLayer::Private::setAlphaMode(AlphaMode mode)
{
    _alphaMode = mode;
}

CompositionLayer::AlphaMode CompositionLayer::Private::getAlphaMode() const
{
    return _alphaMode;
}

bool CompositionLayer::Private::writeCompositionLayer(OpenXR::Session *session,
                                                      OpenXR::CompositionLayer *layer,
                                                      bool disableAlpha) const
{
    if (!_visible)
        return false;

    XrCompositionLayerFlags flags = 0;
    AlphaMode alphaMode = disableAlpha ? BLEND_NONE : _alphaMode;
    switch (alphaMode)
    {
    case BLEND_NONE:
        break;
    case BLEND_ALPHA_PREMULT:
        flags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        break;
    case BLEND_ALPHA_UNPREMULT:
        flags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT |
                 XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
        break;
    }

    layer->setLayerFlags(flags);
    layer->setSpace(session->getLocalSpace());
    return true;
}

// Public API

CompositionLayer::CompositionLayer(Private *priv) :
    _private(priv)
{
}

CompositionLayer::~CompositionLayer()
{
}

void CompositionLayer::setVisible(bool visible)
{
    _private->setVisible(visible);
}

bool CompositionLayer::getVisible() const
{
    return _private->getVisible();
}

void CompositionLayer::setOrder(int order)
{
    _private->setOrder(order);
}

int CompositionLayer::getOrder() const
{
    return _private->getOrder();
}

void CompositionLayer::setAlphaMode(AlphaMode mode)
{
    _private->setAlphaMode(mode);
}

CompositionLayer::AlphaMode CompositionLayer::getAlphaMode() const
{
    return _private->getAlphaMode();
}
