// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#include "DebugUtilsMessenger.h"

using namespace osgXR::OpenXR;

// DebugUtilsCallback

DebugUtilsCallback::DebugUtilsCallback(XrDebugUtilsMessageSeverityFlagsEXT messageSeverities,
                              XrDebugUtilsMessageTypeFlagsEXT messageTypes)
: _messageSeverities(messageSeverities),
  _messageTypes(messageTypes)
{
}

DebugUtilsCallback::~DebugUtilsCallback()
{
}

static XrBool32 debugCallback(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                              XrDebugUtilsMessageTypeFlagsEXT messageTypes,
                              const XrDebugUtilsMessengerCallbackDataEXT *callbackData,
                              void *userData)
{
    auto self = reinterpret_cast<DebugUtilsCallback *>(userData);
    return self->callback(messageSeverity, messageTypes, callbackData);
}

void DebugUtilsCallback::writeCreateInfo(DebugUtilsCallback::CreateInfo *createInfo)
{
    createInfo->type = XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo->next = nullptr;
    createInfo->messageSeverities = _messageSeverities;
    createInfo->messageTypes = _messageTypes;
    createInfo->userCallback = debugCallback;
    createInfo->userData = static_cast<void*>(this);
}

// DebugUtilsMessenger

DebugUtilsMessenger::DebugUtilsMessenger(Instance *instance,
                                         DebugUtilsCallback *callback)
: _instance(instance),
  _callback(callback),
  _messenger(XR_NULL_HANDLE)
{
    XrDebugUtilsMessengerCreateInfoEXT createInfo;
    callback->writeCreateInfo(&createInfo);
    check(instance->xrCreateDebugUtilsMessenger(&createInfo, &_messenger),
          "create OpenXR debug utils messenger");
}

DebugUtilsMessenger::~DebugUtilsMessenger()
{
    if (_instance.valid() && valid())
        check(_instance->xrDestroyDebugUtilsMessenger(_messenger),
              "destroy OpenXR debug utils messenger");
}
