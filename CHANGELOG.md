Version 0.2.1
-------------

Behind the scenes:
* Make frame view location accessors thread safe so multiple cull threads can be
  used.


Version 0.2.0
-------------

API changes:
* Cleanups (moving dynamic bits out of Settings).
* Manager::update() - should be called regularly to allow for incremental
  bringup and OpenXR event handling.
* Manager::setEnabled() - for triggering bringing up/down of VR.

New/expanded APIs:
* Manager::destroyAndWait() and Manager::isDestroying() - for clean shutdown
  before program exit.
* Manager::syncSettings() - to trigger appropriate reinitialisation to handle
  any changed settings.
* Manager::getStateString() - to get a user readable description of the current
  VR state.
* Manager::onRunning() - virtual callback when VR has started (consider setting
  up desktop mirrors).
* Manager::onStopped() - virtual callback when VR has stopped (consider
  removing desktop mirrors).
* Manager::onFocus() - virtual callback when VR app is running in focus
  (consider resuming if paused).
* Manager::onUnfocus() - virtual callback when VR app has lost focus (consider
  pausing the experience).

Behind the scenes highlights:
* Make OpenXR bringup incremental, reversible and restartable.
* Improved handling of SteamVR's messing with GL context and threading.
* Separated event handling.

Version 0.1.0
-------------

This represents early development with the API still in heavy flux.

It supported:
* A Manager class with virtual callbacks for configuring views.
* Desktop mirrors of the VR views.
