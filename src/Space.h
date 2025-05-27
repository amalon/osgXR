// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2025 James Hogan <james@albanarts.com>

#ifndef OSGXR_SPACE
#define OSGXR_SPACE 1

#include <osgXR/Space>

#include <osg/ref_ptr>

#include "OpenXR/Space.h"

namespace osgXR {

class XRState;

class Space::Private
{
    public:

        static Private *get(Space *pub)
        {
            return pub->_private.get();
        }

        Private(XRState *state, XrReferenceSpaceType type);
        Private(XRState *state, XrReferenceSpaceType type,
                const osg::Quat &orientationInRef,
                const osg::Vec3f &positionInRef);

        virtual ~Private();

        /// Clean up space before an OpenXR session is destroyed
        void cleanupSession();

        bool locate(Pose &pose);

    protected:

        osg::observer_ptr<XRState> _state;
        XrReferenceSpaceType _refType;
        OpenXR::Space::Location _poseInRef;
        osg::ref_ptr<OpenXR::Space> _space;
};

} // osgXR

#endif
