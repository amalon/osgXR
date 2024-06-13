// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_SWAPCHAIN_GROUP_SUB_IMAGE
#define OSGXR_OPENXR_SWAPCHAIN_GROUP_SUB_IMAGE 1

#include "SwapchainGroup.h"

#include <osg/ref_ptr>

#include <openxr/openxr.h>

namespace osgXR {

namespace OpenXR {

class SwapchainGroupSubImage
{
    public:
        SwapchainGroupSubImage(SwapchainGroup *group) :
            _group(group),
            _x(0),
            _y(0),
            _width(group->getWidth()),
            _height(group->getHeight()),
            _arrayIndex(0)
        {
        }

        SwapchainGroupSubImage(SwapchainGroup *group,
                               const System::ViewConfiguration::View::Viewport &vp) :
            _group(group),
            _x(vp.x),
            _y(vp.y),
            _width(vp.width),
            _height(vp.height),
            _arrayIndex(vp.arrayIndex)
        {
        }

        // Error checking

        bool valid() const
        {
            return _group->valid();
        }

        bool depthValid() const
        {
            return _group->depthValid();
        }

        // Accessors

        inline const osg::ref_ptr<Instance> getInstance() const
        {
            return _group->getInstance();
        }

        osg::ref_ptr<SwapchainGroup> getSwapchainGroup() const
        {
            return _group;
        }

        uint32_t getX() const
        {
            return _x;
        }

        uint32_t getY() const
        {
            return _y;
        }

        uint32_t getWidth() const
        {
            return _width;
        }

        uint32_t getHeight() const
        {
            return _height;
        }

        uint32_t getArrayIndex() const
        {
            return _arrayIndex;
        }

        void getXrSubImage(XrSwapchainSubImage *out) const
        {
            out->swapchain = _group->getXrSwapchain();
            out->imageRect.offset = { (int32_t)_x, (int32_t)_y };
            out->imageRect.extent = { (int32_t)_width, (int32_t)_height };
            out->imageArrayIndex = _arrayIndex;

            // Some runtimes don't correctly flip OpenGL subimage Y coordinates
            if (getInstance()->getQuirk(QUIRK_SUBIMAGE_FLIP_Y))
                out->imageRect.offset.y = _group->getHeight() - _height - _y;
        }

        void getDepthXrSubImage(XrSwapchainSubImage *out) const
        {
            out->swapchain = _group->getDepthXrSwapchain();
            out->imageRect.offset = { (int32_t)_x, (int32_t)_y };
            out->imageRect.extent = { (int32_t)_width, (int32_t)_height };
            out->imageArrayIndex = _arrayIndex;
        }

    protected:
        osg::ref_ptr<SwapchainGroup> _group;
        uint32_t _x;
        uint32_t _y;
        uint32_t _width;
        uint32_t _height;
        uint32_t _arrayIndex;
};

}

}

#endif
