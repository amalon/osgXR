// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#ifndef OSGXR_HAND_POSE
#define OSGXR_HAND_POSE 1

#include <osgXR/HandPose>

namespace osgXR {

class HandPose::Private
{
    public:

        static Private *get(HandPose *pub)
        {
            return pub->_private.get();
        }

        Private();
        Private(const Private &other);
        virtual ~Private();

        Private &operator =(const Private &other);

    protected:

        friend class HandPose;

        bool _active;
        JointLocation _jointLocations[JOINT_COUNT];
        JointVelocity _jointVelocities[JOINT_COUNT];
};

} // osgXR

#endif
