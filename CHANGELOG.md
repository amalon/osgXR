Version 0.3.2
-------------

Bug fixes:
* Fix a couple of bugs around session recreation (used when VR or swapchain mode
  changes).

Behaviour changes:
* Pick depth buffer format based on GraphicsContext traits depth bits.
* Enable depth info submission at session state to allow it to be dynamically
  switched without hitting a SteamVR hang during instance destruction.

Behind the scenes:
* Fix a few inconsequential compiler warnings with -Wall.
* Minor cosmetic cleanups (whitespace, explicit include).

Version 0.3.1
-------------

Behind the scenes:
* Fix possible crash in XRState::isRunning() if session is delayed coming up.

Version 0.3.0
-------------

API changes:
* Manager::checkAndResetStateChanged() - to detect when VR state may have
  changed, requiring app caches to be invalidated or synchronised with the new
  state.

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
