// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "Instance.h"
#include "Quirks.h"

#include <cstdlib>
#include <cstring>

#ifdef OSGXR_USE_X11
#define USING_X11 1
#else
#define USING_X11 0
#endif

using namespace osgXR::OpenXR;

void Quirks::probe(Instance *instance)
{
    static struct {
        Quirk       quirk;
        const char *envName;

        bool        condition;
        const char *runtimeMatch;
        XrVersion   runtimeVersionMin;
        XrVersion   runtimeVersionMax;

        const char *description;
    } quirkInfo[] = {
#define QUIRK(NAME, COND, RUNTIME, VMIN, VMAX, LINK) \
        { NAME, "OSGXR_"#NAME, COND, RUNTIME, VMIN, VMAX, \
          #NAME LINK }
#define MIN_XR_VERSION XR_MAKE_VERSION(0, 0, 0)
#define MAX_XR_VERSION XR_MAKE_VERSION(-1, -1, -1)
#define MATCH_MONADO   "Monado"
#define MATCH_STEAMVR  "SteamVR"

        // As of 2021-12-16 Monado expects the GL context to be current.
        // See https://gitlab.freedesktop.org/monado/monado/-/issues/145
        // Fixed by https://gitlab.freedesktop.org/monado/monado/-/merge_requests/1216
        QUIRK(QUIRK_GL_CONTEXT_IGNORED,
              USING_X11,
              MATCH_MONADO, MIN_XR_VERSION, XR_MAKE_VERSION(21, 0, 0),
              " (https://gitlab.freedesktop.org/monado/monado/-/issues/145)"),

        // Prior to around 1.16.2 SteamVR linux_v1.14 switched context but
        // didn't restore. Until 1.26.2 the SteamVR runtimeVersion was
        // unfortunately fairly useless (always reporting 0.1.0), so quirk is
        // enabled until 1.26.2.
        QUIRK(QUIRK_GL_CONTEXT_CHANGED,
              USING_X11,
              MATCH_STEAMVR, MIN_XR_VERSION, XR_MAKE_VERSION(0, 1, 0),
              ""),

        // Since around SteamVR 1.16.2 and until around 1.25.1, the GL context
        // is cleared by various calls. Until 1.26.2 the SteamVR runtimeVersion
        // was unfortunately fairly useless (always reporting 0.1.0), so quirk
        // is enabled on all versions until 1.26.2.
        QUIRK(QUIRK_GL_CONTEXT_CLEARED,
              USING_X11,
              MATCH_STEAMVR, XR_MAKE_VERSION(0, 1, 0), XR_MAKE_VERSION(0, 1, 0),
              " (https://github.com/ValveSoftware/SteamVR-for-Linux/issues/421)"),

        // Since SteamVR 1.15.x and until around SteamVR 2.11.2 apps hang during
        // xrDestroyInstance. Until 1.26.2 the SteamVR runtimeVersion was
        // unfortunately fairly useless (always reporting 0.1.0), so quirk is
        // enabled on all those early versions.
        QUIRK(QUIRK_AVOID_DESTROY_INSTANCE,
              USING_X11,
              MATCH_STEAMVR, XR_MAKE_VERSION(0, 1, 0), XR_MAKE_VERSION(2, 11, 1),
              " (https://github.com/ValveSoftware/SteamVR-for-Linux/issues/422)"),

        // SteamVR treats OpenGL subimages with the Y coordinates flipped (+Y
        // down) and the top-left at the origin, instead of the correct +Y up
        // and bottom-left origin for OpenGL.
        QUIRK(QUIRK_SUBIMAGE_FLIP_Y,
              true,
              MATCH_STEAMVR, MIN_XR_VERSION, MAX_XR_VERSION,
              " (https://steamcommunity.com/app/250820/discussions/3/4343239199138604289/)"),

        // apitrace doesn't understand GL_EXT_memory_object functions, requiring
        // OpenXR textures to be initialised with glTexImage before use.
        QUIRK(QUIRK_APITRACE_TEXIMAGE,
              false, "", MIN_XR_VERSION, MIN_XR_VERSION, ""),

        // SteamVR reports XR_EXT_user_presence events with a null session.
        QUIRK(QUIRK_PRESENCE_SESSION_NULL,
              true,
              MATCH_STEAMVR, MIN_XR_VERSION, MAX_XR_VERSION,
              " (https://steamcommunity.com/app/250820/discussions/3/596277178174319549/)"),


#undef QUIRK
    };

    const char *runtime = instance->getRuntimeName();
    XrVersion version = instance->getRuntimeVersion();

    // Clear all quirks
    reset();

    // Probe each quirk
    for (auto &quirk: quirkInfo)
    {
        const char *env = getenv(quirk.envName);
        if (env)
        {
            if (!strncmp(env, "0", 2))
            {
                set(quirk.quirk, false);
            }
            else if (!strncmp(env, "1", 2))
            {
                set(quirk.quirk, true);
            }
            else
            {
                env = nullptr;
                OSG_WARN << "osgXR: Unknown value for env \""
                    << quirk.envName << "\", ignored" << std::endl;
            }
        }
        // Probe the runtime name and version
        if (!env &&
            quirk.condition &&
            !strncmp(runtime, quirk.runtimeMatch, strlen(quirk.runtimeMatch)) &&
            version >= quirk.runtimeVersionMin &&
            version <= quirk.runtimeVersionMax)
        {
            set(quirk.quirk, true);
        }
    }

    // Print to log any enabled quirks
    if (any())
    {
        OSG_WARN << "osgXR: OpenXR Runtime: \"" << runtime
            << "\" version " << XR_VERSION_MAJOR(version)
            << "." << XR_VERSION_MINOR(version)
            << "." << XR_VERSION_PATCH(version) << std::endl;
        for (auto &quirk: quirkInfo)
            if (test(quirk.quirk))
                OSG_WARN << "osgXR: Enabling " << quirk.description << std::endl;
    }
}
