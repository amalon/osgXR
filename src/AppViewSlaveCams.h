// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#ifndef OSGXR_APP_VIEW_SLAVE_CAMS
#define OSGXR_APP_VIEW_SLAVE_CAMS 1

#include "AppView.h"
#include "XRState.h"

#include <cstdint>

namespace osgXR {

/// Represents an app level view in slave cams mode
class AppViewSlaveCams : public AppView
{
    public:

        AppViewSlaveCams(XRState *state,
                         uint32_t viewIndex,
                         osgViewer::GraphicsWindow *window,
                         osgViewer::View *osgView);

        void addSlave(osg::Camera *slaveCamera,
                      View::Flags flags) override;
        void removeSlave(osg::Camera *slaveCamera) override;

        void setupCamera(osg::Camera *camera, View::Flags flags);

    protected:

        // Slave update callback

        class UpdateSlaveCallback;

        void updateSlave(osg::View& view, osg::View::Slave& slave,
                         View::Flags flags);

    protected:

        uint32_t _viewIndex;
};

} // osgXR

#endif
