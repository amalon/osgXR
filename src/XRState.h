// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_XRSTATE
#define OSGXR_XRSTATE 1

#include "OpenXR/Instance.h"
#include "OpenXR/System.h"
#include "OpenXR/Session.h"
#include "OpenXR/Swapchain.h"
#include "OpenXR/Compositor.h"

#include "XRFramebuffer.h"

#include <osg/Referenced>
#include <osg/ref_ptr>

#include <osgXR/OpenXRDisplay>

#include <vector>

namespace osgXR {

class XRState : public osg::Referenced
{
    public:
        typedef OpenXRDisplay::VRMode VRMode;
        typedef OpenXRDisplay::SwapchainMode SwapchainMode;

        XRState(const OpenXRDisplay *xrDisplay);

        class XRSwapchain : public OpenXR::Swapchain
        {
            public:

                XRSwapchain(XRState *state,
                            osg::ref_ptr<OpenXR::Session> session,
                            const OpenXR::System::ViewConfiguration::View &view,
                            int64_t chosenSwapchainFormat);

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

                void preDrawCallback(osg::RenderInfo &renderInfo);
                // return true if okay
                bool postDrawCallback(osg::RenderInfo &renderInfo);

            protected:

                std::vector<osg::ref_ptr<XRFramebuffer> > _imageFramebuffers;

                int _currentImage;
                unsigned int _numDrawPasses;
                unsigned int _drawPassesDone;
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
                    return static_cast<XRSwapchain *>(_swapchainSubImage.getSwapchain().get());
                }

                osg::ref_ptr<osg::Camera> createCamera(osg::ref_ptr<osg::GraphicsContext> gc,
                                                       osg::ref_ptr<osg::Camera> mainCamera);

                void initialDrawCallback(osg::RenderInfo &renderInfo);
                void preDrawCallback(osg::RenderInfo &renderInfo);
                void postDrawCallback(osg::RenderInfo &renderInfo);

            protected:

                class InitialDrawCallback : public osg::Camera::DrawCallback
                {
                    public:

                        InitialDrawCallback(osg::ref_ptr<XRView> xrView) :
                            _xrView(xrView)
                        {
                        }

                        virtual void operator()(osg::RenderInfo& renderInfo) const
                        {
                            _xrView->initialDrawCallback(renderInfo);
                        }

                    protected:

                        osg::observer_ptr<XRView> _xrView;
                };
                class PreDrawCallback : public osg::Camera::DrawCallback
                {
                    public:

                        PreDrawCallback(osg::ref_ptr<XRView> xrView) :
                            _xrView(xrView)
                        {
                        }

                        virtual void operator()(osg::RenderInfo& renderInfo) const
                        {
                            _xrView->preDrawCallback(renderInfo);
                        }

                    protected:

                        osg::observer_ptr<XRView> _xrView;
                };
                class PostDrawCallback : public osg::Camera::DrawCallback
                {
                    public:

                        PostDrawCallback(osg::ref_ptr<XRView> xrView) :
                            _xrView(xrView)
                        {
                        }

                        virtual void operator()(osg::RenderInfo& renderInfo) const
                        {
                            _xrView->postDrawCallback(renderInfo);
                        }

                    protected:

                        osg::observer_ptr<XRView> _xrView;
                };

                XRState *_state;
                XRSwapchain::SubImage _swapchainSubImage;

                uint32_t _viewIndex;
        };

        void init(osgViewer::GraphicsWindow *window,
                  osgViewer::View *view);

        void startFrame();
        void startRendering();
        void endFrame();

        void updateSlave(uint32_t viewIndex, osg::View& view,
                         osg::View::Slave& slave);

        void swapBuffersImplementation(osg::GraphicsContext* gc);

        inline osg::ref_ptr<OpenXR::CompositionLayerProjection> getProjectionLayer()
        {
            return _projectionLayer;
        }

    protected:

        class UpdateSlaveCallback : public osg::View::Slave::UpdateSlaveCallback
        {
            public:

                UpdateSlaveCallback(uint32_t viewIndex, osg::ref_ptr<XRState> xrState) :
                    _viewIndex(viewIndex),
                    _xrState(xrState)
                {
                }

                virtual void updateSlave(osg::View& view, osg::View::Slave& slave)
                {
                    _xrState->updateSlave(_viewIndex, view, slave);
                }

            protected:

                uint32_t _viewIndex;
                osg::observer_ptr<XRState> _xrState;
        };

        class SwapCallback : public osg::GraphicsContext::SwapCallback
        {
            public:

                explicit SwapCallback(osg::ref_ptr<XRState> xrState) :
                    _xrState(xrState),
                    _frameIndex(0)
                {
                }

                void swapBuffersImplementation(osg::GraphicsContext* gc)
                {
                    _xrState->swapBuffersImplementation(gc);
                }

                int frameIndex() const
                {
                    return _frameIndex;
                }

            private:

                osg::observer_ptr<XRState> _xrState;
                int _frameIndex;
        };

        VRMode _vrMode;
        SwapchainMode _swapchainMode;

        osg::ref_ptr<OpenXR::Instance> _instance;
        XrFormFactor _formFactor;
        OpenXR::System *_system;
        const OpenXR::System::ViewConfiguration *_chosenViewConfig;
        XrEnvironmentBlendMode _chosenEnvBlendMode;
        float _unitsPerMeter;

        osg::ref_ptr<OpenXR::Session> _session;
        std::vector<osg::ref_ptr<XRView> > _views;
        osg::ref_ptr<OpenXR::Session::Frame> _frame;
        osg::ref_ptr<OpenXR::CompositionLayerProjection> _projectionLayer;
};

} // osgXR

#endif
