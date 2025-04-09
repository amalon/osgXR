// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_SPACE
#define OSGXR_OPENXR_SPACE 1

#include "Action.h"
#include "Path.h"

#include <osg/Quat>
#include <osg/Vec3f>
#include <osg/observer_ptr>

namespace osgXR {

namespace OpenXR {

class Session;

class Space : public osg::Referenced
{
    public:

        class Location;

        /// Create a reference space with a pose
        Space(Session *session, XrReferenceSpaceType type,
              const Location &locInRefSpace);
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

        bool check(XrResult result, const char *actionMsg) const;

        // Conversions

        inline OpenXR::Session *getSession() const
        {
            return _session.get();
        }

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

                osg::Quat &getOrientation()
                {
                    return _orientation;
                }

                const osg::Quat &getOrientation() const
                {
                    return _orientation;
                }

                osg::Vec3f &getPosition()
                {
                    return _position;
                }

                const osg::Vec3f &getPosition() const
                {
                    return _position;
                }

                // Adjust by another relative location pose

                Location operator *(const Location &rel) const
                {
                    return Space::Location(_flags | rel._flags,
                                           _orientation * rel._orientation,
                                           _position + _orientation * rel._position);
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
