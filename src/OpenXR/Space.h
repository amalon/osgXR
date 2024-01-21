// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_SPACE
#define OSGXR_OPENXR_SPACE 1

#include "Action.h"
#include "Path.h"
#include "Session.h"

#include <osg/Quat>
#include <osg/Vec3f>
#include <osg/observer_ptr>

namespace osgXR {

namespace OpenXR {

class Space : public osg::Referenced
{
    public:

        /// Create a reference space
        Space(Session *session, XrReferenceSpaceType type);
        /// Create an action space
        Space(Session *session, ActionPose *action,
              Path subactionPath = Path());
        virtual ~Space();

        // Error checking

        inline bool valid() const
        {
            return _space != XR_NULL_HANDLE;
        }

        inline bool check(XrResult result, const char *actionMsg) const
        {
            return _session->check(result, actionMsg);
        }

        // Conversions

        inline XrSpace getXrSpace() const
        {
            return _space;
        }

        // Locating a space

        class Location
        {
            public:

                // Constructors

                Location();
                Location(XrSpaceLocationFlags flags,
                         const osg::Quat &orientation,
                         const osg::Vec3f &position);

                // Error checking

                inline bool valid() const
                {
                    return _flags != 0;
                }

                // Accessors

                bool isOrientationValid() const
                {
                    return _flags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
                }

                bool isPositionValid() const
                {
                    return _flags & XR_SPACE_LOCATION_POSITION_VALID_BIT;
                }

                bool isOrientationTracked() const
                {
                    return _flags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
                }

                bool isPositionTracked() const
                {
                    return _flags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
                }

                XrSpaceLocationFlags getFlags() const
                {
                    return _flags;
                }

                const osg::Quat &getOrientation() const
                {
                    return _orientation;
                }

                const osg::Vec3f &getPosition() const
                {
                    return _position;
                }

            protected:

                XrSpaceLocationFlags _flags;
                osg::Quat _orientation;
                osg::Vec3f _position;
        };

        bool locate(const Space *baseSpace, XrTime time,
                    Location &location);

    protected:

        osg::observer_ptr<Session> _session;
        XrSpace _space;
};

} // osgXR::OpenXR

} // osgXR

#endif
