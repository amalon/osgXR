// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "Instance.h"
#include "Quirks.h"

#include <cstdlib>
#include <cstring>

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
    } quirkInfo[1] = {
#define QUIRK(NAME, COND, RUNTIME, VMIN, VMAX, LINK) \
        { NAME, "OSGXR_"#NAME, COND, RUNTIME, VMIN, VMAX, \
          #NAME LINK }

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
