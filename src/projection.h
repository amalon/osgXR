// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_PROJECTION
#define OSGXR_PROJECTION 1

#include <osg/Matrix>

#include <openxr/openxr.h>

namespace osgXR {

void createProjectionFov(osg::Matrix& result,
                         const XrFovf& fov,
                         const float nearZ,
                         const float farZ);

} // osgXR

#endif
