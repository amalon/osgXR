// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_SYSTEM
#define OSGXR_OPENXR_SYSTEM 1

#include "Instance.h"

#include <osg/DisplaySettings>
#include <osg/ref_ptr>

#include <algorithm>
#include <vector>

namespace osgXR {

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

        inline bool check(XrResult result, const char *actionMsg) const
        {
            return _instance->check(result, actionMsg);
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

        inline const char *getSystemName() const
        {
            if (!_readProperties)
                getProperties();
            return _systemName;
        }

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

                        struct Viewport
                        {
                            uint32_t x, y, width, height, arrayIndex;
                        };

                        View(uint32_t recommendedWidth,
                             uint32_t recommendedHeight,
                             uint32_t recommendedSamples = 1) :
                            _recommendedWidth(recommendedWidth),
                            _recommendedHeight(recommendedHeight),
                            _recommendedSamples(recommendedSamples)
                        {
                        }

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

                        /// Tile another view horizontally after this one
                        struct Viewport tileHorizontally(const View &other)
                        {
                            struct Viewport vp;
                            vp.x = _recommendedWidth;
                            vp.y = 0;
                            vp.width = other._recommendedWidth;
                            vp.height = other._recommendedHeight;
                            vp.arrayIndex = 0;

                            _recommendedWidth += other._recommendedWidth;
                            _recommendedHeight = std::max(_recommendedHeight,
                                                          other._recommendedHeight);
                            return vp;
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

                inline bool check(XrResult result, const char *actionMsg) const
                {
                    return _system->getInstance()->check(result, actionMsg);
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

        // Properties
        mutable char _systemName[XR_MAX_SYSTEM_NAME_SIZE];
        mutable bool _readProperties;
        mutable bool _orientationTracking;
        mutable bool _positionTracking;

        // View configurations
        mutable bool _readViewConfigurations;
        mutable ViewConfigurations _viewConfigurations;

};

} // osgXR::OpenXR

} // osgXR

#endif
