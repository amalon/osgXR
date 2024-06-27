// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "XRFramebuffer.h"

#include "OpenXR/Instance.h"

#include <osg/FrameBufferObject>
#include <osg/Image>
#include <osg/State>
#include <osg/Version>

using namespace osgXR;

bool XRFramebuffer::supportsSingleLayer(osg::State &state)
{
    const auto *ext = state.get<osg::GLExtensions>();
    return ext->glFramebufferTextureLayer != nullptr;
}

bool XRFramebuffer::supportsGeomLayer(osg::State &state)
{
    const auto *ext = state.get<osg::GLExtensions>();
    return ext->glFramebufferTexture != nullptr;
}

bool XRFramebuffer::supportsMultiview(osg::State &state)
{
#ifdef OSGXR_USE_OVR_MULTIVIEW
    const auto *ext = state.get<osg::GLExtensions>();
    return ext->glFramebufferTextureMultiviewOVR != nullptr;
#else
    return false;
#endif
}

XRFramebuffer::XRFramebuffer(uint32_t width, uint32_t height,
                             uint32_t arraySize, uint32_t arrayIndex,
                             GLuint texture, GLuint depthTexture,
                             GLint textureFormat, GLint depthFormat) :
    _width(width),
    _height(height),
    _arraySize(arraySize),
    _arrayIndex(arrayIndex),
    _textureFormat(textureFormat),
    _depthFormat(depthFormat),
    _fallbackDepthFormat(GL_DEPTH_COMPONENT16),
    _fbo(0),
    _texture(texture),
    _depthTexture(depthTexture),
    _generated(false),
    _boundTexture(false),
    _boundDepthTexture(false),
    _deleteDepthTexture(false)
{
}

XRFramebuffer::~XRFramebuffer()
{
}

bool XRFramebuffer::valid(osg::State &state) const
{
    if (!_fbo)
        return false;

    const auto *ext = state.get<osg::GLExtensions>();
    GLenum complete = ext->glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT);
    switch (complete)
    {
    case GL_FRAMEBUFFER_COMPLETE_EXT:
        return true;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
        OSG_WARN << "osgXR: FBO Incomplete attachment" << std::endl;
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
        OSG_WARN << "osgXR: FBO Incomplete missing attachment" << std::endl;
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
        OSG_WARN << "osgXR: FBO Incomplete draw buffer" << std::endl;
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
        OSG_WARN << "osgXR: FBO Incomplete read buffer" << std::endl;
        break;
    case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
        OSG_WARN << "osgXR: FBO Incomplete unsupported" << std::endl;
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT:
        OSG_WARN << "osgXR: FBO Incomplete multisample" << std::endl;
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_OVR:
        OSG_WARN << "osgXR: FBO Incomplete view targets" << std::endl;
        break;
    default:
        OSG_WARN << "osgXR: FBO Incomplete ??? (0x" << std::hex << complete << std::dec << ")" << std::endl;
        break;
    }
    return false;
}

void XRFramebuffer::bind(osg::State &state, const OpenXR::Instance *instance)
{
    const auto *ext = state.get<osg::GLExtensions>();

    if (!_fbo && !_generated)
    {
        ext->glGenFramebuffers(1, &_fbo);
        _generated = true;
    }

    if (_fbo)
    {
        ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, _fbo);
        if (!_boundTexture && _texture)
        {
            if (instance->getQuirk(OpenXR::QUIRK_APITRACE_TEXIMAGE) && _textureFormat)
            {
                // For the sake of apitrace, specify the format
                if (_arraySize <= 1)
                {
                    glBindTexture(GL_TEXTURE_2D, _texture);
                    glTexImage2D(GL_TEXTURE_2D, 0, _textureFormat, _width,
                                 _height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
                    glBindTexture(GL_TEXTURE_2D, 0);
                }
                else
                {
                    glBindTexture(GL_TEXTURE_2D_ARRAY, _texture);
                    ext->glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, _textureFormat, _width,
                                      _height, _arraySize, 0, GL_RGB,
                                      GL_UNSIGNED_BYTE, nullptr);
                    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
                }
            }

#ifdef OSGXR_USE_OVR_MULTIVIEW
            if (_arrayIndex == ARRAY_INDEX_MULTIVIEW && ext->glFramebufferTextureMultiviewOVR)
                ext->glFramebufferTextureMultiviewOVR(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, _texture, 0, 0, _arraySize);
            else
#endif
            if (_arrayIndex == ARRAY_INDEX_GEOMETRY)
                ext->glFramebufferTexture(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, _texture, 0);
            else if (_arraySize > 1)
                ext->glFramebufferTextureLayer(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, _texture, 0, _arrayIndex);
            else
                ext->glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, _texture, 0);
            _boundTexture = true;
        }
        if (!_boundDepthTexture)
        {
            if (!_depthTexture)
            {
                glGenTextures(1, &_depthTexture);
                if (_arraySize <= 1)
                {
                    glBindTexture(GL_TEXTURE_2D, _depthTexture);
                    glTexImage2D(GL_TEXTURE_2D, 0, _fallbackDepthFormat, _width,
                                 _height, 0, GL_DEPTH_COMPONENT,
                                 GL_UNSIGNED_BYTE, nullptr);
                    glBindTexture(GL_TEXTURE_2D, 0);
                }
                else
                {
                    glBindTexture(GL_TEXTURE_2D_ARRAY, _depthTexture);
                    ext->glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, _fallbackDepthFormat,
                                          _width, _height, _arraySize, 0,
                                          GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr);
                    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
                }

                _deleteDepthTexture = true;
            }
            else if (instance->getQuirk(OpenXR::QUIRK_APITRACE_TEXIMAGE) && _depthFormat)
            {
                // For the sake of apitrace, specify the format
                if (_arraySize <= 1)
                {
                    glBindTexture(GL_TEXTURE_2D, _depthTexture);
                    glTexImage2D(GL_TEXTURE_2D, 0, _depthFormat, _width,
                                 _height, 0, GL_DEPTH_COMPONENT,
                                 GL_UNSIGNED_BYTE, nullptr);
                    glBindTexture(GL_TEXTURE_2D, 0);
                }
                else
                {
                    glBindTexture(GL_TEXTURE_2D_ARRAY, _depthTexture);
                    ext->glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, _depthFormat, _width,
                                          _height, _arraySize, 0, GL_DEPTH_COMPONENT,
                                          GL_UNSIGNED_BYTE, nullptr);
                    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
                }
            }

#ifdef OSGXR_USE_OVR_MULTIVIEW
            if (_arrayIndex == ARRAY_INDEX_MULTIVIEW && ext->glFramebufferTextureMultiviewOVR)
                ext->glFramebufferTextureMultiviewOVR(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, _depthTexture, 0, 0, _arraySize);
            else
#endif
            if (_arrayIndex == ARRAY_INDEX_GEOMETRY)
                ext->glFramebufferTexture(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, _depthTexture, 0);
            else if (_arraySize > 1)
                ext->glFramebufferTextureLayer(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, _depthTexture, 0, _arrayIndex);
            else
                ext->glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, _depthTexture, 0);
            _boundDepthTexture = true;

            valid(state);
        }
    }
}

void XRFramebuffer::unbind(osg::State &state)
{
    const auto *ext = state.get<osg::GLExtensions>();

    if (_fbo && _generated)
        ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
}

void XRFramebuffer::releaseGLObjects(osg::State &state)
{
    // FIXME can we do it like RenderBuffer::releaseGLObjects?
    // FIXME better yet, switch to use FrameBufferObject, dynamically bound

    // GL context must be current
    if (_fbo)
    {
        /*
        unsigned int contextID = state->getContextID();
        osg::get<GLFrameBufferObjectManager>(contextID)->scheduleGLObjectForDeletion(_fbo);
        */
        const auto *ext = state.get<osg::GLExtensions>();
        ext->glDeleteFramebuffers(1, &_fbo);
        _fbo = 0;
    }
    if (_deleteDepthTexture)
    {
        glDeleteTextures(1, &_depthTexture);
        _depthTexture = 0;
        _deleteDepthTexture = false;
    }
}
