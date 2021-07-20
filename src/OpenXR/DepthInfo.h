// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_DEPTH_INFO
#define OSGXR_OPENXR_DEPTH_INFO 1

#include <osg/Matrixd>

namespace osgXR {

namespace OpenXR {

// Represents depth information for a view
class DepthInfo
{
    public:

        DepthInfo() :
            _minDepth(0),
            _maxDepth(1),
            _nearZ(1),
            _farZ(10)
        {
        }

        // Mutators

        void setDepthRange(float minDepth, float maxDepth)
        {
            _minDepth = minDepth;
            _maxDepth = maxDepth;
        }

        void setZRange(float nearZ, float farZ)
        {
            _nearZ = nearZ;
            _farZ = farZ;
        }

        void setZRangeFromProjection(const osg::Matrixd &proj)
        {
            float left, right, bottom, top;
            proj.getFrustum(left, right, bottom, top, _nearZ, _farZ);
        }

        // Accessors

        float getMinDepth() const
        {
            return _minDepth;
        }

        float getMaxDepth() const
        {
            return _maxDepth;
        }

        float getNearZ() const
        {
            return _nearZ;
        }

        float getFarZ() const
        {
            return _farZ;
        }

    protected:

        float _minDepth;
        float _maxDepth;
        float _nearZ;
        float _farZ;
};

} // osgXR::OpenXR

} // osgXR

#endif
