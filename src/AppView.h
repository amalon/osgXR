// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#ifndef OSGXR_APP_VIEW
#define OSGXR_APP_VIEW 1

#include <osgXR/View>

#include "XRState.h"

#include <osgViewer/GraphicsWindow>
#include <osgViewer/View>

namespace osgXR {

/** Represents a generic app level view.
 * This may handle multiple OpenXR views.
 */
class AppView : public View
{
    public:

        AppView(XRState *state,
                osgViewer::GraphicsWindow *window,
                osgViewer::View *osgView);
        virtual ~AppView();

        void destroy();

        void init();

    protected:

        bool _valid;

        XRState *_state;
};

} // osgXR

#endif
