osgXR API Documentation
=======================

The osgXR API is currently considered unstable, however it is versioned. Only
matching minor version numbers should be expected to be source and binary
compatible with one another.

The osgXR headers can be found in [include/osgXR/](../include/osgXR/).

The initial version of this library exposed ``<osgXR/OpenXRDisplay>`` for
configuring a view for VR, and ``<osgXR/osgXR>`` with a convenience wrapper
``osgXR::setupViewerDefaults`` to set up VR automatically based on environment
variables. This worked for most simple OpenSceneGraph examples, however for real
projects something more capable is needed, so it is likely these will be removed
in a future version (they are not currently working due to the new
``XRState::update()`` based state machine).

It is instead recommended to extend the ``osgXR::Manager`` class from
``<osgXR/Manager>`` and implement the callbacks.

## <[osgXR/Action](../include/osgXR/Action)>

This header provides the ``osgXR::Action`` base class, and the
``osgXR::ActionBoolean``, ``osgXR::ActionFloat``, ``osgXR::ActionVector2f``,
``osgXR::ActionPose`` and ``osgXR::ActionVibration`` classes which an
application can use to define OpenXR actions, read input state, and send haptic
output.

## <[osgXR/ActionSet](../include/osgXR/ActionSet)>

This header provides the ``osgXR::ActionSet`` class which an application uses
to group actions into groups which can be separately activated and deactivated.

## <[osgXR/CompositionLayer](../include/osgXR/CompositionLayer)>

This header provides the ``osgXR::CompositionLayer`` base class, from which
other composition layer classes are derived. It provides generic composition
layer capabilities such as visibility, ordering, and alpha blending modes.

## <[osgXR/CompositionLayerQuad](../include/osgXR/CompositionLayerQuad)>

This header provides the ``osgXR::CompositionLayerQuad`` class which an
application can use to control an OpenXR runtime composited quad in the VR
space.

## <[osgXR/InteractionProfile](../include/osgXR/InteractionProfile)>

This header provides the ``osgXR::InteractionProfile`` class which an
application uses to suggest bindings between OpenXR actions and the physical
interactions of a given OpenXR controller profile.

## <[osgXR/Manager](../include/osgXR/Manager)>

This header provides the ``osgXR::Manager`` class which an application can
extend to implement a VR manager class. Virtual callbacks tell the application
which camera views are required to implement VR, and an ``update()`` function
gives osgXR a chance to incrementally bring up or tear down VR.

## <[osgXR/Mirror](../include/osgXR/Mirror)>

This header provides the ``osgXR::Mirror`` class which an application can use to
register a desktop mirror of the VR view.

## <[osgXR/MirrorSettings](../include/osgXR/MirrorSettings)>

This header provides the ``osgXR::MirrorSettings`` class which encapsulates
configuration data for desktop mirrors of VR views.

## <[osgXR/OpenXRDisplay](../include/osgXR/OpenXRDisplay)>

This header provides the ``osgXR::OpenXRDisplay`` ViewConfig class. It is
largely replaced by ``osgXR::Manager``.

## <[osgXR/Settings](../include/osgXR/Settings)>

This header provides the ``osgXR::Settings`` class which encapsulates all the VR
configuration data.

## <[osgXR/Subaction](../include/osgXR/Subaction)>

This header provides the ``osgXR::Subaction`` class which an application can
use to represent OpenXR subaction paths (top level user paths), which allow a
single OpenXR action to represent the same action on both hands. It can be
passed to other action related classes to filter actions by hand, and it can be
extended by the application to implement a callback for InteractionProfile
changes.

## <[osgXR/SubImage](../include/osgXR/SubImage)>

This header provides the simple ``osgXR::SubImage`` class which wraps a
``osgXR::Swapchain`` smart pointer and describes a rectangular area within each
swapchain image. An application can optionally use this to specify what sub-part
of a swapchain image should be rendered by a CompositionLayer.

## <[osgXR/Swapchain](../include/osgXR/Swapchain)>

This header provides the ``osgXR::Swapchain`` class which represents a chain of
images which are passed to the OpenXR runtime. These can be attached to a camera
to allow it to be rendered into, and also attached to a mirror object to allow
the resulting texture (which switches every frame) to be used for further
rendering.

## <[osgXR/View](../include/osgXR/View)>

This header provides the ``osgXR::View`` class which represents a view of the
world which ``osgXR::Manager`` will request to be configured by the application.

## <[osgXR/osgXR](../include/osgXR/osgXR)>

This header provides convenience functions for quick and easy integration of VR
capabilities into a simple OpenSceneGraph application or example. It is
recommended ``osgXR::Manager`` be used instead for most projects.

### osgXR::setupViewerDefaults()

This sets up VR on the provided viewer based on the content of the following
environment variables:
 * ``OSGXR=1``                  enables VR.
 * ``OSGXR_MODE=SLAVE_CAMERAS`` forces the use of separate slave cameras per view.
 * ``OSGXR_MODE=SCENE_VIEW``    forces the use of OpenSceneGraph's SceneView stereo (default).
 * ``OSGXR_SWAPCHAIN=MULTIPLE`` forces the use of separate swapchains per view.
 * ``OSGXR_SWAPCHAIN=SINGLE``   forces the use of a single swapchain containing all views.
 * ``OSGXR_UNITS_PER_METER=10`` allows the scale of the environment to be controlled.
 * ``OSGXR_VALIDATION_LAYER=1`` enables the OpenXR validation layer (off by default).
 * ``OSGXR_DEPTH_INFO=1``       enables passing of depth information to OpenXR (off by default).
 * ``OSGXR_MIRROR=NONE``        use a blank screen as the default mirror.
 * ``OSGXR_MIRROR=LEFT``        use OpenXR view 0 (left) as the default mirror.
 * ``OSGXR_MIRROR=RIGHT``       use OpenXR view 1 (right) as the default mirror.
 * ``OSGXR_MIRROR=LEFT_RIGHT``  use both left and right views side by side as the default mirror.
