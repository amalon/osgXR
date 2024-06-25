// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2024 James Hogan <james@albanarts.com>

#ifndef OSGXR_MULTIVIEW
#define OSGXR_MULTIVIEW 1

#include "OpenXR/Session.h"

#include <osg/Referenced>

namespace osg {
    class StateSet;
};

namespace osgXR {

/// Represents a group of related / overlapping views.
class MultiView : public osg::Referenced
{
    public:

        struct SharedView {
            /// Pose of view.
            XrPosef pose;
            /// FOV angles of view.
            XrFovf fov;
            /// Z offset for projection matrix (positive).
            float zoffset;
        };

        /// Create a MultiView object for a given session.
        static MultiView *create(const OpenXR::Session *session);

        virtual ~MultiView();

        /// Load all view information from a frame.
        virtual void loadFrame(OpenXR::Session::Frame *frame);

        /** Get a shared view encompassing all XR views.
         * @param[out] view Shared view information.
         * @returns true on success.
         */
        bool getSharedView(SharedView &view) const;

    protected:

        virtual void _addView(unsigned int viewIndex,
                              const XrPosef &pose, const XrFovf &fov) = 0;

        virtual bool _getSharedView(SharedView &view) const = 0;

        /// Whether a valid view arrangement has been provided.
        bool _valid = false;
        /// Whether the shared view cache is valid.
        mutable bool _cachedSharedView = false;
        /// Cache of shared view information.
        mutable SharedView _sharedView;
};

}

#endif
