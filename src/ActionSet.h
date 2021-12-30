// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_ACTION_SET
#define OSGXR_ACTION_SET 1

#include <osgXR/ActionSet>
#include <osgXR/Action>

#include "OpenXR/Path.h"

#include "Subaction.h"

#include <osg/observer_ptr>
#include <osg/ref_ptr>

#include <memory>
#include <string>
#include <set>

namespace osgXR {

class Action;
class XRState;

namespace OpenXR {
    class ActionSet;
    class Instance;
    class Session;
};

class ActionSet::Private
{
    public:

        static Private *get(ActionSet *pub)
        {
            return pub->_private.get();
        }

        Private(XRState *state);
        ~Private();

        void setName(const std::string &name);
        const std::string &getName() const;

        void setLocalizedName(const std::string &localizedName);
        const std::string &getLocalizedName() const;

        void setPriority(uint32_t priority);
        uint32_t getPriority() const;

        bool getUpdated() const;

        void activate(std::shared_ptr<Subaction::Private> subaction = nullptr);
        void deactivate(std::shared_ptr<Subaction::Private> subaction = nullptr);
        bool isActive();

        void registerAction(Action::Private *action);
        void unregisterAction(Action::Private *action);

        /// Setup action set with an OpenXR instance
        OpenXR::ActionSet *setup(OpenXR::Instance *instance);
        /// Setup action set with an OpenXR session
        bool setup(OpenXR::Session *session);
        /// Clean up action before an OpenXR session is destroyed
        void cleanupSession();
        /// Clean up action before an OpenXR instance is destroyed
        void cleanupInstance();

        OpenXR::Session *getSession()
        {
            return _session.get();
        }

    protected:

        osg::observer_ptr<XRState> _state;
        std::string _name;
        std::string _localizedName;
        uint32_t _priority;
        std::set<std::shared_ptr<Subaction::Private>> _activeSubactions;

        std::set<Action::Private *> _actions;

        bool _updated;
        osg::ref_ptr<OpenXR::ActionSet> _actionSet;
        osg::observer_ptr<OpenXR::Session> _session;
};

} // osgXR

#endif
