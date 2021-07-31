// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_FRAME_STAMPED_VECTOR
#define OSGXR_FRAME_STAMPED_VECTOR 1

#include <osg/FrameStamp>

#include <optional>
#include <utility>
#include <vector>

namespace osgXR {

/**
 * Manages frame stamping of vector items.
 * Contains a vector of the chosen type, with each item stamped with a
 * FrameStamp. Items can be retrieved by index or FrameStamp.
 */
template <typename T>
class FrameStampedVector
{
    public:

        typedef T Item;
        typedef unsigned int FrameNumber;
        typedef const osg::FrameStamp *Stamp;
        typedef std::pair<Item, FrameNumber> StampedItem;

        void reserve(unsigned int len)
        {
            _vec.reserve(len);
        }

        void resize(unsigned int len, Item item = Item())
        {
            _vec.resize(len, StampedItem(item, ~0));
        }

        unsigned int size() const
        {
            return _vec.size();
        }

        void push_back(const Item &item)
        {
            _vec.push_back(StampedItem(item, ~0));
        }

        // operator [] provides an Item if indexed directly
        const Item &operator [] (unsigned int index) const
        {
            return _vec[index].first;
        }

        // operator [] provides an optional Item if indexed by stamp
        std::optional<const Item> operator [] (Stamp stamp) const
        {
            int index = findStamp(stamp);
            if (index < 0)
                return std::nullopt;
            return _vec[index].first;
        }

        int findStamp(Stamp stamp) const
        {
            unsigned int frameNumber = stamp->getFrameNumber();
            for (unsigned int i = 0; i < _vec.size(); ++i)
                if (_vec[i].second == frameNumber)
                    return i;
            return -1;
        }

        void setStamp(unsigned int index, Stamp stamp)
        {
            _vec[index].second = stamp->getFrameNumber();
        }

    protected:

        std::vector<StampedItem> _vec;
};

}

#endif
