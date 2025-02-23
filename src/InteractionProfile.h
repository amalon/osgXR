// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_INTERACTION_PROFILE
#define OSGXR_INTERACTION_PROFILE 1

#include <osgXR/InteractionProfile>

#include <osg/observer_ptr>
#include <osg/ref_ptr>

#include <list>
#include <string>

namespace osgXR {

class Condition;
class XRState;

namespace OpenXR {
    class InteractionProfile;
    class Path;
    class Session;
};

class InteractionProfile::Private
{
    public:

        static Private *get(InteractionProfile *pub)
        {
            return pub->_private.get();
        }

        Private(InteractionProfile *pub,
                XRState *newState,
                const std::string &newVendor,
                const std::string &newType);
        ~Private();

        void addCondition(Condition *condition);

        void suggestBinding(Action *action, const std::string &binding,
                            Condition *condition);

        bool getUpdated() const
        {
            return _updated;
        }

        /// Setup bindings with an OpenXR instance
        bool setup(OpenXR::Instance *instance);
        /// Clean up bindings before an OpenXR instance is destroyed
        void cleanupInstance();

        // Accessors

        /// Get the public object.
        InteractionProfile *getPublic()
        {
            return _pub;
        }

        /// Get the vendor segment of the OpenXR interaction profile path.
        const std::string &getVendor() const
        {
            return _vendor;
        }

        /// Get the type segment of the OpenXR interaction profile path.
        const std::string &getType() const
        {
            return _type;
        }

        OpenXR::Path getPath() const;

    private:

        InteractionProfile *_pub;
        osg::observer_ptr<XRState> _state;
        std::string _vendor;
        std::string _type;

        std::vector<osg::ref_ptr<Condition>> _conditions;

        struct Binding {
            osg::ref_ptr<Action> action;
            std::string binding;
            osg::ref_ptr<Condition> condition;
        };
        std::list<Binding> _bindings;

        bool _updated;
        osg::ref_ptr<OpenXR::InteractionProfile> _profile;
};

} // osgXR

#endif
