// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/OpenXRDisplay>

#include <osg/ref_ptr>
#include <osgViewer/ViewerBase>

#include "XRState.h"

using namespace osgXR;

namespace osgXR {

class XRRealizeOperation : public osg::GraphicsOperation
{
    public:

        explicit XRRealizeOperation(osg::ref_ptr<XRState> state,
                                        osgViewer::View *view) :
            osg::GraphicsOperation("XRRealizeOperation", false),
            _state(state),
            _view(view),
            _realized(false)
        {
        }

        virtual void operator () (osg::GraphicsContext *gc)
        {
            if (!_realized) {
                OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);
                gc->makeCurrent();

                auto *window = dynamic_cast<osgViewer::GraphicsWindow *>(gc);
                if (window) {
                    _state->init(window, _view);
                    _realized = true;
                }
            }
        }

        bool realized() const
        {
            return _realized;
        }

    protected:

        OpenThreads::Mutex _mutex;
        osg::ref_ptr<XRState> _state;
        osgViewer::View *_view;
        bool _realized;
};

} // osgXR

// OpenXRDisplay

OpenXRDisplay::OpenXRDisplay()
{
}

OpenXRDisplay::OpenXRDisplay(const std::string &appName,
                             uint32_t appVersion,
                             enum FormFactor formFactor):
    _appName(appName),
    _appVersion(appVersion),
    _validationLayer(false),
    _formFactor(formFactor),
    _preferredEnvBlendModeMask(0),
    _allowedEnvBlendModeMask(0),
    _unitsPerMeter(1.0f)
{
}

OpenXRDisplay::OpenXRDisplay(const OpenXRDisplay& rhs,
                             const osg::CopyOp& copyop):
    ViewConfig(rhs,copyop),
    _appName(rhs._appName),
    _appVersion(rhs._appVersion),
    _validationLayer(rhs._validationLayer),
    _formFactor(rhs._formFactor),
    _preferredEnvBlendModeMask(rhs._preferredEnvBlendModeMask),
    _allowedEnvBlendModeMask(rhs._allowedEnvBlendModeMask),
    _unitsPerMeter(rhs._unitsPerMeter)
{
}

OpenXRDisplay::~OpenXRDisplay()
{
}

void OpenXRDisplay::configure(osgViewer::View &view) const
{
    osgViewer::ViewerBase *viewer = dynamic_cast<osgViewer::ViewerBase *>(&view);
    if (!viewer)
        return;

    _state = new XRState(this);
    viewer->setRealizeOperation(new XRRealizeOperation(_state, &view));
}
