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

        void addSlave(osg::Camera *slaveCamera,
                      View::Flags flags) override;
        void removeSlave(osg::Camera *slaveCamera) override;

        void setupCamera(osg::Camera *camera, View::Flags flags);

    protected:

        // Slave update callback

        class UpdateSlaveCallback;

        void updateSlave(osg::View& view, osg::View::Slave& slave);

        // Initial draw callback

        class InitialDrawCallback;

        void initialDraw(osg::RenderInfo& renderInfo, View::Flags flags);

        // Compute stereo matrices callbacks

        class ComputeStereoMatricesCallback;
        class ComputeStereoMatricesCallbackNop;

        osg::Matrixd getEyeProjection(osg::FrameStamp *stamp, int eye,
                                      const osg::Matrixd& projection);
        osg::Matrixd getEyeView(osg::FrameStamp *stamp, int eye,
                                const osg::Matrixd& view);

    protected:

        osg::ref_ptr<osg::DisplaySettings> _stereoDisplaySettings;
        uint32_t _viewIndices[2];
        unsigned int _lastUpdate;

        // osgxr_ViewIndex
        osg::ref_ptr<osg::Uniform> _uniformViewIndex;
        // osgxr_ViewIndexPriv
        osg::ref_ptr<osg::Uniform> _uniformViewIndexPriv;
        // osgxr_viewport_offsets[]
        osg::ref_ptr<osg::Uniform> _uniformViewportOffsets;
        // osgxr_viewport_scales[]
        osg::ref_ptr<osg::Uniform> _uniformViewportScales;
};

} // osgXR

#endif
