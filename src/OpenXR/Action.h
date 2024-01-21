// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_ACTION
#define OSGXR_OPENXR_ACTION 1

#include "ActionSet.h"
#include "Path.h"

#include <osg/Vec2f>
#include <osg/ref_ptr>

#include <cassert>
#include <string>
#include <vector>

namespace osgXR {

namespace OpenXR {

class Path;
class Space;

class Action : public osg::Referenced
{
    public:

        Action(ActionSet *actionSet,
               const std::string &name,
               const std::string &localizedName,
               XrActionType type);
        virtual ~Action();

        // Action initialisation

        void addSubaction(const Path &path);

        // Returns true on success
        bool init();

        // Error checking

        inline bool valid() const
        {
            return _action != XR_NULL_HANDLE;
        }

        inline bool check(XrResult result, const char *actionMsg) const
        {
            return _actionSet->check(result, actionMsg);
        }

        // Conversions

        inline const osg::ref_ptr<ActionSet> getActionSet() const
        {
            return _actionSet;
        }

        inline const osg::ref_ptr<Instance> getInstance() const
        {
            return _actionSet->getInstance();
        }

        inline XrInstance getXrInstance() const
        {
            return _actionSet->getXrInstance();
        }

        inline XrActionSet getXrActionSet() const
        {
            return _actionSet->getXrActionSet();
        }

        inline XrAction getXrAction() const
        {
            return _action;
        }

    protected:

        // Action data
        osg::ref_ptr<ActionSet> _actionSet;
        std::vector<XrPath> _subactionPaths;
        XrActionCreateInfo _createInfo;
        XrAction _action;
};

/// Base action state for inputs.
class ActionStateBase : public osg::Referenced
{
    public:

        // Constructors

        ActionStateBase(Action *action, Session *session,
                        Path subactionPath = Path());
        virtual ~ActionStateBase();

        // Error checking

        inline bool valid() const
        {
            return _valid;
        }

        inline bool check(XrResult result, const char *actionMsg) const
        {
            return _action->check(result, actionMsg);
        }

    protected:

        // Utilities for synchronisation

        // Find whether the state needs update and update sync counter
        bool checkUpdate();

        // Member data

        osg::ref_ptr<Action> _action;
        osg::ref_ptr<Session> _session;
        Path _subactionPath;
        bool _valid;
        unsigned int _syncCount;
};

/// All action states have an isActive field.
template <typename T>
class ActionStateCommon : public ActionStateBase
{
    private:

        typedef ActionStateBase Base;

    public:

        // Constructors

        ActionStateCommon(Action *action, Session *session,
                          Path subactionPath = Path()) :
            Base(action, session, subactionPath)
        {
        }

        // Accessors

        bool isActive() const
        {
            assert(valid());
            return _state.isActive;
        }

        // Operations

        /// Update state if a sync has taken place
        bool update()
        {
            if (Base::checkUpdate())
                return updateState();
            return valid();
        }

    protected:

        // Protected operations

        bool updateState();

        // Data members

        T _state;
};

// These are the base action state classes
typedef ActionStateCommon<XrActionStateBoolean>  ActionStateCommonBoolean;
typedef ActionStateCommon<XrActionStateFloat>    ActionStateCommonFloat;
typedef ActionStateCommon<XrActionStateVector2f> ActionStateCommonVector2f;
typedef ActionStateCommon<XrActionStatePose>     ActionStateCommonPose;

// Convert action values to app / OSG friendly formats
template <typename T>
struct ActionTypeInfo;

// XrBool32 -> bool
template <>
struct ActionTypeInfo<XrActionStateBoolean>
{
    static bool convert(XrBool32 value)
    {
        return value;
    }

    static bool defaultValue()
    {
        return false;
    }
};

// float -> float
template <>
struct ActionTypeInfo<XrActionStateFloat>
{
    static float convert(float value)
    {
        return value;
    }

    static float defaultValue()
    {
        return 0.0f;
    }
};

// XrVector2f -> osg::Vec2f
template <>
struct ActionTypeInfo<XrActionStateVector2f>
{
    static osg::Vec2f convert(const XrVector2f &value)
    {
        return osg::Vec2f(value.x, value.y);
    }

    static osg::Vec2f defaultValue()
    {
        return osg::Vec2f(0.0f, 0.0f);
    }
};

/// Some action states have currentValue and related fields.
template <typename T>
class ActionStateSimple : public ActionStateCommon<T>
{
    private:

        typedef ActionStateCommon<T> Base;

    public:

        typedef ActionTypeInfo<T> Info;

        // Constructors

        ActionStateSimple(Action *action, Session *session,
                          Path subactionPath = Path()) :
            Base(action, session, subactionPath)
        {
        }

        // Accessors

        auto getCurrentState() const
        {
            assert(this->valid());
            return Info::convert(Base::_state.currentState);
        }

        bool hasChangedSinceLastSync() const
        {
            assert(this->valid());
            return Base::_state.changedSinceLastSync;
        }

        XrTime getLastChangedTime() const
        {
            assert(this->valid());
            return Base::_state.lastChangedTime;
        }
};

// These are the simple action state classes
typedef ActionStateSimple<XrActionStateBoolean>  ActionStateBoolean;
typedef ActionStateSimple<XrActionStateFloat>    ActionStateFloat;
typedef ActionStateSimple<XrActionStateVector2f> ActionStateVector2f;

/// Specialise Action for a specific input type
template <XrActionType type, typename T>
class ActionTyped : public Action
{
    public:

        typedef T State;

        ActionTyped(ActionSet *actionSet,
                    const std::string &name,
                    const std::string &localizedName) :
            Action(actionSet, name, localizedName, type)
        {
        }

        osg::ref_ptr<State> createState(Session *session,
                                        Path subactionPath = Path())
        {
            return new State(this, session, subactionPath);
        }
};

// So ActionStatePose etc can take ActionPose etc in constructor
class ActionStatePose;
class ActionStateVibration;

// These are the final typed action classes
typedef ActionTyped<XR_ACTION_TYPE_BOOLEAN_INPUT,    ActionStateBoolean>   ActionBoolean;
typedef ActionTyped<XR_ACTION_TYPE_FLOAT_INPUT,      ActionStateFloat>     ActionFloat;
typedef ActionTyped<XR_ACTION_TYPE_VECTOR2F_INPUT,   ActionStateVector2f>  ActionVector2f;
typedef ActionTyped<XR_ACTION_TYPE_POSE_INPUT,       ActionStatePose>      ActionPose;
typedef ActionTyped<XR_ACTION_TYPE_VIBRATION_OUTPUT, ActionStateVibration> ActionVibration;

/// Pose actions have their own way to get the pose
class ActionStatePose : public ActionStateCommon<XrActionStatePose>
{
    private:

        typedef ActionStateCommon<XrActionStatePose> Base;

    public:

        // Constructors

        ActionStatePose(ActionPose *action, Session *session,
                        Path subactionPath = Path());
        ~ActionStatePose();

        // Accessors

        Space *getSpace()
        {
            return _space.get();
        }

    protected:

        osg::ref_ptr<Space> _space;
};

class ActionStateVibration : public osg::Referenced
{
    public:

        // Constructors

        ActionStateVibration(ActionVibration *action, Session *session,
                             Path subactionPath = Path());

        // Error checking

        inline bool check(XrResult result, const char *actionMsg) const
        {
            return _action->check(result, actionMsg);
        }

        // Haptic vibrations

        bool applyHapticFeedback(int64_t duration_ns, float frequency,
                                 float amplitude) const;
        bool stopHapticFeedback() const;

    protected:

        // Member data

        osg::ref_ptr<Action> _action;
        osg::ref_ptr<Session> _session;
        Path _subactionPath;
};

} // osgXR::OpenXR

} // osgXR

#endif
