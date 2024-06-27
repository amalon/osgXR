// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#ifndef OSGXR_APP_VIEW_OVR_MULTIVIEW
#define OSGXR_APP_VIEW_OVR_MULTIVIEW 1

#include "AppView.h"
#include "MultiView.h"

#include <osg/Uniform>
#include <osg/ref_ptr>

#include <cstdint>
#include <vector>

namespace osgXR {

/// Represents an app level view in OVR_multiview mode
class AppViewOVRMultiview : public AppView
{
    public:

        AppViewOVRMultiview(XRState *state,
                            const std::vector<uint32_t> &viewIndices,
                            osgViewer::GraphicsWindow *window,
                            osgViewer::View *osgView);

        // osgXR::View overrides
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

        std::vector<uint32_t> _viewIndices;
        osg::ref_ptr<MultiView> _multiView;
        unsigned int _lastUpdate;

        // osgxr_transforms[]
        osg::ref_ptr<osg::Uniform> _uniformTransforms;
        // osgxr_view_matrices[]
        osg::ref_ptr<osg::Uniform> _uniformViewMatrices;
        // osgxr_normal_matrices[]
        osg::ref_ptr<osg::Uniform> _uniformNormalMatrices;
        // osgxr_viewport_offsets[]
        osg::ref_ptr<osg::Uniform> _uniformViewportOffsets;
        // osgxr_viewport_scales[]
        osg::ref_ptr<osg::Uniform> _uniformViewportScales;
};

} // osgXR

#endif
