// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/PoseVelocity>

using namespace osgXR;

// Public API

PoseVelocity::PoseVelocity() :
    _flags((Flags)0)
{
}

PoseVelocity::PoseVelocity(Flags flags,
                           const osg::Vec3f &linear,
                           const osg::Vec3f &angular) :
    _flags(flags),
    _linear(linear),
    _angular(angular)
{
}