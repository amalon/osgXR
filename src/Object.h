// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#ifndef OSGXR_OBJECT
#define OSGXR_OBJECT 1

#include <osg/observer_ptr>

namespace osgXR {

class XRState;

namespace OpenXR {
    class Instance;
    class Session;
}

/// Base class for persistent objects which need to hook into an XR session.
class Object
{
    public:

        virtual ~Object();

        /** Set the state after construction (may result in virtual method
         * calls.
         */
        void registerState(XRState *state);
        /// Clear the state before destruction.
        void unregisterState();

    protected:

        friend class XRState;

        /// Callback for setting up a session.
        virtual void setup(OpenXR::Session *session);
        /// Callback for cleaning up after a session.
        virtual void cleanupSession();

        osg::observer_ptr<XRState> _state;
};

} // osgXR

#endif
