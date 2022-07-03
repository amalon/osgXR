// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "Space.h"

#include <cassert>

using namespace osgXR::OpenXR;

static XrPosef poseIdentity = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } };

Space::Space(Session *session, XrReferenceSpaceType type) :
    _session(session),
    _space(XR_NULL_HANDLE)
{
    // Attempt to create a reference space
    XrReferenceSpaceCreateInfo createInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    createInfo.referenceSpaceType = type;
    createInfo.poseInReferenceSpace = poseIdentity;

    check(xrCreateReferenceSpace(session->getXrSession(), &createInfo, &_space),
          "Failed to create OpenXR reference space");
}

Space::Space(Session *session, ActionPose *action,
             Path subactionPath) :
    _session(session),
    _space(XR_NULL_HANDLE)
{
    // Attempt to create an action space for this pose action
    XrActionSpaceCreateInfo createInfo{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
    createInfo.action = action->getXrAction();
    createInfo.subactionPath = subactionPath.getXrPath();
    createInfo.poseInActionSpace = poseIdentity;

    check(xrCreateActionSpace(session->getXrSession(), &createInfo, &_space),
          "Failed to create OpenXR action space");
}

Space::~Space()
{
    if (_session.valid() && _session->valid() && valid())
    {
        check(xrDestroySpace(_space),
              "Failed to destroy OpenXR space");
    }
}

Space::Location::Location() :
    _flags(0)
{
}

Space::Location::Location(XrSpaceLocationFlags flags,
                          const osg::Quat &orientation,
                          const osg::Vec3f &position) :
    _flags(flags),
    _orientation(orientation),
    _position(position)
{
}

bool Space::locate(const Space *baseSpace, XrTime time,
                   Space::Location &location)
{
    if (!_session.valid() || !valid())
        return false;
    assert(_session == baseSpace->_session);

    XrSpaceLocation spaceLocation{ XR_TYPE_SPACE_LOCATION };
    bool ret = check(xrLocateSpace(getXrSpace(),
                                   baseSpace->getXrSpace(),
                                   time,
                                   &spaceLocation),
                     "Failed to locate OpenXR space");
    if (ret)
    {
        osg::Quat orientation(spaceLocation.pose.orientation.x,
                              spaceLocation.pose.orientation.y,
                              spaceLocation.pose.orientation.z,
                              spaceLocation.pose.orientation.w);
        osg::Vec3f position(spaceLocation.pose.position.x,
                            spaceLocation.pose.position.y,
                            spaceLocation.pose.position.z);
        location = Location(spaceLocation.locationFlags,
                            orientation,
                            position);
    }
    else
    {
        location = Location();
    }
    return ret;
}
