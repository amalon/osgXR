// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_HAND_TRACKER
#define OSGXR_OPENXR_HAND_TRACKER 1

#include "Session.h"
#include "Space.h"

#include <osg/ref_ptr>

#include <cassert>

namespace osgXR {

namespace OpenXR {

class HandTracker : public osg::Referenced
{
    public:

        /// Create a hand tracker
        HandTracker(Session *session, XrHandEXT hand,
                    XrHandJointSetEXT handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT,
                    unsigned int jointCount = XR_HAND_JOINT_COUNT_EXT);
        virtual ~HandTracker();

        // Error checking

        inline bool valid() const
        {
            return _handTracker != XR_NULL_HANDLE;
        }

        inline bool check(XrResult result, const char *warnMsg) const
        {
            return _session->check(result, warnMsg);
        }

        // Conversions

        inline const osg::ref_ptr<Instance> getInstance() const
        {
            return _session->getInstance();
        }

        inline const osg::observer_ptr<Session> getSession() const
        {
            return _session;
        }

        inline XrHandTrackerEXT getXrHandTracker() const
        {
            return _handTracker;
        }

        // Locating a space

        class JointLocation : public Space::Location
        {
            public:

                // Constructors

                JointLocation();
                JointLocation(XrSpaceLocationFlags flags,
                              const osg::Quat &orientation,
                              const osg::Vec3f &position,
                              float radius);

                // Accessors

                // This is undefined unless isPositionValid().
                float getRadius() const
                {
                    return _radius;
                }

            protected:

                float _radius;
        };

        class JointLocations
        {
            public:

                // Constructors

                JointLocations();

                // Accessors

                bool isActive() const
                {
                    return _isActive;
                }

                unsigned int getNumJoints() const
                {
                    return _jointLocations.size();
                }

                const JointLocation &operator [](XrHandJointEXT joint) const
                {
                    assert(!_jointLocations.empty());
                    return _jointLocations[joint];
                }

            protected:

                friend class HandTracker;

                bool _isActive;
                std::vector<JointLocation> _jointLocations;

        };

        bool locate(const Space *baseSpace, XrTime time,
                    JointLocations &locations);

    protected:

        // useful info
        unsigned int _jointCount;

        osg::observer_ptr<Session> _session;
        XrHandTrackerEXT _handTracker;
};

} // osgXR::OpenXR

} // osgXR

#endif
