// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_ACTION_SET
#define OSGXR_OPENXR_ACTION_SET 1

#include "Instance.h"

#include <osg/ref_ptr>

#include <cstdint>
#include <string>

namespace osgXR {

namespace OpenXR {

class ActionSet : public osg::Referenced
{
    public:

        ActionSet(Instance *instance,
                  const std::string &name,
                  const std::string &localizedName,
                  uint32_t priority);
        virtual ~ActionSet();

        // Error checking

        bool valid() const
        {
            return _actionSet != XR_NULL_HANDLE;
        }

        bool check(XrResult result, const char *actionMsg) const
        {
            return _instance->check(result, actionMsg);
        }

        // Conversions

        const osg::ref_ptr<Instance> getInstance() const
        {
            return _instance;
        }

        XrInstance getXrInstance() const
        {
            return _instance->getXrInstance();
        }

        XrActionSet getXrActionSet() const
        {
            return _actionSet;
        }


    protected:

        // Action set data
        osg::ref_ptr<Instance> _instance;
        XrActionSet _actionSet;
};

} // osgXR::OpenXR

} // osgXR

#endif
