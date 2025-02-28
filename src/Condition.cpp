// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2025 James Hogan <james@albanarts.com>

#include "Extension.h"

#include <osgXR/Condition>
#include <osgXR/Manager>

using namespace osgXR;

// ConditionApi

ConditionApi::ConditionApi()
{
}

ConditionApi::ConditionApi(Version apiVersion) :
    _apiVersion(apiVersion)
{
}

ConditionApi::ConditionApi(Extension *extension) :
    _extension(Extension::Private::get(extension))
{
}

ConditionApi::ConditionApi(Extension *extension,
                           Version apiVersion) :
    _extension(Extension::Private::get(extension)),
    _apiVersion(apiVersion)
{
}

void ConditionApi::setApiVersion(Version apiVersion)
{
    _apiVersion = apiVersion;
    invalidate();
}

void ConditionApi::setExtension(Extension *extension)
{
    _extension = Extension::Private::get(extension);
    invalidate();
}

bool ConditionApi::evaluate(Manager *manager)
{
    // If the API version is set, the condition is met by that version or later
    if (_apiVersion && manager->getApiVersion() >= _apiVersion)
        return true;

    // If an extension is set, the condition is met by it being enabled
    if (_extension && _extension->getEnabled())
        return true;

    return false;
}
