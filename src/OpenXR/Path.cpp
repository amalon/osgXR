// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2021 James Hogan <james@albanarts.com>

#include "Path.h"

using namespace osgXR::OpenXR;

Path::Path(Instance *instance,
           XrPath path) :
    _instance(instance),
    _path(path)
{
}

Path::Path(Instance *instance,
           const std::string &path) :
    _instance(instance),
    _path(XR_NULL_PATH)
{
    check(xrStringToPath(getXrInstance(), path.c_str(), &_path),
          "create OpenXR path from string");
}

std::string Path::toString() const
{
    if (!valid())
        return "";

    uint32_t count;
    if (!check(xrPathToString(getXrInstance(), _path,
                              0, &count, nullptr),
               "size OpenXR path string"))
        return "";
    std::vector<char> buffer(count);
    if (!check(xrPathToString(getXrInstance(), _path,
                              buffer.size(), &count, buffer.data()),
               "get OpenXR path string"))
        return "";

    return buffer.data();
}
