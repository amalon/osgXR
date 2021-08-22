// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_XRSTATE
#define OSGXR_XRSTATE 1

#include "OpenXR/EventHandler.h"
#include "OpenXR/Instance.h"
#include "OpenXR/System.h"
#include "OpenXR/Session.h"
#include "OpenXR/SwapchainGroup.h"
#include "OpenXR/SwapchainGroupSubImage.h"
#include "OpenXR/Compositor.h"
#include "OpenXR/DepthInfo.h"

#include "XRFramebuffer.h"
#include "FrameStampedVector.h"
#include "FrameStore.h"

#include <osg/DisplaySettings>
#include <osg/Referenced>
#include <osg/observer_ptr>
#include <osg/ref_ptr>

#include <osgXR/Settings>
#include <osgXR/View>

#include <vector>

namespace osg {
    class FrameStamp;
}

namespace osgViewer {
    class ViewerBase;
}

namespace osgXR {

class Manager;

class XRState : public OpenXR::EventHandler
{
    public:
        typedef Settings::VRMode VRMode;
        typedef Settings::SwapchainMode SwapchainMode;

        XRState(Settings *settings, Manager *manager = nullptr);

        class XRSwapchain : public OpenXR::SwapchainGroup
        {
            public:

                XRSwapchain(XRState *state,
                            osg::ref_ptr<OpenXR::Session> session,
                            const OpenXR::System::ViewConfiguration::View &view,
                            int64_t chosenSwapchainFormat,
                            int64_t chosenDepthSwapchainFormat);

                // GL context must be current (for XRFramebuffer)
                virtual ~XRSwapchain();

                void incNumDrawPasses()
                {
                    ++_numDrawPasses;
                }

                void decNumDrawPasses()
                {
                    --_numDrawPasses;
                }

                void setupImage(const osg::FrameStamp *stamp);

                void preDrawCallback(osg::RenderInfo &renderInfo);
                void postDrawCallback(osg::RenderInfo &renderInfo);
                void endFrame();

                osg::ref_ptr<osg::Texture2D> getOsgTexture(const osg::FrameStamp *stamp);

            protected:

                XRState *_state;
                FrameStampedVector<osg::ref_ptr<XRFramebuffer> > _imageFramebuffers;

                unsigned int _numDrawPasses;
                unsigned int _drawPassesDone;
                bool _imagesReady;
        };

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

                void setupCamera(osg::ref_ptr<osg::Camera> camera);

                void endFrame(OpenXR::Session::Frame *frame);

            protected:

                XRState *_state;
                XRSwapchain::SubImage _swapchainSubImage;

                uint32_t _viewIndex;
        };

        class AppView : public View
        {
            public:

                AppView(XRState *state,
                        osgViewer::GraphicsWindow *window,
                        osgViewer::View *osgView);
                virtual ~AppView();

                void destroy();

                void init();

            protected:

                bool _valid;

                XRState *_state;
        };

        class SlaveCamsAppView : public AppView
        {
            public:

                SlaveCamsAppView(XRState *state,
                                 uint32_t viewIndex,
                                 osgViewer::GraphicsWindow *window,
                                 osgViewer::View *osgView);

                void addSlave(osg::Camera *slaveCamera) override;
                void removeSlave(osg::Camera *slaveCamera) override;

            protected:

                uint32_t _viewIndex;
        };
        class SceneViewAppView : public AppView
        {
            public:

                SceneViewAppView(XRState *state,
                                 osgViewer::GraphicsWindow *window,
                                 osgViewer::View *osgView);

                void addSlave(osg::Camera *slaveCamera) override;
                void removeSlave(osg::Camera *slaveCamera) override;
        };

        bool hasValidationLayer() const;
        bool hasDepthInfoExtension() const;

        inline unsigned int getPassesPerView() const
        {
            return _passesPerView;
        }

        inline const char *getRuntimeName() const
        {
            if (_currentState < VRSTATE_INSTANCE)
                return "";
            return _instance->getRuntimeName();
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

            VRSTATE_MAX,
        } VRState;

        /// Set the init state to drop down to before returning to prior level.
        void setDownState(VRState downState)
        {
            if (downState < _downState && downState < _currentState)
                _downState = downState;
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
            _upState = upState;
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

        /// Find if a VR session is running.
        bool isRunning() const
        {
            if (_upState < VRSTATE_SESSION)
                return false;
            return _session->isRunning();
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
                if (_upState < VRSTATE_SYSTEM)
                    _upState = VRSTATE_SYSTEM;
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
            return _probing ? VRSTATE_SYSTEM : VRSTATE_DISABLED;
        }

        void setViewer(osgViewer::ViewerBase *viewer)
        {
            _viewer = viewer;
        }

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

        /// Perform a regular update.
        void update();

        // Extending OpenXR::EventManager
        void onInstanceLossPending(OpenXR::Instance *instance,
                                   const XrEventDataInstanceLossPending *event) override;
        void onSessionStateStart(OpenXR::Session *session) override;
        void onSessionStateEnd(OpenXR::Session *session, bool retry) override;
        void onSessionStateReady(OpenXR::Session *session) override;
        void onSessionStateStopping(OpenXR::Session *session, bool loss) override;
        void onSessionStateFocus(OpenXR::Session *session) override;
        void onSessionStateUnfocus(OpenXR::Session *session) override;

        osg::ref_ptr<OpenXR::Session::Frame> getFrame(osg::FrameStamp *stamp);
        void startRendering(osg::FrameStamp *stamp);
        void endFrame(osg::FrameStamp *stamp);

        void updateSlave(uint32_t viewIndex, osg::View& view,
                         osg::View::Slave& slave);

        osg::Matrixd getEyeProjection(osg::FrameStamp *stamp,
                                      uint32_t viewIndex,
                                      const osg::Matrixd& projection);
        osg::Matrixd getEyeView(osg::FrameStamp *stamp, uint32_t viewIndex,
                                const osg::Matrixd& view);

        void initialDrawCallback(osg::RenderInfo &renderInfo);
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

                TextureRect(const OpenXR::SwapchainGroup::SubImage &subImage)
                {
                    float w = subImage.getSwapchainGroup()->getWidth();
                    float h = subImage.getSwapchainGroup()->getHeight();
                    x = (float)subImage.getX() / w;
                    y = (float)subImage.getY() / h;
                    width = (float)subImage.getWidth() / w;
                    height = (float)subImage.getHeight() / h;
                }
        };

        unsigned int getViewCount() const
        {
            return _xrViews.size();
        }

        TextureRect getViewTextureRect(unsigned int viewIndex) const
        {
            return TextureRect(_xrViews[viewIndex]->getSubImage());
        }

        // Caller must validate viewIndex using getViewCount()
        osg::ref_ptr<osg::Texture2D> getViewTexture(unsigned int viewIndex,
                                                    const osg::FrameStamp *stamp) const
        {
            return _xrViews[viewIndex]->getSwapchain()->getOsgTexture(stamp);
        }

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

        // These are called during update to raise or lower VR state level
        UpResult upInstance();
        DownResult downInstance();
        UpResult upSystem();
        DownResult downSystem();
        UpResult upSession();
        DownResult downSession();

        // Set up a single swapchain containing multiple viewports
        bool setupSingleSwapchain(int64_t format, int64_t depthFormat = 0);
        // Set up a swapchain for each view
        bool setupMultipleSwapchains(int64_t format, int64_t depthFormat = 0);
        // Set up slave cameras
        void setupSlaveCameras();
        // Set up SceneView VR mode cameras
        void setupSceneViewCameras();
        void setupSceneViewCamera(osg::ref_ptr<osg::Camera> camera);

        osg::ref_ptr<Settings> _settings;
        Settings _settingsCopy;
        osg::observer_ptr<Manager> _manager;

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

        // Session setup
        osg::observer_ptr<osgViewer::ViewerBase> _viewer;
        osg::observer_ptr<osgViewer::GraphicsWindow> _window;
        osg::observer_ptr<osgViewer::View> _view;

        // Pre-Instance related
        mutable bool _probed;
        mutable bool _hasValidationLayer;
        mutable bool _hasDepthInfoExtension;

        // Instance related
        osg::ref_ptr<OpenXR::Instance> _instance;
        bool _useDepthInfo;

        // System related
        XrFormFactor _formFactor;
        OpenXR::System *_system;
        const OpenXR::System::ViewConfiguration *_chosenViewConfig;
        XrEnvironmentBlendMode _chosenEnvBlendMode;

        // Session related
        VRMode _vrMode;
        SwapchainMode _swapchainMode;
        unsigned int _passesPerView;
        osg::ref_ptr<OpenXR::Session> _session;
        std::vector<osg::ref_ptr<XRView> > _xrViews;
        std::vector<osg::ref_ptr<AppView> > _appViews;
        FrameStore _frames;
        osg::ref_ptr<OpenXR::CompositionLayerProjection> _projectionLayer;
        OpenXR::DepthInfo _depthInfo;
        osg::ref_ptr<osg::DisplaySettings> _stereoDisplaySettings;
};

} // osgXR

#endif
