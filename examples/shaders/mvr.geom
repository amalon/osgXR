#version 330 core
/*
 * Abstract osgXR's multiview rendering defines & macros so they can be
 * cleanly used from vertex, geometry, and fragment shaders
 * regardless of whether multiview rendering is in use.
 */

#pragma requires(OSGXR_GEOM)

#pragma import_defines(OSGXR_GEOM_GLOBAL)
OSGXR_GEOM_GLOBAL

// Return pos (from vertex shader) transformed to per-view clip space.
vec4 mvr_vert_transform_cs(vec4 pos)
{
#pragma import_defines(OSGXR_GEOM_TRANSFORM(POS))
#ifdef OSGXR_GEOM_TRANSFORM
    return OSGXR_GEOM_TRANSFORM(pos);
#else
    return pos;
#endif
}

// Prepare vertex for MVR.
void mvr_prepare_vert()
{
#pragma import_defines(OSGXR_GEOM_PREPARE_VERTEX)
    OSGXR_GEOM_PREPARE_VERTEX;
}

// Prepare vertex for MVR and return pos (from vertex shader) transformed to
// per-view clip space.
vec4 mvr_prepare_vert_transform_cs(vec4 pos)
{
    mvr_prepare_vert();
    return mvr_vert_transform_cs(pos);
}

vec3 mvr_vs_norm_transform(vec3 vs_normal)
{
#pragma import_defines(OSGXR_GEOM_NORMAL_MATRIX)
#ifdef OSGXR_GEOM_NORMAL_MATRIX
    return OSGXR_GEOM_NORMAL_MATRIX * vs_normal;
#else
    return vs_normal;
#endif
}

vec3 mvr_vs_pos_transform(vec3 vs_pos)
{
#pragma import_defines(OSGXR_GEOM_VIEW_MATRIX)
#ifdef OSGXR_GEOM_VIEW_MATRIX
    return (OSGXR_GEOM_VIEW_MATRIX * vec4(vs_pos, 1.0)).xyz;
#else
    return vs_pos;
#endif
}

vec2 mvr_raw_texcoord_transform_fb(vec2 raw_texcoord)
{
#pragma import_defines(OSGXR_GEOM_MVR_TEXCOORD(UV))
#ifdef OSGXR_GEOM_MVR_TEXCOORD
    return OSGXR_GEOM_MVR_TEXCOORD(raw_texcoord);
#else
    return raw_texcoord;
#endif
}

vec2 mvr_raw_texcoord_transform_buf(vec2 raw_texcoord)
{
#pragma import_defines(OSGXR_GEOM_MVB_TEXCOORD(UV))
#ifdef OSGXR_GEOM_MVB_TEXCOORD
    return OSGXR_GEOM_MVB_TEXCOORD(raw_texcoord);
#else
    return raw_texcoord;
#endif
}
