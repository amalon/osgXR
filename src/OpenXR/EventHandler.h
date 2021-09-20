// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_EVENT_HANDLER
#define OSGXR_OPENXR_EVENT_HANDLER 1

#include <osg/Referenced>

#include <openxr/openxr.h>

namespace osgXR {

namespace OpenXR {

class Instance;
class Session;

/// This class handles OpenXR events.
class EventHandler : public osg::Referenced
{
    public:

        // Instance events

        /// Top level OpenXR event handler.
        void onEvent(Instance *instance, const XrEventDataBuffer *event);
        /// Handle an otherwise unhandled event.
        virtual void onUnhandledEvent(Instance *instance,
                                      const XrEventDataBuffer *event);

        /// Handle an events lost event.
        virtual void onEventsLost(Instance *instance,
                                  const XrEventDataEventsLost *event);
        /// Handle an instance loss pending event.
        virtual void onInstanceLossPending(Instance *instance,
                                           const XrEventDataInstanceLossPending *event);

        // Session events

        /// Handle an interaction profile changed event.
        virtual void onInteractionProfileChanged(Session *session,
                                                 const XrEventDataInteractionProfileChanged *event);
        /// Handle a reference space change pending event.
        virtual void onReferenceSpaceChangePending(Session *session,
                                                   const XrEventDataReferenceSpaceChangePending *event);
        /// Handle a visibility mask change event.
        virtual void onVisibilityMaskChanged(Session *session,
                                             const XrEventDataVisibilityMaskChangedKHR *event);
        /// Handle a session state change event.
        virtual void onSessionStateChanged(Session *session,
                                           const XrEventDataSessionStateChanged *event);

        // Session state events

        /// Transition into initial idle state (idle, after init).
        virtual void onSessionStateStart(Session *session);
        /// Transition into ending state (exiting / loss pending, before cleanup).
        virtual void onSessionStateEnd(Session *session, bool retry);

        /// Transition into a ready state.
        virtual void onSessionStateReady(Session *session);
        /// Transition out of running state (stopping, before end).
        virtual void onSessionStateStopping(Session *session, bool loss);

        /// Transition into focused session state.
        virtual void onSessionStateFocus(Session *session);
        /// Transition out of focused session state.
        virtual void onSessionStateUnfocus(Session *session);
};

} // osgXR::OpenXR

} // osgXR

#endif
