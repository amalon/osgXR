// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_SUBACTION
#define OSGXR_SUBACTION 1

#include <osgXR/Subaction>

#include "OpenXR/Path.h"

#include <set>
#include <string>

namespace osgXR {

class XRState;

namespace OpenXR {
    class Instance;
};

class Subaction::Private
{
    public:

        static std::shared_ptr<Private> get(Subaction *pub)
        {
            if (pub)
                return pub->_private;
            else
                return nullptr;
        }

        Private(XRState *state, const std::string &path);

        // Public object registration
        void registerPublic(Subaction *subaction);
        void unregisterPublic(Subaction *subaction);

        // Accessors

        /// Get the subaction's path as a string.
        const std::string &getPathString() const
        {
            return _pathString;
        }

        /// Find the current interaction profile.
        InteractionProfile *getCurrentProfile();

        // Events

        /// Notify that an interaction profile has changed.
        void onInteractionProfileChanged(OpenXR::Session *session);

        /// Setup path with an OpenXR instance.
        const OpenXR::Path &setup(OpenXR::Instance *instance);
        /// Clean up current profile before an OpenXR session is destroyed.
        void cleanupSession();
        /// Clean up path before an OpenXR instance is destroyed.
        void cleanupInstance();

    private:

        XRState *_state;
        std::string _pathString;
        std::set<Subaction *> _publics;

        OpenXR::Path _path;
        osg::ref_ptr<InteractionProfile> _currentProfile;
};

} // osgXR

#endif
