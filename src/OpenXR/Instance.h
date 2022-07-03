// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_INSTANCE
#define OSGXR_OPENXR_INSTANCE 1

#include "Quirks.h"

#include <map>
#include <vector>

#include <osg/Referenced>
#include <osg/observer_ptr>

#include <openxr/openxr.h>
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr_platform.h>

#define XR_APILAYER_LUNARG_core_validation  "XR_APILAYER_LUNARG_core_validation"

namespace osgXR {

namespace OpenXR {

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

        // Error checking

        inline bool valid() const
        {
            return _instance != XR_NULL_SYSTEM_ID;
        }

        inline bool lost() const
        {
            return _lost;
        }

        bool check(XrResult result, const char *warnMsg) const;

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

        XrResult xrGetVisibilityMask(XrSession session,
                                     XrViewConfigurationType viewConfigurationType,
                                     uint32_t viewIndex,
                                     XrVisibilityMaskTypeKHR visibilityMaskType,
                                     XrVisibilityMaskKHR *visibilityMask)
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
        bool _depthInfo;
        bool _visibilityMask;

        // Instance data
        XrInstance _instance;
        mutable bool _lost;

        // Extension presence
        bool _supportsCompositionLayerDepth;
        bool _supportsVisibilityMask;
        // Extension functions
        mutable PFN_xrGetOpenGLGraphicsRequirementsKHR _xrGetOpenGLGraphicsRequirementsKHR;
        mutable PFN_xrGetVisibilityMaskKHR _xrGetVisibilityMaskKHR;

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
