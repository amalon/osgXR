// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_FRAME_STORE
#define OSGXR_FRAME_STORE 1

#include "OpenXR/Session.h"

#include <osg/ref_ptr>
#include <OpenThreads/Mutex>

#include <vector>

namespace osg {
    class FrameStamp;
}

namespace osgXR {

/**
 * Manages concurrent frames.
 * A FrameStore stores any concurrent OpenXR frames and allows them to be
 * created and retrieved in a thread-safe way based on an osg::FrameStamp.
 */
class FrameStore
{
    public:

        typedef OpenXR::Session::Frame Frame;
        typedef const osg::FrameStamp *Stamp;

        FrameStore();

        /// Get a frame by FrameStamp.
        osg::ref_ptr<Frame> getFrame(Stamp stamp);

        /// Get or wait for a frame by FrameStamp.
        osg::ref_ptr<Frame> getFrame(Stamp stamp, OpenXR::Session *session);

        /**
         * End a frame by FrameStamp.
         * @return true on success, false otherwise.
         */
        bool endFrame(Stamp stamp);

        /**
         * Kill (without ending) a frame by FrameStamp.
         * @return true on success, false otherwise.
         */
        bool killFrame(Stamp stamp);

        /// Count the number of frames.
        unsigned int countFrames() const;

    protected:

        // These return cache index or -1
        int lookupFrame(Stamp stamp) const;
        int blankFrame() const;

        // 2 allows work to start on next frame before the prior one has ended
        static constexpr unsigned int maxFrames = 2;
        // Protected by _mutex
        osg::ref_ptr<Frame> _store[maxFrames];

        // For access to _store
        OpenThreads::Mutex _mutex;
};

}

#endif
