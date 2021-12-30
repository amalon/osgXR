// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_ACTION
#define OSGXR_ACTION 1

#include <osgXR/Action>

#include "Subaction.h"

#include <osg/ref_ptr>

#include <memory>
#include <set>
#include <string>

namespace osgXR {

namespace OpenXR {
    class Action;
    class Instance;
};

class Action::Private
{
    public:

        static Private *get(Action *pub)
        {
            return pub->_private.get();
        }

        Private(ActionSet *actionSet);
        virtual ~Private();

        void setName(const std::string &name);
        const std::string &getName() const;

        void setLocalizedName(const std::string &localizedName);
        const std::string &getLocalizedName() const;

        void addSubaction(std::shared_ptr<Subaction::Private> subaction);

        bool getUpdated() const
        {
            return _updated;
        }

        /// Setup action with an OpenXR instance
        virtual OpenXR::Action *setup(OpenXR::Instance *instance) = 0;
        /// Clean up action before an OpenXR session is destroyed
        virtual void cleanupSession() = 0;
        /// Clean up action before an OpenXR instance is destroyed
        void cleanupInstance();

        /**
         * Get a list of currently bound source paths for this action.
         * @param sourcePaths[out] Vector of source paths to write into.
         */
        void getBoundSources(std::vector<std::string> &sourcePaths) const;

        /**
         * Get a list of currently bound source localized names for this action.
         * @param whichComponents  Which components to include.
         * @param names[out] Vector of names to write into.
         */
        void getBoundSourcesLocalizedNames(XrInputSourceLocalizedNameFlags whichComponents,
                                           std::vector<std::string> &names) const;

    protected:

        std::string _name;
        std::string _localizedName;

        osg::ref_ptr<ActionSet> _actionSet;
        std::set<std::shared_ptr<Subaction::Private>> _subactions;

        bool _updated;
        osg::ref_ptr<OpenXR::Action> _action;
};

} // osgXR

#endif
