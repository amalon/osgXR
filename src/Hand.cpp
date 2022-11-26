// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#include "Hand.h"

#include <osgXR/HandPose>

#include <osg/Geometry>
#include <osg/Geode>
#include <osg/Material>
#include <osg/MatrixTransform>

#include <cassert>

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

static osg::Geometry *buildHandJointGeometry()
{
    // Create buffers for a simple cuboid
    const float w = 1.0f;
    const float h = 1.0f;
#define VERT_COUNT (4 * 6)
    const osg::Vec3 verticesRaw[VERT_COUNT] = {
        /* bottom */
        { -w, -w, -h }, { -w,  w, -h }, {  w,  w, -h }, {  w, -w, -h },
        /* top */
        { -w, -w,  h }, { -w,  w,  h }, {  w,  w,  h }, {  w, -w,  h },
        /* sides */
        { -w, -w, -h }, { -w, -w,  h }, { -w,  w,  h }, { -w,  w, -h },
        { -w,  w, -h }, { -w,  w,  h }, {  w,  w,  h }, {  w,  w, -h },
        {  w,  w, -h }, {  w,  w,  h }, {  w, -w,  h }, {  w, -w, -h },
        {  w, -w, -h }, {  w, -w,  h }, { -w, -w,  h }, { -w, -w, -h },
    };
    const osg::Vec3 normalsRaw[VERT_COUNT] = {
        /* bottom */
        {  0,  0, -1 }, {  0,  0, -1 }, {  0,  0, -1 }, {  0,  0, -1 },
        /* top */
        {  0,  0,  1 }, {  0,  0,  1 }, {  0,  0,  1 }, {  0,  0,  1 },
        /* sides */
        { -1,  0,  0 }, { -1,  0,  0 }, { -1,  0,  0 }, { -1,  0,  0 },
        {  0,  1,  0 }, {  0,  1,  0 }, {  0,  1,  0 }, {  0,  1,  0 },
        {  1,  0,  0 }, {  1,  0,  0 }, {  1,  0,  0 }, {  1,  0,  0 },
        {  0, -1,  0 }, {  0, -1,  0 }, {  0, -1,  0 }, {  0, -1,  0 },
    };
    osg::Vec3Array* vertices = new osg::Vec3Array(VERT_COUNT, verticesRaw);
    osg::Vec3Array* normals = new osg::Vec3Array(VERT_COUNT, normalsRaw);
    osg::DrawArrays* prim = new osg::DrawArrays(osg::PrimitiveSet::QUADS, 0, VERT_COUNT);

    // Create a geometry for the controller
    osg::Geometry* geom = new osg::Geometry;
    geom->setVertexArray(vertices);
    geom->setNormalArray(normals, osg::Array::BIND_PER_VERTEX);
    geom->addPrimitiveSet(prim);

    return geom;
}

// Internal API

void Hand::Private::update(Hand *hand)
{
    // Try and get an updated position
    // FIXME for the next frame that is!
    _pose->update();

    if (_pose->isActive()) {
        // All joint locations and orientations are valid
        for (unsigned int i = 0; i < HandPose::JOINT_COUNT; ++i) {
            auto &loc = _pose->getJointLocation((HandPose::Joint)i);
            float radius = loc.getRadius();
            osg::Matrix mat(loc.getOrientation());
            mat.setTrans(loc.getPosition());
            mat.preMultScale(osg::Vec3f(radius, radius, radius));

            auto *transform = dynamic_cast<osg::MatrixTransform *>(hand->getChild(i));
            assert(transform);
            transform->setMatrix(mat);
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

    osg::Geometry *geom = buildHandJointGeometry();

    for (unsigned int i = 0; i < HandPose::JOINT_COUNT; ++i) {
        // Create a matrix transform for the joint
        osg::MatrixTransform *transform = new osg::MatrixTransform;
        std::string jointName = HandPose::getJointName((HandPose::Joint)i);
        transform->setName(jointName + " transform");
        addChild(transform);

        // Create a geode in the transform
        osg::Geode* geode = new osg::Geode;
        geode->setName(jointName + " geode");
        geode->addDrawable(geom);
        transform->addChild(geode);
    }

    // And a state set
    osg::ref_ptr<osg::StateSet> state = getOrCreateStateSet();

    // Material setup
    osg::Material* material = new osg::Material;
    material->setColorMode(osg::Material::OFF);
    material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4f(0.5f, 0.5f, 0.5f, 1.0f));
    state->setAttribute(material);

    setUpdateCallback(new HandUpdateCallback);
}
