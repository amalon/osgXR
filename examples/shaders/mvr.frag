#version 330 core
/*
 * Abstract osgXR's multiview rendering defines & macros so they can be
 * cleanly used from vertex, geometry, and fragment shaders
 * regardless of whether multiview rendering is in use.
 */

#pragma import_defines(OSGXR_FRAG_GLOBAL)
#ifdef OSGXR_FRAG_GLOBAL
OSGXR_FRAG_GLOBAL
#endif

vec2 mvr_raw_texcoord_transform_fb(vec2 raw_texcoord)
{
#pragma import_defines(OSGXR_FRAG_MVR_TEXCOORD(UV))
#ifdef OSGXR_FRAG_MVR_TEXCOORD
    return OSGXR_FRAG_MVR_TEXCOORD(raw_texcoord);
#else
    return raw_texcoord;
#endif
}

vec2 mvr_raw_texcoord_transform_buf(vec2 raw_texcoord)
{
#pragma import_defines(OSGXR_FRAG_MVB_TEXCOORD(UV))
#ifdef OSGXR_FRAG_MVB_TEXCOORD
    return OSGXR_FRAG_MVB_TEXCOORD(raw_texcoord);
#else
    return raw_texcoord;
#endif
}
