// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_XRFRAMEBUFFER
#define OSGXR_XRFRAMEBUFFER 1

#include <osg/GL>
#include <osg/Referenced>

#include <cinttypes>

namespace osgXR {

class XRFramebuffer : public osg::Referenced
{
    public:

        explicit XRFramebuffer(uint32_t width, uint32_t height,
                               GLuint texture, GLuint depthTexture = 0);
        // releaseGLObjects() first
        virtual ~XRFramebuffer();

        void setDepthFormat(GLenum depthFormat)
        {
            _depthFormat = depthFormat;
        }

        bool valid(osg::State &state) const;
        void bind(osg::State &state);
        void unbind(osg::State &state);
        // GL context must be current
        void releaseGLObjects(osg::State &state);

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

} // osgXR

#endif
