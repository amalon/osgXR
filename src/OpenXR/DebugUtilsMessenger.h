// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_DEBUG_UTILS_MESSENGER
#define OSGXR_OPENXR_DEBUG_UTILS_MESSENGER 1

#include "Instance.h"

#include <osg/Referenced>
#include <osg/ref_ptr>

namespace osgXR {

namespace OpenXR {

class Instance;

class DebugUtilsCallback : public osg::Referenced
{
    public:
        typedef XrDebugUtilsMessengerCreateInfoEXT CreateInfo;

        DebugUtilsCallback(XrDebugUtilsMessageSeverityFlagsEXT messageSeverities,
                           XrDebugUtilsMessageTypeFlagsEXT messageTypes);

        virtual ~DebugUtilsCallback();

        void writeCreateInfo(CreateInfo *createInfo);

        virtual bool callback(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                              XrDebugUtilsMessageTypeFlagsEXT messageTypes,
                              const XrDebugUtilsMessengerCallbackDataEXT *callbackData) = 0;

    private:
        XrDebugUtilsMessageSeverityFlagsEXT _messageSeverities;
        XrDebugUtilsMessageTypeFlagsEXT _messageTypes;
};

class DebugUtilsMessenger : public osg::Referenced
{
    public:

        DebugUtilsMessenger(Instance *instance, DebugUtilsCallback *callback);
        virtual ~DebugUtilsMessenger();

        // Error checking

        bool check(XrResult result, const char *actionMsg) const
        {
            return _instance->check(result, actionMsg);
        }

        bool valid() const
        {
            return _messenger != XR_NULL_HANDLE;
        }

        // Conversions

        const osg::ref_ptr<Instance> getInstance() const
        {
            return _instance;
        }

        XrInstance getXrInstance() const
        {
            if (!_instance.valid())
                return XR_NULL_HANDLE;
            return _instance->getXrInstance();
        }

        XrDebugUtilsMessengerEXT getXrDebugUtilsMessenger() const
        {
            return _messenger;
        }

    private:

        // Messenger data
        osg::ref_ptr<Instance> _instance;
        osg::ref_ptr<DebugUtilsCallback> _callback;
        XrDebugUtilsMessengerEXT _messenger;
};

} // osgXR::OpenXR

} // osgXR

#endif
