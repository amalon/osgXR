// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2025 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_MANAGED_SPACE
#define OSGXR_OPENXR_MANAGED_SPACE 1

#include "Space.h"

#include <osg/ref_ptr>

#include <list>

namespace osgXR {

namespace OpenXR {

/**
 * Helper to manage OpenXR spaces which can be recentered by the app and
 * OpenXR runtime.
 */
class ManagedSpace
{
    public:

        typedef Space::Location Location;

        /// Create a reference space with a pose.
        ManagedSpace(Session *session, XrReferenceSpaceType type,
                     const Location &locInRefSpace);
        /// Create a reference space.
        ManagedSpace(Session *session, XrReferenceSpaceType type);

        // Error checking

        inline bool valid(XrTime time) const
        {
            return getSpace(time)->valid();
        }

        inline bool check(XrResult result, const char *actionMsg) const
        {
            return _stateQueue.front().space->check(result, actionMsg);
        }

        // Conversions

        /// Find the last current or pending space before @p time.
        osg::ref_ptr<Space> getSpace(XrTime time) const;

        inline XrSpace getXrSpace(XrTime time) const
        {
            return getSpace(time)->getXrSpace();
        }

        inline bool locate(const Space *baseSpace, XrTime time,
                           Location &location)
        {
            return getSpace(time)->locate(baseSpace, time, location);
        }

        // Events

        /// Notify that a frame has ended.
        void endFrame(XrTime time);

        /** Recenter the space now.
         * @return true on success, false otherwise
         */
        bool recenter(XrTime changeTime,
                      const Location &locInPreviousSpace);

        /// Notify that the underlying reference space is changing.
        void onChangePending(const XrEventDataReferenceSpaceChangePending *event);

    protected:

        /// Reference space type.
        XrReferenceSpaceType _type;

        struct SpaceState
        {
            XrTime changeTime;
            osg::ref_ptr<Space> space;
            Location loc;

            SpaceState(XrTime newChangeTime, Space *newSpace,
                       const Location &newLoc) :
                changeTime(newChangeTime),
                space(newSpace),
                loc(newLoc)
            {
            }
        };
        /// Pending space states.
        std::list<SpaceState> _stateQueue;
};

} // osgXR::OpenXR

} // osgXR

#endif
