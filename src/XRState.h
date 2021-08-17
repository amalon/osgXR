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

                virtual ~XRSwapchain()
                {
                }

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

                void init();

            protected:

                XRState *_state;
        };

        class SlaveCamsAppView : public AppView
        {
            public:

                SlaveCamsAppView(XRState *state,
                                 uint32_t viewIndex,
                                 osgViewer::GraphicsWindow *window,
                                 osgViewer::View *osgView);

                virtual void addSlave(osg::Camera *slaveCamera);
                virtual void removeSlave(osg::Camera *slaveCamera);

            protected:

                uint32_t _viewIndex;
        };
        class SceneViewAppView : public AppView
        {
            public:

                SceneViewAppView(XRState *state,
                                 osgViewer::GraphicsWindow *window,
                                 osgViewer::View *osgView);

                virtual void addSlave(osg::Camera *slaveCamera);
                virtual void removeSlave(osg::Camera *slaveCamera);
        };

        inline unsigned int getPassesPerView() const
        {
            return _passesPerView;
        }

        inline const char *getRuntimeName() const
        {
            if (!_valid)
                return nullptr;
            return _instance->getRuntimeName();
        }

        inline const char *getSystemName() const
        {
            if (!_valid)
                return nullptr;
            return _system->getSystemName();
        }

        inline bool getPresent() const
        {
            return _instance.valid() && _instance->valid();
        }

        inline bool valid() const
        {
            return _valid;
        }

        void init(osgViewer::GraphicsWindow *window,
                  osgViewer::View *view);

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

        // Set up a single swapchain containing multiple viewports
        bool setupSingleSwapchain(int64_t format, int64_t depthFormat = 0);
        // Set up a swapchain for each view
        bool setupMultipleSwapchains(int64_t format, int64_t depthFormat = 0);
        // Set up slave cameras
        void setupSlaveCameras(osgViewer::GraphicsWindow *window,
                               osgViewer::View *view);
        // Set up SceneView VR mode cameras
        void setupSceneViewCameras(osgViewer::GraphicsWindow *window,
                                   osgViewer::View *view);
        void setupSceneViewCamera(osg::ref_ptr<osg::Camera> camera);

        osg::ref_ptr<Settings> _settings;
        osg::observer_ptr<Manager> _manager;

        VRMode _vrMode;
        SwapchainMode _swapchainMode;

        osg::ref_ptr<OpenXR::Instance> _instance;
        XrFormFactor _formFactor;
        OpenXR::System *_system;
        const OpenXR::System::ViewConfiguration *_chosenViewConfig;
        XrEnvironmentBlendMode _chosenEnvBlendMode;
        float _unitsPerMeter;
        unsigned int _passesPerView;
        bool _useDepthInfo;
        bool _valid;

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
