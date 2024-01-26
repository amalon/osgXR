// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#pragma once

#include "OpenXR/DebugUtilsMessenger.h"

namespace osgXR {

class DebugCallbackOsg : public OpenXR::DebugUtilsCallback
{
    public:
        DebugCallbackOsg(XrDebugUtilsMessageSeverityFlagsEXT messageSeverities,
                         XrDebugUtilsMessageTypeFlagsEXT messageTypes);

        bool callback(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                      XrDebugUtilsMessageTypeFlagsEXT messageTypes,
                      const XrDebugUtilsMessengerCallbackDataEXT *callbackData) override;
};

} // osgXR
