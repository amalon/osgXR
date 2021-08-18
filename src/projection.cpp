// =============================================================================
// Derived from openxr-simple-example
// Copyright 2019-2021, Collabora, Ltd.
// Which was adapted from
// https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/master/src/common/xr_linear.h
// Copyright (c) 2017 The Khronos Group Inc.
// Copyright (c) 2016 Oculus VR, LLC.
// SPDX-License-Identifier: Apache-2.0
// =============================================================================

#include "projection.h"

void osgXR::createProjectionFov(osg::Matrix& result,
                                const XrFovf& fov,
                                const float nearZ,
                                const float farZ)
{
    const float tanAngleLeft = tanf(fov.angleLeft);
    const float tanAngleRight = tanf(fov.angleRight);

    const float tanAngleDown = tanf(fov.angleDown);
    const float tanAngleUp = tanf(fov.angleUp);

    const float tanAngleWidth = tanAngleRight - tanAngleLeft;

    // Set to tanAngleDown - tanAngleUp for a clip space with positive Y
    // down (Vulkan). Set to tanAngleUp - tanAngleDown for a clip space with
    // positive Y up (OpenGL / D3D / Metal).
    const float tanAngleHeight = tanAngleUp - tanAngleDown;

    // Set to nearZ for a [-1,1] Z clip space (OpenGL / OpenGL ES).
    // Set to zero for a [0,1] Z clip space (Vulkan / D3D / Metal).
    const float offsetZ = nearZ;

    if (farZ <= nearZ)
    {
        // place the far plane at infinity
        result(0, 0) = 2 / tanAngleWidth;
        result(1, 0) = 0;
        result(2, 0) = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
        result(3, 0) = 0;

        result(0, 1) = 0;
        result(1, 1) = 2 / tanAngleHeight;
        result(2, 1) = (tanAngleUp + tanAngleDown) / tanAngleHeight;
        result(3, 1) = 0;

        result(0, 2) = 0;
        result(1, 2) = 0;
        result(2, 2) = -1;
        result(3, 2) = -(nearZ + offsetZ);

        result(0, 3) = 0;
        result(1, 3) = 0;
        result(2, 3) = -1;
        result(3, 3) = 0;
    } else {
        // normal projection
        result(0, 0) = 2 / tanAngleWidth;
        result(1, 0) = 0;
        result(2, 0) = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
        result(3, 0) = 0;

        result(0, 1) = 0;
        result(1, 1) = 2 / tanAngleHeight;
        result(2, 1) = (tanAngleUp + tanAngleDown) / tanAngleHeight;
        result(3, 1) = 0;

        result(0, 2) = 0;
        result(1, 2) = 0;
        result(2, 2) = -(farZ + offsetZ) / (farZ - nearZ);
        result(3, 2) = -(farZ * (nearZ + offsetZ)) / (farZ - nearZ);

        result(0, 3) = 0;
        result(1, 3) = 0;
        result(2, 3) = -1;
        result(3, 3) = 0;
    }
}
