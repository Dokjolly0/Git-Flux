import QtQuick
import "SmoothScrollRegistry.js" as Registry

// Smooth, kinetic touchpad scrolling for a Flickable / ListView / ScrollView.
//
// Why this exists: on Wayland (and X11 with libinput) touchpads deliver wheel
// events that carry a pixelDelta at the device's report rate (~90-120 Hz), with
// no kinetic momentum: the compositor leaves inertia to the client. Qt's
// Flickable applies each pixelDelta as an immediate jump, so on a high refresh
// display (e.g. 165 Hz) some frames move 10+ px and the next ones don't move at
// all, and the motion stops dead when the fingers lift. On Windows the same
// gestures reach Qt as animated "mouse wheel" deltas with OS-provided inertia,
// which is why scrolling feels much smoother there.
//
// This handler takes over touchpad wheel events only (mouse wheels keep Qt's
// own animated path): every delta is spread over the following frames, and when
// the gesture ends (Qt.ScrollEnd) the measured finger velocity keeps the view
// coasting with exponential friction. When the platform already provides
// momentum events (macOS) they are smoothed but no extra inertia is added.
//
// If the gesture's dominant axis can't be scrolled by this Flickable (e.g. a
// vertical swipe over a horizontal-only code block), the nearest enclosing
// Flickable that can scroll that axis is driven instead.
//
// Usage: declare it inside the scrollable (ListView, Flickable or ScrollView):
//     ListView { SmoothWheelScroller {} }
// or point it at a Flickable explicitly with `flickable: someFlickable`.
WheelHandler {
    id: scroller

    // The Flickable to drive. When left unset, the nearest Flickable enclosing
    // the declaration is used (children declared inside a Flickable land in its
    // contentItem, so the handler's parent is not the Flickable itself).
    property Item flickable: null

    // Time constant (s) used to spread each delta over the next frames.
    property real smoothingTime: 0.018
    // Friction time constant (s) of the post-gesture coasting.
    property real frictionTime: 0.325
    // Minimum release speed (px/s) that starts coasting.
    property real minFlingVelocity: 120
    // Upper bound for the coasting speed (px/s).
    property real maxFlingVelocity: 9000
    // Multiplier applied to the touchpad pixel deltas.
    property real speed: 1.0

    readonly property bool scrolling: frameDriver.running

    // --- internal state ---
    property Item _target: null       // Flickable driven by the current gesture
    property bool _vertical: true     // axis locked for the current gesture
    property real _pending: 0         // distance still to apply (px, content coords)
    property real _velocity: 0        // coasting speed (px/s)
    property bool _coasting: false
    property bool _gestureActive: false
    property bool _platformMomentum: false
    property real _lastWritten: NaN   // last position we wrote, to detect external moves
    property var _samples: []

    target: null
    acceptedDevices: PointerDevice.TouchPad
    // Leave Shift/Ctrl + wheel to the views' own handlers (horizontal scroll, zoom).
    acceptedModifiers: Qt.NoModifier

    property Item _registeredFlickable: null

    Component.onCompleted: {
        if (!flickable) {
            flickable = _findFlickable(parent);
        }
        _register();
    }
    onFlickableChanged: _register()
    Component.onDestruction: {
        stop();
        Registry.unregister(_registeredFlickable, scroller);
    }

    function _register() {
        if (_registeredFlickable === flickable) {
            return;
        }
        Registry.unregister(_registeredFlickable, scroller);
        _registeredFlickable = flickable;
        Registry.register(flickable, scroller);
    }

    function _isFlickable(item) {
        return item && item.contentY !== undefined && item.flickableDirection !== undefined;
    }

    function _findFlickable(item) {
        for (var it = item; it; it = it.parent) {
            if (_isFlickable(it)) {
                return it;
            }
        }
        return null;
    }

    function _canScroll(f, vertical) {
        if (!f.interactive || !f.visible) {
            return false;
        }
        if (vertical) {
            return f.flickableDirection !== Flickable.HorizontalFlick
                && f.contentHeight + f.topMargin + f.bottomMargin > f.height + 0.5;
        }
        return f.flickableDirection !== Flickable.VerticalFlick
            && (f.flickableDirection !== Flickable.AutoFlickDirection || f.contentWidth > f.width)
            && f.contentWidth + f.leftMargin + f.rightMargin > f.width + 0.5;
    }

    // Nearest Flickable, starting from ours, able to scroll the given axis.
    function _resolveTarget(vertical) {
        for (var f = flickable; f; f = _findFlickable(f.parent)) {
            if (_canScroll(f, vertical)) {
                return f;
            }
        }
        return null;
    }

    function _pos() { return _vertical ? _target.contentY : _target.contentX; }
    function _setPos(v) {
        if (_vertical) {
            _target.contentY = v;
        } else {
            _target.contentX = v;
        }
        _lastWritten = v;
    }
    function _min() {
        return _vertical ? _target.originY - _target.topMargin : _target.originX - _target.leftMargin;
    }
    function _max() {
        var hi = _vertical
            ? _target.originY + _target.contentHeight + _target.bottomMargin - _target.height
            : _target.originX + _target.contentWidth + _target.rightMargin - _target.width;
        return Math.max(_min(), hi);
    }

    function stop() {
        _pending = 0;
        _velocity = 0;
        _coasting = false;
        frameDriver.stop();
    }

    function _startGesture(target, vertical) {
        stop();
        _target = target;
        _vertical = vertical;
        _gestureActive = true;
        _platformMomentum = false;
        _samples = [];
        if (_target.flicking) {
            _target.cancelFlick();
        }
        _lastWritten = _pos();
    }

    function _feed(dx, dy, momentum) {
        if (momentum) {
            // The platform generates its own inertia (macOS): only smooth it.
            _platformMomentum = true;
        }
        // In content coordinates: swiping "up" (negative delta) scrolls down.
        var d = -(_vertical ? dy : dx) * speed;
        var pos = _pos();
        var dest = Math.max(_min(), Math.min(_max(), pos + _pending + d));
        _pending = dest - pos;
        _samples.push({ t: Date.now(), d: d });
        if (_samples.length > 32) {
            _samples.shift();
        }
        endTimer.restart();
        frameDriver.start();
    }

    function _endGesture() {
        _gestureActive = false;
        endTimer.stop();
        var samples = _samples;
        _samples = [];
        if (!_target || _platformMomentum || samples.length < 2) {
            return;
        }
        // Fingers rested before lifting: no fling.
        var now = Date.now();
        var last = samples[samples.length - 1].t;
        if (now - last > 50) {
            return;
        }
        // Release velocity: distance travelled over the last ~100 ms of samples.
        var sum = 0;
        var first = last;
        for (var i = samples.length - 1; i >= 0 && last - samples[i].t <= 100; --i) {
            sum += samples[i].d;
            first = samples[i].t;
        }
        var span = Math.max(16, now - first) / 1000.0;
        var v = Math.max(-maxFlingVelocity, Math.min(maxFlingVelocity, sum / span));
        if (Math.abs(v) < minFlingVelocity) {
            return;
        }
        _velocity = v;
        _coasting = true;
        frameDriver.start();
    }

    onWheel: (event) => {
        if (!flickable) {
            event.accepted = false;
            return;
        }
        var active = Registry.active;
        if (event.phase === Qt.ScrollEnd) {
            if (active) {
                active._endGesture();
            }
            event.accepted = false;
            return;
        }
        if (event.phase === Qt.ScrollBegin) {
            // Fingers back on the touchpad: stop any coasting view.
            if (active) {
                active._gestureActive = false;
                active.stop();
            }
            Registry.active = null;
            event.accepted = false;
            return;
        }

        var dx = event.pixelDelta.x;
        var dy = event.pixelDelta.y;
        if (dx === 0 && dy === 0) {
            // Touchpad without pixel deltas: 120 angle units ~ one 50 px step.
            dx = event.angleDelta.x / 120 * 50;
            dy = event.angleDelta.y / 120 * 50;
        }
        // Note: event.inverted ("natural scrolling") is informational only: the
        // deltas already point the way the content should move, exactly as
        // Flickable consumes them. Flipping them again reverses the scroll.

        // A gesture in progress keeps its Flickable, whatever is under the
        // pointer now. ScrollBegin isn't always delivered to handlers, and some
        // platforms (X11) send no phases at all, so the first update of a
        // gesture also starts it.
        var owner = (active && active._gestureActive) ? active : null;
        if (!owner) {
            if (dx === 0 && dy === 0) {
                event.accepted = false;
                return;
            }
            var vertical = Math.abs(dy) >= Math.abs(dx);
            var target = _resolveTarget(vertical);
            if (!target) {
                // Nothing here scrolls that way: let it propagate.
                event.accepted = false;
                return;
            }
            owner = Registry.ownerOf(target) || scroller;
            if (active && active !== owner) {
                active.stop();
            }
            owner._startGesture(target, vertical);
            Registry.active = owner;
        }
        owner._feed(dx, dy, event.phase === Qt.ScrollMomentum);
        event.accepted = true;
    }

    // Platforms without ScrollEnd: close the gesture after a short silence
    // (no fling, since the fingers' release time is unknown).
    property Timer _endTimer: Timer {
        id: endTimer
        interval: 120
        onTriggered: {
            if (scroller._gestureActive) {
                scroller._samples = [];
                scroller._endGesture();
            }
        }
    }

    // Spread the pending distance and integrate the coasting velocity once per frame.
    property FrameAnimation _frameDriver: FrameAnimation {
        id: frameDriver
        onTriggered: {
            var f = scroller._target;
            if (!f) {
                scroller.stop();
                return;
            }
            var pos = scroller._pos();
            // Someone else moved the view (drag, scrollbar, keyboard,
            // positionViewAtIndex, a real flick): yield to them.
            if (f.dragging || f.flicking || Math.abs(pos - scroller._lastWritten) > 0.5) {
                scroller.stop();
                return;
            }

            var dt = Math.min(0.05, Math.max(0.001, frameTime));
            if (scroller._coasting) {
                scroller._pending += scroller._velocity * dt;
                scroller._velocity *= Math.exp(-dt / scroller.frictionTime);
                if (Math.abs(scroller._velocity) < 15) {
                    scroller._coasting = false;
                    scroller._velocity = 0;
                }
            }

            var alpha = 1 - Math.exp(-dt / scroller.smoothingTime);
            var step = Math.abs(scroller._pending) < 0.5 ? scroller._pending : scroller._pending * alpha;
            var lo = scroller._min();
            var hi = scroller._max();
            var next = pos + step;
            if (next <= lo || next >= hi) {
                next = Math.max(lo, Math.min(hi, next));
                scroller._pending = 0;
                scroller._velocity = 0;
                scroller._coasting = false;
            } else {
                scroller._pending -= step;
            }
            if (next !== pos) {
                scroller._setPos(next);
            }
            if (!scroller._coasting && scroller._pending === 0) {
                scroller.stop();
            }
        }
    }
}
