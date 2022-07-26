// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#include "Swapchain.h"

#include "OpenXR/System.h"

#include "XRState.h"

#include <osg/Notify>
#include <osg/observer_ptr>

#include <osgViewer/Renderer>

#include <memory>
#include <sstream>

using namespace osgXR;

namespace {

// Draw callbacks

class InitialDrawCallback : public osg::Camera::DrawCallback
{
    public:
        InitialDrawCallback(std::shared_ptr<Swapchain::Private> &swapchain) :
            _swapchain(swapchain)
        {
        }

        void operator()(osg::RenderInfo& renderInfo) const override
        {
            auto swapchain = _swapchain.lock();
            if (swapchain)
                swapchain->initialDrawCallback(renderInfo);
        }

    protected:
        std::weak_ptr<Swapchain::Private> _swapchain;
};

class PreDrawCallback : public osg::Camera::DrawCallback
{
    public:
        PreDrawCallback(std::shared_ptr<Swapchain::Private> &swapchain) :
            _swapchain(swapchain)
        {
            swapchain->incNumDrawPasses();
        }

        ~PreDrawCallback()
        {
            auto swapchain = _swapchain.lock();
            if (swapchain)
                swapchain->decNumDrawPasses();
        }

        void operator()(osg::RenderInfo& renderInfo) const override
        {
            auto swapchain = _swapchain.lock();
            if (swapchain)
                swapchain->preDrawCallback(renderInfo);
        }

    protected:
        std::weak_ptr<Swapchain::Private> _swapchain;
};

class PostDrawCallback : public osg::Camera::DrawCallback
{
    public:
        PostDrawCallback(std::shared_ptr<Swapchain::Private> &swapchain) :
            _swapchain(swapchain)
        {
        }

        void operator()(osg::RenderInfo& renderInfo) const override
        {
            auto swapchain = _swapchain.lock();
            if (swapchain)
                swapchain->postDrawCallback(renderInfo);
        }

    protected:
        std::weak_ptr<Swapchain::Private> _swapchain;
};

}

// Internal API

Swapchain::Private::Private(uint32_t width, uint32_t height) :
    _preferredRGBEncodingMask(0),
    _allowedRGBEncodingMask(0),
    _rgbBits(10),  // 8-bits is unlikely to be sufficient for linear encodings
    _alphaBits(0), // Alpha channel not required by default
    _width(width),
    _height(height),
    _forcedAlpha(-1.0f),
    _numDrawPasses(0),
    _updated(true)
{
}

Swapchain::Private::~Private()
{
}

void Swapchain::Private::attachToCamera(std::shared_ptr<Private> &self,
                                        osg::Camera *camera)
{
    camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER);
    camera->setInitialDrawCallback(new InitialDrawCallback(self));
    camera->setPreDrawCallback(new PreDrawCallback(self));
    camera->setFinalDrawCallback(new PostDrawCallback(self));

    // FIXME Do all cameras inherit the main display settings... eeek!
    //camera->setDisplaySettings(osg::DisplaySettings::instance());

    if (_swapchain.valid())
        _swapchain->incNumDrawPasses(1); // FIXME HACK depends on where node is!
}

void Swapchain::Private::attachToMirror(std::shared_ptr<Private> &self,
                                        osg::StateSet *stateSet)
{
    _stateSets.push_back(stateSet);
}

void Swapchain::Private::preferRGBEncoding(Encoding encoding)
{
    uint32_t mask = (1u << (unsigned int)encoding);
    _preferredRGBEncodingMask |= mask;
    _allowedRGBEncodingMask |= mask;
}

void Swapchain::Private::allowRGBEncoding(Encoding encoding)
{
    uint32_t mask = (1u << (unsigned int)encoding);
    _allowedRGBEncodingMask |= mask;
}

void Swapchain::Private::setRGBBits(unsigned int rgbBits)
{
    _rgbBits = rgbBits;
}

unsigned int Swapchain::Private::getRGBBits() const
{
    return _rgbBits;
}

void Swapchain::Private::setAlphaBits(unsigned int alphaBits)
{
    _alphaBits = alphaBits;
}

unsigned int Swapchain::Private::getAlphaBits() const
{
    return _alphaBits;
}

void Swapchain::Private::setWidth(uint32_t width)
{
    if (width != _width)
        _updated = true;
    _width = width;
}

uint32_t Swapchain::Private::getWidth() const
{
    return _width;
}

void Swapchain::Private::setHeight(uint32_t height)
{
    if (height != _height)
        _updated = true;
    _height = height;
}

uint32_t Swapchain::Private::getHeight() const
{
    return _height;
}

void Swapchain::Private::setForcedAlpha(float alpha)
{
    if (alpha < 0.0f)
        alpha = 0.0f;
    if (alpha > 1.0f)
        alpha = 1.0f;
    _forcedAlpha = alpha;
}

void Swapchain::Private::disableForcedAlpha()
{
    _forcedAlpha = -1.0f;
}

float Swapchain::Private::getForcedAlpha() const
{
    return _forcedAlpha;
}

bool Swapchain::Private::setup(XRState *state, OpenXR::Session *session)
{
    XRState *oldState = _state.get();
    if (oldState)
    {
        if (oldState != state)
        {
            OSG_WARN << "Swapchain XRState conflict" << std::endl;
            return false;
        }
        if (!_updated)
            return true;
    }

    OpenXR::System::ViewConfiguration::View view(_width, _height);
    int64_t rgbaFormat = state->chooseRGBAFormat(_rgbBits, _alphaBits,
                                                 _preferredRGBEncodingMask,
                                                 _allowedRGBEncodingMask);
    if (!rgbaFormat)
    {
        std::stringstream formats;
        formats << std::hex;
        for (int64_t format: session->getSwapchainFormats())
            formats << " 0x" << format;
        OSG_WARN << "Swapchain setup: No supported swapchain format found in ["
                 << formats.str() << " ]" << std::endl;
        return false;
    }

    _state = state;
    _updated = false;
    _session = session;
    _swapchain = new XRState::XRSwapchain(state, session,
                                          view, rgbaFormat,
                                          0, GL_DEPTH_COMPONENT16);
    _swapchain->setForcedAlpha(_forcedAlpha);
    _swapchain->incNumDrawPasses(_numDrawPasses);

    return true;
}

bool Swapchain::Private::sync()
{
    if (_updated && _session.valid())
        return setup(_state.get(), _session.get());
    return true;
}

void Swapchain::Private::cleanupSession()
{
    _swapchain = nullptr;
    _session = nullptr;
    _state = nullptr;
}

bool Swapchain::Private::valid() const
{
    return _swapchain.valid();
}

void Swapchain::Private::initialDrawCallback(osg::RenderInfo &renderInfo)
{
    if (_swapchain.valid())
    {
        // FIXME this isn't the ideal place, but it'll probably do in practice
        // due to lack of concurrency
        sync();
        osg::GraphicsOperation *graphicsOperation = renderInfo.getCurrentCamera()->getRenderer();
        osgViewer::Renderer *renderer = dynamic_cast<osgViewer::Renderer*>(graphicsOperation);
        if (renderer != nullptr)
        {
            // Disable normal OSG FBO camera setup because it will undo the MSAA FBO configuration.
            renderer->setCameraRequiresSetUp(false);
        }

        _state->startRendering(renderInfo.getState()->getFrameStamp());
    }
}

void Swapchain::Private::preDrawCallback(osg::RenderInfo &renderInfo)
{
    if (_swapchain.valid())
        _swapchain->preDrawCallback(renderInfo);
}

void Swapchain::Private::postDrawCallback(osg::RenderInfo &renderInfo)
{
    if (_swapchain.valid())
    {
        _swapchain->setForcedAlpha(_forcedAlpha);
        _swapchain->postDrawCallback(renderInfo);

        const osg::FrameStamp *stamp = renderInfo.getState()->getFrameStamp();
        auto texture = _swapchain->getOsgTexture(stamp);
        if (texture.valid())
        {
            for (auto it = _stateSets.begin(); it != _stateSets.end(); ++it)
            {
                osg::ref_ptr<osg::StateSet> stateSet = *it;
                if (stateSet.valid())
                    // update state set's texture object
                    stateSet->setTextureAttributeAndModes(0, texture);
                else
                    // clean up after stale state sets
                    it = _stateSets.erase(it);
            }
        }

        // FIXME somewhere we should remove or reset the texture:
        // stateSet->removeTextureAttribute(0, osg::StateAttribute::Type::TEXTURE);
    }
}

OpenXR::SwapchainGroupSubImage Swapchain::Private::convertSubImage(const SubImage &subImage) const
{
    OpenXR::System::ViewConfiguration::View::Viewport vp = {};
    vp.x = subImage.getX();
    vp.y = subImage.getY();
    vp.width = subImage.getWidth();
    if (!vp.width)
        vp.width = _width;
    vp.height = subImage.getHeight();
    if (!vp.height)
        vp.height = _height;
    return OpenXR::SwapchainGroupSubImage(_swapchain.get(), vp);
}

// Public API

Swapchain::Swapchain(uint32_t width, uint32_t height) :
    _private(new Private(width, height))
{
}

Swapchain::~Swapchain()
{
}

void Swapchain::attachToCamera(osg::Camera *camera)
{
    _private->attachToCamera(_private, camera);
}

void Swapchain::attachToMirror(osg::StateSet *stateSet)
{
    _private->attachToMirror(_private, stateSet);
}

void Swapchain::preferRGBEncoding(Swapchain::Encoding encoding)
{
    _private->preferRGBEncoding(encoding);
}

void Swapchain::allowRGBEncoding(Swapchain::Encoding encoding)
{
    _private->allowRGBEncoding(encoding);
}

void Swapchain::setRGBBits(unsigned int rgbBits)
{
    _private->setRGBBits(rgbBits);
}

unsigned int Swapchain::getRGBBits() const
{
    return _private->getRGBBits();
}

void Swapchain::setAlphaBits(unsigned int alphaBits)
{
    _private->setAlphaBits(alphaBits);
}

unsigned int Swapchain::getAlphaBits() const
{
    return _private->getAlphaBits();
}

void Swapchain::setSize(uint32_t width, uint32_t height)
{
    _private->setWidth(width);
    _private->setHeight(height);
}

void Swapchain::setWidth(uint32_t width)
{
    _private->setWidth(width);
}

uint32_t Swapchain::getWidth() const
{
    return _private->getWidth();
}

void Swapchain::setHeight(uint32_t height)
{
    _private->setHeight(height);
}

uint32_t Swapchain::getHeight() const
{
    return _private->getHeight();
}

void Swapchain::setForcedAlpha(float alpha)
{
    _private->setForcedAlpha(alpha);
}

void Swapchain::disableForcedAlpha()
{
    _private->disableForcedAlpha();
}

float Swapchain::getForcedAlpha() const
{
    return _private->getForcedAlpha();
}
