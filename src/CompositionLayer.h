// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#ifndef OSGXR_COMPOSITION_LAYER
#define OSGXR_COMPOSITION_LAYER 1

#include <osgXR/CompositionLayer>

#include "OpenXR/Compositor.h"
#include "OpenXR/Session.h"

#include <osg/observer_ptr>

namespace osgXR {

class XRState;

class CompositionLayer::Private
{
    public:

        static Private *get(CompositionLayer *pub)
        {
            return pub->_private.get();
        }

        static const Private *get(const CompositionLayer *pub)
        {
            return pub->_private.get();
        }

        Private(XRState *state);
        virtual ~Private();

        void setVisible(bool visible);
        bool getVisible() const;

        void setOrder(int order);
        int getOrder() const;

        void setAlphaMode(AlphaMode mode);
        AlphaMode getAlphaMode() const;

        /// Write to composition layer
        bool writeCompositionLayer(OpenXR::Session *session,
                                   OpenXR::CompositionLayer *layer,
                                   bool disableAlpha) const;

        /// Setup composition layer with an OpenXR session
        virtual bool setup(OpenXR::Session *session) = 0;

        /// Add composition layers to the frame
        virtual void endFrame(OpenXR::Session::Frame *frame) = 0;

        /// Clean up composition layer before an OpenXR session is destroyed
        virtual void cleanupSession() = 0;

        /// For sorting operations
        static bool compareOrder(Private *a, Private *b)
        {
            return a->_order < b->_order;
        }

    protected:

        osg::observer_ptr<XRState> _state;

        bool _visible;
        int _order;
        AlphaMode _alphaMode;
};

} // osgXR

#endif
