// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2025 James Hogan <james@albanarts.com>

#include "ManagedSpace.h"

using namespace osgXR::OpenXR;

ManagedSpace::ManagedSpace(Session *session, XrReferenceSpaceType type,
                           const Location &locInRefSpace) :
    _type(type)
{
    _stateQueue.emplace_back(0, new Space(session, type, locInRefSpace),
                             locInRefSpace);
}

ManagedSpace::ManagedSpace(Session *session, XrReferenceSpaceType type) :
    _type(type)
{
    Location nop;
    _stateQueue.emplace_back(0, new Space(session, type), nop);
}

osg::ref_ptr<Space> ManagedSpace::getSpace(XrTime time) const
{
    Space *ret = nullptr;
    for (auto &state: _stateQueue) {
        if (time < state.changeTime)
            break;
        ret = state.space.get();
    }
    return ret;
}

void ManagedSpace::endFrame(XrTime time)
{
    // Pop any pending states that are definitively superceded by the next state
    while (_stateQueue.size() > 1 &&
           time >= (++_stateQueue.begin())->changeTime)
        _stateQueue.pop_front();
}

bool ManagedSpace::recenter(XrTime changeTime,
                            const Location &locInPreviousSpace)
{
    // If a change is already queued after changeTime, we can't change it now
    if (changeTime < _stateQueue.back().changeTime)
        return false;

    Location prevLoc = _stateQueue.back().loc;
    Location loc = prevLoc * locInPreviousSpace;

    // Add a new pending state
    Space *finalSpace = new Space(_stateQueue.front().space->getSession(),
                                  _type, loc);
    _stateQueue.emplace_back(changeTime, finalSpace, loc);
    return true;
}

void ManagedSpace::onChangePending(const XrEventDataReferenceSpaceChangePending *event)
{
    // If the final state is posed, the system level recentering should reset
    // it, so the space needs recreating
    Space *finalSpace = _stateQueue.back().space.get();
    if (_stateQueue.back().loc.valid())
        finalSpace = new Space(finalSpace->getSession(), _type);

    // Add a new pending state
    _stateQueue.emplace_back(event->changeTime, finalSpace, Location());
}
