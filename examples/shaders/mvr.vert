#version 330 core
/*
 * Abstract osgXR's multiview rendering defines & macros so they can be
 * cleanly used from vertex, geometry, and fragment shaders
 * regardless of whether multiview rendering is in use.
 */

#pragma import_defines(OSGXR_VERT_GLOBAL)
#ifdef OSGXR_VERT_GLOBAL
OSGXR_VERT_GLOBAL
#endif

// May be used by mvr_prepare_vert_transform()
uniform mat4 osg_ModelViewMatrix;
uniform mat4 osg_ModelViewProjectionMatrix;

// Return ls_pos transformed to the appropriate space for gl_Position.
vec4 mvr_ls_vert_transform(vec4 ls_pos)
{
#pragma import_defines(OSGXR_VERT_TRANSFORM(POS))
#ifdef OSGXR_VERT_TRANSFORM
    return OSGXR_VERT_TRANSFORM(ls_pos);
#else
    return osg_ModelViewProjectionMatrix * ls_pos;
#endif
}

// Prepare vertex for MVR.
void mvr_prepare_vert()
{
#pragma import_defines(OSGXR_VERT_PREPARE_VERTEX)
#ifdef OSGXR_VERT_PREPARE_VERTEX
    OSGXR_VERT_PREPARE_VERTEX;
#endif
}

// Prepare vertex for MVR and return ls_pos transformed to the appropriate
// space for gl_Position.
vec4 mvr_prepare_ls_vert_transform(vec4 ls_pos)
{
    mvr_prepare_vert();
    return mvr_ls_vert_transform(ls_pos);
}

vec3 mvr_vs_norm_transform(vec3 vs_normal)
{
#pragma import_defines(OSGXR_VERT_NORMAL_MATRIX)
#ifdef OSGXR_VERT_NORMAL_MATRIX
    return OSGXR_VERT_NORMAL_MATRIX * vs_normal;
#else
    return vs_normal;
#endif
}

vec3 mvr_vs_pos_transform(vec3 vs_pos)
{
#pragma import_defines(OSGXR_VERT_VIEW_MATRIX)
#ifdef OSGXR_VERT_VIEW_MATRIX
    return (OSGXR_VERT_VIEW_MATRIX * vec4(vs_pos, 1.0)).xyz;
#else
    return vs_pos;
#endif
}

vec2 mvr_raw_texcoord_transform_fb(vec2 raw_texcoord)
{
#pragma import_defines(OSGXR_VERT_MVR_TEXCOORD(UV))
#ifdef OSGXR_VERT_MVR_TEXCOORD
    return OSGXR_VERT_MVR_TEXCOORD(raw_texcoord);
#else
    return raw_texcoord;
#endif
}

vec2 mvr_raw_texcoord_transform_buf(vec2 raw_texcoord)
{
#pragma import_defines(OSGXR_VERT_MVB_TEXCOORD(UV))
#ifdef OSGXR_VERT_MVB_TEXCOORD
    return OSGXR_VERT_MVB_TEXCOORD(raw_texcoord);
#else
    return raw_texcoord;
#endif
}
