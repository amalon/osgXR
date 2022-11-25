// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

// for M_PI from cmath
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include "HandPose.h"

#include <osg/BoundingSphere>

#include <cmath>

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

// HandPose::JointMotionRanges

HandPose::JointMotionRanges::JointMotionRanges()
{
    for (unsigned int j = 0; j < JOINT_COUNT; ++j) {
        _minJointAngles[j].set( M_PI,  M_PI,  M_PI);
        _maxJointAngles[j].set(-M_PI, -M_PI, -M_PI);
    }
}

static osg::Vec3f quatToEuler(const osg::Quat &q)
{
    // https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
    osg::Vec3f angles;

    // roll (x-axis rotation)
    float sinr_cosp = 2.0f * (q.w() * q.x() + q.y() * q.z());
    float cosr_cosp = 1.0f - 2.0f * (q.x() * q.x() + q.y() * q.y());
    angles[0] = std::atan2(sinr_cosp, cosr_cosp);

    // pitch (y-axis rotation)
    float sinp = 2.0f * (q.w() * q.y() - q.z() * q.x());
    if (std::abs(sinp) >= 1.0f)
        angles[1] = std::copysign(M_PI / 2, sinp); // use 90 degrees if out of range
    else
        angles[1] = std::asin(sinp);

    // yaw (z-axis rotation)
    float siny_cosp = 2.0f * (q.w() * q.z() + q.x() * q.y());
    float cosy_cosp = 1.0f - 2.0f * (q.y() * q.y() + q.z() * q.z());
    angles[2] = std::atan2(siny_cosp, cosy_cosp);

    return angles;
}

void HandPose::JointMotionRanges::extend(const HandPose &handPose)
{
    for (unsigned int j = 0; j < JOINT_COUNT; ++j) {
        RelativeJointLocation loc = handPose.getJointLocationRelative((Joint)j);
        if (loc.isOrientationValid()) {
            osg::Vec3f vec = quatToEuler(loc.getOrientation());
            for (unsigned int i = 0; i < 3; ++i) {
                if (vec[i] > _maxJointAngles[j][i])
                    _maxJointAngles[j][i] = vec[i];
                if (vec[i] < _minJointAngles[j][i])
                    _minJointAngles[j][i] = vec[i];
            }
        }
    }
}

void HandPose::JointMotionRanges::extend(Joint joint, const osg::Vec3f &angles)
{
    for (unsigned int i = 0; i < 3; ++i) {
        if (angles[i] > _maxJointAngles[joint][i])
            _maxJointAngles[joint][i] = angles[i];
        if (angles[i] < _minJointAngles[joint][i])
            _minJointAngles[joint][i] = angles[i];
    }
}

void HandPose::JointMotionRanges::extendX(Joint joint, float angle)
{
    if (angle > _maxJointAngles[joint][0])
        _maxJointAngles[joint][0] = angle;
    if (angle < _minJointAngles[joint][0])
        _minJointAngles[joint][0] = angle;
}

void HandPose::JointMotionRanges::extendY(Joint joint, float angle)
{
    if (angle > _maxJointAngles[joint][1])
        _maxJointAngles[joint][1] = angle;
    if (angle < _minJointAngles[joint][1])
        _minJointAngles[joint][1] = angle;
}

void HandPose::JointMotionRanges::extendZ(Joint joint, float angle)
{
    if (angle > _maxJointAngles[joint][2])
        _maxJointAngles[joint][2] = angle;
    if (angle < _minJointAngles[joint][2])
        _minJointAngles[joint][2] = angle;
}

// HandPose::JointAngles

static osg::Vec3f getJointAngles(const HandPose &handPose,
                                 HandPose::Joint joint)
{
    HandPose::RelativeJointLocation loc = handPose.getJointLocationRelative(joint);
    if (!loc.isOrientationValid())
        return osg::Vec3f();

    return quatToEuler(loc.getOrientation());
}

static osg::Vec2f getJointAnglesXY(const HandPose &handPose,
                                   HandPose::Joint joint)
{
    osg::Vec3f v3 = getJointAngles(handPose, joint);
    return osg::Vec2f(v3.x(), v3.y());
}

HandPose::JointAngles::JointAngles(const HandPose &handPose)
{
    // Apply joint angles
    // Wrist
    setWrist(getJointAnglesXY(handPose, JOINT_WRIST));
    // Thumb Metacarpal joint
    setThumbMetacarpal(getJointAngles(handPose, JOINT_THUMB_METACARPAL));
    // Proximal joints
    setProximal(FINGER_THUMB,  getJointAnglesXY(handPose, JOINT_THUMB_PROXIMAL));
    setProximal(FINGER_INDEX,  getJointAnglesXY(handPose, JOINT_INDEX_PROXIMAL));
    setProximal(FINGER_MIDDLE, getJointAnglesXY(handPose, JOINT_MIDDLE_PROXIMAL));
    setProximal(FINGER_RING,   getJointAnglesXY(handPose, JOINT_RING_PROXIMAL));
    setProximal(FINGER_LITTLE, getJointAnglesXY(handPose, JOINT_LITTLE_PROXIMAL));
    // Intermediate joints
    setIntermediate(FINGER_INDEX,  getJointAngles(handPose, JOINT_INDEX_INTERMEDIATE).x());
    setIntermediate(FINGER_MIDDLE, getJointAngles(handPose, JOINT_MIDDLE_INTERMEDIATE).x());
    setIntermediate(FINGER_RING,   getJointAngles(handPose, JOINT_RING_INTERMEDIATE).x());
    setIntermediate(FINGER_LITTLE, getJointAngles(handPose, JOINT_LITTLE_INTERMEDIATE).x());
    // Distal joints
    setDistal(FINGER_THUMB,  getJointAngles(handPose, JOINT_THUMB_DISTAL).x());
    setDistal(FINGER_INDEX,  getJointAngles(handPose, JOINT_INDEX_DISTAL).x());
    setDistal(FINGER_MIDDLE, getJointAngles(handPose, JOINT_MIDDLE_DISTAL).x());
    setDistal(FINGER_RING,   getJointAngles(handPose, JOINT_RING_DISTAL).x());
    setDistal(FINGER_LITTLE, getJointAngles(handPose, JOINT_LITTLE_DISTAL).x());
}

HandPose::JointAngles::JointAngles(const SqueezeValues &squeezeValues,
                                   const JointMotionRanges &motionRanges)
{
    _wrist.set(motionRanges.interpolateJointAngleXY(JOINT_WRIST,
                                1.0f - squeezeValues.getWristBend(), 0.5f));

    float thumbSqueeze = 1.0f - squeezeValues.getFingerSqueeze(FINGER_THUMB);
    _thumbMetacarpal.set(motionRanges.interpolateJointAngle(JOINT_THUMB_METACARPAL,
                                thumbSqueeze,
                                0.5f - 0.5f*squeezeValues.getThumbX().value_or(0.0f),
                                0.5f - 0.5f*squeezeValues.getThumbX().value_or(0.0f)));
    float thumbDistal = thumbSqueeze;
    if (squeezeValues.getThumbY()) {
        thumbDistal = 0.5f + 0.5f*squeezeValues.getThumbY().value();
    }
    _proximals[FINGER_THUMB] = motionRanges.interpolateJointAngleXY(JOINT_THUMB_PROXIMAL,
                                thumbSqueeze,
                                0.5f - 0.5f*squeezeValues.getThumbX().value_or(0.0f));
    _proximals[FINGER_INDEX] = motionRanges.interpolateJointAngleXY(JOINT_INDEX_PROXIMAL,
                                1.0f - squeezeValues.getFingerSqueeze(FINGER_INDEX), 0.5f);
    _proximals[FINGER_MIDDLE] = motionRanges.interpolateJointAngleXY(JOINT_MIDDLE_PROXIMAL,
                                1.0f - squeezeValues.getFingerSqueeze(FINGER_MIDDLE), 0.5f);
    _proximals[FINGER_RING] = motionRanges.interpolateJointAngleXY(JOINT_RING_PROXIMAL,
                                1.0f - squeezeValues.getFingerSqueeze(FINGER_RING), 0.5f);
    _proximals[FINGER_LITTLE] = motionRanges.interpolateJointAngleXY(JOINT_LITTLE_PROXIMAL,
                                1.0f - squeezeValues.getFingerSqueeze(FINGER_LITTLE), 0.5f);

    _intermediates[0] = motionRanges.interpolateJointAngleX(JOINT_INDEX_INTERMEDIATE,
                                1.0f - squeezeValues.getFingerSqueeze(FINGER_INDEX));
    _intermediates[1] = motionRanges.interpolateJointAngleX(JOINT_MIDDLE_INTERMEDIATE,
                                1.0f - squeezeValues.getFingerSqueeze(FINGER_MIDDLE));
    _intermediates[2] = motionRanges.interpolateJointAngleX(JOINT_RING_INTERMEDIATE,
                                1.0f - squeezeValues.getFingerSqueeze(FINGER_RING));
    _intermediates[3] = motionRanges.interpolateJointAngleX(JOINT_LITTLE_INTERMEDIATE,
                                1.0f - squeezeValues.getFingerSqueeze(FINGER_LITTLE));

    _distals[FINGER_THUMB] = motionRanges.interpolateJointAngleX(JOINT_THUMB_DISTAL,
                                thumbDistal);
    _distals[FINGER_INDEX] = motionRanges.interpolateJointAngleX(JOINT_INDEX_DISTAL,
                                1.0f - squeezeValues.getFingerSqueeze(FINGER_INDEX));
    _distals[FINGER_MIDDLE] = motionRanges.interpolateJointAngleX(JOINT_MIDDLE_DISTAL,
                                1.0f - squeezeValues.getFingerSqueeze(FINGER_MIDDLE));
    _distals[FINGER_RING] = motionRanges.interpolateJointAngleX(JOINT_RING_DISTAL,
                                1.0f - squeezeValues.getFingerSqueeze(FINGER_RING));
    _distals[FINGER_LITTLE] = motionRanges.interpolateJointAngleX(JOINT_LITTLE_DISTAL,
                                1.0f - squeezeValues.getFingerSqueeze(FINGER_LITTLE));
}

// HandPose::SqueezeValues

HandPose::SqueezeValues::SqueezeValues(const JointAngles &jointAngles,
                                       const JointMotionRanges &motionRanges)
{
    // Approximate reverse of JointAngles(SqueezeValues, JointMotionRanges)
    _wristBend = 1.0f - motionRanges.ratioJointAngle(JOINT_WRIST,
                                        jointAngles.getWrist());

    _fingerSqueeze[FINGER_THUMB]  = motionRanges.ratioJointAngle(JOINT_THUMB_PROXIMAL,
                                        jointAngles.getProximal(FINGER_THUMB));
    _fingerSqueeze[FINGER_INDEX]  = motionRanges.ratioJointAngle(JOINT_INDEX_PROXIMAL,
                                        jointAngles.getProximal(FINGER_INDEX));
    _fingerSqueeze[FINGER_MIDDLE] = motionRanges.ratioJointAngle(JOINT_MIDDLE_PROXIMAL,
                                        jointAngles.getProximal(FINGER_MIDDLE));
    _fingerSqueeze[FINGER_RING]   = motionRanges.ratioJointAngle(JOINT_RING_PROXIMAL,
                                        jointAngles.getProximal(FINGER_RING));
    _fingerSqueeze[FINGER_LITTLE] = motionRanges.ratioJointAngle(JOINT_LITTLE_PROXIMAL,
                                        jointAngles.getProximal(FINGER_LITTLE));

    _fingerSqueeze[FINGER_INDEX]  += motionRanges.ratioJointAngle(JOINT_INDEX_INTERMEDIATE,
                                        jointAngles.getIntermediate(FINGER_INDEX));
    _fingerSqueeze[FINGER_MIDDLE] += motionRanges.ratioJointAngle(JOINT_MIDDLE_INTERMEDIATE,
                                        jointAngles.getIntermediate(FINGER_MIDDLE));
    _fingerSqueeze[FINGER_RING]   += motionRanges.ratioJointAngle(JOINT_RING_INTERMEDIATE,
                                        jointAngles.getIntermediate(FINGER_RING));
    _fingerSqueeze[FINGER_LITTLE] += motionRanges.ratioJointAngle(JOINT_LITTLE_INTERMEDIATE,
                                        jointAngles.getIntermediate(FINGER_LITTLE));

    _fingerSqueeze[FINGER_THUMB]  += motionRanges.ratioJointAngle(JOINT_THUMB_DISTAL,
                                        jointAngles.getDistal(FINGER_THUMB));
    _fingerSqueeze[FINGER_INDEX]  += motionRanges.ratioJointAngle(JOINT_INDEX_DISTAL,
                                        jointAngles.getDistal(FINGER_INDEX));
    _fingerSqueeze[FINGER_MIDDLE] += motionRanges.ratioJointAngle(JOINT_MIDDLE_DISTAL,
                                        jointAngles.getDistal(FINGER_MIDDLE));
    _fingerSqueeze[FINGER_RING]   += motionRanges.ratioJointAngle(JOINT_RING_DISTAL,
                                        jointAngles.getDistal(FINGER_RING));
    _fingerSqueeze[FINGER_LITTLE] += motionRanges.ratioJointAngle(JOINT_LITTLE_DISTAL,
                                        jointAngles.getDistal(FINGER_LITTLE));

    _fingerSqueeze[FINGER_THUMB]  = 1.0f - _fingerSqueeze[FINGER_THUMB]  / 2;
    _fingerSqueeze[FINGER_INDEX]  = 1.0f - _fingerSqueeze[FINGER_INDEX]  / 3;
    _fingerSqueeze[FINGER_MIDDLE] = 1.0f - _fingerSqueeze[FINGER_MIDDLE] / 3;
    _fingerSqueeze[FINGER_RING]   = 1.0f - _fingerSqueeze[FINGER_RING]   / 3;
    _fingerSqueeze[FINGER_LITTLE] = 1.0f - _fingerSqueeze[FINGER_LITTLE]  / 3;
}

// HandPose::JointLocation

HandPose::JointLocation::JointLocation() :
    _radius(0.0f)
{
}

HandPose::JointLocation::JointLocation(const JointLocation &other) :
    Pose(other),
    _radius(other._radius)
{
}

HandPose::JointLocation::JointLocation(Flags flags,
                                       const osg::Quat &orientation,
                                       const osg::Vec3f &position,
                                       float radius) :
    Pose(flags, orientation, position),
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

HandPose::JointLocation HandPose::RelativeJointLocation::getAbsolute(const JointLocation &relativeTo) const
{
    unsigned int flags = 0;
    osg::Vec3f pos;
    osg::Quat ori;

    if (isOrientationValid() && relativeTo.isOrientationValid())
    {
        ori = getOrientation() * relativeTo.getOrientation();
        flags |= ORIENTATION_VALID_BIT;
    }
    if (isOrientationTracked() && relativeTo.isOrientationTracked())
        flags |= ORIENTATION_TRACKED_BIT;
    if (isPositionValid() && relativeTo.isPositionValid() && relativeTo.isOrientationValid())
    {
        pos = relativeTo.getPosition() + relativeTo.getOrientation() * getPosition();
        flags |= POSITION_VALID_BIT;
    }
    if (isPositionTracked() && relativeTo.isPositionTracked() && relativeTo.isOrientationTracked())
        flags |= POSITION_TRACKED_BIT;

    return JointLocation((Flags)flags, ori, pos, getRadius());
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

HandPose::Private::Private(const Private &other)
{
    operator =(other);
}

HandPose::Private::~Private()
{
}

HandPose::Private &HandPose::Private::operator =(const Private &other)
{
    _active = other._active;
    for (unsigned int j = 0; j < JOINT_COUNT; ++j) {
        _jointLocations[j] = other._jointLocations[j];
        _jointVelocities[j] = other._jointVelocities[j];
    }
    return *this;
}

// HandPose

static HandPose::Joint jointsDepthFirst[HandPose::JOINT_COUNT] = {
    HandPose::JOINT_ELBOW,
    HandPose::JOINT_WRIST,
    HandPose::JOINT_THUMB_METACARPAL,
    HandPose::JOINT_THUMB_PROXIMAL,
    HandPose::JOINT_THUMB_DISTAL,
    HandPose::JOINT_THUMB_TIP,
    HandPose::JOINT_INDEX_METACARPAL,
    HandPose::JOINT_INDEX_PROXIMAL,
    HandPose::JOINT_INDEX_INTERMEDIATE,
    HandPose::JOINT_INDEX_DISTAL,
    HandPose::JOINT_INDEX_TIP,
    HandPose::JOINT_MIDDLE_METACARPAL,
    HandPose::JOINT_PALM,
    HandPose::JOINT_MIDDLE_PROXIMAL,
    HandPose::JOINT_MIDDLE_INTERMEDIATE,
    HandPose::JOINT_MIDDLE_DISTAL,
    HandPose::JOINT_MIDDLE_TIP,
    HandPose::JOINT_RING_METACARPAL,
    HandPose::JOINT_RING_PROXIMAL,
    HandPose::JOINT_RING_INTERMEDIATE,
    HandPose::JOINT_RING_DISTAL,
    HandPose::JOINT_RING_TIP,
    HandPose::JOINT_LITTLE_METACARPAL,
    HandPose::JOINT_LITTLE_PROXIMAL,
    HandPose::JOINT_LITTLE_INTERMEDIATE,
    HandPose::JOINT_LITTLE_DISTAL,
    HandPose::JOINT_LITTLE_TIP,
};

HandPose::HandPose(const HandPose &other) :
    _private(new Private(*other._private))
{
}

HandPose::HandPose() :
    _private(new Private())
{
}

HandPose::~HandPose()
{
}

void HandPose::update()
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

void HandPose::expandBoundingBox(osg::BoundingBox &bb) const
{
    if (isActive())
    {
        for (unsigned int j = 0; j < JOINT_COUNT; ++j)
        {
            auto &loc = getJointLocation((Joint)j);
            bb.expandBy(osg::BoundingSphere(loc.getPosition(),
                                            loc.getRadius()));
        }
    }
}

void HandPose::expandBoundingBox(osg::BoundingBoxd &bb,
                                 const osg::Matrix &transform) const
{
    if (isActive())
    {
        for (unsigned int j = 0; j < JOINT_COUNT; ++j)
        {
            auto &loc = getJointLocation((Joint)j);
            bb.expandBy(osg::BoundingSphered((osg::Vec3d)loc.getPosition() * transform,
                                             loc.getRadius()));
        }
    }
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

unsigned int HandPose::getJointsDescendents(unsigned int jointMask)
{
    if (jointMask & JOINT_MIDDLE_METACARPAL_BIT)
        jointMask |= JOINT_PALM_BIT;

    for (unsigned int j = 0; j < JOINT_COUNT; ++j)
        if (jointMask & (1 << j))
            jointMask |= getJointDescendents((Joint)j);
    return jointMask;
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
        "elbow",
    };
    return jointNames[joint];
}

HandPose &HandPose::operator =(const HandPose &other)
{
    *_private = *other._private;
    return *this;
}

void HandPose::setActive(bool active)
{
    _private->_active = active;
}

void HandPose::slerp(const HandPose &other, float ratio, unsigned int jointMask)
{
    // No change unless ratio non-zero
    if (ratio <= 0.0f)
        return;
    // Just assign to other if ratio is one
    if (ratio >= 1.0f) {
        *this = other;
        return;
    }

    // Find which joints are affected by jointMask
    unsigned int jointsAffected = getJointsDescendents(jointMask);

    // Convert those joints to relative locations
    RelativeJointLocation relJoints[JOINT_COUNT];
    RelativeJointLocation relJointsOther[JOINT_COUNT];
    for (unsigned int j = 0; j < JOINT_COUNT; ++j) {
        if (jointsAffected & (1 << j)) {
            relJoints[j] = getJointLocationRelative((Joint)j);
            relJointsOther[j] = other.getJointLocationRelative((Joint)j);
        }
    }

    float invRatio = 1.0f - ratio;
    for (unsigned int j = 0; j < JOINT_COUNT; ++j) {
        if (jointMask & (1 << j)) {
            // Slerp the orientations
            osg::Quat ori;
            ori.slerp(ratio,
                      relJoints[j].getOrientation(),
                      relJointsOther[j].getOrientation());
            relJoints[j].setOrientation(ori);
            // Lerp the positions
            relJoints[j].setPosition(relJoints[j].getPosition()*invRatio +
                                     relJointsOther[j].getPosition()*ratio);
        }
    }

    // Convert the affected joints back to absolute locations
    for (unsigned int i = 0; i < JOINT_COUNT; ++i) {
        Joint joint = jointsDepthFirst[i];
        if (jointsAffected & (1 << joint))
            setJointLocationRelative(joint, relJoints[joint]);
    }
}

static void setJointAngles(HandPose::RelativeJointLocation &rel,
                           float x, float y, float z)
{
    osg::Quat q;
    q.makeRotate(x, osg::Vec3f(1.0f, 0.0f, 0.0f),
                 y, osg::Vec3f(0.0f, 1.0f, 0.0f),
                 z, osg::Vec3f(0.0f, 0.0f, 1.0f));
    rel.setOrientation(q);
}

static void setJointAngles(HandPose::RelativeJointLocation *rel,
                           unsigned int jointMask,
                           HandPose::Joint joint,
                           const HandPose::JointMotionRanges &motionRanges,
                           float x)
{
    if (jointMask & (1 << joint))
    {
        auto mid = motionRanges.getMidJointAngle(joint);
        setJointAngles(rel[joint], x, mid.y(), mid.z());
    }
}

static void setJointAngles(HandPose::RelativeJointLocation *rel,
                           unsigned int jointMask,
                           HandPose::Joint joint,
                           const HandPose::JointMotionRanges &motionRanges,
                           const osg::Vec2f &xy)
{
    if (jointMask & (1 << joint))
    {
        auto mid = motionRanges.getMidJointAngle(joint);
        setJointAngles(rel[joint], xy.x(), xy.y(), mid.z());
    }
}

static void setJointAngles(HandPose::RelativeJointLocation *rel,
                           unsigned int jointMask,
                           HandPose::Joint joint,
                           const osg::Vec3f &xyz)
{
    if (jointMask & (1 << joint))
        setJointAngles(rel[joint], xyz.x(), xyz.y(), xyz.z());
}

void HandPose::setPose(const JointAngles &jointAngles,
                       const JointMotionRanges &motionRanges,
                       const HandDimentions *dimentions,
                       unsigned int jointMask)
{
    // Find which joints are affected by jointMask
    unsigned int jointsAffected = getJointsDescendents(jointMask);

    // Convert those joints to relative locations
    RelativeJointLocation relJoints[JOINT_COUNT];
    for (unsigned int j = 0; j < JOINT_COUNT; ++j)
        if (jointsAffected & (1 << j))
            relJoints[j] = getJointLocationRelative((Joint)j);

    // Apply joint angles
    // Wrist
    setJointAngles(relJoints, jointMask, JOINT_WRIST, motionRanges,
                   jointAngles.getWrist());
    // Thumb Metacarpal joint
    setJointAngles(relJoints, jointMask, JOINT_THUMB_METACARPAL,
                   jointAngles.getThumbMetacarpal());
    // Proximal joints
    setJointAngles(relJoints, jointMask, JOINT_THUMB_PROXIMAL, motionRanges,
                   jointAngles.getProximal(FINGER_THUMB));
    setJointAngles(relJoints, jointMask, JOINT_INDEX_PROXIMAL, motionRanges,
                   jointAngles.getProximal(FINGER_INDEX));
    setJointAngles(relJoints, jointMask, JOINT_MIDDLE_PROXIMAL, motionRanges,
                   jointAngles.getProximal(FINGER_MIDDLE));
    setJointAngles(relJoints, jointMask, JOINT_RING_PROXIMAL, motionRanges,
                   jointAngles.getProximal(FINGER_RING));
    setJointAngles(relJoints, jointMask, JOINT_LITTLE_PROXIMAL, motionRanges,
                   jointAngles.getProximal(FINGER_LITTLE));
    // Intermediate joints
    setJointAngles(relJoints, jointMask, JOINT_INDEX_INTERMEDIATE, motionRanges,
                   jointAngles.getIntermediate(FINGER_INDEX));
    setJointAngles(relJoints, jointMask, JOINT_MIDDLE_INTERMEDIATE, motionRanges,
                   jointAngles.getIntermediate(FINGER_MIDDLE));
    setJointAngles(relJoints, jointMask, JOINT_RING_INTERMEDIATE, motionRanges,
                   jointAngles.getIntermediate(FINGER_RING));
    setJointAngles(relJoints, jointMask, JOINT_LITTLE_INTERMEDIATE, motionRanges,
                   jointAngles.getIntermediate(FINGER_LITTLE));
    // Distal joints
    setJointAngles(relJoints, jointMask, JOINT_THUMB_DISTAL, motionRanges,
                   jointAngles.getDistal(FINGER_THUMB));
    setJointAngles(relJoints, jointMask, JOINT_INDEX_DISTAL, motionRanges,
                   jointAngles.getDistal(FINGER_INDEX));
    setJointAngles(relJoints, jointMask, JOINT_MIDDLE_DISTAL, motionRanges,
                   jointAngles.getDistal(FINGER_MIDDLE));
    setJointAngles(relJoints, jointMask, JOINT_RING_DISTAL, motionRanges,
                   jointAngles.getDistal(FINGER_RING));
    setJointAngles(relJoints, jointMask, JOINT_LITTLE_DISTAL, motionRanges,
                   jointAngles.getDistal(FINGER_LITTLE));

    // Convert the affected joints back to absolute locations
    for (unsigned int i = 0; i < JOINT_COUNT; ++i) {
        Joint joint = jointsDepthFirst[i];
        if (jointsAffected & (1 << joint))
            setJointLocationRelative(joint, relJoints[joint]);
    }
}

void HandPose::setPose(const SqueezeValues &squeezeValues,
                       const JointMotionRanges &motionRanges,
                       const HandDimentions *dimentions,
                       unsigned int jointMask)
{
    JointAngles angles(squeezeValues, motionRanges);
    setPose(angles, motionRanges, dimentions, jointMask);
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
    int parent = getJointParent(joint);
    if (parent < 0)
        setJointLocation(joint, relative);
    else
        setJointLocation(joint, relative.getAbsolute(getJointLocation((Joint)parent)));
}

#if 0
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
#endif
