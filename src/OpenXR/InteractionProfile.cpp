// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "InteractionProfile.h"

#include <cassert>
#include <vector>

using namespace osgXR::OpenXR;

InteractionProfile::InteractionProfile(const Path &path) :
    _path(path)
{
}

InteractionProfile::InteractionProfile(Instance *instance,
                                       const char *vendor, const char *type) :
    _path(instance, (std::string)"/interaction_profiles/" + vendor + "/" + type)
{
}

InteractionProfile::~InteractionProfile()
{
}

void InteractionProfile::addBinding(Action *action, const Path &binding)
{
    assert(binding.getInstance() == getInstance());
    _bindings.insert(ActionBindingPair(action, binding.getXrPath()));
}

bool InteractionProfile::suggestBindings()
{
    // No bindings: nothing to do!
    if (_bindings.empty())
        return true;

    // Construct binding vector from _bindings map
    std::vector<XrActionSuggestedBinding> bindings;
    bindings.reserve(_bindings.size());
    for (auto pair: _bindings)
    {
        if (pair.first->init())
            bindings.push_back({ pair.first->getXrAction(),
                               pair.second });
    }

    // Suggest the bindings
    XrInteractionProfileSuggestedBinding suggestedBinding{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING
    };
    suggestedBinding.interactionProfile = _path.getXrPath();
    suggestedBinding.countSuggestedBindings = bindings.size();
    suggestedBinding.suggestedBindings = bindings.data();

    return check(xrSuggestInteractionProfileBindings(getXrInstance(),
                                                     &suggestedBinding),
                 "suggest interaction profile bindings");
}
