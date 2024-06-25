// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

// for M_PI from cmath
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include "MultiView.h"

#include <osg/Quat>
#include <osg/Vec2>
#include <osg/Vec3>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace osgXR {

class MultiViewMono : public MultiView
{
    public:

        // MultiView overrides

        void _addView(unsigned int viewIndex,
                      const XrPosef &pose, const XrFovf &fov) override
        {
            _pose = pose;
            _fov = fov;
            _valid = true;
        }

        bool _getSharedView(SharedView &view) const override
        {
            if (!_valid)
                return false;
            view.pose = _pose;
            view.fov = _fov;
            view.zoffset = 0.0f;
            return true;
        }

    protected:

        XrPosef _pose = {};
        XrFovf _fov = {};
};

class MultiViewMultiple : public MultiView
{
    public:

        // MultiView overrides

        void loadFrame(OpenXR::Session::Frame *frame) override
        {
            _numViews = 0;
            _flags = 0;
            _boundingFov = XrFovf{ M_PI/2, -M_PI/2, -M_PI/2, M_PI/2 };
            _positions.clear();

            MultiView::loadFrame(frame);

            // Flag whether FOV reaches 180
            if (fabsf(_boundingFov.angleRight - _boundingFov.angleLeft) >= M_PI)
                _flags |= HORIZONTAL_180_BIT;
            if (fabsf(_boundingFov.angleUp - _boundingFov.angleDown) >= M_PI)
                _flags |= VERTICAL_180_BIT;

            // Canted only, < 180 degrees
            if ((_flags & (ORIENTATION_BITS | HORIZONTAL_180_BIT)) == CANTED_BIT)
            {
                // point the frustum mid way between full extent
                float sharedCant = (_boundingFov.angleLeft + _boundingFov.angleRight) * 0.5;
                _boundingFov.angleLeft -= sharedCant;
                _boundingFov.angleRight -= sharedCant;
                // Rotate _orientation by sharedCant
                _orientation = osg::Quat(sharedCant, osg::Vec3(0.0, -1.0, 0.0)) * _orientation;
            }

            _orientationInverse = _orientation.inverse();
        }

        void _addView(unsigned int viewIndex,
                      const XrPosef &pose, const XrFovf &fov) override
        {
            // pose & fov define an infinite perspective frustum
            osg::Vec3 position(pose.position.x,
                               pose.position.y,
                               pose.position.z);
            osg::Quat orientation(pose.orientation.x,
                                  pose.orientation.y,
                                  pose.orientation.z,
                                  pose.orientation.w);
            XrFovf offsetFov = fov;
            if (!_numViews)
            {
                _orientation = orientation;
            }
            else if (orientation != _orientation)
            {
                osg::Quat diff = orientation / _orientation;
                double angle;
                osg::Vec3 axis;
                diff.getRotate(angle, axis);
                if (fabsf(axis[0]) > 0.001f)
                    _flags |= PITCHED_BIT;
                if (fabsf(axis[1]) > 0.001f)
                    _flags |= CANTED_BIT;
                if (fabsf(axis[2]) > 0.001f)
                    _flags |= ROTATED_BIT;

                // Offset FOV angles for solely canted screens
                if ((_flags & ORIENTATION_BITS) == CANTED_BIT)
                {
                    double cant = -angle * axis[1];
                    offsetFov.angleLeft += cant;
                    offsetFov.angleRight += cant;
                }
            }
            ++_numViews;

            // Save position for later
            _positions.push_back(position);

            // Expand bounding fov
            _boundingFov.angleLeft  = std::min(_boundingFov.angleLeft, offsetFov.angleLeft);
            _boundingFov.angleRight = std::max(_boundingFov.angleRight, offsetFov.angleRight);
            _boundingFov.angleUp    = std::max(_boundingFov.angleUp, offsetFov.angleUp);
            _boundingFov.angleDown  = std::min(_boundingFov.angleDown, offsetFov.angleDown);

            _valid = true;
        }

        bool _getSharedView(SharedView &view) const override
        {
            if (!_valid)
                return false;

            // Not pitched or rotated and < 180 degrees
            if (!(_flags & ((ORIENTATION_BITS | HORIZONTAL_180_BIT) & ~CANTED_BIT)))
            {
                // Bounding frustum normals (facing inwards) in shared space
                osg::Vec4 planes[4] = {
                    osg::Vec4(cos(_boundingFov.angleLeft), 0.0f,
                              sin(_boundingFov.angleLeft), 0.0f),
                    osg::Vec4(-cos(_boundingFov.angleRight), 0.0f,
                              -sin(_boundingFov.angleRight), 0.0f),
                    osg::Vec4(0.0f, cos(_boundingFov.angleDown),
                              sin(_boundingFov.angleDown), 0.0f),
                    osg::Vec4(0.0f, -cos(_boundingFov.angleUp),
                              -sin(_boundingFov.angleUp), 0.0f)
                };

                // Position planes to enclose all the views
                const float inf = std::numeric_limits<float>::infinity();
                float distances[4] = { inf, inf, inf, inf };
                float maxZ = -inf;
                for (auto &position: _positions)
                {
                    // Transform view position into shared orientation
                    osg::Vec3 sharedPos = _orientationInverse * position;

                    // Find furthest position away from frustum for each normal
                    for (int i = 0; i < 4; ++i)
                        distances[i] = std::min(distances[i],
                                                planes[i] * sharedPos);
                    maxZ = std::max(maxZ, sharedPos[2]);
                }
                for (int i = 0; i < 4; ++i)
                    planes[i][3] = -distances[i];

                // Calculate XZ plane intersections (horizontal)
                // and YZ plane intersections (vertical)
                float z2[2] = {
                    (planes[1][3] / planes[1][0] - planes[0][3] / planes[0][0])
                        / (planes[0][2] / planes[0][0] - planes[1][2] / planes[1][0]),
                    (planes[3][3] / planes[3][1] - planes[2][3] / planes[2][1])
                        / (planes[2][2] / planes[2][1] - planes[3][2] / planes[3][1])
                };
                // Position view to cover both intersections
                osg::Vec3 sharedView((-planes[0][2] * z2[0] - planes[0][3]) / planes[0][0],
                                     (-planes[2][2] * z2[1] - planes[2][3]) / planes[2][1],
                                     std::max(z2[0], z2[1]));

                // Transform back into XR space.
                osg::Vec3 sharedViewGlobal = _orientation * sharedView;

                view.pose.position.x = sharedViewGlobal[0];
                view.pose.position.y = sharedViewGlobal[1];
                view.pose.position.z = sharedViewGlobal[2];

                view.pose.orientation.x = _orientation[0];
                view.pose.orientation.y = _orientation[1];
                view.pose.orientation.z = _orientation[2];
                view.pose.orientation.w = _orientation[3];

                view.fov = _boundingFov;

                view.zoffset = sharedView[2] - maxZ;

                return true;
            }

            OSG_WARN << "osgXR: Unhandled MultiView:"
                     << ((_flags & PITCHED_BIT) ? " pitched" : "")
                     << ((_flags & CANTED_BIT) ? " canted" : "")
                     << ((_flags & ROTATED_BIT) ? " rotated" : "")
                     << ((_flags & HORIZONTAL_180_BIT) ? " horizontal-180" : "")
                     << ((_flags & VERTICAL_180_BIT) ? " vertical-180" : "")
                     << " bounding-fov: " << _boundingFov.angleLeft * 180.0/M_PI
                     << ".." << _boundingFov.angleRight * 180.0/M_PI
                     << " H, " << _boundingFov.angleDown * 180.0/M_PI
                     << ".." << _boundingFov.angleUp * 180.0/M_PI
                     << " V" << std::endl;

            return false;
        }

    protected:

        unsigned int _numViews;
        osg::Quat _orientation;
        osg::Quat _orientationInverse;
        std::vector<osg::Vec3> _positions;

        enum {
            /// Views are pitched, about the X axis.
            PITCHED_BIT = 0x1,
            /// Views are canted, rotated about the Y axis.
            CANTED_BIT  = 0x2,
            /// Views are rotated, about the Z axis.
            ROTATED_BIT = 0x4,

            ORIENTATION_BITS = (PITCHED_BIT | CANTED_BIT | ROTATED_BIT),

            /// Views cover at least 180 degrees horizontally
            HORIZONTAL_180_BIT = 0x8,
            /// Views cover at least 180 degrees vertically
            VERTICAL_180_BIT = 0x10,
        };
        unsigned int _flags;

        XrFovf _boundingFov;
};

} // osgXR

using namespace osgXR;

MultiView *MultiView::create(const OpenXR::Session *session)
{
    auto *viewConfiguration = session->getViewConfiguration();
    if (!viewConfiguration)
        return nullptr;

    switch (viewConfiguration->getType())
    {
    case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
        return new MultiViewMono();
    case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
        return new MultiViewMultiple();
    default:
        return nullptr;
    }
}

MultiView::~MultiView()
{
}

void MultiView::loadFrame(OpenXR::Session::Frame *frame)
{
    _valid = false;
    _cachedSharedView = false;

    if (!frame->isPositionValid() || !frame->isOrientationValid())
        return;

    unsigned int numViews = frame->getNumViews();
    for (unsigned int i = 0; i < numViews; ++i)
        _addView(i, frame->getViewPose(i), frame->getViewFov(i));
}

bool MultiView::getSharedView(SharedView &view) const
{
    if (_cachedSharedView)
    {
        view = _sharedView;
        return true;
    }
    _cachedSharedView = _getSharedView(_sharedView);
    if (_cachedSharedView)
        view = _sharedView;
    return _cachedSharedView;
}
