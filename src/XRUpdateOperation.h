// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#ifndef OSGXR_XRUPDATEOPERATION
#define OSGXR_XRUPDATEOPERATION 1

#include <osg/ref_ptr>
#include <osg/OperationThread>

namespace osgXR {

class XRState;

class XRUpdateOperation : public osg::Operation
{
    public:

        explicit XRUpdateOperation(osg::ref_ptr<XRState> state);

        void operator () (osg::Object *ojb) override;

    protected:

        osg::ref_ptr<XRState> _state;
};

} // osgXR

#endif
