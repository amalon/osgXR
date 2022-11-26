// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#ifndef OSGXR_HAND
#define OSGXR_HAND 1

#include <osgXR/Hand>

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
};

} // osgXR

#endif
