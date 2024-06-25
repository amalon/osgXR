// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#ifndef OSGXR_APP_VIEW
#define OSGXR_APP_VIEW 1

#include <osgXR/View>

#include "XRState.h"

#include <osgViewer/GraphicsWindow>
#include <osgViewer/View>

#include <string>

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

        // Overridden from View
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

        bool _valid;

        XRState *_state;
        unsigned int _mvrViews;
        std::string _mvrViewIdGlobalStr;
        std::string _mvrViewIdStr[3];
};

} // osgXR

#endif
