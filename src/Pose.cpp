// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/Pose>

using namespace osgXR;

// Public API

Pose::Pose() :
    _flags((Flags)0)
{
}

Pose::Pose(const Pose &other) :
    _flags(other._flags),
    _orientation(other._orientation),
    _position(other._position)
{
}

Pose::Pose(Flags flags,
           const osg::Quat &orientation,
           const osg::Vec3f &position) :
    _flags(flags),
    _orientation(orientation),
    _position(position)
{
}
