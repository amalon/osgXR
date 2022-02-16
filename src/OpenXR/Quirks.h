// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#ifndef OSGXR_OPENXR_QUIRKS
#define OSGXR_OPENXR_QUIRKS 1

#include <bitset>

namespace osgXR {

namespace OpenXR {

class Instance;

typedef enum Quirk {
    /**
     * This quirk indicates that the GLX context may be assumed to be current by
     * certain XR calls.
     * The affected calls are:
     * - xrCreateSession
     * - xrCreateSwapchain
     */
    QUIRK_GL_CONTEXT_IGNORED = 0,

    /**
     * This quirk indicates that the GLX context may be switched but not
     * restored by certain XR calls.
     * The affected calls are:
     * - xrCreateSwapchain
     */
    QUIRK_GL_CONTEXT_CHANGED,

    /**
     * This quirk indicates that the GLX context may be unconditionally cleared
     * by various XR calls.
     * The affected calls are:
     * - xrCreateSwapchain
     * - xrAcquireSwapchainImage
     * - xrWaitSwapchainImage
     * - xrReleaseSwapchainImage
     * - xrEndFrame
     */
    QUIRK_GL_CONTEXT_CLEARED,

    /**
     * This quirk indicates that the app should avoid destroying the XR instance
     * to avoid hangs.
     */
    QUIRK_AVOID_DESTROY_INSTANCE,

    /**
     * This quirk indicates that swapchain subimage coordinates are treated with
     * the Y coordinates flipped (+Y down) and the top-left at the origin,
     * instead of the correct +Y up and bottom-left origin for OpenGL.
     */
    QUIRK_SUBIMAGE_FLIP_Y,

    /**
     * This quirk indicates that textures obtained from OpenXR should be
     * allocated with glTexImage before use so that apitrace replays work even
     * though it doesn't understand GL_EXT_memory_object functions.
     */
    QUIRK_APITRACE_TEXIMAGE,

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
