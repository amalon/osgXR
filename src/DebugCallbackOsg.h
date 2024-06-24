// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#ifndef OSGXR_DEBUG_CALLBACK_OSG
#define OSGXR_DEBUG_CALLBACK_OSG 1

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

#endif
