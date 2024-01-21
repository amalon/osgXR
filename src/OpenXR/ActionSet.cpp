// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "ActionSet.h"

#include <cstring>

using namespace osgXR::OpenXR;

ActionSet::ActionSet(Instance *instance,
                     const std::string &name,
                     const std::string &localizedName,
                     uint32_t priority) :
    _instance(instance),
    _actionSet(XR_NULL_HANDLE)
{
    XrActionSetCreateInfo createInfo{ XR_TYPE_ACTION_SET_CREATE_INFO };
    strncpy(createInfo.actionSetName, name.c_str(),
            XR_MAX_ACTION_SET_NAME_SIZE - 1);
    strncpy(createInfo.localizedActionSetName, localizedName.c_str(),
            XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
    createInfo.priority = priority;

    check(xrCreateActionSet(getXrInstance(), &createInfo, &_actionSet),
          "create OpenXR action set");
}

ActionSet::~ActionSet()
{
    if (_actionSet != XR_NULL_HANDLE)
    {
        check(xrDestroyActionSet(_actionSet),
              "destroy OpenXR action set");
    }
}
