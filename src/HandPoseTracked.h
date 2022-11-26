// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#ifndef OSGXR_HAND_POSE_TRACKED
#define OSGXR_HAND_POSE_TRACKED 1

#include <osgXR/HandPoseTracked>

#include <osg/ref_ptr>

#include "Object.h"

namespace osgXR {

namespace OpenXR {
    class HandTracker;
};

class HandPoseTracked::Private : public Object
{
    public:

        static Private *get(HandPoseTracked *pub)
        {
            return pub->_private.get();
        }

        Private(Hand hand);

        // Object notifications
        void setup(OpenXR::Session *session) override;
        void cleanupSession() override;

    protected:

        friend class HandPoseTracked;

        Hand _hand;

        osg::ref_ptr<OpenXR::HandTracker> _handTracker;
};

} // osgXR

#endif
