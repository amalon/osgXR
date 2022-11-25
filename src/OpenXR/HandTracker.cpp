// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "HandTracker.h"

using namespace osgXR::OpenXR;

HandTracker::HandTracker(Session *session, XrHandEXT hand,
                         XrHandJointSetEXT handJointSet,
                         unsigned int jointCount) :
    _jointCount(jointCount),
    _session(session),
    _handTracker(XR_NULL_HANDLE)
{
    // Attempt to create a hand tracker
    XrHandTrackerCreateInfoEXT createInfo{ XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT };
    createInfo.hand = hand;
    createInfo.handJointSet = handJointSet;

    check(getInstance()->xrCreateHandTracker(_session->getXrSession(),
                                             &createInfo, &_handTracker),
          "Failed to create OpenXR hand tracker");
}

HandTracker::~HandTracker()
{
    if (_session.valid() && _session->valid() && valid())
    {
        check(getInstance()->xrDestroyHandTracker(_handTracker),
              "Failed to destroy OpenXR hand tracker");
    }
}

HandTracker::JointLocation::JointLocation() :
    _radius(0.0f)
{
}

HandTracker::JointLocation::JointLocation(XrSpaceLocationFlags flags,
                                          const osg::Quat &orientation,
                                          const osg::Vec3f &position,
                                          float radius) :
    Space::Location(flags, orientation, position),
    _radius(radius)
{
}

HandTracker::JointLocations::JointLocations() :
    _isActive(false)
{
}

bool HandTracker::locate(const Space *baseSpace, XrTime time,
                         JointLocations &locations)
{
    if (!_session.valid() || !valid())
        return false;
    assert(_session == baseSpace->getSession());

    // Locate info
    XrHandJointsLocateInfoEXT locateInfo{ XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT };
    locateInfo.baseSpace = baseSpace->getXrSpace();
    locateInfo.time = time;

    // Temporary storage for joint locations
    std::vector<XrHandJointLocationEXT> xrJointLocations;
    xrJointLocations.resize(_jointCount);

    // Locations output
    XrHandJointLocationsEXT xrLocations{ XR_TYPE_HAND_JOINT_LOCATIONS_EXT };
    xrLocations.jointCount = xrJointLocations.size();
    xrLocations.jointLocations = xrJointLocations.data();

    bool ret = check(getInstance()->xrLocateHandJoints(_handTracker,
                                                       &locateInfo,
                                                       &xrLocations),
                     "Failed to locate OpenXR hand joints");
    if (ret)
    {
        // Copy into JointLocation array
        locations._isActive = xrLocations.isActive;
        locations._jointLocations.reserve(_jointCount);
        for (unsigned int i = 0; i < _jointCount; ++i)
        {
            const struct XrHandJointLocationEXT &loc = xrLocations.jointLocations[i];
            osg::Quat orientation(loc.pose.orientation.x,
                                  loc.pose.orientation.y,
                                  loc.pose.orientation.z,
                                  loc.pose.orientation.w);
            osg::Vec3f position(loc.pose.position.x,
                                loc.pose.position.y,
                                loc.pose.position.z);
            locations._jointLocations.push_back(JointLocation(loc.locationFlags,
                                                              orientation,
                                                              position,
                                                              loc.radius));
        }
    }
    else
    {
        locations = JointLocations();
    }
    return ret;
}
