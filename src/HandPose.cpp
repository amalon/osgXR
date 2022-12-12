// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#include "HandPose.h"

#include <osg/BoundingSphere>

using namespace osgXR;

// HandPose::HandDimentions

HandPose::HandDimentions::HandDimentions(const HandPose &handPose)
{
    _palmWidth = handPose.getPalmWidth();
    for (unsigned int j = 0; j < JOINT_COUNT; ++j) {
        RelativeJointLocation loc = handPose.getJointLocationRelative((Joint)j);
        if (j != JOINT_ROOT && loc.isPositionValid())
            _jointLengths[j] = loc.getLength();
        else
            _jointLengths[j] = 0.0f;
    }
}

// HandPose::HandMotionRanges

HandPose::HandMotionRanges::HandMotionRanges()
{
    for (unsigned int j = 0; j < JOINT_COUNT; ++j) {
        _minJointAngles[j].set( M_PI,  M_PI,  M_PI);
        _maxJointAngles[j].set(-M_PI, -M_PI, -M_PI);
    }
}

void HandPose::HandMotionRanges::extend(const HandPose &handPose)
{
    for (unsigned int j = 0; j < JOINT_COUNT; ++j) {
        RelativeJointLocation loc = handPose.getJointLocationRelative((Joint)j);
        if (loc.isOrientationValid()) {
            double angle;
            osg::Vec3f vec;
            loc.getOrientation().getRotate(angle, vec);
            vec *= angle;
            for (unsigned int i = 0; i < 3; ++i) {
                if (vec[i] > _maxJointAngles[j][i])
                    _maxJointAngles[j][i] = vec[i];
                if (vec[i] < _minJointAngles[j][i])
                    _minJointAngles[j][i] = vec[i];
            }
        }
    }
}

// HandPose::BasicHandPose

HandPose::BasicHandPose::BasicHandPose(const HandPose &handPose) :
    _fingerSqueeze{}
{
    // FIXME implement
}

// HandPose::JointLocation

HandPose::JointLocation::JointLocation() :
    _radius(0.0f)
{
}

HandPose::JointLocation::JointLocation(Flags flags,
                                       const osg::Quat &orientation,
                                       const osg::Vec3f &position,
                                       float radius) :
    ActionPose::Location(flags, orientation, position),
    _radius(radius)
{
}

// HandPose::RelativeJointLocation

HandPose::RelativeJointLocation::RelativeJointLocation(Flags flags,
                                                       const osg::Quat &orientation,
                                                       const osg::Vec3f &position,
                                                       float radius) :
    JointLocation(flags, orientation, position, radius)
{
}

HandPose::RelativeJointLocation::RelativeJointLocation(const JointLocation &location,
                                                       const JointLocation &relativeTo) :
    JointLocation(location)
{
    unsigned int flags = _flags;

    // Rotate joint orientation by inverse of parent orientation
    if (isOrientationValid() && relativeTo.isOrientationValid()) {
        _orientation /= relativeTo.getOrientation();
    } else {
        flags &= ~JointLocation::ORIENTATION_VALID_BIT;
    }

    // Subtract other location from joint location and rotate
    if (isPositionValid() && relativeTo.isPositionValid()) {
        _position -= relativeTo.getPosition();
        if (relativeTo.isOrientationValid())
            _position = relativeTo.getOrientation().inverse() * _position;
    } else {
        flags &= ~JointLocation::POSITION_VALID_BIT;
    }

    _flags = (JointLocation::Flags)flags;
}

// HandPose::RelativeJointVelocity

HandPose::RelativeJointVelocity::RelativeJointVelocity(Flags flags,
                                                       const osg::Vec3f &linear,
                                                       const osg::Vec3f &angular) :
    JointVelocity(flags, linear, angular)
{
}

HandPose::RelativeJointVelocity::RelativeJointVelocity(const JointVelocity &velocity,
                                                       const JointLocation &relativeLoc,
                                                       const JointVelocity &relativeVel) :
    JointVelocity(velocity)
{
    unsigned int flags = _flags;

    // Subtract other linear velocity from joint velocity and rotate
    if (isLinearValid() && relativeVel.isLinearValid()) {
        _linear -= relativeVel.getLinear();
        if (relativeLoc.isOrientationValid())
            _linear = relativeLoc.getOrientation().inverse() * _linear;
    } else {
        flags &= ~JointVelocity::LINEAR_VALID_BIT;
    }

    // Subtract other angular velocity from joint velocity and rotate
    if (isAngularValid() && relativeVel.isAngularValid()) {
        _angular -= relativeVel.getAngular();
        if (relativeLoc.isOrientationValid())
            _angular = relativeLoc.getOrientation().inverse() * _angular;
    } else {
        flags &= ~JointVelocity::ANGULAR_VALID_BIT;
    }

    _flags = (JointVelocity::Flags)flags;
}

// Internal HandPose API

HandPose::Private::Private() :
    _active(false)
{
}

HandPose::Private::~Private()
{
}

// HandPose

HandPose::HandPose() :
    _private(new Private())
{
}

void HandPose::advance(float dt)
{
}

bool HandPose::isActive() const
{
    return _private->_active;
}

float HandPose::getPalmWidth() const
{
    const JointLocation &proxIndex = _private->_jointLocations[JOINT_INDEX_PROXIMAL];
    const JointLocation &proxLittle = _private->_jointLocations[JOINT_LITTLE_PROXIMAL];
    if (proxIndex.isPositionValid() && proxLittle.isPositionValid())
        return proxIndex.getRadius() + proxLittle.getRadius() +
            (proxIndex.getPosition() - proxLittle.getPosition()).length();
    else
        return 0.0f;
}

osg::BoundingBox HandPose::getBoundingBox() const
{
    osg::BoundingBox bb;
    if (isActive())
    {
        for (unsigned int j = 0; j < JOINT_COUNT; ++j)
        {
            auto &loc = getJointLocation((Joint)j);
            bb.expandBy(osg::BoundingSphere(loc.getPosition(),
                                            loc.getRadius()));
        }
    }
    return bb;
}

osg::BoundingBox HandPose::getBoundingBox(const osg::Matrix &transform) const
{
    osg::BoundingBox bb;
    if (isActive())
    {
        for (unsigned int j = 0; j < JOINT_COUNT; ++j)
        {
            auto &loc = getJointLocation((Joint)j);
            bb.expandBy(osg::BoundingSphere(loc.getPosition() * transform,
                                            loc.getRadius()));
        }
    }
    return bb;
}

const HandPose::JointLocation &HandPose::getJointLocation(Joint joint) const
{
    return _private->_jointLocations[joint];
}

const HandPose::JointVelocity &HandPose::getJointVelocity(Joint joint) const
{
    return _private->_jointVelocities[joint];
}

HandPose::RelativeJointLocation HandPose::getJointLocationRelative(Joint joint) const
{
    int parent = getJointParent(joint);

    const JointLocation &jointLoc = _private->_jointLocations[joint];
    if (parent < 0)
        return RelativeJointLocation(jointLoc);

    return RelativeJointLocation(jointLoc, _private->_jointLocations[parent]);
}

HandPose::RelativeJointVelocity HandPose::getJointVelocityRelative(Joint joint) const
{
    int parent = getJointParent(joint);
    const JointVelocity &jointVel = _private->_jointVelocities[joint];
    if (parent < 0)
        return RelativeJointVelocity(jointVel);

    return RelativeJointVelocity(jointVel,
                                 _private->_jointLocations[parent],
                                 _private->_jointVelocities[parent]);
}

const char *HandPose::getJointName(Joint joint)
{
    static const char *jointNames[JOINT_COUNT] = {
        "palm",
        "wrist",
        "thumb_metacarpal",
        "thumb_proximal",
        "thumb_distal",
        "thumb_tip",
        "index_metacarpal",
        "index_proximal",
        "index_intermediate",
        "index_distal",
        "index_tip",
        "middle_metacarpal",
        "middle_proximal",
        "middle_intermediate",
        "middle_distal",
        "middle_tip",
        "ring_metacarpal",
        "ring_proximal",
        "ring_intermediate",
        "ring_distal",
        "ring_tip",
        "little_metacarpal",
        "little_proximal",
        "little_intermediate",
        "little_distal",
        "little_tip",
    };
    return jointNames[joint];
}

void HandPose::setActive(bool active)
{
    _private->_active = active;
}

void HandPose::setBasicPose(const BasicHandPose *basicPose,
                            const HandMotionRanges *motionRanges,
                            const HandDimentions *dimentions,
                            JointMask jointMask)
{
    // FIXME implement
}

void HandPose::setJointLocation(Joint joint, const JointLocation &location)
{
    _private->_jointLocations[joint] = location;
}

void HandPose::setJointVelocity(Joint joint, const JointVelocity &velocity)
{
    _private->_jointVelocities[joint] = velocity;
}

void HandPose::setJointLocationRelative(Joint joint,
                                        const RelativeJointLocation &relative)
{
    // FIXME implement
}

void HandPose::setJointVelocityRelative(Joint joint,
                                        const RelativeJointVelocity &relative)
{
    // FIXME implement
}

void HandPose::setJointsInterpolate(const HandPose &first,
                                    const HandPose &second, float howFar,
                                    JointMask jointMask)
{
    // FIXME implement
}

void HandPose::moveJointsTowards(const HandPose &other, float howFar,
                                 JointMask jointMask)
{
    // FIXME implement
}
