// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_INTERACTION_PROFILE
#define OSGXR_OPENXR_INTERACTION_PROFILE 1

#include "Action.h"
#include "Path.h"

#include <osg/ref_ptr>

#include <set>
#include <utility>

namespace osgXR {

namespace OpenXR {

class InteractionProfile : public osg::Referenced
{
    public:

        InteractionProfile(const Path &path);
        InteractionProfile(Instance *instance,
                           const char *vendor, const char *type);
        virtual ~InteractionProfile();

        // Accessors
        
        void addBinding(Action *action, const std::string &binding)
        {
            Path path(_path.getInstance(), binding);
            addBinding(action, path);
        }

        void addBinding(Action *action, const Path &binding);

        // returns true on success
        bool suggestBindings();

        // Error checking

        inline bool check(XrResult result, const char *actionMsg) const
        {
            return _path.check(result, actionMsg);
        }

        // Conversions

        inline const osg::ref_ptr<Instance> getInstance() const
        {
            return _path.getInstance();
        }

        inline XrInstance getXrInstance() const
        {
            return _path.getXrInstance();
        }

        inline const Path &getPath() const
        {
            return _path;
        }

    protected:

        // Interaction profile data
        Path _path;
        typedef std::pair<osg::ref_ptr<Action>, XrPath> ActionBindingPair;
        std::set<ActionBindingPair> _bindings;
};

} // osgXR::OpenXR

} // osgXR

#endif
