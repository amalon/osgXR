// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2026 James Hogan <james@albanarts.com>

#ifndef OSGXR_GL_DEFINES
#define OSGXR_GL_DEFINES 1

#include <osg/GLDefines>

#ifndef GL_DEPTH32F_STENCIL8
#define GL_DEPTH32F_STENCIL8 0x8CAD
#endif

#ifndef GL_OVR_multiview
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR       0x9630
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR 0x9632
#define GL_MAX_VIEWS_OVR                                      0x9631
#define GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_OVR            0x9633
#endif

#endif
