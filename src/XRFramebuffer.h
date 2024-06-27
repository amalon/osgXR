// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_XRFRAMEBUFFER
#define OSGXR_XRFRAMEBUFFER 1

#include <osg/GL>
#include <osg/Referenced>

#include <cstdint>

namespace osgXR {

namespace OpenXR {
    class Instance;
};

class XRFramebuffer : public osg::Referenced
{
    public:

        enum {
            // Must match OSG
            ARRAY_INDEX_GEOMETRY = 0xffffffff,
            ARRAY_INDEX_MULTIVIEW = 0xfffffff0,
        };

        static bool supportsSingleLayer(osg::State &state);
        static bool supportsGeomLayer(osg::State &state);
        static bool supportsMultiview(osg::State &state);

        explicit XRFramebuffer(uint32_t width, uint32_t height,
                               uint32_t arraySize, uint32_t arrayIndex,
                               GLuint texture, GLuint depthTexture = 0,
                               GLint textureFormat = 0, GLint depthFormat = 0);
        // releaseGLObjects() first
        virtual ~XRFramebuffer();

        void setFallbackDepthFormat(GLint depthFormat)
        {
            _fallbackDepthFormat = depthFormat;
        }

        bool valid(osg::State &state) const;
        void bind(osg::State &state, const OpenXR::Instance *instance);
        void unbind(osg::State &state);
        // GL context must be current
        void releaseGLObjects(osg::State &state);

    protected:

        uint32_t _width;
        uint32_t _height;
        uint32_t _arraySize;
        uint32_t _arrayIndex;
        GLint _textureFormat;
        GLint _depthFormat;
        GLint _fallbackDepthFormat;

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
