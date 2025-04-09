// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "Space.h"
#include "Session.h"

#include <cassert>

using namespace osgXR::OpenXR;

static const XrPosef poseIdentity = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } };

Space::Space(Session *session, XrReferenceSpaceType type,
             const Location &locInRefSpace) :
    _session(session),
    _space(XR_NULL_HANDLE)
{
    // Attempt to create a reference space
    XrReferenceSpaceCreateInfo createInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    createInfo.referenceSpaceType = type;
    if (locInRefSpace.isOrientationValid()) {
        auto &orientation = locInRefSpace.getOrientation();
        createInfo.poseInReferenceSpace.orientation.x = orientation.x();
        createInfo.poseInReferenceSpace.orientation.y = orientation.y();
        createInfo.poseInReferenceSpace.orientation.z = orientation.z();
        createInfo.poseInReferenceSpace.orientation.w = orientation.w();
    } else {
        createInfo.poseInReferenceSpace.orientation = poseIdentity.orientation;
    }
    if (locInRefSpace.isPositionValid()) {
        auto &position = locInRefSpace.getPosition();
        createInfo.poseInReferenceSpace.position.x = position.x();
        createInfo.poseInReferenceSpace.position.y = position.y();
        createInfo.poseInReferenceSpace.position.z = position.z();
    } else {
        createInfo.poseInReferenceSpace.position = poseIdentity.position;
    }

    check(xrCreateReferenceSpace(session->getXrSession(), &createInfo, &_space),
          "create OpenXR reference space");
}

Space::Space(Session *session, XrReferenceSpaceType type) :
    Space(session, type, Location())
{
}

Space::Space(Session *session, ActionPose *action,
             Path subactionPath) :
    _session(session),
    _space(XR_NULL_HANDLE)
{
    // Action must be registered with OpenXR
    assert(action->valid());

    // Attempt to create an action space for this pose action
    XrActionSpaceCreateInfo createInfo{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
    createInfo.action = action->getXrAction();
    createInfo.subactionPath = subactionPath.getXrPath();
    createInfo.poseInActionSpace = poseIdentity;

    check(xrCreateActionSpace(session->getXrSession(), &createInfo, &_space),
          "create OpenXR action space");
}

Space::~Space()
{
    if (_session.valid() && _session->valid() && valid())
    {
        check(xrDestroySpace(_space),
              "destroy OpenXR space");
    }
}

bool Space::check(XrResult result, const char *actionMsg) const
{
    return _session->check(result, actionMsg);
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
                     "locate OpenXR space");
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
