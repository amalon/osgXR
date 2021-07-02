// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_SESSION
#define OSGXR_OPENXR_SESSION 1

#include "System.h"

#include <osg/Referenced>
#include <osg/ref_ptr>
#include <osgViewer/GraphicsWindow>

namespace osgXR {

namespace OpenXR {

class CompositionLayer;

class Session : public osg::Referenced
{
    public:

        Session(System *system, osgViewer::GraphicsWindow *window);
        virtual ~Session();

        // Error checking

        inline bool valid() const
        {
            return _session != XR_NULL_HANDLE;
        }

        inline bool check(XrResult result, const char *warnMsg) const
        {
            return _system->check(result, warnMsg);
        }

        // Accessors

        // Find whether the session is ready to start submitting frames
        inline bool isReady() const
        {
            return _ready;
        }

        // Find whether the session has begun
        inline bool hasBegun() const
        {
            return _begun;
        }

        // Conversions

        inline const osg::ref_ptr<Instance> getInstance() const
        {
            return _instance;
        }

        inline const System *getSystem() const
        {
            return _system;
        }

        inline XrInstance getXrInstance() const
        {
            return _system->getXrInstance();
        }

        inline XrSystemId getXrSystemId() const
        {
            return _system->getXrSystemId();
        }

        inline XrSession getXrSession()
        {
            return _session;
        }

        // Queries

        typedef std::vector<int64_t> SwapchainFormats;
        const SwapchainFormats &getSwapchainFormats() const;

        XrSpace getLocalSpace() const;

        // Events
        
        void handleEvent(const XrEventDataSessionStateChanged &event);

        // Operations

        void makeCurrent() const;

        bool begin(const System::ViewConfiguration &viewConfiguration);
        void end();

        const System::ViewConfiguration *getViewConfiguration() const
        {
            return _viewConfiguration;
        }

        class Frame : public osg::Referenced {
            public:

                Frame(osg::ref_ptr<Session> session, XrFrameState *frameState);

                virtual ~Frame();

                // Error checking

                inline bool check(XrResult result, const char *warnMsg) const
                {
                    return _session->check(result, warnMsg);
                }

                // Accessors

                inline bool shouldRender() const
                {
                    return _shouldRender;
                }

                inline bool hasBegun() const
                {
                    return _begun;
                }

                void locateViews();

                bool isOrientationValid()
                {
                    if (!_locatedViews)
                        locateViews();
                    return _viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT;
                }
                bool isPositionValid()
                {
                    if (!_locatedViews)
                        locateViews();
                    return _viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT;
                }
                bool isOrientationTracked()
                {
                    if (!_locatedViews)
                        locateViews();
                    return _viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
                }
                bool isPositionTracked()
                {
                    if (!_locatedViews)
                        locateViews();
                    return _viewState.viewStateFlags & XR_VIEW_STATE_POSITION_TRACKED_BIT;
                }
                uint32_t getNumViews()
                {
                    if (!_locatedViews)
                        locateViews();
                    return _views.size();
                }
                const XrFovf &getViewFov(uint32_t index)
                {
                    if (!_locatedViews)
                        locateViews();
                    return _views[index].fov;
                }
                const XrPosef &getViewPose(uint32_t index)
                {
                    if (!_locatedViews)
                        locateViews();
                    return _views[index].pose;
                }

                // Modifiers

                inline void setEnvBlendMode(XrEnvironmentBlendMode envBlendMode)
                {
                    _envBlendMode = envBlendMode;
                }
                inline XrEnvironmentBlendMode getEnvBlendMode() const
                {
                    return _envBlendMode;
                }

                void addLayer(osg::ref_ptr<CompositionLayer> layer);

                // Operations

                bool begin();
                bool end();

            protected:

                // Frame info
                osg::ref_ptr<Session> _session;
                XrTime _time;
                XrDuration _period;
                bool _shouldRender;

                // View locations
                bool _locatedViews;
                XrViewState _viewState;
                std::vector<XrView> _views;

                // Frame end info
                bool _begun;
                XrEnvironmentBlendMode _envBlendMode;
                std::vector<osg::ref_ptr<CompositionLayer> > _layers;
        };

        osg::ref_ptr<Frame> waitFrame();

    protected:

        // Init data
        osg::observer_ptr<osgViewer::GraphicsWindow> _window;

        // Session data
        osg::ref_ptr<Instance> _instance;
        const System *_system;
        XrSession _session;
        const System::ViewConfiguration *_viewConfiguration;

        // Session state
        XrSessionState _state;
        bool _ready;
        bool _begun;
        bool _shouldEnd;
        bool _shouldDestroy;

        // Swapchain formats
        mutable bool _readSwapchainFormats;
        mutable SwapchainFormats _swapchainFormats;

        // Reference spaces
        mutable bool _createdLocalSpace;
        mutable XrSpace _localSpace;
};

} // osgXR::OpenXR

} // osgXR

#endif
