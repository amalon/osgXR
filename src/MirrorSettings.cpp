// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include <osgXR/MirrorSettings>

using namespace osgXR;

MirrorSettings::MirrorSettings() :
    _mirrorMode(MIRROR_AUTOMATIC),
    _mirrorViewIndex(-1)
{
}
