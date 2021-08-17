// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_XRREALIZEOPERATION
#define OSGXR_XRREALIZEOPERATION 1

#include <osg/ref_ptr>
#include <osgViewer/View>

namespace osgXR {

class XRState;

class XRRealizeOperation : public osg::GraphicsOperation
{
    public:

        explicit XRRealizeOperation(osg::ref_ptr<XRState> state,
                                    osgViewer::View *view);

        void operator () (osg::GraphicsContext *gc) override;

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

#endif
