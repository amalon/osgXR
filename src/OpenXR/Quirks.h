// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_QUIRKS
#define OSGXR_OPENXR_QUIRKS 1

#include <bitset>

namespace osgXR {

namespace OpenXR {

class Instance;

typedef enum Quirk {
    QUIRK_MAX
} Quirk;

/**
 * Represents a set of OpenXR runtime quirks which require workarounds.
 */
class Quirks : public std::bitset<QUIRK_MAX>
{
    public:

        /**
         * Probe the OpenXR instance to see which quirks are required.
         */
        void probe(Instance *instance);
};

} // osgXR::OpenXR

} // osgXR

#endif
