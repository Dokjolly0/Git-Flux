.pragma library

// Shared state for SmoothWheelScroller instances (one per QML engine).
//
// A touchpad gesture must keep driving the same Flickable even when the item
// under the pointer changes mid-gesture (e.g. a list scrolls a delegate that
// contains its own horizontal Flickable under the fingers). The scroller that
// owns the running gesture is therefore tracked here, together with the
// scroller attached to each Flickable.

var owners = new Map();   // Flickable -> SmoothWheelScroller attached to it
var active = null;        // scroller owning the gesture in progress

function register(flickable, scroller) {
    if (flickable) {
        owners.set(flickable, scroller);
    }
}

function unregister(flickable, scroller) {
    if (flickable && owners.get(flickable) === scroller) {
        owners.delete(flickable);
    }
    if (active === scroller) {
        active = null;
    }
}

function ownerOf(flickable) {
    return owners.get(flickable) || null;
}
