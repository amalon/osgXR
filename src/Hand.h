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

	void update(Hand *hand);

    protected:

        friend class Hand;

        std::shared_ptr<HandPose> _pose;
        osg::ref_ptr<osg::TessellationHints> _tessellationHints;
};

} // osgXR

#endif
