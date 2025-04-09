// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_XRSTATE
#define OSGXR_XRSTATE 1

#include "OpenXR/ActionSet.h"
#include "OpenXR/EventHandler.h"
#include "OpenXR/Instance.h"
#include "OpenXR/InteractionProfile.h"
#include "OpenXR/System.h"
#include "OpenXR/Session.h"
#include "OpenXR/SwapchainGroup.h"
#include "OpenXR/SwapchainGroupSubImage.h"
#include "OpenXR/Compositor.h"
#include "OpenXR/DepthInfo.h"

#include "XRFramebuffer.h"
#include "FrameStampedVector.h"
#include "FrameStore.h"

#include <osg/Referenced>
#include <osg/observer_ptr>
#include <osg/ref_ptr>

#include <osgXR/ActionSet>
#include <osgXR/CompositionLayer>
#include <osgXR/Extension>
#include <osgXR/InteractionProfile>
#include <osgXR/Settings>
#include <osgXR/Subaction>
#include <osgXR/View>

#include <list>
#include <memory>
#include <vector>

namespace osg {
    class FrameStamp;
    class Program;
}

namespace osgViewer {
    class ViewerBase;
}

namespace osgXR {

class AppView;
class Manager;

class XRState : public OpenXR::EventHandler
{
    public:
        typedef Settings::VRMode VRMode;
        typedef Settings::SwapchainMode SwapchainMode;

        XRState(Settings *settings, Manager *manager = nullptr);

        /// Represents a swapchain group
        class XRSwapchain : public OpenXR::SwapchainGroup
        {
            public:

                XRSwapchain(XRState *state,
                            osg::ref_ptr<OpenXR::Session> session,
                            const OpenXR::System::ViewConfiguration::View &view,
                            int64_t chosenRGBAFormat,
                            int64_t chosenDepthFormat,
                            GLenum fallbackDepthFormat,
                            unsigned int fbPerLayer = 0);

                // GL context must be current (for XRFramebuffer)
                virtual ~XRSwapchain();

                void setForcedAlpha(float alpha = -1.0f)
                {
                    _forcedAlpha = alpha;
                }

                void incNumDrawPasses(unsigned int num = 1)
                {
                    _numDrawPasses += num;
                }

                void decNumDrawPasses(unsigned int num = 1)
                {
                    _numDrawPasses -= num;
                }

                unsigned int getNumDrawPasses()
                {
                    return _numDrawPasses;
                }

                void setupImage(const osg::FrameStamp *stamp);

                void preDrawCallback(osg::RenderInfo &renderInfo,
                                     unsigned int arrayIndex);
                void postDrawCallback(osg::RenderInfo &renderInfo,
                                      unsigned int arrayIndex);
                void endFrame();

                osg::ref_ptr<osg::Texture> getOsgTexture(const osg::FrameStamp *stamp);

            protected:

                XRState *_state;
                // Framebuffer for each layer, for each swapchain number
                typedef std::vector<osg::ref_ptr<XRFramebuffer> > FBVec;
                FrameStampedVector<FBVec> _imageFramebuffers;

                float _forcedAlpha;

                /// Number of expected draw passes.
                unsigned int _numDrawPasses;
                unsigned int _drawPassesDone;
                bool _imagesReady;
        };

        /// Represents an OpenXR view
        class XRView : public osg::Referenced
        {
            public:

                XRView(XRState *state,
                       uint32_t viewIndex,
                       osg::ref_ptr<XRSwapchain> swapchain);
                XRView(XRState *state,
                       uint32_t viewIndex,
                       osg::ref_ptr<XRSwapchain> swapchain,
                       const OpenXR::System::ViewConfiguration::View::Viewport &viewport);

                // GL context must be current (for XRFramebuffer)
                virtual ~XRView();

                bool valid() const
                {
                    return _swapchainSubImage.valid();
                }

                osg::ref_ptr<XRSwapchain> getSwapchain()
                {
                    return static_cast<XRSwapchain *>(_swapchainSubImage.getSwapchainGroup().get());
                }

                const XRSwapchain::SubImage &getSubImage() const
                {
                    return _swapchainSubImage;
                }

                void endFrame(OpenXR::Session::Frame *frame);

            protected:

                XRState *_state;
                XRSwapchain::SubImage _swapchainSubImage;

                uint32_t _viewIndex;
        };

        /// Public View::SubView interface.
        class AppSubView : public View::SubView
        {
            public:
                AppSubView(XRView *xrView,
                           const osg::Matrix &viewMatrix,
                           const osg::Matrix &projectionMatrix);

                // Inherited from View::SubView
                unsigned int getArrayIndex() const override;
                Viewport getViewport() const override;
                const osg::Matrix &getViewMatrix() const override;
                const osg::Matrix &getProjectionMatrix() const override;

            protected:
                XRView *_xrView;

                osg::Matrix _viewMatrix;
                osg::Matrix _projectionMatrix;
        };

        inline Manager *getManager()
        {
            return _manager.get();
        }

        bool hasValidationLayer() const;
        bool hasDepthInfoExtension() const;
        bool hasVisibilityMaskExtension() const;

        inline XrVersion getApiVersion() const
        {
            if (_currentState < VRSTATE_INSTANCE)
                return 0;
            return _instance->getApiVersion();
        }

        inline const char *getRuntimeName() const
        {
            if (_currentState < VRSTATE_INSTANCE)
                return "";
            return _instance->getRuntimeName();
        }

        inline XrVersion getRuntimeVersion() const
        {
            if (_currentState < VRSTATE_INSTANCE)
                return 0;
            return _instance->getRuntimeVersion();
        }

        inline const char *getSystemName() const
        {
            if (_currentState < VRSTATE_SYSTEM)
                return "";
            return _system->getSystemName();
        }

        inline bool getPresent() const
        {
            return _instance.valid() && _instance->valid();
        }

        inline bool valid() const
        {
            return _currentState >= VRSTATE_SESSION;
        }

        typedef enum {
            /// No OpenXR instance.
            VRSTATE_DISABLED = 0,
            /// OpenXR instance created.
            VRSTATE_INSTANCE,
            /// Valid OpenXR system found.
            VRSTATE_SYSTEM,
            /// Session created
            VRSTATE_SESSION,
            /// Actions configured
            VRSTATE_ACTIONS,

            VRSTATE_MAX,
        } VRState;

        /// Set the init state to drop down to before returning to prior level.
        void setDownState(VRState downState)
        {
            if (downState < _downState && downState < _currentState)
            {
                _downState = downState;
                _stateChanged = true;
            }
        }
        /// Get the current init state to rise up to.
        VRState getUpState() const
        {
            return _upState;
        }
        /// Get the current init state to rise up to.
        VRState getCurrentState() const
        {
            return _currentState;
        }
        /// Set the init state to rise up to.
        void setUpState(VRState upState)
        {
            if (upState != _upState)
            {
                _upState = upState;
                _stateChanged = true;
            }
        }
        /// Set the minimum init state to rise up to.
        void setMinUpState(VRState minUpState)
        {
            if (minUpState > _upState)
            {
                _upState = minUpState;
                _stateChanged = true;
            }
        }
        /// Set destination state, both up and down.
        void setDestState(VRState destState)
        {
            setDownState(destState);
            setUpState(destState);
        }
        /// Find if updates are needed for state changes.
        bool isStateUpdateNeeded() const
        {
            return _currentState > _downState || _currentState < _upState;
        }

        /// Get the session object.
        OpenXR::Session *getSession()
        {
            return _session;
        }

        /// Find if a VR session is running.
        bool isRunning() const
        {
            if (_currentState < VRSTATE_SESSION)
                return false;
            return _session->isRunning() && !_session->isLost();
        }

        /// Get the VR mode in use.
        VRMode getVRMode() const
        {
            return _vrMode;
        }

        /// Get the swapchain mode in use.
        SwapchainMode getSwapchainMode() const
        {
            return _swapchainMode;
        }

        /// Set whether probing should be active.
        void setProbing(bool probing)
        {
            if (_probing == probing)
                return;
            _probing = probing;
            if (probing)
            {
                // Init at least up to system
                setMinUpState(VRSTATE_SYSTEM);
            }
            else
            {
                // If only initing to system, shutdown
                if (_upState <= VRSTATE_SYSTEM)
                    setDestState(VRSTATE_DISABLED);
            }
        }

        VRState getProbingState() const
        {
            if (_instance.valid() && _instance->getQuirk(OpenXR::QUIRK_AVOID_DESTROY_INSTANCE))
                return _probing ? VRSTATE_SYSTEM : VRSTATE_DISABLED;
            return VRSTATE_DISABLED;
        }

        void setViewer(osgViewer::ViewerBase *viewer)
        {
            _viewer = viewer;
        }

        /// Set the NodeMasks to use for visibility masks.
        void setVisibilityMaskNodeMasks(osg::Node::NodeMask left,
                                        osg::Node::NodeMask right)
        {
            _visibilityMaskLeft = left;
            _visibilityMaskRight = right;
        }

        /// Get the extension object for an extension name.
        std::shared_ptr<Extension::Private> getExtension(const std::string &name);
        /// Get a vector of all available extension names.
        std::vector<std::string> getExtensionNames();
        /// Enable an OpenXR extension.
        void enableExtension(std::shared_ptr<Extension::Private> extension)
        {
            _enabledExtensions.insert(extension);
        }
        /// Disable an OpenXR extension.
        void disableExtension(std::shared_ptr<Extension::Private> extension)
        {
            _enabledExtensions.erase(extension);
        }
        /// Enable an OpenXR extension by name.
        std::shared_ptr<Extension::Private> enableExtension(const std::string &name)
        {
            auto extension = getExtension(name);
            enableExtension(extension);
            return extension;
        }

        /// Get the subaction object for a subaction path string.
        std::shared_ptr<Subaction::Private> getSubaction(const std::string &path);

        /// Add an action set
        void addActionSet(ActionSet::Private *actionSet)
        {
            _actionSets.insert(actionSet);
            _actionsUpdated = true;
        }

        /// Remove an action set
        void removeActionSet(ActionSet::Private *actionSet)
        {
            _actionSets.erase(actionSet);
            _actionsUpdated = true;
        }

        /// Add an interaction profile
        void addInteractionProfile(InteractionProfile::Private *interactionProfile)
        {
            _interactionProfiles.insert(interactionProfile);
            _actionsUpdated = true;
        }

        /// Remove an interaction profile
        void removeInteractionProfile(InteractionProfile::Private *interactionProfile)
        {
            _interactionProfiles.erase(interactionProfile);
            _actionsUpdated = true;
        }

        /// Get the current interaction profile for the given subaction path.
        InteractionProfile *getCurrentInteractionProfile(const OpenXR::Path &subactionPath) const;

        /// Get a string describing the state (for user consumption).
        const char *getStateString() const;

        // Initialize information required for setting up VR
        void init(osgViewer::GraphicsWindow *window,
                  osgViewer::View *view = nullptr)
        {
            _window = window;
            _view = view;
        }

        /// Update down state depending on any changed settings.
        void syncSettings();

        /// Get the number of virtual world units to fit per real world meter.
        float getUnitsPerMeter() const
        {
            return _settings->getUnitsPerMeter();
        }

        /// Find whether actions have been updated.
        bool getActionsUpdated() const;

        /// Arrange reinit as needed of action setup.
        void syncActionSetup();

        /// Add a composition layer
        void addCompositionLayer(CompositionLayer::Private *layer);

        /// Remove a composition layer
        void removeCompositionLayer(CompositionLayer::Private *layer);

        /// Find whether state has changed since last call, and reset.
        bool checkAndResetStateChanged();

        /// Perform a regular update.
        void update();

        /// Recenter the local space.
        bool recenterLocalSpace();

        // Extending OpenXR::EventManager
        void onInstanceLossPending(OpenXR::Instance *instance,
                                   const XrEventDataInstanceLossPending *event) override;
        void onInteractionProfileChanged(OpenXR::Session *session,
                                         const XrEventDataInteractionProfileChanged *event) override;
        void onReferenceSpaceChangePending(OpenXR::Session *session,
                                           const XrEventDataReferenceSpaceChangePending *event) override;
        void onSessionStateChanged(OpenXR::Session *session,
                                   const XrEventDataSessionStateChanged *event) override;
        void onSessionStateStart(OpenXR::Session *session) override;
        void onSessionStateEnd(OpenXR::Session *session, bool retry) override;
        void onSessionStateReady(OpenXR::Session *session) override;
        void onSessionStateStopping(OpenXR::Session *session, bool loss) override;
        void onSessionStateFocus(OpenXR::Session *session) override;
        void onSessionStateUnfocus(OpenXR::Session *session) override;

        osg::ref_ptr<OpenXR::Session::Frame> getFrame(osg::FrameStamp *stamp);
        void startRendering(osg::FrameStamp *stamp);
        void endFrame(osg::FrameStamp *stamp);

        static void updateVisibilityMaskTransform(osg::Camera *camera,
                                                  osg::MatrixTransform *transform);

        void initialDrawCallback(osg::RenderInfo &renderInfo,
                                 View::Flags flags);
        void releaseGLObjects(osg::State *state);
        void swapBuffersImplementation(osg::GraphicsContext* gc);

        inline osg::ref_ptr<OpenXR::CompositionLayerProjection> getProjectionLayer()
        {
            return _projectionLayer;
        }

        class TextureRect
        {
            public:

                float x, y;
                float width, height;
                unsigned int arrayIndex;

                TextureRect(const OpenXR::SwapchainGroup::SubImage &subImage)
                {
                    float w = subImage.getSwapchainGroup()->getWidth();
                    float h = subImage.getSwapchainGroup()->getHeight();
                    x = (float)subImage.getX() / w;
                    y = (float)subImage.getY() / h;
                    width = (float)subImage.getWidth() / w;
                    height = (float)subImage.getHeight() / h;
                    arrayIndex = subImage.getArrayIndex();
                }
        };

        unsigned int getViewCount() const
        {
            return _xrViews.size();
        }

        XRView *getView(unsigned int viewIndex) const
        {
            return _xrViews[viewIndex].get();
        }

        TextureRect getViewTextureRect(unsigned int viewIndex) const
        {
            return TextureRect(_xrViews[viewIndex]->getSubImage());
        }

        // Caller must validate viewIndex using getViewCount()
        osg::ref_ptr<osg::Texture> getViewTexture(unsigned int viewIndex,
                                                  const osg::FrameStamp *stamp) const
        {
            return _xrViews[viewIndex]->getSwapchain()->getOsgTexture(stamp);
        }

        /**
         * Validate a particular VR and swapchain mode combination.
         * @param[in]  vrMode        VR mode.
         * @param[in]  swapchainMode Swapchain mode.
         * @param[out] outErrors     Vector of error messages (cleared).
         * @return true on success, false on failure.
         */
        bool validateMode(VRMode vrMode, SwapchainMode swapchainMode,
                          std::vector<const char *> &outErrors) const;
        /**
         * Choose a VR and swapchain mode.
         * @param[out] outVRMode        Pointer to write chosen VR mode to.
         * @param[out] outSwapchainMode Pointer to write chosen swapchain mode
         *                              to.
         */
        void chooseMode(VRMode *outVRMode,
                        SwapchainMode *outSwapchainMode) const;

        /**
         * Choose an RGBA swapchain format.
         * @param bestRGBBits               Desired number of combined RGB bits.
         * @param bestAlphaBits             Desired number of alpha bits.
         * @param preferredRGBEncodingMask  Mask of preferred RGB encodings (see
         *                                  Settings::Encoding).
         * @param allowedRGBEncodingMask    Mask of allowed RGB encodings (see
         *                                  Settings::Encoding).
         * @return The chosen OpenGL swapchain format.
         */
        int64_t chooseRGBAFormat(unsigned int bestRGBBits,
                                 unsigned int bestAlphaBits,
                                 uint32_t preferredRGBEncodingMask,
                                 uint32_t allowedRGBEncodingMask) const;
        /**
         * Choose a fallback depth / stencil format for use if the OpenXR
         * runtime doesn't support depth swapchains.
         * @param bestDepthBits              Desired number of depth bits.
         * @param bestStencilBits            Desired number of stencil bits.
         * @param preferredDepthEncodingMask Mask of preferred depth encodings
         *                                   (see Settings::Encoding).
         * @param allowedDepthEncodingMask   Mask of allowed depth encodings
         *                                   (see Settings::Encoding).
         * @return The chosen fallback OpenGL depth / stencil format.
         */
        GLenum chooseFallbackDepthFormat(unsigned int bestDepthBits,
                                         unsigned int bestStencilBits,
                                         uint32_t preferredDepthEncodingMask,
                                         uint32_t allowedDepthEncodingMask) const;
        /**
         * Choose a depth / stencil swapchain format for submission to OpenXR.
         * @param bestDepthBits              Desired number of depth bits.
         * @param bestStencilBits            Desired number of stencil bits.
         * @param preferredDepthEncodingMask Mask of preferred depth encodings
         *                                   (see Settings::Encoding).
         * @param allowedDepthEncodingMask   Mask of allowed depth encodings
         *                                   (see Settings::Encoding).
         * @return The chosen OpenGL depth / stencil swapchain format.
         */
        int64_t chooseDepthFormat(unsigned int bestDepthBits,
                                  unsigned int bestStencilBits,
                                  uint32_t preferredDepthEncodingMask,
                                  uint32_t allowedDepthEncodingMask) const;


        // Notify app of new AppView (for use by AppView)
        void initAppView(AppView *appView);
        // Notify app of destroyed AppView (for use by AppView)
        void destroyAppView(AppView *appView);

        // Visibility mask setup
        inline bool needsVisibilityMask(osg::Camera *camera)
        {
            return _useVisibilityMask &&
                (camera->getClearMask() & GL_DEPTH_BUFFER_BIT);
        }
        void setupSceneViewVisibilityMasks(osg::Camera *camera,
                                           osg::ref_ptr<osg::MatrixTransform> &transform);
        osg::ref_ptr<osg::Geode> setupVisibilityMask(osg::Camera *camera,
                                                     uint32_t viewIndex,
                                                     osg::ref_ptr<osg::MatrixTransform> &transform);

    protected:

        typedef enum {
            /// Successfully completed operation.
            UP_SUCCESS,
            /// Operation not possible at the moment, try again soon.
            UP_SOON,
            /// Operation not possible at the moment, try again later.
            UP_LATER,
            /// Operation permanently failed, disable VR.
            UP_ABORT,
        } UpResult;

        typedef enum {
            /// Successfully completed operation.
            DOWN_SUCCESS,
            /// Operation not possible at the moment, try again soon.
            DOWN_SOON,
        } DownResult;

        // Pre-instance probing
        void probe() const;
        void unprobe() const;

        // These are called during update to raise or lower VR state level
        UpResult upInstance();
        DownResult downInstance();
        UpResult upSystem();
        DownResult downSystem();
        UpResult upSession();
        DownResult downSession();
        UpResult upActions();
        DownResult downActions();

        // Drop the _session and check it gets cleaned up
        bool dropSessionCheck();

        // Set up a single swapchain containing multiple viewports
        bool setupSingleSwapchain(int64_t format, int64_t depthFormat = 0,
                                  GLenum fallbackDepthFormat = 0);
        // Set up a single swapchain containing multiple layers
        bool setupLayeredSwapchain(int64_t format, int64_t depthFormat = 0,
                                   GLenum fallbackDepthFormat = 0);
        // Set up a swapchain for each view
        bool setupMultipleSwapchains(int64_t format, int64_t depthFormat = 0,
                                     GLenum fallbackDepthFormat = 0);
        // Set up slave cameras
        void setupSlaveCameras();
        // Set up SceneView VR mode cameras
        void setupSceneViewCameras();
        // Set up geometry shaders VR mode cameras
        void setupGeomShadersCameras();
        // Set up OVR_multiview VR mode cameras
        void setupOVRMultiviewCameras();

        osg::ref_ptr<Settings> _settings;
        Settings _settingsCopy;
        osg::observer_ptr<Manager> _manager;
        std::map<std::string, std::weak_ptr<Extension::Private>> _extensions;
        std::shared_ptr<Extension::Private> _extDepthInfo;
        std::shared_ptr<Extension::Private> _extDepthUtils;
        std::shared_ptr<Extension::Private> _extVisibilityMask;
        std::set<std::shared_ptr<Extension::Private>> _enabledExtensions;

        // app configuration
        osg::Node::NodeMask _visibilityMaskLeft;
        osg::Node::NodeMask _visibilityMaskRight;

        // Actions
        bool _actionsUpdated;
        std::set<ActionSet::Private *> _actionSets;
        std::set<InteractionProfile::Private *> _interactionProfiles;
        std::map<std::string, std::weak_ptr<Subaction::Private>> _subactions;

        // Composition layers
        bool _compositionLayersUpdated;
        std::list<CompositionLayer::Private *> _compositionLayers;

        /// Current state of OpenXR initialization.
        VRState _currentState;
        /// State of OpenXR initialisation to drop down to.
        VRState _downState;
        /// State of OpenXR initialisation to rise up to.
        VRState _upState;
        /// Number of attempts made to rise VR state.
        unsigned int _upDelay;
        /// Whether probing should be kept active.
        bool _probing;
        /// Last read state as a user readable string.
        mutable std::string _stateString;
        /// Whether state has changed since the last update.
        bool _stateChanged;
        /// Whether threading was in use prior to update().
        bool _wasThreading;

        // Session setup
        osg::observer_ptr<osgViewer::ViewerBase> _viewer;
        osg::observer_ptr<osgViewer::GraphicsWindow> _window;
        osg::observer_ptr<osgViewer::View> _view;

        // Pre-Instance related
        mutable bool _probed;
        mutable bool _hasValidationLayer;
        mutable bool _hasDepthInfoExtension;
        mutable bool _hasVisibilityMaskExtension;

        // Instance related
        osg::ref_ptr<OpenXR::Instance> _instance;
        bool _useDepthInfo;
        bool _useVisibilityMask;
        OpenXR::Instance::Result _lastError;
        OpenXR::Instance::Result _lastRunError;

        // System related
        XrFormFactor _formFactor;
        OpenXR::System *_system;
        const OpenXR::System::ViewConfiguration *_chosenViewConfig;
        XrEnvironmentBlendMode _chosenEnvBlendMode;

        // Session related
        VRMode _vrMode;
        SwapchainMode _swapchainMode;
        osg::ref_ptr<OpenXR::Session> _session;
        std::vector<osg::ref_ptr<XRView> > _xrViews;
        std::vector<osg::ref_ptr<AppView> > _appViews;
        FrameStore _frames;
        osg::ref_ptr<OpenXR::CompositionLayerProjection> _projectionLayer;
        OpenXR::DepthInfo _depthInfo;
        osg::ref_ptr<osg::Program> _visibilityMaskProgram;
};

} // osgXR

#endif
