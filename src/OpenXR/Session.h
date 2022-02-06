// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_SESSION
#define OSGXR_OPENXR_SESSION 1

#include "Path.h"
#include "System.h"

#include <osg/Geometry>
#include <osg/Referenced>
#include <osg/ref_ptr>
#include <osgViewer/GraphicsWindow>
#include <OpenThreads/Mutex>

#include <set>

namespace osgXR {

namespace OpenXR {

class Action;
class ActionSet;
class CompositionLayer;
class Space;

class Session : public osg::Referenced
{
    public:

        // GL context must not be bound in another thread
        Session(System *system, osgViewer::GraphicsWindow *window);
        // GL context must not be bound in another thread
        virtual ~Session();

        // GL context must not be bound in another thread
        void releaseGLObjects(osg::State *state = nullptr);

        // Error checking

        inline bool valid() const
        {
            return _session != XR_NULL_HANDLE;
        }

        inline bool check(XrResult result, const char *warnMsg) const
        {
            return _system->check(result, warnMsg);
        }

        // Action set attachment

        /// Add an action set to the list.
        void addActionSet(ActionSet *actionSet);
        /**
         * Attach the added action sets to the OpenXR session.
         * @return true on success, false on failure.
         */
        bool attachActionSets();

        /// Get the current interaction profile for the given subaction path.
        Path getCurrentInteractionProfile(const Path &subactionPath) const;

        /// Get a list of bound source paths for an action.
        bool getActionBoundSources(Action *action,
                                   std::vector<XrPath> &sourcePaths) const;

        /**
         * Get a localized name for an input source.
         * @param sourcePath      Input source path.
         * @param whichComponents Which components to include.
         * @return Localized name string
         */
        std::string getInputSourceLocalizedName(XrPath sourcePath,
                                                XrInputSourceLocalizedNameFlags whichComponents) const;

        // Action syncing

        /// Activate a certain action set.
        void activateActionSet(ActionSet *actionSet,
                               Path subactionPath = Path());
        /// Deactivate a certain action set.
        void deactivateActionSet(ActionSet *actionSet,
                                 Path subactionPath = Path());
        /// Sync active action sets.
        bool syncActions();

        /// Get the number of action sync counts that have taken place.
        unsigned int getActionSyncCount() const
        {
            return _actionSyncCount;
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

        Space *getLocalSpace();
        XrTime getLastDisplayTime() const
        {
            return _lastDisplayTime;
        }

        void updateVisibilityMasks(XrViewConfigurationType viewConfigurationType,
                                   uint32_t viewIndex);
        osg::ref_ptr<osg::Geometry> getVisibilityMask(uint32_t viewIndex,
                                                      XrVisibilityMaskTypeKHR visibilityMaskType,
                                                      bool force = false);

        // Operations

        /**
         * Check whether the session's GLX context is current.
         * This is intended for internal use only by the functions below.
         * @return whether the session's GLX context is current.
         */
        bool checkCurrent() const;

        /**
         * Make the GLX context current.
         * This makes the GLX context current to workaround broken XR runtimes.
         * It should be used to ensure the context is current before XR calls
         * affected by QUIRK_GL_CONTEXT_IGNORED if shouldSwitchContext()
         * (because the XR runtime will fail to do so), and to restore the
         * context after XR calls affected by QUIRK_GL_CONTEXT_CLEARED if
         * shouldRestoreContext() or getRestoreAction() == CONTEXT_RESTORE
         * (because the XR runtime will have explicitly released the context).
         */
        void makeCurrent() const;
        /**
         * Release the GLX context so that none is current.
         * This makes no GLX context current to workaround broken XR runtimes.
         * It should be used release the switched context after XR calls
         * affected by QUIRK_GL_CONTEXT_IGNORED if shouldSwitchContext()
         * (because we had to call makeCurrent() before the call), and after XR
         * calls affected by QUIRK_GL_CONTEXT_CHANGED if getRestoreAction() ==
         * CONTEXT_RELEASE (because the XR runtime failed to do so).
         */
        void releaseContext() const;

        /// Action to perform before or after XR call due to quirks.
        typedef enum ContextAction {
            /// No action is necessary.
            CONTEXT_IGNORE = 0,
            /// The GLX context should be restored.
            CONTEXT_RESTORE,
            /**
             * The GLX context should be released leaving no context current.
             * It is assumed that no other context will need restoring in its
             * place.
             */
            CONTEXT_RELEASE
        } ContextAction;

        /**
         * Find whether for an XR call affected by QUIRK_GL_CONTEXT_IGNORED it
         * is necessary to switch the context prior to the XR call with
         * makeCurrent(), and also release it afterwards with releaseContext.
         * @return true if the context should be switched and released,
         *         false otherwise.
         */
        bool shouldSwitchContext() const
        {
            return _instance->getQuirk(QUIRK_GL_CONTEXT_IGNORED) &&
                   !checkCurrent();
        }
        /**
         * Find whether for an XR call affected by QUIRK_GL_CONTEXT_CLEARED it
         * is necessary to restore the context after the XR call.
         * @return true if the context should be restored after,
         *         false otherwise.
         */
        bool shouldRestoreContext() const
        {
            return _instance->getQuirk(QUIRK_GL_CONTEXT_CLEARED) &&
                   checkCurrent();
        }
        /**
         * Find what GL context action to perform after an XR call affected by
         * QUIRK_GL_CONTEXT_CLEARED or QUIRK_GL_CONTEXT_CHANGED.
         * @return CONTEXT_RESTORE if the runtime may clear the context and it
         *                         will need restoring.
         *         CONTEXT_RELEASE if the runtime may change the context and
         *                         fail to release it.
         *         CONTEXT_IGNORE  otherwise.
         */
        ContextAction getRestoreAction() const
        {
            bool cleared = _instance->getQuirk(QUIRK_GL_CONTEXT_CLEARED);
            bool changed = _instance->getQuirk(QUIRK_GL_CONTEXT_CHANGED);
            if (cleared || changed)
            {
                bool current = checkCurrent();
                if (cleared && current)
                    return CONTEXT_RESTORE;
                if (changed && !current)
                    return CONTEXT_RELEASE;
            }
            return CONTEXT_IGNORE;
        }


        bool begin(const System::ViewConfiguration &viewConfiguration);
        void end();
        void requestExit();

        const System::ViewConfiguration *getViewConfiguration() const
        {
            return _viewConfiguration;
        }

        class Frame : public osg::Referenced
        {
            public:

                Frame(osg::ref_ptr<Session> session, XrFrameState *frameState);

                virtual ~Frame();

                // Error checking

                inline bool check(XrResult result, const char *warnMsg) const
                {
                    return _session->check(result, warnMsg);
                }

                // Accessors

                inline Session *getSession()
                {
                    return _session;
                }

                inline bool shouldRender() const
                {
                    return _shouldRender;
                }

                inline bool hasBegun() const
                {
                    return _begun;
                }

                inline XrTime getTime() const
                {
                    return _time;
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

        // Action sets
        std::set<osg::ref_ptr<ActionSet>> _actionSets;
        typedef std::pair<ActionSet *, XrPath> ActionSetSubactionPair;
        std::set<ActionSetSubactionPair> _activeActionSets;
        unsigned int _actionSyncCount;

        // Session state
        XrSessionState _state;
        bool _running;
        bool _exiting;

        // Swapchain formats
        mutable bool _readSwapchainFormats;
        mutable SwapchainFormats _swapchainFormats;

        // Reference spaces
        osg::ref_ptr<Space> _localSpace;
        XrTime _lastDisplayTime;

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
