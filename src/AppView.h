// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#ifndef OSGXR_APP_VIEW
#define OSGXR_APP_VIEW 1

#include <osgXR/View>

#include "XRState.h"

#include <osg/Camera>
#include <osgViewer/GraphicsWindow>
#include <osgViewer/View>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

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

        void setMVRSize(unsigned int width, unsigned int height)
        {
            _mvrWidth = width;
            _mvrHeight = height;
        }

        void setMVRViews(unsigned int views,
                         const std::string &viewIdGlobalStr,
                         const std::string &viewIdVertStr,
                         const std::string &viewIdGeomStr,
                         const std::string &viewIdFragStr)
        {
            _mvrViews = views;
            _mvrViewIdGlobalStr = viewIdGlobalStr;
            _mvrViewIdStr[0] = viewIdVertStr;
            _mvrViewIdStr[1] = viewIdGeomStr;
            _mvrViewIdStr[2] = viewIdFragStr;
        }

        void setMVRCells(unsigned int cells)
        {
            _mvrCells = cells;
        }

        void setMVRLayers(unsigned int layers, unsigned int attachmentFace,
                          const std::string &layerVertStr,
                          const std::string &layerGeomStr,
                          const std::string &layerFragStr)
        {
            _mvrLayers = layers;
            _mvrAttachmentFace = attachmentFace;
            _mvrLayerStr[0] = layerVertStr;
            _mvrLayerStr[1] = layerGeomStr;
            _mvrLayerStr[2] = layerFragStr;
        }

        // Overridden from View
        unsigned int getMVRWidth() const override
        {
            return _mvrWidth;
        }
        unsigned int getMVRHeight() const override
        {
            return _mvrHeight;
        }
        unsigned int getMVRViews() const override
        {
            return _mvrViews;
        }
        std::string getMVRViewIdGlobalStr() const override
        {
            return _mvrViewIdGlobalStr;
        }
        std::string getMVRViewIdStr(GLenum stage) const override
        {
            int index = shaderStageToIndex(stage);
            if (index < 0)
                return "";
            return _mvrViewIdStr[index];
        }
        unsigned int getMVRCells() const override
        {
            return _mvrCells;
        }
        unsigned int getMVRLayers() const override
        {
            return _mvrLayers;
        }
        unsigned int getMVRAttachmentFace() const override
        {
            return _mvrAttachmentFace;
        }
        std::string getMVRLayerStr(GLenum stage) const override
        {
            int index = shaderStageToIndex(stage);
            if (index < 0)
                return "";
            return _mvrLayerStr[index];
        }

    protected:
        static inline int shaderStageToIndex(GLenum stage)
        {
            switch (stage) {
            case GL_VERTEX_SHADER:
                return 0;
            case GL_GEOMETRY_SHADER:
                return 1;
            case GL_FRAGMENT_SHADER:
                return 2;
            default:
                return -1;
            }
        }
        void setCamFlags(osg::Camera* cam, View::Flags flags);
        View::Flags getCamFlagsAndDrop(osg::Camera* cam);

        /// Configure indexed viewports
        void setupIndexedViewports(osg::StateSet *stateSet,
                                   const std::vector<uint32_t> &viewIndices,
                                   uint32_t width, uint32_t height,
                                   View::Flags flags);

        bool _valid;

        XRState *_state;
        std::map<osg::Camera*, View::Flags> _camFlags;
        unsigned int _mvrWidth;
        unsigned int _mvrHeight;
        unsigned int _mvrViews;
        std::string _mvrViewIdGlobalStr;
        std::string _mvrViewIdStr[3];
        unsigned int _mvrCells;
        unsigned int _mvrLayers;
        unsigned int _mvrAttachmentFace;
        std::string _mvrLayerStr[3];
};

} // osgXR

#endif
