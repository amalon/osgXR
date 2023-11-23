// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "EventHandler.h"
#include "Instance.h"
#include "Session.h"

#include <osg/Notify>

using namespace osgXR::OpenXR;

void EventHandler::onEvent(Instance *instance,
                           const XrEventDataBuffer *event)
{
    switch (event->type)
    {
    case XR_TYPE_EVENT_DATA_EVENTS_LOST:
        onEventsLost(instance,
            reinterpret_cast<const XrEventDataEventsLost *>(event));
        break;
    case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
        onInstanceLossPending(instance,
            reinterpret_cast<const XrEventDataInstanceLossPending *>(event));
        break;
    case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
        {
            auto *profileEvent = reinterpret_cast<const XrEventDataInteractionProfileChanged *>(event);
            Session *session = instance->getSession(profileEvent->session);
            if (session)
                onInteractionProfileChanged(session, profileEvent);
            else
                OSG_WARN << "Unhandled OpenXR interaction profile changed event: Session not registered" << std::endl;
            break;
        }
    case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
        {
            auto *spaceEvent = reinterpret_cast<const XrEventDataReferenceSpaceChangePending *>(event);
            Session *session = instance->getSession(spaceEvent->session);
            if (session)
                onReferenceSpaceChangePending(session, spaceEvent);
            else
                OSG_WARN << "Unhandled OpenXR reference space change pending event: Session not registered" << std::endl;
            break;
        }
    case XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR:
        {
            auto *maskEvent = reinterpret_cast<const XrEventDataVisibilityMaskChangedKHR *>(event);
            Session *session = instance->getSession(maskEvent->session);
            if (session)
                onVisibilityMaskChanged(session, maskEvent);
            else
                OSG_WARN << "Unhandled OpenXR visibility mask change event: Session not registered" << std::endl;
            break;
        }
        break;
    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
        {
            auto *stateEvent = reinterpret_cast<const XrEventDataSessionStateChanged *>(event);
            Session *session = instance->getSession(stateEvent->session);
            if (session)
                onSessionStateChanged(session, stateEvent);
            else
                OSG_WARN << "Unhandled OpenXR session state change event: Session not registered" << std::endl;
            break;
        }
    default:
        onUnhandledEvent(instance, event);
        break;
    }
}

void EventHandler::onUnhandledEvent(Instance *instance,
                                    const XrEventDataBuffer *event)
{
    OSG_WARN << "Unhandled OpenXR Event: " << event->type << std::endl;
}

void EventHandler::onEventsLost(Instance *instance,
                                const XrEventDataEventsLost *event)
{
    OSG_WARN << event->lostEventCount << " OpenXR events lost" << std::endl;
}

void EventHandler::onInstanceLossPending(Instance *instance,
                                         const XrEventDataInstanceLossPending *event)
{
    OSG_WARN << "OpenXR instance loss pending" << std::endl;
}

void EventHandler::onInteractionProfileChanged(Session *session,
                                               const XrEventDataInteractionProfileChanged *event)
{
    OSG_WARN << "OpenXR interaction profile changed" << std::endl;
}

void EventHandler::onReferenceSpaceChangePending(Session *session,
                                                 const XrEventDataReferenceSpaceChangePending *event)
{
    OSG_WARN << "OpenXR reference space change pending" << std::endl;
}

void EventHandler::onVisibilityMaskChanged(Session *session,
                                           const XrEventDataVisibilityMaskChangedKHR *event)
{
    session->updateVisibilityMasks(event->viewConfigurationType,
                                   event->viewIndex);
}

void EventHandler::onSessionStateChanged(Session *session,
                                         const XrEventDataSessionStateChanged *event)
{
    XrSessionState oldState = session->getState();
    session->setState(event->state);
    switch (event->state)
    {
    case XR_SESSION_STATE_IDLE:
        // Either starting or soon to be stopping
        if (oldState == XR_SESSION_STATE_UNKNOWN)
            onSessionStateStart(session);
        break;
    case XR_SESSION_STATE_READY:
        // Session ready to begin
        onSessionStateReady(session);
        break;
    case XR_SESSION_STATE_SYNCHRONIZED:
        // Either session synchronised or no longer visible
        break;
    case XR_SESSION_STATE_VISIBLE:
        // Either session now visible or lost focus
        if (oldState == XR_SESSION_STATE_FOCUSED)
            onSessionStateUnfocus(session);
        break;
    case XR_SESSION_STATE_FOCUSED:
        // Session visible and in focus
        onSessionStateFocus(session);
        break;
    case XR_SESSION_STATE_STOPPING:
        // Session now stopping
        onSessionStateStopping(session, false);
        break;
    case XR_SESSION_STATE_LOSS_PENDING:
        // Session loss is pending, which can happen at any time
        if (oldState == XR_SESSION_STATE_FOCUSED)
            onSessionStateUnfocus(session);
        if (session->isRunning())
            onSessionStateStopping(session, true);
        // Attempt restart
        onSessionStateEnd(session, true);
        break;
    case XR_SESSION_STATE_EXITING:
        // Session is exiting and should be cleaned up
        onSessionStateEnd(session, false);
        break;
    default:
        OSG_WARN << "Unknown OpenXR session state: " << event->state << std::endl;
        break;
    }
}

void EventHandler::onSessionStateStart(Session *session)
{
}

void EventHandler::onSessionStateEnd(Session *session, bool retry)
{
}

void EventHandler::onSessionStateReady(Session *session)
{
}

void EventHandler::onSessionStateStopping(Session *session, bool loss)
{
}

void EventHandler::onSessionStateFocus(Session *session)
{
}

void EventHandler::onSessionStateUnfocus(Session *session)
{
}
