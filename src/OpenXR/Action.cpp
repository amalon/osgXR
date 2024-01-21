// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "Action.h"
#include "Path.h"
#include "Session.h"
#include "Space.h"

#include <cassert>
#include <cstring>

using namespace osgXR::OpenXR;

Action::Action(ActionSet *actionSet,
               const std::string &name,
               const std::string &localizedName,
               XrActionType type) :
    _actionSet(actionSet),
    _createInfo{ XR_TYPE_ACTION_CREATE_INFO },
    _action(XR_NULL_HANDLE)
{
    strncpy(_createInfo.actionName, name.c_str(),
            XR_MAX_ACTION_NAME_SIZE - 1);
    strncpy(_createInfo.localizedActionName, localizedName.c_str(),
            XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
    _createInfo.actionType = type;
}

Action::~Action()
{
    if (_action != XR_NULL_HANDLE)
    {
        check(xrDestroyAction(_action),
              "destroy OpenXR action");
    }
}

void Action::addSubaction(const Path &path)
{
    assert(path.getInstance() == getInstance());
    _subactionPaths.push_back(path.getXrPath());
}

bool Action::init()
{
    if (valid())
        return true;

    if (!_subactionPaths.empty())
    {
        _createInfo.countSubactionPaths = _subactionPaths.size();
        _createInfo.subactionPaths = _subactionPaths.data();
    }
    return check(xrCreateAction(getXrActionSet(), &_createInfo, &_action),
                 "create OpenXR action");
}

ActionStateBase::ActionStateBase(Action *action, Session *session,
                                 Path subactionPath) :
    _action(action),
    _session(session),
    _subactionPath(subactionPath),
    _valid(false),
    _syncCount(0)
{
}

ActionStateBase::~ActionStateBase()
{
}

bool ActionStateBase::checkUpdate()
{
    unsigned int sessionSyncCount = _session->getActionSyncCount();
    // If an xrSyncActions has taken place, the state is out of date
    bool needsUpdate = (_syncCount < sessionSyncCount);
    // Update the counter as caller is expected to update the state
    _syncCount = sessionSyncCount;
    return needsUpdate;
}

template <>
bool ActionStateCommonBoolean::updateState()
{
    XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
    getInfo.action = _action->getXrAction();
    getInfo.subactionPath = _subactionPath.getXrPath();

    _state = { XR_TYPE_ACTION_STATE_BOOLEAN };

    _valid = check(xrGetActionStateBoolean(_session->getXrSession(), &getInfo,
                                           &_state),
                   "get boolean OpenXR action state");
    return _valid;
}

template <>
bool ActionStateCommonFloat::updateState()
{
    XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
    getInfo.action = _action->getXrAction();
    getInfo.subactionPath = _subactionPath.getXrPath();

    _state = { XR_TYPE_ACTION_STATE_FLOAT };

    _valid = check(xrGetActionStateFloat(_session->getXrSession(), &getInfo,
                                         &_state),
                   "get float OpenXR action state");
    return _valid;
}

template <>
bool ActionStateCommonVector2f::updateState()
{
    XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
    getInfo.action = _action->getXrAction();
    getInfo.subactionPath = _subactionPath.getXrPath();

    _state = { XR_TYPE_ACTION_STATE_VECTOR2F };

    _valid = check(xrGetActionStateVector2f(_session->getXrSession(), &getInfo,
                                            &_state),
                   "get vector2f OpenXR action state");
    return _valid;
}

template <>
bool ActionStateCommonPose::updateState()
{
    XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
    getInfo.action = _action->getXrAction();
    getInfo.subactionPath = _subactionPath.getXrPath();

    _state = { XR_TYPE_ACTION_STATE_POSE };

    _valid = check(xrGetActionStatePose(_session->getXrSession(), &getInfo,
                                        &_state),
                   "get pose OpenXR action state");
    return _valid;
}

ActionStatePose::ActionStatePose(ActionPose *action, Session *session,
                                 Path subactionPath) :
    Base(action, session, subactionPath),
    _space(new Space(session, action, subactionPath))
{
}

ActionStatePose::~ActionStatePose()
{
}

ActionStateVibration::ActionStateVibration(ActionVibration *action,
                                           Session *session,
                                           Path subactionPath) :
    _action(action),
    _session(session),
    _subactionPath(subactionPath)
{
}

bool ActionStateVibration::applyHapticFeedback(int64_t duration_ns,
                                               float frequency,
                                               float amplitude) const
{
    XrHapticActionInfo actionInfo{ XR_TYPE_HAPTIC_ACTION_INFO };
    actionInfo.action = _action->getXrAction();
    actionInfo.subactionPath = _subactionPath.getXrPath();

    XrHapticVibration vibration{ XR_TYPE_HAPTIC_VIBRATION };
    vibration.duration = duration_ns;
    vibration.frequency = frequency;
    vibration.amplitude = amplitude;

    return check(xrApplyHapticFeedback(_session->getXrSession(), &actionInfo,
                       reinterpret_cast<XrHapticBaseHeader*>(&vibration)),
                 "apply haptic feedback");
}

bool ActionStateVibration::stopHapticFeedback() const
{
    XrHapticActionInfo actionInfo{ XR_TYPE_HAPTIC_ACTION_INFO };
    actionInfo.action = _action->getXrAction();
    actionInfo.subactionPath = _subactionPath.getXrPath();

    return check(xrStopHapticFeedback(_session->getXrSession(), &actionInfo),
                 "stop haptic feedback");
}
