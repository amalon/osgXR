// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#include "DebugCallbackOsg.h"

#include <osg/Notify>

using namespace osgXR;

DebugCallbackOsg::DebugCallbackOsg(XrDebugUtilsMessageSeverityFlagsEXT messageSeverities,
                                   XrDebugUtilsMessageTypeFlagsEXT messageTypes)
: OpenXR::DebugUtilsCallback(messageSeverities, messageTypes)
{
}

bool DebugCallbackOsg::callback(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                                XrDebugUtilsMessageTypeFlagsEXT messageTypes,
                                const XrDebugUtilsMessengerCallbackDataEXT *callbackData)
{
    osg::NotifySeverity severity = osg::DEBUG_INFO;
    if (messageSeverity >= XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        severity = osg::FATAL;
    else if (messageSeverity >= XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        severity = osg::WARN;
    else if (messageSeverity >= XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        severity = osg::INFO;

    const char *msgId = callbackData->messageId;
    const char *fnName = callbackData->functionName;
    if (!msgId)
        msgId = "-";
    if (!fnName)
        fnName = "-";
    char types[5] = {};
    unsigned int numTypes = 0;
    if (messageTypes & XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
        types[numTypes++] = 'G';
    if (messageTypes & XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        types[numTypes++] = 'V';
    if (messageTypes & XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        types[numTypes++] = 'P';
    if (messageTypes & XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT)
        types[numTypes++] = 'C';
    OSG_NOTIFY(severity) << "OpenXR [" << types << " - " << msgId << " - " << fnName << "]: "
                         << callbackData->message << std::endl;

    return false;
}
