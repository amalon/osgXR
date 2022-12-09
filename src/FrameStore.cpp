// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "FrameStore.h"

#include <osg/FrameStamp>

#include <cassert>

using namespace osgXR;

FrameStore::FrameStore()
{
}

osg::ref_ptr<FrameStore::Frame> FrameStore::getFrame(FrameStore::Stamp stamp)
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);

    int index = lookupFrame(stamp);
    if (index < 0)
        return nullptr;

    return _store[index];
}

osg::ref_ptr<FrameStore::Frame> FrameStore::getFrame(FrameStore::Stamp stamp,
                                                     OpenXR::Session *session)
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);

    int index = lookupFrame(stamp);
    if (index < 0)
    {
        index = blankFrame();
        // there surely shouldn't be more than 2 frames in parallel
        assert(index >= 0);
        if (index < 0)
            return nullptr;

        osg::ref_ptr<OpenXR::Session::Frame> frame = session->waitFrame();
        if (frame.valid())
        {
            frame->setOsgFrameNumber(stamp->getFrameNumber());
            _store[index] = frame;
        }
        return frame;
    }

    return _store[index];
}

bool FrameStore::endFrame(FrameStore::Stamp stamp)
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);

    int index = lookupFrame(stamp);
    if (index < 0)
        return false;

    _store[index]->end();
    _store[index] = nullptr;

    return true;
}

bool FrameStore::killFrame(FrameStore::Stamp stamp)
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);

    int index = lookupFrame(stamp);
    if (index < 0)
        return false;

    _store[index] = nullptr;

    return true;
}

unsigned int FrameStore::countFrames() const
{
    unsigned int ret = 0;
    for (unsigned int i = 0; i < maxFrames; ++i)
        if (_store[i].valid())
            ++ret;
    return ret;
}

int FrameStore::lookupFrame(FrameStore::Stamp stamp) const
{
    unsigned int frameNumber = stamp->getFrameNumber();
    for (unsigned int i = 0; i < maxFrames; ++i)
    {
        if (_store[i].valid() &&
            _store[i]->getOsgFrameNumber() == frameNumber)
        {
            return i;
        }
    }
    return -1;
}

int FrameStore::blankFrame() const
{
    for (unsigned int i = 0; i < maxFrames; ++i)
        if (!_store[i].valid())
            return i;
    return -1;
}
