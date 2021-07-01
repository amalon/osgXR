#ifndef OSGVIEWER_OPENXR_INSTANCE
#define OSGVIEWER_OPENXR_INSTANCE 1

#include <vector>
#include <map>

#include <osg/Referenced>
#include <osg/observer_ptr>

#include <openxr/openxr.h>
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr_platform.h>

namespace osgViewer {

namespace OpenXR {

class System;
class Session;

class Instance : public osg::Referenced
{
    public:

        static Instance *instance();

        Instance();
        virtual ~Instance();

        // Instance initialisation

        void setValidationLayer(bool layerValidation)
        {
            _layerValidation = layerValidation;
        }

        bool init(const char *appName, uint32_t appVersion);

        // Error checking

        inline bool valid() const
        {
            return _instance != XR_NULL_SYSTEM_ID;
        }

        bool check(XrResult result, const char *warnMsg) const;

        // Conversions

        inline XrInstance getXrInstance() const
        {
            return _instance;
        }

        // Extensions

        PFN_xrVoidFunction getProcAddr(const char *name) const;

        XrResult getOpenGLGraphicsRequirements(XrSystemId systemId, XrGraphicsRequirementsOpenGLKHR* graphicsRequirements) const
        {
          if (!_xrGetOpenGLGraphicsRequirementsKHR)
            return XR_ERROR_FUNCTION_UNSUPPORTED;
          return _xrGetOpenGLGraphicsRequirementsKHR(_instance, systemId, graphicsRequirements);
        }

        // Queries

        System *getSystem(XrFormFactor formFactor);
        void registerSession(Session *session);
        void unregisterSession(Session *session);

        // Events

        void handleEvent(const XrEventDataBuffer &event);
        void handleEvents();

    protected:

        // Setup data
        bool _layerValidation;

        // Instance data
        XrInstance _instance;

        // Extension functions
        mutable PFN_xrGetOpenGLGraphicsRequirementsKHR _xrGetOpenGLGraphicsRequirementsKHR;
        // Systems
        mutable std::vector<System *> _systems;

        // Sessions
        std::map<XrSession, Session *> _sessions;
};

} // osgViewer::OpenXR

} // osgViewer

#endif
