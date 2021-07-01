#ifndef OSGVIEWER_OPENXR_SYSTEM
#define OSGVIEWER_OPENXR_SYSTEM 1

#include "Instance.h"

#include <osg/DisplaySettings>
#include <osg/ref_ptr>

#include <vector>

namespace osgViewer {

namespace OpenXR {

class System
{
    public:

        System(Instance *instance, XrSystemId systemId) :
            _instance(instance),
            _systemId(systemId),
            _readProperties(false),
            _orientationTracking(false),
            _positionTracking(false),
            _readViewConfigurations(false)
        {
        }

        // Error checking

        inline bool check(XrResult result, const char *warnMsg) const
        {
            return _instance->check(result, warnMsg);
        }

        // Conversions

        inline Instance *getInstance()
        {
            return _instance;
        }
        inline const Instance *getInstance() const
        {
            return _instance;
        }

        inline XrInstance getXrInstance() const
        {
            return _instance->getXrInstance();
        }

        inline XrSystemId getXrSystemId() const
        {
            return _systemId;
        }

        // Queries

        void getProperties() const;

        inline bool getOrientationTracking() const
        {
            if (!_readProperties)
                getProperties();
            return _orientationTracking;
        }

        inline bool getPositionTracking() const
        {
            if (!_readProperties)
                getProperties();
            return _positionTracking;
        }

        class ViewConfiguration
        {

            public:

                ViewConfiguration(const System *system, XrViewConfigurationType type) :
                    _system(system),
                    _type(type),
                    _readViews(false),
                    _readEnvBlendModes(false)
                {
                }

                XrViewConfigurationType getType() const
                {
                    return _type;
                }

                // Queries

                class View
                {

                    public:

                        View(const XrViewConfigurationView &view) :
                            _recommendedWidth(view.recommendedImageRectWidth),
                            _recommendedHeight(view.recommendedImageRectHeight),
                            _recommendedSamples(view.recommendedSwapchainSampleCount)
                        {
                        }

                        uint32_t getRecommendedWidth() const
                        {
                            return _recommendedWidth;
                        }

                        uint32_t getRecommendedHeight() const
                        {
                            return _recommendedHeight;
                        }


                        uint32_t getRecommendedSamples() const
                        {
                            return _recommendedSamples;
                        }

                    protected:

                        uint32_t _recommendedWidth;
                        uint32_t _recommendedHeight;
                        uint32_t _recommendedSamples;
                };

                typedef std::vector<View> Views;
                const Views &getViews() const;

                typedef std::vector<XrEnvironmentBlendMode> EnvBlendModes;
                const EnvBlendModes &getEnvBlendModes() const;

            protected:

                inline bool check(XrResult result, const char *warnMsg) const
                {
                    return _system->getInstance()->check(result, warnMsg);
                }

                const System *_system;
                XrViewConfigurationType _type;

                // Views
                mutable bool _readViews;
                mutable Views _views;

                // Environment blend modes
                mutable bool _readEnvBlendModes;
                mutable EnvBlendModes _envBlendModes;
        };

        typedef std::vector<ViewConfiguration> ViewConfigurations;
        const ViewConfigurations &getViewConfigurations() const;

    protected:

        // System data
        Instance *_instance;
        XrSystemId _systemId;

        // Tracking Properties
        mutable bool _readProperties;
        mutable bool _orientationTracking;
        mutable bool _positionTracking;

        // View configurations
        mutable bool _readViewConfigurations;
        mutable ViewConfigurations _viewConfigurations;

};

} // osgViewer::OpenXR

} // osgViewer

#endif
