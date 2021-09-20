// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_SESSION
#define OSGXR_OPENXR_SESSION 1

#include "System.h"

#include <osg/Geometry>
#include <osg/Referenced>
#include <osg/ref_ptr>
#include <osgViewer/GraphicsWindow>
#include <OpenThreads/Mutex>

namespace osgXR {

namespace OpenXR {

class CompositionLayer;

class Session : public osg::Referenced
{
    public:

        // GL context must not be bound in another thread
        Session(System *system, osgViewer::GraphicsWindow *window);
        // GL context must not be bound in another thread
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

        // Find whether the session is ready to begin
        inline bool isReady() const
        {
            return _state == XR_SESSION_STATE_READY;
        }

        // Find whether the session is running
        inline bool isRunning() const
        {
            return _running;
        }

        // Find whether the session is already in the process of exiting
        inline bool isExiting() const
        {
            return _exiting;
        }

        inline osgViewer::GraphicsWindow *getWindow() const
        {
            return _window.get();
        }

        // State management

        inline XrSessionState getState() const
        {
            return _state;
        }

        inline void setState(XrSessionState state)
        {
            _state = state;
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

        void updateVisibilityMasks(XrViewConfigurationType viewConfigurationType,
                                   uint32_t viewIndex);
        osg::ref_ptr<osg::Geometry> getVisibilityMask(uint32_t viewIndex,
                                                      XrVisibilityMaskTypeKHR visibilityMaskType,
                                                      bool force = false);

        // Operations

        bool checkCurrent() const;
        void makeCurrent() const;
        void releaseContext() const;

        bool begin(const System::ViewConfiguration &viewConfiguration);
        void end();
        void requestExit();

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

                void checkLocateViews()
                {
                    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_locateViewsMutex);
                    if (!_locatedViews)
                        locateViews();
                }

                bool isOrientationValid()
                {
                    checkLocateViews();
                    return _viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT;
                }
                bool isPositionValid()
                {
                    checkLocateViews();
                    return _viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT;
                }
                bool isOrientationTracked()
                {
                    checkLocateViews();
                    return _viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
                }
                bool isPositionTracked()
                {
                    checkLocateViews();
                    return _viewState.viewStateFlags & XR_VIEW_STATE_POSITION_TRACKED_BIT;
                }
                uint32_t getNumViews()
                {
                    checkLocateViews();
                    return _views.size();
                }
                const XrFovf &getViewFov(uint32_t index)
                {
                    checkLocateViews();
                    return _views[index].fov;
                }
                const XrPosef &getViewPose(uint32_t index)
                {
                    checkLocateViews();
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

                inline void setOsgFrameNumber(unsigned int osgFrameNumber)
                {
                    _osgFrameNumber = osgFrameNumber;
                }
                inline unsigned int getOsgFrameNumber() const
                {
                    return _osgFrameNumber;
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

                // OpenSceneGraph frame
                unsigned int _osgFrameNumber;

                // For access to _locatedViews etc
                OpenThreads::Mutex _locateViewsMutex;

                // View locations (protected by _locateViewsMutex)
                bool _locatedViews;
                XrViewState _viewState;
                std::vector<XrView> _views;

                // Frame end info
                bool _begun;
                XrEnvironmentBlendMode _envBlendMode;
                std::vector<osg::ref_ptr<CompositionLayer> > _layers;
        };

        osg::ref_ptr<Frame> waitFrame();

        // OpenXR extension wrappers
        XrResult xrGetVisibilityMask(const System::ViewConfiguration &viewConfiguration,
                                   uint32_t viewIndex,
                                   XrVisibilityMaskTypeKHR visibilityMaskType,
                                   XrVisibilityMaskKHR *visibilityMask)
        {
            return _instance->xrGetVisibilityMask(_session,
                                                  viewConfiguration.getType(),
                                                  viewIndex, visibilityMaskType,
                                                  visibilityMask);
        }

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
        bool _running;
        bool _exiting;

        // Swapchain formats
        mutable bool _readSwapchainFormats;
        mutable SwapchainFormats _swapchainFormats;

        // Reference spaces
        mutable bool _createdLocalSpace;
        mutable XrSpace _localSpace;

        /*
         * Visibility mask geometry cache.
         * We keep visibility mask geometries cached to avoid duplication and so
         * we can update them after a VisibilityMaskChangedKHR event.
         */
        typedef osg::ref_ptr<osg::Geometry> VisMaskGeometry;
        typedef std::vector<VisMaskGeometry> VisMaskGeometryView;
        typedef std::vector<VisMaskGeometryView> VisMaskGeometries;
        VisMaskGeometries _visMaskCache;
};

} // osgXR::OpenXR

} // osgXR

#endif
