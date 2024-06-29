// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#ifndef OSGXR_APP_VIEW_SCENE_VIEW
#define OSGXR_APP_VIEW_SCENE_VIEW 1

#include "AppView.h"

#include <osg/DisplaySettings>
#include <osg/ref_ptr>
#include <cstdint>

namespace osgXR {

/// Represents an app level view in scene view mode
class AppViewSceneView : public AppView
{
    public:

        AppViewSceneView(XRState *state,
                         uint32_t viewIndices[2],
                         osgViewer::GraphicsWindow *window,
                         osgViewer::View *osgView);

        void addSlave(osg::Camera *slaveCamera) override;
        void removeSlave(osg::Camera *slaveCamera) override;

        void setupCamera(osg::Camera *camera);

    protected:

        // Slave update callback

        class UpdateSlaveCallback;

        void updateSlave(osg::View& view, osg::View::Slave& slave);

        // Compute stereo matrices callbacks

        class ComputeStereoMatricesCallback;

        osg::Matrixd getEyeProjection(osg::FrameStamp *stamp, int eye,
                                      const osg::Matrixd& projection);
        osg::Matrixd getEyeView(osg::FrameStamp *stamp, int eye,
                                const osg::Matrixd& view);

    protected:

        osg::ref_ptr<osg::DisplaySettings> _stereoDisplaySettings;
        uint32_t _viewIndices[2];
};

} // osgXR

#endif
