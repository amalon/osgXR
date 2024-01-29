// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_INSTANCE
#define OSGXR_OPENXR_INSTANCE 1

#include "Quirks.h"

#include <map>
#include <vector>

#include <osg/Referenced>
#include <osg/observer_ptr>
#include <osg/ref_ptr>

#include <openxr/openxr.h>
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr_platform.h>

#define XR_APILAYER_LUNARG_core_validation  "XR_APILAYER_LUNARG_core_validation"

namespace osgXR {

namespace OpenXR {

class DebugUtilsCallback;
class DebugUtilsMessenger;
class EventHandler;
class System;
class Session;

class Instance : public osg::Referenced
{
    public:

        static Instance *instance();

        Instance();
        virtual ~Instance();

        // Layers and extensions

        static void invalidateLayers();
        static void invalidateExtensions();
        static bool hasLayer(const char *name);
        static bool hasExtension(const char *name);

        // Instance initialisation

        void setValidationLayer(bool layerValidation)
        {
            _layerValidation = layerValidation;
        }

        void setDebugUtils(bool debugUtils)
        {
            _debugUtils = debugUtils;
        }

        void setDefaultDebugCallback(OpenXR::DebugUtilsCallback *callback);

        void setDepthInfo(bool depthInfo)
        {
            _depthInfo = depthInfo;
        }

        void setVisibilityMask(bool visibilityMask)
        {
            _visibilityMask = visibilityMask;
        }

        typedef enum {
            /// Instance creation successful.
            INIT_SUCCESS,
            /// Instance creation not possible at the moment, try again later.
            INIT_LATER,
            /// Instance creation failed.
            INIT_FAIL,
        } InitResult;
        InitResult init(const char *appName, uint32_t appVersion);

        void deinit();

        // Error checking

        inline bool valid() const
        {
            return _instance != XR_NULL_HANDLE;
        }

        inline bool lost() const
        {
            return _lost;
        }

        bool check(XrResult result, const char *actionMsg) const;

        class Result
        {
            public:
                XrResult result = XR_SUCCESS;
                const char *action = nullptr;
                char resultName[XR_MAX_RESULT_STRING_SIZE] = {};

                bool failed() const
                {
                    return XR_FAILED(result);
                }
        };

        // returns false if no error is stored
        bool getError(Result &error) const
        {
            error = _lastError;
            return error.failed();
        }

        // Conversions

        inline XrInstance getXrInstance() const
        {
            return _instance;
        }

        // Instance properties
        inline const char *getRuntimeName() const
        {
            return _properties.runtimeName;
        }
        inline XrVersion getRuntimeVersion() const
        {
            return _properties.runtimeVersion;
        }

        inline bool getQuirk(Quirk quirk) const
        {
            return _quirks[quirk];
        }

        // Extensions

        bool supportsDebugUtils() const
        {
            return _supportsDebugUtils;
        }

        bool supportsCompositionLayerDepth() const
        {
            return _supportsCompositionLayerDepth;
        }

        bool supportsVisibilityMask() const
        {
            return _supportsVisibilityMask;
        }

        PFN_xrVoidFunction getProcAddr(const char *name) const;

        XrResult getOpenGLGraphicsRequirements(XrSystemId systemId,
                                               XrGraphicsRequirementsOpenGLKHR* graphicsRequirements) const
        {
            if (!_xrGetOpenGLGraphicsRequirementsKHR)
                return XR_ERROR_FUNCTION_UNSUPPORTED;
            return _xrGetOpenGLGraphicsRequirementsKHR(_instance, systemId,
                                                       graphicsRequirements);
        }

        XrResult xrSetDebugUtilsObjectName(const XrDebugUtilsObjectNameInfoEXT *nameInfo) const
        {
            if (!_xrSetDebugUtilsObjectNameEXT)
                return XR_ERROR_FUNCTION_UNSUPPORTED;
            return _xrSetDebugUtilsObjectNameEXT(_instance, nameInfo);
        }

        XrResult xrCreateDebugUtilsMessenger(const XrDebugUtilsMessengerCreateInfoEXT *createInfo,
                                             XrDebugUtilsMessengerEXT *messenger) const
        {
            if (!_xrCreateDebugUtilsMessengerEXT)
                return XR_ERROR_FUNCTION_UNSUPPORTED;
            return _xrCreateDebugUtilsMessengerEXT(_instance, createInfo,
                                                   messenger);
        }

        XrResult xrDestroyDebugUtilsMessenger(XrDebugUtilsMessengerEXT messenger) const
        {
            if (!_xrDestroyDebugUtilsMessengerEXT)
                return XR_ERROR_FUNCTION_UNSUPPORTED;
            return _xrDestroyDebugUtilsMessengerEXT(messenger);
        }

        XrResult xrSubmitDebugUtilsMessage(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                                           XrDebugUtilsMessageTypeFlagsEXT messageTypes,
                                           const XrDebugUtilsMessengerCallbackDataEXT* callbackData) const
        {
            if (!_xrSubmitDebugUtilsMessageEXT)
                return XR_ERROR_FUNCTION_UNSUPPORTED;
            return _xrSubmitDebugUtilsMessageEXT(_instance, messageSeverity,
                                                 messageTypes, callbackData);
        }

        XrResult xrSessionBeginDebugUtilsLabelRegion(XrSession session,
                                                     const XrDebugUtilsLabelEXT *labelInfo) const
        {
            if (!_xrSessionBeginDebugUtilsLabelRegionEXT)
                return XR_ERROR_FUNCTION_UNSUPPORTED;
            return _xrSessionBeginDebugUtilsLabelRegionEXT(session, labelInfo);
        }

        XrResult xrSessionEndDebugUtilsLabelRegion(XrSession session) const
        {
            if (!_xrSessionEndDebugUtilsLabelRegionEXT)
                return XR_ERROR_FUNCTION_UNSUPPORTED;
            return _xrSessionEndDebugUtilsLabelRegionEXT(session);
        }

        XrResult xrSessionInsertDebugUtilsLabel(XrSession session,
                                                const XrDebugUtilsLabelEXT *labelInfo) const
        {
            if (!_xrSessionInsertDebugUtilsLabelEXT)
                return XR_ERROR_FUNCTION_UNSUPPORTED;
            return _xrSessionInsertDebugUtilsLabelEXT(session, labelInfo);
        }

        XrResult xrGetVisibilityMask(XrSession session,
                                     XrViewConfigurationType viewConfigurationType,
                                     uint32_t viewIndex,
                                     XrVisibilityMaskTypeKHR visibilityMaskType,
                                     XrVisibilityMaskKHR *visibilityMask) const
        {
            if (!_xrGetVisibilityMaskKHR)
                return XR_ERROR_FUNCTION_UNSUPPORTED;
            return _xrGetVisibilityMaskKHR(session, viewConfigurationType,
                                           viewIndex, visibilityMaskType,
                                           visibilityMask);
        }

        // Queries

        System *getSystem(XrFormFactor formFactor, bool *supported = nullptr);

        // Up to caller to ensure no session
        void invalidateSystem(XrFormFactor formFactor);
        void registerSession(Session *session);
        void unregisterSession(Session *session);
        Session *getSession(XrSession xrSession);

        // Events

        void pollEvents(EventHandler *handler);

    protected:

        // Setup data
        bool _layerValidation;
        bool _debugUtils;
        bool _depthInfo;
        bool _visibilityMask;

        // Default debug callback to configure
        osg::ref_ptr<DebugUtilsCallback> _defaultDebugCallback;
        osg::ref_ptr<DebugUtilsMessenger> _defaultDebugMessenger;

        // Instance data
        XrInstance _instance;
        mutable bool _lost;
        mutable Result _lastError;

        // Extension presence
        bool _supportsDebugUtils;
        bool _supportsCompositionLayerDepth;
        bool _supportsVisibilityMask;
        // Extension functions
        PFN_xrGetOpenGLGraphicsRequirementsKHR _xrGetOpenGLGraphicsRequirementsKHR = nullptr;
        PFN_xrSetDebugUtilsObjectNameEXT _xrSetDebugUtilsObjectNameEXT = nullptr;
        PFN_xrCreateDebugUtilsMessengerEXT _xrCreateDebugUtilsMessengerEXT = nullptr;
        PFN_xrDestroyDebugUtilsMessengerEXT _xrDestroyDebugUtilsMessengerEXT = nullptr;
        PFN_xrSubmitDebugUtilsMessageEXT _xrSubmitDebugUtilsMessageEXT = nullptr;
        PFN_xrSessionBeginDebugUtilsLabelRegionEXT _xrSessionBeginDebugUtilsLabelRegionEXT = nullptr;
        PFN_xrSessionEndDebugUtilsLabelRegionEXT _xrSessionEndDebugUtilsLabelRegionEXT = nullptr;
        PFN_xrSessionInsertDebugUtilsLabelEXT _xrSessionInsertDebugUtilsLabelEXT = nullptr;
        PFN_xrGetVisibilityMaskKHR _xrGetVisibilityMaskKHR = nullptr;

        // Instance properties
        XrInstanceProperties _properties;

        // Quirks
        Quirks _quirks;

        // Systems
        mutable std::vector<System *> _systems;

        // Sessions
        std::map<XrSession, Session *> _sessions;
};

} // osgXR::OpenXR

} // osgXR

#endif
