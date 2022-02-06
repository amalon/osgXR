// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#include <osgXR/CompositionLayerQuad>
#include <osgXR/Manager>
#include <osgXR/SubImage>
#include <osgXR/Swapchain>

#include "OpenXR/Compositor.h"

#include "CompositionLayer.h"
#include "Swapchain.h"

using namespace osgXR;

// Internal API

namespace osgXR {

class CompositionLayerPrivateQuad : public CompositionLayer::Private
{
    public:

        typedef CompositionLayerQuad::EyeVisibility EyeVisibility;

        CompositionLayerPrivateQuad(XRState *state) :
            CompositionLayer::Private(state),
            _eyeVisibility(EyeVisibility::EYES_BOTH),
            _orientation(0.0f, 0.0f, 0.0f, 1.0f),
            _position(0.0f, 0.0f, -1.0f),
            _size(1.0f, 1.0f)
        {
        }

        ~CompositionLayerPrivateQuad()
        {
        }

        void setEyeVisibility(EyeVisibility eyes)
        {
            _eyeVisibility = eyes;
        }

        EyeVisibility getEyeVisibility() const
        {
            return _eyeVisibility;
        }

        void setSubImage(const SubImage &subImage)
        {
            _subImage = subImage;
        }

        const SubImage &getSubImage() const
        {
            return _subImage;
        }

        void setOrientation(const osg::Quat &quat)
        {
            _orientation = quat;
        }

        const osg::Quat &getOrientation() const
        {
            return _orientation;
        }

        void setPosition(const osg::Vec3f &pos)
        {
            _position = pos;
        }

        const osg::Vec3f &getPosition() const
        {
            return _position;
        }

        void setSize(const osg::Vec2f &size)
        {
            _size = size;
        }

        const osg::Vec2f &getSize() const
        {
            return _size;
        }

        bool setup(OpenXR::Session *session) override
        {
            Swapchain *swapchain = _subImage.getSwapchain();
            if (swapchain)
                return Swapchain::Private::get(swapchain)->setup(_state.get(), session);
            return false;
        }

        bool writeCompositionLayerQuad(OpenXR::Session *session,
                                       OpenXR::CompositionLayerQuad *layer) const
        {
            auto swapchain = Swapchain::Private::get(_subImage.getSwapchain());
            bool ret = writeCompositionLayer(session, layer,
                                             swapchain->getForcedAlpha() >= 1.0f);
            if (!ret)
                return ret;

            _quadLayer->setEyeVisibility(static_cast<XrEyeVisibility>(_eyeVisibility));
            _quadLayer->setSubImage(swapchain->convertSubImage(_subImage));
            _quadLayer->setOrientation(_orientation);
            _quadLayer->setPosition(_position);
            _quadLayer->setSize(_size);
            return true;
        }

        void endFrame(OpenXR::Session::Frame *frame) override
        {
            Swapchain *swapchain = _subImage.getSwapchain();
            if (!swapchain)
                return;
            auto *swapchainPriv = Swapchain::Private::get(swapchain);
            if (!swapchainPriv->valid())
                return;

            _quadLayer = new OpenXR::CompositionLayerQuad();
            if (writeCompositionLayerQuad(frame->getSession(), _quadLayer))
                frame->addLayer(_quadLayer.get());
        }

        void cleanupSession() override
        {
            Swapchain *swapchain = _subImage.getSwapchain();
            if (swapchain)
                Swapchain::Private::get(swapchain)->cleanupSession();
        }

    protected:

        EyeVisibility _eyeVisibility;
        SubImage _subImage;
        osg::Quat _orientation;
        osg::Vec3f _position;
        osg::Vec2f _size;

        osg::ref_ptr<OpenXR::CompositionLayerQuad> _quadLayer;
};

}

// Public API

CompositionLayerQuad::CompositionLayerQuad(Manager *manager) :
    CompositionLayer(new CompositionLayerPrivateQuad(manager->_getXrState()))
{
}

CompositionLayerQuad::~CompositionLayerQuad()
{
}

void CompositionLayerQuad::setEyeVisibility(EyeVisibility eyes)
{
    auto priv = static_cast<CompositionLayerPrivateQuad *>(Private::get(this));
    priv->setEyeVisibility(eyes);
}

CompositionLayerQuad::EyeVisibility CompositionLayerQuad::getEyeVisibility() const
{
    auto priv = static_cast<const CompositionLayerPrivateQuad *>(Private::get(this));
    return priv->getEyeVisibility();
}

void CompositionLayerQuad::setSubImage(Swapchain *swapchain)
{
    setSubImage((SubImage)swapchain);
}

void CompositionLayerQuad::setSubImage(const SubImage &subImage)
{
    auto priv = static_cast<CompositionLayerPrivateQuad *>(Private::get(this));
    priv->setSubImage(subImage);
}

const SubImage &CompositionLayerQuad::getSubImage() const
{
    auto priv = static_cast<const CompositionLayerPrivateQuad *>(Private::get(this));
    return priv->getSubImage();
}

void CompositionLayerQuad::setOrientation(const osg::Quat &quat)
{
    auto priv = static_cast<CompositionLayerPrivateQuad *>(Private::get(this));
    priv->setOrientation(quat);
}

const osg::Quat &CompositionLayerQuad::getOrientation() const
{
    auto priv = static_cast<const CompositionLayerPrivateQuad *>(Private::get(this));
    return priv->getOrientation();
}

void CompositionLayerQuad::setPosition(const osg::Vec3f &pos)
{
    auto priv = static_cast<CompositionLayerPrivateQuad *>(Private::get(this));
    priv->setPosition(pos);
}

const osg::Vec3f &CompositionLayerQuad::getPosition() const
{
    auto priv = static_cast<const CompositionLayerPrivateQuad *>(Private::get(this));
    return priv->getPosition();
}

void CompositionLayerQuad::setSize(const osg::Vec2f &size)
{
    auto priv = static_cast<CompositionLayerPrivateQuad *>(Private::get(this));
    priv->setSize(size);
}

const osg::Vec2f &CompositionLayerQuad::getSize() const
{
    auto priv = static_cast<const CompositionLayerPrivateQuad *>(Private::get(this));
    return priv->getSize();
}
