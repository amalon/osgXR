// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/Manager>
#include <osgXR/Mirror>

#include "XRState.h"

#include <osg/PolygonMode>

using namespace osgXR;

static osg::ref_ptr<osg::Program> shaderProgram;
static unsigned int mirrorCount = 0;

Mirror::Mirror(Manager *manager, osg::Camera *camera) :
    _manager(manager),
    _camera(camera),
    _mirrorSettings(manager->_getSettings()->getMirrorSettings())
{
    if (!shaderProgram.valid())
    {
        const char* vertSrc =
            "#version 140\n"
            "out vec3 texcoord;\n"
            "void main()\n"
            "{\n"
            "    // Discard gl_Vertex.z, which stores array index\n"
            "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex.xyww;\n"
            "    texcoord.st = gl_MultiTexCoord0.st;\n"
            "    texcoord.p = gl_Vertex.z;\n"
            "}\n";
        const char* fragSrc =
            "#version 140\n"
            "#pragma import_defines (OSGXR_SWAPCHAIN_LAYERED)\n"
            "in vec3 texcoord;\n"
            "#ifdef OSGXR_SWAPCHAIN_LAYERED\n"
            "    uniform sampler2DArray tex;\n"
            "    #define TEXCOORD texcoord\n"
            "#else\n"
            "    uniform sampler2D tex;\n"
            "    #define TEXCOORD texcoord.st\n"
            "#endif\n"
            "void main()\n"
            "{\n"
            "    gl_FragColor = texture(tex, TEXCOORD);\n"
            "}\n";
        auto* vertShader = new osg::Shader(osg::Shader::VERTEX, vertSrc);
        auto* fragShader = new osg::Shader(osg::Shader::FRAGMENT, fragSrc);
        auto* program = new osg::Program();
        program->addShader(vertShader);
        program->addShader(fragShader);
        program->setName("osgXR Mirror");
        shaderProgram = program;
    }
    ++mirrorCount;
}

Mirror::~Mirror()
{
    if (!--mirrorCount)
        shaderProgram = nullptr;
}

void Mirror::_init()
{
    _camera->setAllowEventFocus(false);
    _camera->setViewMatrix(osg::Matrix::identity());
    _camera->setProjectionMatrix(osg::Matrix::ortho(0, 1, 0, 1, -1, 2));

    // Find the mirror settings
    MirrorSettings *mirrorSettings = &_mirrorSettings;
    // but fall back to the manager's mirror settings
    if (mirrorSettings->getMirrorMode() == MirrorSettings::MIRROR_AUTOMATIC)
        mirrorSettings = &_manager->_getSettings()->getMirrorSettings();
    switch (mirrorSettings->getMirrorMode())
    {
    case MirrorSettings::MIRROR_NONE:
        // Draw nothing, but still clear the viewport
        _camera->setClearMask(GL_COLOR_BUFFER_BIT);
        break;
    case MirrorSettings::MIRROR_AUTOMATIC:
        // Fall-through: Default to MIRROR_SINGLE
    case MirrorSettings::MIRROR_SINGLE:
        {
            int viewIndex = mirrorSettings->getMirrorViewIndex();
            if (viewIndex < 0)
                viewIndex = 0;
            setupQuad(viewIndex, 0.0f, 1.0f);
        }
        break;
    case MirrorSettings::MIRROR_LEFT_RIGHT:
        for (unsigned int viewIndex = 0; viewIndex < 2; ++viewIndex)
            setupQuad(viewIndex, 0.5f * viewIndex, 0.5f);
        break;
    }
}

namespace {

class MirrorPreDrawCallback : public osg::Camera::DrawCallback
{
    public:

        MirrorPreDrawCallback(osg::ref_ptr<XRState> xrState,
                              osg::ref_ptr<osg::StateSet> stateSet,
                              unsigned int viewIndex) :
            _xrState(xrState),
            _stateSet(stateSet),
            _viewIndex(viewIndex)
        {
        }

        void operator()(osg::RenderInfo& renderInfo) const override
        {
            const osg::FrameStamp *stamp = renderInfo.getState()->getFrameStamp();
            _stateSet->setTextureAttributeAndModes(0,
                                _xrState->getViewTexture(_viewIndex, stamp));
        }

    protected:

        osg::observer_ptr<XRState> _xrState;
        osg::ref_ptr<osg::StateSet> _stateSet;
        unsigned int _viewIndex;
};

class MirrorPostDrawCallback : public osg::Camera::DrawCallback
{
    public:

        MirrorPostDrawCallback(osg::ref_ptr<osg::StateSet> stateSet) :
            _stateSet(stateSet)
        {
        }

        void operator()(osg::RenderInfo& renderInfo) const override
        {
            _stateSet->removeTextureAttribute(0, osg::StateAttribute::Type::TEXTURE);
        }

    protected:

        osg::ref_ptr<osg::StateSet> _stateSet;
};

}

void Mirror::setupQuad(unsigned int viewIndex,
                       float x, float w)
{
    XRState *xrState = _manager->_getXrState();

    if (viewIndex >= xrState->getViewCount())
        return;

    // Build an always-visible quad to draw the view texture on
    osg::ref_ptr<osg::Geode> quad = new osg::Geode;

    char name[32];
    snprintf(name, sizeof(name), "osgXR Mirror view#%u", viewIndex);
    quad->setName(name);

    quad->setCullingActive(false);

    XRState::TextureRect rect = xrState->getViewTextureRect(viewIndex);
    // Z cooordinate stores array index
    quad->addDrawable(osg::createTexturedQuadGeometry(
                                  osg::Vec3(x,    0.0f, rect.arrayIndex),
                                  osg::Vec3(w,    0.0f, 0.0f),
                                  osg::Vec3(0.0f, 1.0f, 0.0f),
                                  rect.x, rect.y,
                                  rect.x + rect.width, rect.y + rect.height));

    osg::ref_ptr<osg::StateSet> state = quad->getOrCreateStateSet();
    int forceOff = osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED;
    int forceOn = osg::StateAttribute::ON | osg::StateAttribute::PROTECTED;
    state->setMode(GL_LIGHTING, forceOff);
    state->setMode(GL_DEPTH_TEST, forceOff);
    state->setMode(GL_FRAMEBUFFER_SRGB, forceOn);

    // Shaders are required with layered swapchains or with core profile
    auto gc = _camera->getGraphicsContext();
    bool layered = (xrState->getSwapchainMode() == Settings::SWAPCHAIN_LAYERED);
    if (layered || gc->getState()->getUseVertexAttributeAliasing())
    {
        state->setAttribute(shaderProgram);
        state->addUniform(new osg::Uniform("tex", 0));
        if (layered)
            state->setDefine("OSGXR_SWAPCHAIN_LAYERED");
    }

    _camera->addChild(quad);

    // Set a callback so we can switch the texture to the active swapchain image
    _camera->addPreDrawCallback(new MirrorPreDrawCallback(_manager->_getXrState(),
                                                          state, viewIndex));
    _camera->addPostDrawCallback(new MirrorPostDrawCallback(state));
}
