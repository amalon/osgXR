// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#ifndef OSGXR_HAND
#define OSGXR_HAND 1

#include <osgXR/Hand>

#include <osg/ref_ptr>
#include <osg/Shape>

namespace osgXR {

class Hand::Private
{
    public:

        static Private *get(Hand *pub)
        {
            return pub->_private.get();
        }

        Private();

        void update(Hand *hand);

    protected:

        void setup(Hand *hand);

        friend class Hand;

        std::shared_ptr<HandPose> _pose;
        bool _setUp;
        osg::ref_ptr<osg::TessellationHints> _tessellationHints;
};

} // osgXR

#endif
