// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#include "Hand.h"

#include <osgXR/HandPose>

#include <osg/Geometry>
#include <osg/Geode>
#include <osg/Material>
#include <osg/MatrixTransform>

#include <osg/Shape>
#include <osg/ShapeDrawable>

#include <cassert>
#include <cmath>

//#define DRAW_AXES

using namespace osgXR;

// Update callback

namespace {

class HandUpdateCallback : public osg::NodeCallback
{
    public:

        virtual void operator () (osg::Node *node, osg::NodeVisitor *nv)
        {
            Hand *hand = dynamic_cast<Hand *>(node);
            assert(hand);

            Hand::Private::get(hand)->update(hand);

            traverse(node, nv);
        }
};

}

#ifdef DRAW_AXES
static osg::Geometry *buildAxes(float size)
{
    // Create buffers for a simple cuboid
#define VERT_COUNT (2 * 3)
    const osg::Vec3 verticesRaw[VERT_COUNT] = {
        { 0.0, 0.0, 0.0 }, { size, 0.0, 0.0 },
        { 0.0, 0.0, 0.0 }, { 0.0, size, 0.0 },
        { 0.0, 0.0, 0.0 }, { 0.0, 0.0, size },
    };
    const osg::Vec3 coloursRaw[VERT_COUNT] = {
        { 1.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 }, { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 }, { 0.0, 0.0, 1.0 },
    };
    osg::Vec3Array* vertices = new osg::Vec3Array(VERT_COUNT, verticesRaw);
    osg::Vec3Array* colours = new osg::Vec3Array(VERT_COUNT, coloursRaw);
    osg::DrawArrays* prim = new osg::DrawArrays(osg::PrimitiveSet::LINES, 0, VERT_COUNT);

    // Create a geometry for the controller
    osg::Geometry* geom = new osg::Geometry;
    geom->setVertexArray(vertices);
    geom->setColorArray(colours, osg::Array::BIND_PER_VERTEX);
    geom->addPrimitiveSet(prim);

    return geom;
}
#endif

// Internal API

Hand::Private::Private() :
    _setUp(false)
{
}

void Hand::Private::setup(Hand *hand)
{
#ifdef DRAW_AXES
    osg::Switch *axesSwitch = new osg::Switch;
    osg::ref_ptr<osg::StateSet> axesState = axesSwitch->getOrCreateStateSet();
    int forceOff = osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED;
    int forceOn = osg::StateAttribute::ON | osg::StateAttribute::PROTECTED;
    axesState->setMode(GL_LIGHTING, forceOff);
    axesState->setMode(GL_COLOR_MATERIAL, forceOn);

    osg::Geometry *axesGeom = buildAxes(2.0f);
#endif

    for (unsigned int i = 0; i < HandPose::JOINT_COUNT; ++i) {
        // Create a matrix transform for the joint
        osg::MatrixTransform *transform = new osg::MatrixTransform;
        std::string jointName = HandPose::getJointName((HandPose::Joint)i);
        transform->setName(jointName + " transform");
        hand->addChild(transform);

        // Create a geode in the transform
        osg::Geode *geode = hand->generateGeode();
        geode->setName(jointName + " geode");

        osg::ShapeDrawable *geom = new osg::ShapeDrawable(new osg::Sphere(osg::Vec3(0, 0, 0),
                                                                          1.0f),
                                                          _tessellationHints);
        geode->addDrawable(geom);
        transform->addChild(geode);

#ifdef DRAW_AXES
        osg::MatrixTransform *axesTransform = new osg::MatrixTransform;
        axesTransform->setName(jointName + " axes transform");
        axesSwitch->addChild(axesTransform);

        osg::Geode *axesGeode = hand->generateGeode();
        axesGeode->addDrawable(axesGeom);
        axesTransform->addChild(axesGeode);
#endif
    }

#ifdef DRAW_AXES
    hand->addChild(axesSwitch);
#endif
}

void Hand::Private::update(Hand *hand)
{
    // Make sure the hand is all set up
    // We can't do this at creation time as we need to call virtual functions of
    // Hand.
    if (!_setUp) {
        setup(hand);
        _setUp = true;
    }

    // Try and get an updated position
    // FIXME for the next frame that is!
    _pose->update();

#ifdef DRAW_AXES
    auto *axesSwitch = dynamic_cast<osg::Switch *>(hand->getChild(HandPose::JOINT_COUNT));
    assert(axesSwitch);
#endif

    if (_pose->isActive()) {
        // All joint locations and orientations are valid
        //auto *geode = dynamic_cast<osg::Geode *>(hand->getChild(0));
        //assert(geode);
        for (unsigned int i = 0; i < HandPose::JOINT_COUNT; ++i) {
            auto &loc = _pose->getJointLocation((HandPose::Joint)i);
            float radius = loc.getRadius();

#ifdef DRAW_AXES
            auto *axesTransform = dynamic_cast<osg::MatrixTransform *>(axesSwitch->getChild(i));
            assert(axesTransform);

            osg::Matrix axesMat(loc.getOrientation());
            axesMat.setTrans(loc.getPosition());
            axesMat.preMultScale(osg::Vec3f(radius, radius, radius));
            axesTransform->setMatrix(axesMat);
#endif

            auto *transform = dynamic_cast<osg::MatrixTransform *>(hand->getChild(i));
            assert(transform);

            auto *geode = dynamic_cast<osg::Geode *>(transform->getChild(0));
            assert(geode);

            auto *drawable = dynamic_cast<osg::ShapeDrawable *>(geode->getChild(0));
            assert(drawable);

            int parent = HandPose::getJointParent((HandPose::Joint)i);
            if (parent < 0) {
                osg::Matrix mat(loc.getOrientation());
                mat.setTrans(loc.getPosition());
                transform->setMatrix(mat);

                auto *sphere = dynamic_cast<osg::Sphere *>(drawable->getShape());
                if (!sphere || sphere->getRadius() != radius) {
                    auto *shape = new osg::Sphere(osg::Vec3(0, 0, 0),
                                                  radius);
                    drawable->setShape(shape);
                }
            } else {
                auto &locParent = _pose->getJointLocation((HandPose::Joint)parent);
                float radiusParent = locParent.getRadius();
                float minRadius = std::min(radius, radiusParent);

                osg::Quat quat;
                quat.makeRotate(osg::Vec3f(0, 0, -1),
                                loc.getPosition() - locParent.getPosition());
                osg::Matrix mat(quat);
                mat.setTrans((loc.getPosition() + locParent.getPosition()) * 0.5f);
                transform->setMatrix(mat);

                auto *capsule = dynamic_cast<osg::Capsule *>(drawable->getShape());
                if (capsule && capsule->getRadius() != minRadius)
                    capsule = nullptr;
                float oldHeight = 0.0f;
                if (capsule) {
                    float newHeight2 = (loc.getPosition() - locParent.getPosition()).length2();
                    oldHeight = capsule->getHeight();
                    if (newHeight2 != 0.0f) {
                        float ratio2 = oldHeight*oldHeight / newHeight2;
                        // Allow some variance before regenerating shapes
                        static const float variance = 0.05f;
                        static const float varMin = (1.0f - variance);
                        static const float varMax = (1.0f + variance);
                        if (ratio2 < varMin*varMin || ratio2 > varMax*varMax)
                            capsule = nullptr;
                        // For big changes, don't take into account old height
                        if (ratio2 < 0.5f*0.5f || ratio2 > 1.5f*1.5f)
                            oldHeight = 0.0f;
                    }
                }
                if (!capsule) {
                    float newHeight = (loc.getPosition() - locParent.getPosition()).length();
                    if (oldHeight) {
                        // go for something in between to avoid having to change
                        // it again
                        newHeight = (newHeight + oldHeight) * 0.5f;
                    }
                    auto *shape = new osg::Capsule(osg::Vec3(0, 0, 0),
                                                   minRadius, newHeight);
                    drawable->setShape(shape);
                }
            }
        }

        hand->setAllChildrenOn();
    } else {
        hand->setAllChildrenOff();
    }
}

// Hand

Hand::Hand(const std::shared_ptr<HandPose> &pose) :
    _private(new Private)
{
    _private->_pose = pose;

    setName("hand switch");
    setAllChildrenOff();

    _private->_tessellationHints = new osg::TessellationHints();
    _private->_tessellationHints->setTargetNumFaces(100);

    // And a state set
    osg::ref_ptr<osg::StateSet> state = getOrCreateStateSet();

    // Material setup
    osg::Material* material = new osg::Material;
    material->setColorMode(osg::Material::OFF);
    material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4f(0.5f, 0.5f, 0.5f, 1.0f));
    state->setAttribute(material);

    setUpdateCallback(new HandUpdateCallback);
}

Hand::~Hand()
{
}

osg::Geode *Hand::generateGeode()
{
    return new osg::Geode();
}
