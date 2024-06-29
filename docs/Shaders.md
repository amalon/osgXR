osgXR Shader Modifications
==========================

osgXR exposes various preprocessor definitions and macros to GLSL shaders and
the application to allow them to correctly handle the various multiview
rendering modes which osgXR supports.

The documentation & examples below will use the osgXR definitions & macros
directly in order to demonstrate their use, however it is likely that the
cleanest approach to using them is to contain them in a GLSL file for each
shader stage, and abstract the macros as functions for use by other shaders,
along with fallback implementations. This should avoid most of the ifdefs in
normal shader code, as well as allowing common GLSL code to use the appropriate
macros depending on mode. Examples of these can be found here:
 - [`examples/shaders/mvr.vert`](../examples/shaders/mvr.vert)
 - [`examples/shaders/mvr.geom`](../examples/shaders/mvr.geom)
 - [`examples/shaders/mvr.frag`](../examples/shaders/mvr.frag)

## Definitions

### Coordinate Spaces

Shared view space
: A space near to and oriented similarly to all the OpenXR views, used by
cameras representing multiple OpenXR views by one of the multi-pass rendering
modes described below. Transform a model space coordinate to this space by
multiplying it by `osg_ModelViewMatrix`.

Per-view eye space
: The view space for each OpenXR view. May be both rotated and offset from the
shared view space.

Per-view projection space
: The projection space for each OpenXR view. May be asymmetrical (FOV angle
differs in opposite directions, e.g. FOV to left of centre is likely to be
greater than to right of centre for left eye views),


## Intermediate Buffers and Swapchain Modes

The swapchain textures which are passed to OpenXR for display in a VR headset
can use a number of different layouts, depending on the mode of multiview
rendering.

Intermediate frame buffers may also have to follow a similar layout, in which
case shaders which sample from them may need modification.

Per-view intermediate buffers with fixed dimensions (rather than scaling with
the VR view dimensions) also need to follow a similar layout, but without the
possibility of differing sizes per view.

### Multiple

This swapchain mode uses separate textures to store the frame buffer of each
OpenXR view. It is only possible with the Slave Cameras multiview rendering
mode, as that is the only mode using fully separate passes for each view.

In this mode, each osgXR view is expected to use its own simple intermediate
buffers, or to reuse the same frame buffer multiple times.

To indicate that multiple swapchains mode is supported by the application and
its shaders, and to allow this swapchain mode to be chosen, set it as an allowed
swapchain mode in `osgXR::Settings`, e.g. from a derived class of
`osgXR::Manager` you can do this:
```C++
_settings->allowSwapchainMode(osgXR::Settings::SWAPCHAIN_MULTIPLE);
```
This also prevents osgXR from defaulting to allowing both Multiple and Single
(side-by-side) swapchain modes for backwards compatibility when no other
swapchain modes are explicitly allowed.

### Single (side-by-side)

This swapchain mode uses a single texture to store multiple frame buffers, with
each view being rendered to a viewport in the texture, usually stereo with left
and right frame buffers placed side-by-side.

This is the required swapchain layout for the SceneView multiview rendering
mode (which requires stereo views), but is also usable with Slave Cameras and
Geometry Shaders.

Care must be taken to transform texture coordinates into the appropriate
viewport before sampling intermediate buffers, either in the vertex & geometry
shader (see `OSGXR_VERT_MVR_TEXCOORD` & `OSGXR_GEOM_MVR_TEXCOORD` for frame
buffers and `OSGXR_VERT_MVB_TEXCOORD` & `OSGXR_GEOM_MVB_TEXCOORD` for fixed
buffers), or in the fragment shader (see `OSGXR_FRAG_MVR_TEXCOORD` for frame
buffers and `OSGXR_FRAG_MVB_TEXCOORD` for fixed buffers).

To indicate that side-by-side swapchains are supported by the application and
its shaders, and to allow this swapchain mode to be chosen, set it as an allowed
swapchain mode in `osgXR::Settings`, e.g. from a derived class of
`osgXR::Manager` you can do this:
```C++
_settings->allowSwapchainMode(osgXR::Settings::SWAPCHAIN_SINGLE);
```
This also prevents osgXR from defaulting to allowing both Multiple and Single
(side-by-side) swapchain modes for backwards compatibility when no other
swapchain modes are explicitly allowed.

### Layered

This swapchain mode uses a 2D array texture to store multiple frame buffers,
with each view being rendered to a viewport in a single layer of the texture (in
case they have different resolutions).

This is the required swapchain layout for the OVR multiview rendering mode, but
is also usable with Slave Cameras and Geometry Shaders.

Care must be taken to:
 - Transform texture coordinates into the appropriate viewport before sampling
   intermediate frame buffers as for Single (side-by-side) above.
 - Use a sampler2DArray when sampling these buffers when layered rendering is
   in use.
 - Provide the Z texture coordinate from application specific defines as
   provided by osgXR::View.

To indicate that layered swapchains are supported by the application and its
shaders, and to allow this swapchain mode to be chosen, set it as an allowed
swapchain mode in `osgXR::Settings`, e.g. from a derived class of
`osgXR::Manager` you can do this:
```C++
_settings->allowSwapchainMode(osgXR::Settings::SWAPCHAIN_LAYERED);
```
This also prevents osgXR from defaulting to allowing both Multiple and Single
(side-by-side) swapchain modes for backwards compatibility when no other
swapchain modes are explicitly allowed.

## Multiview Rendering Modes

There are 4 multiview rendering modes supported by osgXR:
 - Slave Cameras: Multi-pass rendering.
 - SceneView: OpenSceneGraph's stereo rendering.
 - Geometry Shaders: Single-pass multiview rendering.
 - OVR Multiview: Hardware accelerated single-pass multiview rendering.

### Slave Cameras

**Geometry amplification**: Multiple render passes\
**osgXR::View count**: 1 per OpenXR view\
**OpenXR swapchain layout**: Multiple, Single (side-by-side), or Layered\
**Intermediate buffer layout**: Always multiple

**Shader Changes**:
 - No shader changes are required. Each camera's view space matches the
   corresponding OpenXR view's eye space.

**Performance**:
 - Slowest CPU performance.
 - Due to the multiple render passes this puts more load on the CPU than the
   modes below.

To indicate that Slave Cameras mode is supported by the application and its
shaders, set it as an allowed VR mode in `osgXR::Settings`, e.g. from a derived
class of `osgXR::Manager` you can do this:
```C++
_settings->allowVRMode(osgXR::Settings::VRMODE_SLAVE_CAMERAS);
```
This also prevents osgXR from defaulting to allowing both Slave Camera and
SceneView modes for backwards compatibility when no other VR modes are
explicitly allowed.

### SceneView

**Geometry amplification**: OpenSceneGraph (`osgUtil::SceneView` handles
multiple passes)\
**osgXR::View count**: 1\
**OpenXR swapchain layout**: Single (side-by-side)\
**Intermediate buffer layout**: Same as swapchain - Single (side-by-side)

**Shader changes**:
 - Transform texture coordinates used for sampling intermediate buffers.

**Performance**:
 - Generally faster than slave cameras.
 - There are still multiple passes which puts more load on the CPU than the
   single-pass modes below.

To indicate that SceneView mode is supported by the application and its shaders,
and to allow it to be chosen, set it as an allowed VR mode in `osgXR::Settings`,
e.g. from a derived class of `osgXR::Manager` you can do this:
```C++
_settings->allowVRMode(osgXR::Settings::VRMODE_SCENE_VIEW);
```
This also prevents osgXR from defaulting to allowing both Slave Camera and
SceneView mode for backwards compatibilitys when no other VR modes are
explicitly allowed.

#### Shader definitions

Texture coordinates used for sampling intermediate frame buffers and
intermediate fixed-size buffers must be transformed into the appropriate half
of the intermediate frame buffer texture. When sampling in the fragment shader,
the transformation can be done either in the vertex shader (if the fragment
shader otherwise uses the texture coordinate directly for sampling), or the
fragment shader (if the texture coordinate already needs to be manipulated
before sampling), but not both.

 - `OSGXR_VERT_GLOBAL` (shading passes): Used by vertex shaders to define osgXR
   uniforms for use by `OSGXR_VERT_MVR_TEXCOORD` and `OSGXR_VERT_MVB_TEXCOORD`.
 - `OSGXR_VERT_MVR_TEXCOORD(UV)` (shading passes): Can be used by vertex
   shaders to transform frame buffer texture coordinates into the appropriate
   half of an intermediate frame buffer texture.
 - `OSGXR_VERT_MVB_TEXCOORD(UV)` (shading passes): Can be used by vertex
   shaders to transform fixed-size buffer texture coordinates into the
   appropriate half of an intermediate fixed-size buffer texture.
 - `OSGXR_FRAG_GLOBAL` (shading passes): Used by fragment shaders to define
   osgXR uniforms for use by `OSGXR_FRAG_MVR_TEXCOORD` and
   `OSGXR_FRAG_MVB_TEXCOORD`.
 - `OSGXR_FRAG_MVR_TEXCOORD(UV)` (shading passes): Can be used by fragment
   shaders to transform frame buffer texture coordinates into the appropriate
   half of an intermediate frame buffer texture.
 - `OSGXR_FRAG_MVB_TEXCOORD(UV)` (shading passes): Can be used by fragment
   shaders to transform fixed-size buffer texture coordinates into the
   appropriate half of an intermediate fixed-size buffer texture.

#### Vertex shader requirements

##### Transform intermediate frame buffer coordinates to viewport

Any texture coordinates used for sampling intermediate frame buffers should be
transformed into the appropriate half of that buffer (due to side-by-side frame
buffers).

This can be done in the vertex shader with `OSGXR_VERT_MVR_TEXCOORD`:

```glsl
// Vertex shader
...
#pragma import_defines (OSGXR_VERT_GLOBAL, OSGXR_VERT_MVR_TEXCOORD(UV))
...
#ifdef OSGXR_VERT_GLOBAL
OSGXR_VERT_GLOBAL
#endif
...
#ifdef OSGXR_VERT_MVR_TEXCOORD
    texcoord = OSGXR_VERT_MVR_TEXCOORD(texcoord);
#endif
```

A similar macro exists for fixed-size intermediate buffers
(`OSGXR_VERT_MVB_TEXCOORD`).

#### Fragment shader requirements

##### Transform intermediate frame buffer coordinates to viewport

An alternative macro to `OSGXR_VERT_MVR_TEXCOORD` may instead be used in
fragment shaders, such as if the coordinates need processing in the fragment
shader before sampling:

```glsl
// Fragment shader
...
#pragma import_defines (OSGXR_FRAG_GLOBAL, OSGXR_FRAG_MVR_TEXCOORD(UV))
...
#ifdef OSGXR_FRAG_GLOBAL
OSGXR_FRAG_GLOBAL
#endif
...
    // Per-fragment calculation of texcoord prevents transformation in vertex
    // or geometry stage before interpolation across primitives
    texcoord = foo(...);
#ifdef OSGXR_FRAG_MVR_TEXCOORD
    texcoord = OSGXR_FRAG_MVR_TEXCOORD(texcoord);
#endif
```

A similar macro exists for fixed-size intermediate buffers
(`OSGXR_FRAG_MVB_TEXCOORD`).

### Geometry Shaders

**Geometry amplification**: Geometry shader instancing (single pass)\
**osgXR::View count**: 1\
**OpenXR swapchain layout**: Single (side-by-side) or Layered\
**Intermediate buffer layout**: Same as swapchain - Single (side-by-side) or Layered

**Shader changes**:
 - Transform vertex shader `gl_Position` into shared view space.
 - Full use of interface blocks between vertex and fragment shaders.
 - Add geometry shaders to copy the vertex data to each view.
 - Transform view space vectors (e.g. normals) and view-space positions into
   per-view eye space in geometry shaders.
 - Move any view-specific calculations from vertex shader to geometry shader.
 - Transform texture coordinates used for sampling intermediate buffers.

**Performance**:
 - Better CPU performance than multiple pass modes above.
 - Reduced CPU overhead due to single pass.
 - Potentially worse GPU vertex processing performance than other modes due to
   the use of geometry shaders, however if you are CPU bound or GPU/fragment
   bound (common for high VR resolutions), its usually a net win.

**Caveats**:
 - Standard geometry shaders aren't designed to work with primitives larger than
   triangles (although NVIDIA drivers implementing `NV_geometry_shader4` may
   still support them), therefore any use of `GL_QUADS`, `GL_QUAD_STRIP`, or
   `GL_POLYGON` primitives must be converted to triangle based primitives to
   avoid `INVALID_OPERATION` errors from OpenGL draw calls.

To indicate that Geometry Shaders mode is supported by the application and its
shaders, and to allow it to be chosen, set it as an allowed VR mode in
`osgXR::Settings`, e.g. from a derived class of `osgXR::Manager` you can do
this:
```C++
_settings->allowVRMode(osgXR::Settings::VRMODE_GEOMETRY_SHADERS);
```
This also prevents osgXR from defaulting to allowing both Slave Camera and
SceneView modes for backwards compatibility when no other VR modes are
explicitly allowed.

#### Shader definitions

Texture coordinates used for sampling intermediate frame buffers and
intermediate fixed-size buffers must be transformed into the appropriate
viewport of the intermediate frame buffer texture. When sampling in the
fragment shader, the transformation can be done either in the geometry shader
(if the fragment shader otherwise uses the texture coordinate directly for
sampling), or the fragment shader (if the texture coordinate already needs to
be manipulated before sampling), but not both.

 - `OSGXR_GEOM`: Indicates that geometry shaders should be used. Geometry
   shaders may be conditional upon this, and vertex shaders may use it to
   conditionalise outputs which depend on the specific view (calculating these
   outputs in the geometry shader instead).
 - `OSGXR_VERT_TRANSFORM(POS)` (MVR passes): Transform `POS` from model space
   to the shared view space, which should then be assigned to `gl_Position`.
   The vertex shader should fall back to normal behaviour when undefined, e.g.
   other modes and non-MVR passes. The `osg_ModelViewMatrix` uniform must be
   explicitly declared.
 - `OSGXR_GEOM_GLOBAL`: Used by geometry shaders to enable extensions, set up
   geometry instancing and define uniforms for use by other shader definitions.
 - `OSGXR_GEOM_PREPARE_VERTEX`: Used by geometry shaders to prepare a single
   vertex. This ensures the correct viewport (`gl_ViewportIndex`) and layer
   (`gl_Layer`) are used.
 - `OSGXR_GEOM_TRANSFORM(POS)` (MVR passes): Transform `POS` from shared view
   space to per-view projection space, which should then be assigned to
   `gl_Position`.
 - `OSGXR_GEOM_VIEW_MATRIX` (MVR passes): Provides a `mat4` to transform from
   the shared view space to the per-view eye space, which is used by geometry
   shaders to transform view space position vector outputs from the vertex
   shader.
 - `OSGXR_GEOM_NORMAL_MATRIX` (MVR passes): Provides a `mat3` to transform from
   the shared view space to the per-view eye space orientation, which is used
   by geometry shaders to transform relative vector and normal outputs from the
   vertex shader.
 - `OSGXR_GEOM_MVR_TEXCOORD(UV)` (Shading passes): Used by geometry shaders to
   transform texture coordinates for intermediate frame buffers into the
   appropriate viewport of the buffer (for side-by-side frame buffers and
   layered frame buffers with differing view resolutions).
 - `OSGXR_GEOM_MVB_TEXCOORD(UV)` (Shading passes & Single (side-by-side)): Used
   by geometry shaders to transform texture coordinates for intermediate
   fixed-size buffers into the appropriate viewport of the buffer (for
   side-by-side buffers).
 - `OSGXR_FRAG_GLOBAL` (shading passes): Used by fragment shaders to define
   osgXR uniforms for use by `OSGXR_FRAG_MVR_TEXCOORD` and
   `OSGXR_FRAG_MVB_TEXCOORD`.
 - `OSGXR_FRAG_MVR_TEXCOORD(UV)` (Shading passes): Used by fragment shaders to
   transform texture coordinates for intermediate frame buffers into the
   appropriate viewport of the buffer (for side-by-side frame buffers and
   layered frame buffers with differing view resolutions).
 - `OSGXR_FRAG_MVB_TEXCOORD(UV)` (Shading passes & Single (side-by-side)): Used
   by fragment shaders to transform texture coordinates for intermediate
   fixed-size buffers into the appropriate viewport of the buffer (for
   side-by-side buffers).

#### Vertex shader requirements

##### Interface blocks

In order for a geometry shader to be optionally inserted between the vertex and
fragment stages without modification of the variable names in the vertex &
fragment shaders, all vertex shader outputs (and fragment shader inputs) must be
contained in interface blocks. For example:
```glsl
// Vertex shader
out VS_OUT {
    vec2 texcoord;
    ...
};
...
    texcoord = foo();
...
```
```glsl
// Fragment shader
in VS_OUT {
    vec2 texcoord;
    ...
};
...
    ... = texcoord;
...
```

Where different data is used by different vertex and fragment shader files, it
may make sense to split the interface blocks accordingly.

##### `gl_Position` in shared view space

The vertex shader is invoked only once per osgXR view (which represents
multiple OpenXR views), so it should output a `gl_Position` in shared view
space to the geometry shader (instead of projected into eye space directly to
the fragment shader). Instead of multiplying by `osg_ModelViewProjectionMatrix`
as you normally might:

```glsl
// Vertex shader

uniform mat4 osg_ModelViewProjectionMatrix;

...
    gl_Position = osg_ModelViewProjectionMatrix * pos;
```

You can instead use the `OSGXR_VERT_TRANSFORM` macro, which does the right thing
for any of the multiview rendering modes supported by osgXR, or is left
undefined when the default behaviour of transforming into projection space is
required:

```glsl
// Vertex shader

#pragma import_defines (OSGXR_VERT_GLOBAL, OSGXR_VERT_TRANSFORM(POS))

// Declares the necessary uniforms etc.
#ifdef OSGXR_VERT_GLOBAL
OSGXR_VERT_GLOBAL
#endif

uniform mat4 osg_ModelViewProjectionMatrix;
// May be required by OSGXR_VERT_TRANSFORM or app code, so its up to the app
// code to declare it
uniform mat4 osg_ModelViewMatrix;

...
#ifdef OSGXR_VERT_TRANSFORM
    // Transform pos into the necessary space (shared view space for geometry
    // shaders, view projection space for OVR_multiview).
    gl_Position = OSGXR_VERT_TRANSFORM(pos);
#else
    // Otherwise: Transform pos into eye projection space
    gl_Position = osg_ModelViewProjectionMatrix * pos;
#endif
```

##### No outputs depending on eye space or exact view space

Since neither the projected eye space coordinates, nor the exact view space
coordinates will be available to the vertex shader, any vertex shader outputs
which depend on these should be handled by the geometry shader instead. The
vertex shader should omit such calculations/data when `OSGXR_GEOM` is defined:
```glsl
// Vertex shader

#pragma import_defines (OSGXR_GEOM)

...
out VS_OUT {
#ifndef OSGXR_GEOM
    float pos_dependent;
#endif
    ...
};
...
#ifndef OSGXR_GEOM
    pos_dependent = foo(gl_Position);
#endif
```

And instead perform them in the geometry shader. Note that it is unfortunately
awkward to share shader code between the vertex/fragment and optional geometry
stages, since any common shader file cannot depend on `OSGXR_GEOM` and so would
invoke the geometry stader stage even when not desired resulting in an
incomplete geometry shader.

#### Geometry shader requirements

In this mode all geometry should be rendered with an instanced geometry shader
active to amplify the geometry into each OpenXR view. A custom geometry shader
will be required for each different set of data to be transferred from the
vertex shader to the fragment shader.

##### Making the geometry stage optional

In order to avoid the overhead of geometry shading for non-VR and other
multiview rendering modes, you can make OpenSceneGraph skip the geometry shader
unless `OSGXR_GEOM` is defined by adding the following line near the top of the
shader:
```glsl
// Geometry shader

#pragma requires (OSGXR_GEOM)
```

##### Global setup

Various global boilerplate code is required to define uniforms, set the number
of geometry shader invocations, and declare certain extensions as required. This
is done using the `OSGXR_GEOM_GLOBAL` macro:
```glsl
// Geometry shader
...
#pragma import_defines (OSGXR_GEOM_GLOBAL)
...
OSGXR_GEOM_GLOBAL
...
```

##### Transform `gl_Position` into projected eye space & other preparations

Since the `gl_Position` output by the vertex shader is in shared view space, the
geometry shader must perform the final tranformation into projected eye space
(for all MVR passes). This is performed using the `OSGXR_GEOM_TRANSFORM` macro.

Additionally the `OSGXR_GEOM_PREPARE_VERTEX` macro performs a number of other
operations, such as:
 - Set `gl_Layer` to set the appropriate output layer depending on swapchain
   mode and invocation id.
 - Set `gl_ViewportIndex` to set the appropriate output viewport depending on
   swapchain mode and invocation id.

```glsl
// Geometry shader
...
#pragma import_defines (OSGXR_GEOM_TRANSFORM(POS), OSGXR_GEOM_PREPARE_VERTEX)
...
void main(void)
{
    for (int i = 0; i < gl_in.length(); ++i) {
        gl_Position = gl_in[i].gl_Position;

        // Project gl_Position into eye space
        // Note: If this code is also used for non-MVR (e.g. shading only)
        // passes, OSGXR_GEOM_TRANSFORM may not be needed or defined
        gl_Position = OSGXR_GEOM_TRANSFORM(gl_Position);

        // Set up destination layer/viewport
        OSGXR_GEOM_PREPARE_VERTEX;

        // Copy vertex data
        ...

        EmitVertex();
    }
    EndPrimitive();
}
```

##### Interface blocks

As mentioned above, the use of optional geometry shaders requires the use of
interface blocks for all vertex shader outputs. This allows the geometry shader
to refer to both inputs (from the vertex shader) and outputs (to the fragment
shader) by the same name, like so:
```glsl
// Geometry shader

// Inputs from the vertex shader
in VS_OUT {
    vec2 texcoord;
    ...
    // no pos_dependent, calculation delegated to geometry shader
} gs_in[];

// Outputs to the fragment shader
out VS_OUT {
    vec2 texcoord;
    ...
    float pos_dependent;
    ...
} gs_out;
...
    for (int i = 0; i < gl_in.length(); ++i) {
        ...
        // Copy vertex data
        gs_out.texcoord... = gs_in[i].texcoord;
        ...
        // Calculate eye space dependent data
        // normally calculated by vertex shader
        pos_dependent = foo(gl_Position);
        ...
        EmitVertex();
    }
...
```

##### Transform view space position/normal vectors into eye space

Any view space vectors (both position vectors and normal vectors) that need
passing to the fragment stage will need transforming into the per-view eye
space, as `osg_NormalMatrix` and `osg_ModelViewMatrix` only transform to the
shared view space. The `OSGXR_GEOM_VIEW_MATRIX` and `OSGXR_GEOM_NORMAL_MATRIX`
definitions can be used for this in MVR passes:

```glsl
// Geometry shader
...
#pragma import_defines (OSGXR_GEOM_GLOBAL)
#pragma import_defines (OSGXR_GEOM_NORMAL_MATRIX, OSGXR_GEOM_VIEW_MATRIX)
...
OSGXR_GEOM_GLOBAL
...
        // Copy vertex data
        gs_out.vertex_normal = OSGXR_GEOM_NORMAL_MATRIX * gs_in[i].vertex_normal;
        gs_out.view_vector = vec3(OSGXR_GEOM_VIEW_MATRIX * vec4(gs_in[i].view_vector, 1.0));
```

##### Transform intermediate frame buffer coordinates to viewport

Any texture coordinates used for sampling intermediate frame buffers should be
transformed into the appropriate viewport of that buffer (for side-by-side frame
buffers and layered frame buffers with differing view resolutions).

This can be done in the geometry shader with `OSGXR_GEOM_MVR_TEXCOORD`:

```glsl
// Geometry shader
...
#pragma import_defines (OSGXR_GEOM_GLOBAL, OSGXR_GEOM_MVR_TEXCOORD(UV))
...
OSGXR_GEOM_GLOBAL
...
        // Copy vertex data
        gs_out.texcoord = OSGXR_GEOM_MVR_TEXCOORD(gs_in[i].texcoord);
```

A similar macro exists for fixed-size intermediate buffers
(`OSGXR_GEOM_MVB_TEXCOORD`).

#### Fragment shader requirements

##### Transform intermediate frame buffer coordinates to viewport

An alternative macro to `OSGXR_GEOM_MVR_TEXCOORD` may instead be used in
fragment shaders, such as if the coordinates need processing in the fragment
shader before sampling:

```glsl
// Fragment shader
...
#pragma import_defines (OSGXR_FRAG_GLOBAL, OSGXR_FRAG_MVR_TEXCOORD(UV))
...
#ifdef OSGXR_FRAG_GLOBAL
OSGXR_FRAG_GLOBAL
#endif
...
    // Per-fragment calculation of texcoord prevents transformation in vertex
    // or geometry stage before interpolation across primitives
    texcoord = foo(...);
#ifdef OSGXR_FRAG_MVR_TEXCOORD
    texcoord = OSGXR_FRAG_MVR_TEXCOORD(texcoord);
#endif
```

A similar macro exists for fixed-size intermediate buffers
(`OSGXR_FRAG_MVB_TEXCOORD`).

### OVR Multiview

**Geometry amplification**: `GL_OVR_multiview2` instances vertex shader (single
pass)\
**osgXR::View count**: 1\
**OpenXR swapchain layout**: Layered\
**Intermediate buffer layout**: Same as swapchain - Layered

**Shader changes**:
 - Not as extensive as geometry shaders, but changes are required due to
   multiple views handled in a single pass, and vertex shaders need
   modification.
 - Transform vertex shader `gl_Position` into eye projection space.
 - Transform view space vectors (e.g. normals) and view-space positions into
   per-view eye space in vertex shaders.
 - Transform texture coordinates used for sampling intermediate buffers.

**Performance**:
 - Better CPU performance than multiple pass modes above.
 - Reduced overhead due to single pass.
 - Hardware accelerated multiview vertex processing avoids overhead of geometry
   shaders.

**Availability**:
 - Not as widely available as geometry shaders.
 - On Linux, currently only available for NVIDIA GPUs.

**Caveats**:
 - A [driver bug
   (NVIDIA)](https://forums.developer.nvidia.com/t/opengl-display-lists-dont-work-with-gl-ovr-multiview/294361)
   may prevent the use of display lists with the `GL_OVR_multiview` extension.
   - The environment variable `OSG_VERTEX_BUFFER_HINT=VBO` can be used to tell
     OpenSceneGraph to use vertex buffer objects instead of display lists.
   - The application can do the same thing with
     `osg::DisplaySettings::setVertexBufferHint(osg::DisplaySettings::VertexBufferHint::VERTEX_BUFFER_OBJECT)`,
     however comments in
     [OpenMW-VR](https://gitlab.com/madsbuvi/openmw/-/blob/openmw-vr/components/stereo/multiview.cpp#L122)
     suggest this must be done before `realize()` is called on the viewer, i.e.
     without access to a graphics context which would let us determine whether
     `GL_OVR_multiview2` is even supported.
 - `GL_OVR_multiview` explicitly does not support tessellation or geometry
   shaders during multiview rendering, therefore any use of these must be
   avoided to support this mode.

To indicate that OVR Multiview mode is supported by the application and its
shaders, and to allow it to be chosen, set it as an allowed VR mode in
`osgXR::Settings`, e.g. from a derived class of `osgXR::Manager` you can do
this:
```C++
_settings->allowVRMode(osgXR::Settings::VRMODE_OVR_MULTIVIEW);
```
This also prevents osgXR from defaulting to allowing both Slave Camera and
SceneView modes for backwards compatibility when no other VR modes are
explicitly allowed.

#### Shader definitions

 - `OSGXR_VERT_GLOBAL`: Used by vertex shaders to enable extensions, set up the
   number of views for instancing and define uniforms for use by other shader
   definitions.
 - `OSGXR_VERT_PREPARE_VERTEX`: Used by vertex shaders to set up the
   appropriate viewport (`gl_ViewportIndex`) to use.
 - `OSGXR_VERT_TRANSFORM(POS)` (MVR passes): Transform `POS` from model space
   to the per-view projection space, which should then be assigned to
   `gl_Position`. The vertex shader should fall back to normal behaviour when
   undefined, e.g. other modes and non-MVR passes. The `osg_ModelViewMatrix`
   uniform must be explicitly declared.
 - `OSGXR_VERT_VIEW_MATRIX` (MVR passes): Provides a `mat4` to transform from
   the shared view space to the eye space, which is used by vertex shaders to
   transform view space position vector outputs.
 - `OSGXR_VERT_NORMAL_MATRIX` (MVR passes): Provides a `mat3` to transform from
   the shared view space to the eye space orientation, which is used by vertex
   shaders to transform relative vector and normal outputs.
 - `OSGXR_VERT_MVR_TEXCOORD(UV)` (shading passes): Used by vertex shaders to
   transform texture coordinates for intermediate frame buffers into the
   appropriate viewport of the buffer (for differing view resolutions).
 - `OSGXR_FRAG_GLOBAL` (shading passes): Used by fragment shaders to define
   osgXR uniforms for use by `OSGXR_FRAG_MVR_TEXCOORD`.
 - `OSGXR_FRAG_MVR_TEXCOORD(UV)` (Shading passes): Used by fragment shaders to
   transform texture coordinates for intermediate frame buffers into the
   appropriate viewport of the buffer (for differing view resolutions).

Note that since only layered intermediate buffer layouts are supported by this
mode, fixed-size intermediate buffers do not technically need any special
texture coordinate transformation, however `OSGXR_VERT_MVB_TEXCOORD` and
`OSGXR_FRAG_MVB_TEXCOORD` may still be required to support other modes.

#### Vertex shader requirements

##### Global setup

Various global boilerplate code is required to define uniforms, set the number
of vertex shader multiview invocations, and declare certain extensions as
required. This is done using the `OSGXR_VERT_GLOBAL` macro:

```glsl
// Vertex shader
...
#pragma import_defines (OSGXR_VERT_GLOBAL)
...
#ifdef OSGXR_VERT_GLOBAL
OSGXR_VERT_GLOBAL
#endif
...
```

##### Transform vertex into eye projection space

The vertex shader is invoked for every OpenXR view, so it should output a
`gl_Position` transformed into the per-view projection space. Instead of
multiplying by `osg_ModelViewProjectionMatrix` as you normally might:

```glsl
// Vertex shader

uniform mat4 osg_ModelViewProjectionMatrix;

...
    gl_Position = osg_ModelViewProjectionMatrix * pos;
```

You can instead use the `OSGXR_VERT_TRANSFORM` macro, which does the right thing
for any of the multiview rendering modes supported by osgXR, or is left
undefined when the default behaviour of transforming into projection space is
required:

```glsl
// Vertex shader

#pragma import_defines (OSGXR_VERT_GLOBAL, OSGXR_VERT_TRANSFORM(POS))

// Declares the necessary uniforms etc.
#ifdef OSGXR_VERT_GLOBAL
OSGXR_VERT_GLOBAL
#endif

uniform mat4 osg_ModelViewProjectionMatrix;
// May be required by OSGXR_VERT_TRANSFORM or app code, so its up to the app
// code to declare it
uniform mat4 osg_ModelViewMatrix;

...
#ifdef OSGXR_VERT_TRANSFORM
    // Transform pos into the necessary space (shared view space for geometry
    // shaders, view projection space for OVR_multiview).
    gl_Position = OSGXR_VERT_TRANSFORM(pos);
#else
    // Otherwise: Transform pos into eye projection space
    gl_Position = osg_ModelViewProjectionMatrix * pos;
#endif
```

Note: For shaders which generate `gl_Position` procedurally and identically
between views (e.g. full screen quads for shading), a transform may not be
required.

##### Other preparations

`OSGXR_VERT_PREPARE_VERTEX` should always be used to set the appropriate
viewport (`gl_ViewportIndex`) to use:

```glsl
// Vertex shader

#pragma import_defines (OSGXR_VERT_GLOBAL, OSGXR_VERT_PREPARE_VERTEX)

...
#ifdef OSGXR_VERT_PREPARE_VERTEX
    OSGXR_VERT_PREPARE_VERTEX;
#endif
```

##### Transform view space position/normal vectors into eye space

Any view space vectors (both position vectors and normal vectors) that need
passing to the fragment stage will need transforming into the per-view eye
space, as `osg_NormalMatrix` and `osg_ModelViewMatrix` only transform to the
shared view space. The `OSGXR_VERT_VIEW_MATRIX` and `OSGXR_VERT_NORMAL_MATRIX`
definitions can be used for this:

```glsl
// Vertex shader
...
#pragma import_defines (OSGXR_VERT_GLOBAL)
#pragma import_defines (OSGXR_VERT_NORMAL_MATRIX, OSGXR_VERT_VIEW_MATRIX)
...
#idef OSGXR_VERT_GLOBAL
OSGXR_VERT_GLOBAL
#endif
...
    vertex_normal = osg_NormalMatrix * N;
#ifdef OSGXR_VERT_NORMAL_MATRIX
    vertex_normal = OSGXR_VERT_NORMAL_MATRIX * vertex_normal;
#endif
    ...
    view_vector = osg_ModelViewMatrix * vec4(position, 1.0)).xyz;
#ifdef OSGXR_VERT_VIEW_MATRIX
    view_vector = vec3(OSGXR_VERT_VIEW_MATRIX * vec4(view_vector, 1.0));
#endif
```

##### Transform intermediate frame buffer coordinates to viewport

Any texture coordinates used for sampling intermediate frame buffers should be
transformed into the appropriate viewport of that buffer (for frame buffers
with differing view resolutions).

This can be done in the vertex shader with `OSGXR_VERT_MVR_TEXCOORD`:

```glsl
// Vertex shader
...
#pragma import_defines (OSGXR_VERT_GLOBAL, OSGXR_VERT_MVR_TEXCOORD(UV))
...
#ifdef OSGXR_VERT_GLOBAL
OSGXR_VERT_GLOBAL
#endif
...
    texcoord = multitexcoord0.st;
#ifdef OSGXR_VERT_MVR_TEXCOORD
    texcoord = OSGXR_VERT_MVR_TEXCOORD(texcoord);
#endif
```

Note that since only layered intermediate buffer layouts are supported by this
mode, fixed-size intermediate buffers do not technically need any special
texture coordinate transformation, however `OSGXR_VERT_MVB_TEXCOORD` may still
be required to support other modes (SceneView).

#### Fragment shader requirements

##### Transform intermediate frame buffer coordinates to viewport

An alternative macro to `OSGXR_VERT_MVR_TEXCOORD` may instead be used in
fragment shaders, such as if the coordinates need processing in the fragment
shader before sampling:
```glsl
// Fragment shader
...
#pragma import_defines (OSGXR_FRAG_GLOBAL, OSGXR_FRAG_MVR_TEXCOORD(UV))
...
#ifdef OSGXR_FRAG_GLOBAL
OSGXR_FRAG_GLOBAL
#endif
...
    // Per-fragment calculation of texcoord prevents transformation in vertex
    // or geometry stage before interpolation across primitives
    texcoord = foo(...);
#ifdef OSGXR_FRAG_MVR_TEXCOORD
    texcoord = OSGXR_FRAG_MVR_TEXCOORD(texcoord);
#endif
```

Note that since only layered intermediate buffer layouts are supported by this
mode, fixed-size intermediate buffers do not technically need any special
texture coordinate transformation, however `OSGXR_FRAG_MVB_TEXCOORD` may still
be required to support other modes (SceneView & Geometry Shaders).
