// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_PATH
#define OSGXR_OPENXR_PATH 1

#include "Instance.h"

#include <osg/ref_ptr>

#include <string>

namespace osgXR {

namespace OpenXR {

class Path
{
    public:

        Path(Instance *instance = nullptr, XrPath path = XR_NULL_PATH);
        Path(Instance *instance, const std::string &path);

        // Error checking

        inline bool valid() const
        {
            return _path != XR_NULL_PATH;
        }

        inline bool check(XrResult result, const char *actionMsg) const
        {
            return _instance->check(result, actionMsg);
        }

        // Conversions

        inline const osg::ref_ptr<Instance> getInstance() const
        {
            return _instance;
        }

        inline XrInstance getXrInstance() const
        {
            return _instance->getXrInstance();
        }

        inline XrPath getXrPath() const
        {
            return _path;
        }

        std::string toString() const;

        // Comparisons

        bool operator == (const Path &other) const
        {
            return _path == other._path &&
                   _instance == other._instance;
        }

        bool operator != (const Path &other) const
        {
            return _path != other._path ||
                   _instance != other._instance;
        }

    protected:

        // Path data
        osg::ref_ptr<Instance> _instance;
        XrPath _path;
};

} // osgXR::OpenXR

} // osgXR

#endif
