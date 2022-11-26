// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#include "HandPoseTracked.h"
#include "HandPose.h"

#include "OpenXR/HandTracker.h"

#include <osgXR/Manager>

using namespace osgXR;

// Internal HandPoseTracked API

HandPoseTracked::Private::Private(Hand hand) :
    _hand(hand)
{
}

void HandPoseTracked::Private::setup(OpenXR::Session *session)
{
    // Only create a hand tracker if the system supports it
    if (!session->getSystem()->getHandTracking())
        return;

    _handTracker = new OpenXR::HandTracker(session, (XrHandEXT)_hand);
}

void HandPoseTracked::Private::cleanupSession()
{
    _handTracker = nullptr;
}

// HandPoseTracked

HandPoseTracked::HandPoseTracked(Manager *manager, Hand hand) :
    _private(new Private(hand))
{
    _private->registerState(manager->_getXrState());
}

HandPoseTracked::~HandPoseTracked()
{
    _private->unregisterState();
}

void HandPoseTracked::update()
{
    Private *priv = _private.get();

    if (!priv->_handTracker.valid() || !priv->_handTracker->valid())
        return;

    OpenXR::HandTracker::JointLocations locations;
    if (priv->_handTracker->locate(priv->_handTracker->getSession()->getLocalSpace(),
                                   priv->_handTracker->getSession()->getLastDisplayTime(),
                                   locations))
    {
        setActive(locations.isActive());
        for (unsigned int i = 0; i < locations.getNumJoints(); ++i)
        {
            const auto &loc = locations[(XrHandJointEXT)i];
            setJointLocation((HandPose::Joint)i,
                             JointLocation((JointLocation::Flags)loc.getFlags(),
                                           loc.getOrientation(),
                                           loc.getPosition(),
                                           loc.getRadius()));
        }
        // Just duplicate the wrist as the elbow joint
        setJointLocation(JOINT_ELBOW, getJointLocation(JOINT_WRIST));
    }
    else
    {
        setActive(false);
    }
}
