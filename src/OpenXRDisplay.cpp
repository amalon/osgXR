/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield
 *
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
*/

#include <osgXR/OpenXRDisplay>

#include <osg/ref_ptr>
#include <osg/Version>
#include <osgViewer/Renderer>
#include <osgViewer/ViewerBase>

#include "OpenXR/Compositor.h"
#include "OpenXR/Instance.h"
#include "OpenXR/System.h"
#include "OpenXR/Session.h"
#include "OpenXR/Swapchain.h"

#include <vector>

#if(OSG_VERSION_GREATER_OR_EQUAL(3, 4, 0))
typedef osg::GLExtensions OSG_GLExtensions;
#else
typedef osg::FBOExtensions OSG_GLExtensions;
#endif

static const OSG_GLExtensions* getGLExtensions(const osg::State& state)
{
#if(OSG_VERSION_GREATER_OR_EQUAL(3, 4, 0))
    return state.get<osg::GLExtensions>();
#else
    return osg::FBOExtensions::instance(state.getContextID(), true);
#endif
}

using namespace osgViewer;

// =============================================================================
// Derived from openxr-simple-example
// Copyright 2019-2021, Collabora, Ltd.
// Which was adapted from
// https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/master/src/common/xr_linear.h
// Copyright (c) 2017 The Khronos Group Inc.
// Copyright (c) 2016 Oculus VR, LLC.
// SPDX-License-Identifier: Apache-2.0
// =============================================================================
static void createProjectionFov(osg::Matrix& result,
                                const XrFovf& fov,
                                const float nearZ,
                                const float farZ)
{
    const float tanAngleLeft = tanf(fov.angleLeft);
    const float tanAngleRight = tanf(fov.angleRight);

    const float tanAngleDown = tanf(fov.angleDown);
    const float tanAngleUp = tanf(fov.angleUp);

    const float tanAngleWidth = tanAngleRight - tanAngleLeft;

    // Set to tanAngleDown - tanAngleUp for a clip space with positive Y
    // down (Vulkan). Set to tanAngleUp - tanAngleDown for a clip space with
    // positive Y up (OpenGL / D3D / Metal).
    const float tanAngleHeight = tanAngleUp - tanAngleDown;

    // Set to nearZ for a [-1,1] Z clip space (OpenGL / OpenGL ES).
    // Set to zero for a [0,1] Z clip space (Vulkan / D3D / Metal).
    const float offsetZ = nearZ;

    if (farZ <= nearZ) {
        // place the far plane at infinity
        result(0, 0) = 2 / tanAngleWidth;
        result(1, 0) = 0;
        result(2, 0) = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
        result(3, 0) = 0;

        result(0, 1) = 0;
        result(1, 1) = 2 / tanAngleHeight;
        result(2, 1) = (tanAngleUp + tanAngleDown) / tanAngleHeight;
        result(3, 1) = 0;

        result(0, 2) = 0;
        result(1, 2) = 0;
        result(2, 2) = -1;
        result(3, 2) = -(nearZ + offsetZ);

        result(0, 3) = 0;
        result(1, 3) = 0;
        result(2, 3) = -1;
        result(3, 3) = 0;
    } else {
        // normal projection
        result(0, 0) = 2 / tanAngleWidth;
        result(1, 0) = 0;
        result(2, 0) = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
        result(3, 0) = 0;

        result(0, 1) = 0;
        result(1, 1) = 2 / tanAngleHeight;
        result(2, 1) = (tanAngleUp + tanAngleDown) / tanAngleHeight;
        result(3, 1) = 0;

        result(0, 2) = 0;
        result(1, 2) = 0;
        result(2, 2) = -(farZ + offsetZ) / (farZ - nearZ);
        result(3, 2) = -(farZ * (nearZ + offsetZ)) / (farZ - nearZ);

        result(0, 3) = 0;
        result(1, 3) = 0;
        result(2, 3) = -1;
        result(3, 3) = 0;
    }
}


namespace osgViewer {

class XRFramebuffer : public osg::Referenced
{
    public:

        explicit XRFramebuffer(uint32_t width, uint32_t height,
                               GLuint texture, GLuint depthTexture = 0) :
            _width(width),
            _height(height),
            _depthFormat(GL_DEPTH_COMPONENT16),
            _fbo(0),
            _texture(texture),
            _depthTexture(depthTexture),
            _generated(false),
            _boundTexture(false),
            _boundDepthTexture(false),
            _deleteDepthTexture(false)
        {
        }

        virtual ~XRFramebuffer()
        {
            // FIXME
            /*
            if (_fbo)
            {
                const OSG_GLExtensions *fbo_ext = getGLExtensions(state);
                fbo_ext->glDeleteFramebuffers(1, &_fbo);
            }
            if (_deleteDepthTexture)
            {
                glDeleteTextures(1, &_depthTexture);
            }
            */
        }

        void setDepthFormat(GLenum depthFormat)
        {
            _depthFormat = depthFormat;
        }

        bool valid(osg::State &state) const
        {
            if (!_fbo)
                return false;

            const OSG_GLExtensions *fbo_ext = getGLExtensions(state);
            GLenum complete = fbo_ext->glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT);
            switch (complete) {
            case GL_FRAMEBUFFER_COMPLETE:
                return true;
            case GL_FRAMEBUFFER_UNDEFINED:
                OSG_WARN << "FBO Undefined" << std::endl;
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                OSG_WARN << "FBO Incomplete attachment" << std::endl;
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                OSG_WARN << "FBO Incomplete missing attachment" << std::endl;
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
                OSG_WARN << "FBO Incomplete draw buffer" << std::endl;
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
                OSG_WARN << "FBO Incomplete read buffer" << std::endl;
                break;
            case GL_FRAMEBUFFER_UNSUPPORTED:
                OSG_WARN << "FBO Incomplete unsupported" << std::endl;
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
                OSG_WARN << "FBO Incomplete multisample" << std::endl;
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
                OSG_WARN << "FBO Incomplete layer targets" << std::endl;
                break;
            default:
                OSG_WARN << "FBO Incomplete ???" << std::endl;
                break;
            }
            return false;
        }

        void bind(osg::State &state)
        {
            const OSG_GLExtensions *fbo_ext = getGLExtensions(state);

            if (!_fbo && !_generated)
            {
                fbo_ext->glGenFramebuffers(1, &_fbo);
                _generated = true;
            }

            if (_fbo) {
                fbo_ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, _fbo);
                if (!_boundTexture && _texture)
                {
                    fbo_ext->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _texture, 0);
                    _boundTexture = true;
                }
                if (!_boundDepthTexture)
                {
                    if (!_depthTexture)
                    {
                        glGenTextures(1, &_depthTexture);
                        glBindTexture(GL_TEXTURE_2D, _depthTexture);
                        glTexImage2D(GL_TEXTURE_2D, 0, _depthFormat, _width, _height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr);
                        glBindTexture(GL_TEXTURE_2D, 0);

                        _deleteDepthTexture = true;
                    }

                    fbo_ext->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTexture, 0);
                    _boundDepthTexture = true;

                    valid(state);
                }
            }
        }

        void unbind(osg::State &state)
        {
            const OSG_GLExtensions *fbo_ext = getGLExtensions(state);

            if (_fbo && _generated)
                fbo_ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
        }

        // FIXME destroy

    protected:

        uint32_t _width;
        uint32_t _height;
        GLenum _depthFormat;

        GLuint _fbo;
        GLuint _texture;
        GLuint _depthTexture;

        bool _generated;
        bool _boundTexture;
        bool _boundDepthTexture;
        bool _deleteDepthTexture;
};

class XRState : public osg::Referenced
{
    public:

        XRState(const OpenXRDisplay *xrDisplay) :
            _formFactor(XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY),
            _chosenViewConfig(nullptr),
            _chosenEnvBlendMode(XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM),
            _unitsPerMeter(xrDisplay->getUnitsPerMeter())
        {
            OSG_WARN << "XRState::XRState()" << std::endl;
            // Create OpenXR instance

            _instance = OpenXR::Instance::instance();
            _instance->setValidationLayer(xrDisplay->_validationLayer);
            if (!_instance->init(xrDisplay->_appName.c_str(), xrDisplay->_appVersion))
                return;

            // Get OpenXR system for chosen form factor

            switch (xrDisplay->_formFactor)
            {
                case OpenXRDisplay::HEAD_MOUNTED_DISPLAY:
                    _formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
                    break;
                case OpenXRDisplay::HANDHELD_DISPLAY:
                    _formFactor = XR_FORM_FACTOR_HANDHELD_DISPLAY;
                    break;
            }
            _system = _instance->getSystem(_formFactor);
            if (!_system)
                return;

            // Choose the first supported view configuration

            for (const auto &viewConfig: _system->getViewConfigurations())
            {
                switch (viewConfig.getType())
                {
                    case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
                    case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
                        _chosenViewConfig = &viewConfig;
                        break;
                    default:
                        break;
                }
                if (_chosenViewConfig)
                    break;
            }
            if (!_chosenViewConfig)
            {
                OSG_WARN << "OpenXRDisplay::configure(): No supported view configuration" << std::endl;
                return;
            }

            // Choose an environment blend mode

            for (XrEnvironmentBlendMode envBlendMode: _chosenViewConfig->getEnvBlendModes())
            {
                if ((unsigned int)envBlendMode > 31)
                    continue;
                uint32_t mask = (1u << (unsigned int)envBlendMode);
                if (xrDisplay->_preferredEnvBlendModeMask & mask)
                {
                    _chosenEnvBlendMode = envBlendMode;
                    break;
                }
                if (_chosenEnvBlendMode != XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM &&
                    xrDisplay->_allowedEnvBlendModeMask & mask)
                {
                    _chosenEnvBlendMode = envBlendMode;
                }
            }
            if (_chosenEnvBlendMode == XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM)
            {
                OSG_WARN << "OpenXRDisplay::configure(): No supported environment blend mode" << std::endl;
                return;
            }
            OSG_WARN << "XRState::XRState() done" << std::endl;
        }

        class XRView : public osg::Referenced
        {
            public:

                XRView(XRState *state,
                       osg::ref_ptr<OpenXR::Session> session,
                       uint32_t viewIndex,
                       const OpenXR::System::ViewConfiguration::View &view,
                       int64_t chosenSwapchainFormat) :
                    _state(state),
                    _viewIndex(viewIndex),
                    _currentImage(-1)
                {
                    _swapchain = new OpenXR::Swapchain(session, view,
                                                       XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
                                                       chosenSwapchainFormat);
                    if (_swapchain->valid())
                    {
                        // Create framebuffer objects for each image in swapchain
                        auto &textures = _swapchain->getImageTextures();
                        _imageFramebuffers.reserve(textures.size());
                        for (GLuint texture: textures)
                        {
                            XRFramebuffer *fb = new XRFramebuffer(_swapchain->getWidth(),
                                                                  _swapchain->getHeight(),
                                                                  texture, 0);
                            _imageFramebuffers.push_back(fb);
                        }
                    }
                }

                virtual ~XRView()
                {
                }

                bool valid()
                {
                    return _swapchain->valid();
                }

                osg::ref_ptr<osg::Camera> createCamera(osg::ref_ptr<osg::GraphicsContext> gc,
                                                       osg::ref_ptr<osg::Camera> mainCamera)
                {
                    // Inspired by osgopenvrviewer
                    osg::ref_ptr<osg::Camera> camera = new osg::Camera();
                    camera->setClearColor(mainCamera->getClearColor());
                    camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
                    // FIXME necessary I expect...
                    //camera->setRenderOrder(osg::Camera::PRE_RENDER, eye);
                    //camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
                    camera->setAllowEventFocus(false);
                    camera->setReferenceFrame(osg::Camera::RELATIVE_RF);
                    //camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
                    //camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF_INHERIT_VIEWPOINT);
                    camera->setViewport(0, 0, _swapchain->getWidth(), _swapchain->getHeight());
                    camera->setGraphicsContext(gc);

                    // Here we avoid doing anything regarding OSG camera RTT attachment.
                    // Ideally we would use automatic methods within OSG for handling RTT but in this
                    // case it seemed simpler to handle FBO creation and selection within this class.

                    // This initial draw callback is used to disable normal OSG camera setup which
                    // would undo our RTT FBO configuration.
                    camera->setInitialDrawCallback(new InitialDrawCallback(this));

                    camera->setPreDrawCallback(new PreDrawCallback(this));
                    camera->setFinalDrawCallback(new PostDrawCallback(this));

                    return camera;
                }


                void initialDrawCallback(osg::RenderInfo &renderInfo)
                {
                    osg::GraphicsOperation *graphicsOperation = renderInfo.getCurrentCamera()->getRenderer();
                    osgViewer::Renderer *renderer = dynamic_cast<osgViewer::Renderer*>(graphicsOperation);
                    if (renderer != nullptr)
                    {
                        // Disable normal OSG FBO camera setup because it will undo the MSAA FBO configuration.
                        renderer->setCameraRequiresSetUp(false);
                    }

                    _state->startRendering();
                }

                void preDrawCallback(osg::RenderInfo &renderInfo)
                {
                    osg::State &state = *renderInfo.getState();

                    // Acquire a swapchain image
                    _currentImage = _swapchain->acquireImage();
                    if (_currentImage < 0 || (unsigned int)_currentImage >= _imageFramebuffers.size())
                    {
                        OSG_WARN << "XRView::preDrawCallback(): Failure to acquire OpenXR swapchain image (got image index " << _currentImage << ")" << std::endl;
                        return;
                    }
                    const auto &fbo = _imageFramebuffers[_currentImage];

                    // Bind the framebuffer
                    fbo->bind(state);

                    // Wait for the image to be ready to render into
                    if (!_swapchain->waitImage(100e6 /* 100ms */))
                    {
                        OSG_WARN << "XRView::preDrawCallback(): Failure to wait for OpenXR swapchain image " << _currentImage << std::endl;

                        // Unclear what the best course of action is here...
                        fbo->unbind(state);
                        return;
                    }
                }

                void postDrawCallback(osg::RenderInfo &renderInfo)
                {
                    osg::State& state = *renderInfo.getState();

                    if (_currentImage < 0 || (unsigned int)_currentImage >= _imageFramebuffers.size())
                    {
                        return;
                    }
                    const auto &fbo = _imageFramebuffers[_currentImage];

                    // Unbind the framebuffer
                    fbo->unbind(state);

                    // Done rendering, release the swapchain image
                    _swapchain->releaseImage();

                    if (_state->_frame != nullptr)
                    {
                        // Add view info to projection layer for compositor
                        osg::ref_ptr<OpenXR::CompositionLayerProjection> proj = _state->getProjectionLayer();
                        if (proj != nullptr)
                        {
                            proj->addView(_state->_frame, _viewIndex, _swapchain, _currentImage);
                        }
                        else
                        {
                            OSG_WARN << "No projection layer" << std::endl;
                        }
                    }
                    else
                    {
                        OSG_WARN << "No frame" << std::endl;
                    }
                }

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
                osg::ref_ptr<OpenXR::Swapchain> _swapchain;
                std::vector<osg::ref_ptr<XRFramebuffer> > _imageFramebuffers;

                uint32_t _viewIndex;
                int _currentImage;
        };

        void init(osgViewer::GraphicsWindow *window,
                  osgViewer::View *view)
        {
            // Create session using the GraphicsWindow

            _session = new OpenXR::Session(_system, window);
            if (!_session)
            {
                OSG_WARN << "XRState::init(): No suitable GraphicsWindow to create an OpenXR session" << std::endl;
                return;
            }

            // Choose a swapchain format

            int64_t chosenSwapchainFormat = 0;
            for (int64_t format: _session->getSwapchainFormats())
            {
                switch (format)
                {
                    case GL_RGBA16:
                        // FIXME This one will do for now...
                        chosenSwapchainFormat = format;
                        break;
                    default:
                        break;
                }
                if (chosenSwapchainFormat)
                    break;
            }
            if (!chosenSwapchainFormat)
            {
                OSG_WARN << "XRState::init(): No supported swapchain format found" << std::endl;
                return;
            }

            // Set up a swapchain and slave camera for each view

            osg::ref_ptr<osg::GraphicsContext> gc = window;
            osg::ref_ptr<osg::Camera> camera = view->getCamera();
            //camera->setName("Main");

            const auto &views = _chosenViewConfig->getViews();
            _views.reserve(views.size());
            for (uint32_t i = 0; i < views.size(); ++i)
            {
                const auto &vcView = views[i];
                osg::ref_ptr<XRView> xrView = new XRView(this, _session, i, vcView,
                                                         chosenSwapchainFormat);
                if (!xrView.valid())
                    return;
                _views.push_back(xrView);

                osg::ref_ptr<osg::Camera> cam = xrView->createCamera(gc, camera);

                // Add view camera as slave. We'll find the position once the
                // session is ready.
                if (!view->addSlave(cam.get(), osg::Matrix::identity(), osg::Matrix::identity(), true))
                {
                    OSG_WARN << "XRState::init(): Couldn't add slave camera" << std::endl;
                } else {
                    view->getSlave(i)._updateSlaveCallback = new UpdateSlaveCallback(i, this);
                }
            }

            // Attach a callback to detect swap
            osg::ref_ptr<SwapCallback> swapCallback = new SwapCallback(this);
            gc->setSwapCallback(swapCallback);

            // Disable rendering of main camera since its being overwritten by the swap texture anyway
            camera->setGraphicsContext(nullptr);
        }

        void startFrame()
        {
            if (_frame)
                return;

            _instance->handleEvents();
            if (!_session->isReady())
            {
                OSG_WARN << "OpenXR session not ready for frames yet" << std::endl;
                return;
            }
            if (!_session->hasBegun())
            {
                if (!_session->begin(*_chosenViewConfig))
                    return;
            }

            _frame = _session->waitFrame();
        }

        void startRendering()
        {
            startFrame();
            if (_frame != nullptr && !_frame->hasBegun()) {
                _frame->begin();
                _projectionLayer = new OpenXR::CompositionLayerProjection();
                _projectionLayer->setLayerFlags(XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT);
                _projectionLayer->setSpace(_session->getLocalSpace());
            }
        }

        void endFrame()
        {
            if (_frame == nullptr)
            {
                OSG_WARN << "OpenXR frame not waited for" << std::endl;
                return;
            }
            if (!_frame->hasBegun())
            {
                OSG_WARN << "OpenXR frame not begun" << std::endl;
                return;
            }
            _frame->setEnvBlendMode(_chosenEnvBlendMode);
            _frame->addLayer(_projectionLayer.get());
            _frame->end();
            _frame = nullptr;
        }

        void updateSlave(uint32_t viewIndex, osg::View& view,
                         osg::View::Slave& slave)
        {
            bool setProjection = false;
            osg::Matrix projectionMatrix;

            startFrame();
            if (_frame != nullptr) {
                if (_frame->isPositionValid() && _frame->isOrientationValid()) {
                    const auto &pose = _frame->getViewPose(viewIndex);
                    osg::Vec3 position(pose.position.x,
                                       pose.position.y,
                                       pose.position.z);
                    osg::Quat orientation(pose.orientation.x,
                                          pose.orientation.y,
                                          pose.orientation.z,
                                          pose.orientation.w);

                    osg::Matrix viewOffset;
                    viewOffset.setTrans(viewOffset.getTrans() + position * _unitsPerMeter);
                    viewOffset.preMultRotate(orientation);
                    viewOffset = osg::Matrix::inverse(viewOffset);
                    slave._viewOffset = viewOffset;

                    double left, right, bottom, top, zNear, zFar;
                    if (view.getCamera()->getProjectionMatrixAsFrustum(left, right,
                                                                       bottom, top,
                                                                       zNear, zFar))
                    {
                        const auto &fov = _frame->getViewFov(viewIndex);
                        createProjectionFov(projectionMatrix, fov, zNear, zFar);
                        setProjection = true;
                    }
                }
            }

            //slave._camera->setViewMatrix(view.getCamera()->getViewMatrix() * slave._viewOffset);
            slave.updateSlaveImplementation(view);
            if (setProjection)
            {
                slave._camera->setProjectionMatrix(projectionMatrix);
            }
        }

        void swapBuffersImplementation(osg::GraphicsContext* gc)
        {
            // Submit rendered frame to compositor
            //m_device->submitFrame();

            endFrame();

            // Blit mirror texture to backbuffer
            //m_device->blitMirrorTexture(gc);

            // Run the default system swapBufferImplementation
            //gc->swapBuffersImplementation();
        }

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

class XRRealizeOperation : public osg::GraphicsOperation
{
    public:

        explicit XRRealizeOperation(osg::ref_ptr<XRState> state,
                                        osgViewer::View *view) :
            osg::GraphicsOperation("XRRealizeOperation", false),
            _state(state),
            _view(view),
            _realized(false)
        {
        }

        virtual void operator () (osg::GraphicsContext *gc)
        {
            if (!_realized) {
                OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);
                gc->makeCurrent();

                auto *window = dynamic_cast<osgViewer::GraphicsWindow *>(gc);
                if (window) {
                    _state->init(window, _view);
                    _realized = true;
                }
            }
        }

        bool realized() const
        {
            return _realized;
        }

    protected:

        OpenThreads::Mutex _mutex;
        osg::ref_ptr<XRState> _state;
        osgViewer::View *_view;
        bool _realized;
};

} // osgViewer

// OpenXRDisplay

OpenXRDisplay::OpenXRDisplay()
{
}

OpenXRDisplay::OpenXRDisplay(const std::string &appName,
                             uint32_t appVersion,
                             enum FormFactor formFactor):
    _appName(appName),
    _appVersion(appVersion),
    _validationLayer(false),
    _formFactor(formFactor),
    _preferredEnvBlendModeMask(0),
    _allowedEnvBlendModeMask(0),
    _unitsPerMeter(1.0f)
{
}

OpenXRDisplay::OpenXRDisplay(const OpenXRDisplay& rhs,
                             const osg::CopyOp& copyop):
    ViewConfig(rhs,copyop),
    _appName(rhs._appName),
    _appVersion(rhs._appVersion),
    _validationLayer(rhs._validationLayer),
    _formFactor(rhs._formFactor),
    _preferredEnvBlendModeMask(rhs._preferredEnvBlendModeMask),
    _allowedEnvBlendModeMask(rhs._allowedEnvBlendModeMask),
    _unitsPerMeter(rhs._unitsPerMeter)
{
}

OpenXRDisplay::~OpenXRDisplay()
{
}

void OpenXRDisplay::configure(osgViewer::View &view) const
{

    ViewerBase *viewer = dynamic_cast<ViewerBase *>(&view);
    if (!viewer)
        return;

    _state = new XRState(this);
    viewer->setRealizeOperation(new XRRealizeOperation(_state, &view));
}
