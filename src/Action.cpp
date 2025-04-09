// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "Action.h"
#include "ActionSet.h"

#include "OpenXR/Action.h"
#include "OpenXR/Session.h"
#include "OpenXR/Space.h"

#include <map>

using namespace osgXR;

// Internal API

Action::Private::Private(ActionSet *actionSet) :
    _actionSet(actionSet),
    _updated(true)
{
    ActionSet::Private::get(_actionSet)->registerAction(this);
}

Action::Private::~Private()
{
    ActionSet::Private::get(_actionSet)->unregisterAction(this);
}

void Action::Private::setName(const std::string &name)
{
    _updated = true;
    _name = name;
}

const std::string &Action::Private::getName() const
{
    return _name;
}

void Action::Private::setLocalizedName(const std::string &localizedName)
{
    _updated = true;
    _localizedName = localizedName;
}

const std::string &Action::Private::getLocalizedName() const
{
    return _localizedName;
}

void Action::Private::addSubaction(std::shared_ptr<Subaction::Private> subaction)
{
    _updated = true;
    _subactions.insert(subaction);
}

void Action::Private::cleanupInstance()
{
    _updated = true;
    _action = nullptr;
}

void Action::Private::getBoundSources(std::vector<std::string> &sourcePaths) const
{
    OpenXR::Session *session = ActionSet::Private::get(_actionSet)->getSession();
    if (_action.valid() && session)
    {
        std::vector<XrPath> paths;
        if (session->getActionBoundSources(_action, paths))
        {
            // Convert XrPath's into std::string's
            OpenXR::Instance *instance = session->getInstance();
            sourcePaths.resize(paths.size());
            for (unsigned int i = 0; i < paths.size(); ++i)
                sourcePaths[i] = OpenXR::Path(instance, paths[i]).toString();

            // Success!
            return;
        }
    }

    // Failure, clear output
    sourcePaths.resize(0);
}

void Action::Private::getBoundSourcesLocalizedNames(XrInputSourceLocalizedNameFlags whichComponents,
                                                    std::vector<std::string> &names) const
{
    OpenXR::Session *session = ActionSet::Private::get(_actionSet)->getSession();
    if (_action.valid() && session)
    {
        std::vector<XrPath> paths;
        if (session->getActionBoundSources(_action, paths))
        {
            // Convert XrPath's into localized names
            names.resize(paths.size());
            for (unsigned int i = 0; i < paths.size(); ++i)
                names[i] = session->getInputSourceLocalizedName(paths[i],
                                                                      whichComponents);

            // Success!
            return;
        }
    }

    // Failure, clear output
    names.resize(0);
}

namespace osgXR {

template <typename T>
class ActionPrivateCommon : public Action::Private
{
    public:

        typedef typename T::State State;

        ActionPrivateCommon(ActionSet *actionSet) :
            Private(actionSet)
        {
        }

        void cleanupSession() override
        {
            _states.clear();
        }

        OpenXR::Action *setup(OpenXR::Instance *instance) override
        {
            OpenXR::ActionSet *actionSet = ActionSet::Private::get(_actionSet)->setup(instance);
            if (!actionSet)
            {
                // Can't continue without an action set
                _action = nullptr;
                _updated = true;
            }
            else if (_updated || actionSet != _action->getActionSet())
            {
                _action = new T(actionSet, _name, _localizedName);
                for (auto &subaction: _subactions)
                    _action->addSubaction(subaction->setup(instance));
                _updated = false;
            }
            return _action;
        }

        State *getState(Subaction::Private *subaction = nullptr)
        {
            auto it = _states.find(subaction);
            if (it != _states.end())
                return (*it).second.get();

            OpenXR::Session *session = ActionSet::Private::get(_actionSet)->getSession();
            if (session)
            {
                OpenXR::Path subactionPath;
                if (subaction)
                    subactionPath = subaction->setup(session->getInstance());
                OpenXR::Action *action = setup(session->getInstance());
                if (action && action->valid())
                {
                    osg::ref_ptr<State> ret = static_cast<T*>(_action.get())->createState(session,
                                                                                          subactionPath);
                    _states[subaction] = ret;
                    return ret.get();
                }
            }
            return nullptr;
        }

    protected:

        std::map<Subaction::Private *, osg::ref_ptr<State>> _states;
};

template <typename T>
class ActionPrivateSimple : public ActionPrivateCommon<T>
{
    public:

        typedef typename T::State State;

        ActionPrivateSimple(ActionSet *actionSet) :
            ActionPrivateCommon<T>(actionSet)
        {
        }

        auto getValue(Subaction::Private *subaction)
        {
            State *state = this->getState(subaction);
            if (state && state->update() && state->isActive())
                return state->getCurrentState();
            else
                return T::State::Info::defaultValue();
        }
};

typedef ActionPrivateSimple<OpenXR::ActionBoolean>  ActionPrivateBoolean;
typedef ActionPrivateSimple<OpenXR::ActionFloat>    ActionPrivateFloat;
typedef ActionPrivateSimple<OpenXR::ActionVector2f> ActionPrivateVector2f;

class ActionPrivatePose : public ActionPrivateCommon<OpenXR::ActionPose>
{
    public:

        typedef OpenXR::ActionPose::State State;

        ActionPrivatePose(ActionSet *actionSet) :
            ActionPrivateCommon(actionSet)
        {
        }

        OpenXR::Space *getSpace(Subaction::Private *subaction)
        {
            State *state = getState(subaction);
            if (state && state->update() && state->isActive())
                return state->getSpace();
            else
                return nullptr;
        }

        bool locate(Subaction::Private *subaction,
                    Pose &pose)
        {
            OpenXR::Space *space = getSpace(subaction);
            OpenXR::Session *session = ActionSet::Private::get(_actionSet)->getSession();
            if (session && space)
            {
                OpenXR::Space::Location loc;
                XrTime time = session->getLastDisplayTime();
                bool ret = space->locate(session->getLocalSpace(time), time,
                                         loc);
                pose = Pose((Pose::Flags)loc.getFlags(),
                            loc.getOrientation(),
                            loc.getPosition());
                return ret;
            }
            else
            {
                pose = Pose();
                return false;
            }
        }
};

class ActionPrivateVibration : public ActionPrivateCommon<OpenXR::ActionVibration>
{
    public:

        typedef OpenXR::ActionVibration::State State;

        ActionPrivateVibration(ActionSet *actionSet) :
            ActionPrivateCommon(actionSet)
        {
        }

        bool applyHapticFeedback(Subaction::Private *subaction,
                                 int64_t duration_ns, float frequency,
                                 float amplitude)
        {
            State *state = getState(subaction);
            if (!state)
                return false;
            return state->applyHapticFeedback(duration_ns, frequency,
                                               amplitude);
        }

        bool stopHapticFeedback(Subaction::Private *subaction)
        {
            State *state = getState(subaction);
            if (!state)
                return false;
            return state->stopHapticFeedback();
        }
};

}

// Public API

Action::Action(Private *priv) :
    _private(priv)
{
}

Action::~Action()
{
}

void Action::addSubaction(Subaction *subaction)
{
    _private->addSubaction(Subaction::Private::get(subaction));
}

void Action::setName(const std::string &name,
                     const std::string &localizedName)
{
    _private->setName(name);
    _private->setLocalizedName(localizedName);
}

void Action::setName(const std::string &name)
{
    _private->setName(name);
}

const std::string &Action::getName() const
{
    return _private->getName();
}

void Action::setLocalizedName(const std::string &localizedName)
{
    _private->setLocalizedName(localizedName);
}

const std::string &Action::getLocalizedName() const
{
    return _private->getLocalizedName();
}

void Action::getBoundSources(std::vector<std::string> &sourcePaths) const
{
    _private->getBoundSources(sourcePaths);
}

void Action::getBoundSourcesLocalizedNames(uint32_t whichComponents,
                                           std::vector<std::string> &names) const
{
    _private->getBoundSourcesLocalizedNames(whichComponents, names);
}

// ActionBoolean

ActionBoolean::ActionBoolean(ActionSet *actionSet) :
    Action(new ActionPrivateBoolean(actionSet))
{
}

ActionBoolean::ActionBoolean(ActionSet *actionSet,
                             const std::string &name) :
    Action(new ActionPrivateBoolean(actionSet))
{
    setName(name, name);
}

ActionBoolean::ActionBoolean(ActionSet *actionSet,
                             const std::string &name,
                             const std::string &localizedName) :
    Action(new ActionPrivateBoolean(actionSet))
{
    setName(name, localizedName);
}

bool ActionBoolean::getValue(Subaction *subaction)
{
    auto privSubaction = Subaction::Private::get(subaction);
    return static_cast<ActionPrivateBoolean *>(Private::get(this))->getValue(privSubaction.get());
}

// ActionFloat

ActionFloat::ActionFloat(ActionSet *actionSet) :
    Action(new ActionPrivateFloat(actionSet))
{
}

ActionFloat::ActionFloat(ActionSet *actionSet,
                         const std::string &name) :
    Action(new ActionPrivateFloat(actionSet))
{
    setName(name, name);
}

ActionFloat::ActionFloat(ActionSet *actionSet,
                         const std::string &name,
                         const std::string &localizedName) :
    Action(new ActionPrivateFloat(actionSet))
{
    setName(name, localizedName);
}

float ActionFloat::getValue(Subaction *subaction)
{
    auto privSubaction = Subaction::Private::get(subaction);
    return static_cast<ActionPrivateFloat *>(Private::get(this))->getValue(privSubaction.get());
}

// ActionVector2f

ActionVector2f::ActionVector2f(ActionSet *actionSet) :
    Action(new ActionPrivateVector2f(actionSet))
{
}

ActionVector2f::ActionVector2f(ActionSet *actionSet,
                               const std::string &name) :
    Action(new ActionPrivateVector2f(actionSet))
{
    setName(name, name);
}

ActionVector2f::ActionVector2f(ActionSet *actionSet,
                               const std::string &name,
                               const std::string &localizedName) :
    Action(new ActionPrivateVector2f(actionSet))
{
    setName(name, localizedName);
}

osg::Vec2f ActionVector2f::getValue(Subaction *subaction)
{
    auto privSubaction = Subaction::Private::get(subaction);
    return static_cast<ActionPrivateVector2f *>(Private::get(this))->getValue(privSubaction.get());
}

// ActionPose

ActionPose::ActionPose(ActionSet *actionSet) :
    Action(new ActionPrivatePose(actionSet))
{
}

ActionPose::ActionPose(ActionSet *actionSet,
                       const std::string &name) :
    Action(new ActionPrivatePose(actionSet))
{
    setName(name, name);
}

ActionPose::ActionPose(ActionSet *actionSet,
                       const std::string &name,
                       const std::string &localizedName) :
    Action(new ActionPrivatePose(actionSet))
{
    setName(name, localizedName);
}

Pose ActionPose::getValue(Subaction *subaction)
{
    Pose pose;
    auto privSubaction = Subaction::Private::get(subaction);
    static_cast<ActionPrivatePose *>(Private::get(this))->locate(privSubaction.get(),
                                                                 pose);
    return pose;
}

// ActionVibration

ActionVibration::ActionVibration(ActionSet *actionSet) :
    Action(new ActionPrivateVibration(actionSet))
{
}

ActionVibration::ActionVibration(ActionSet *actionSet,
                       const std::string &name) :
    Action(new ActionPrivateVibration(actionSet))
{
    setName(name, name);
}

ActionVibration::ActionVibration(ActionSet *actionSet,
                       const std::string &name,
                       const std::string &localizedName) :
    Action(new ActionPrivateVibration(actionSet))
{
    setName(name, localizedName);
}

bool ActionVibration::applyHapticFeedback(int64_t duration_ns, float frequency,
                                          float amplitude)
{
    auto priv = static_cast<ActionPrivateVibration *>(Private::get(this));
    return priv->applyHapticFeedback(nullptr, duration_ns, frequency,
                                     amplitude);
}

bool ActionVibration::applyHapticFeedback(Subaction *subaction,
                                          int64_t duration_ns, float frequency,
                                          float amplitude)
{
    auto privSubaction = Subaction::Private::get(subaction);
    auto priv = static_cast<ActionPrivateVibration *>(Private::get(this));
    return priv->applyHapticFeedback(privSubaction.get(), duration_ns, frequency,
                                     amplitude);
}

bool ActionVibration::stopHapticFeedback(Subaction *subaction)
{
    auto privSubaction = Subaction::Private::get(subaction);
    auto priv = static_cast<ActionPrivateVibration *>(Private::get(this));
    return priv->stopHapticFeedback(privSubaction.get());
}
