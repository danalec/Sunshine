# Sunshine — CachyOS patches

Fork of [LizardByte/Sunshine](https://github.com/LizardByte/Sunshine) carrying the
patches used by the `cachyos-rebuilds` PKGBUILD (package `sunshine 2026.818.1-2`).

**Forgejo is the primary source; GitHub is a mirror copy.**

## Branches

- `master` — upstream LizardByte/Sunshine (commit `6ad5f973`) + documentation
- `cachyos` — `master` with the patches below applied to the code

## Patches

### `patches/abs-mouse-relative.patch`

Converts absolute mouse input to relative deltas before they reach the compositor.

**Why:** Moonlight TV clients send absolute mouse positions
(`MOUSE_MOVE_ABS_MAGIC`). COSMIC's magnifier does not track the cursor on
absolute motion (`PointerMotionAbsolute` never calls `update_focal_point` —
pop-os/cosmic-comp #2760; same class as #1418/VirtualBox). Emitting relative
deltas makes the magnifier follow the cursor.

**How:** the first absolute event anchors the position (placed absolutely);
subsequent events are converted to relative deltas.

**v2 (2026-08-25) — phantom walls fix:** the client clamps its coordinates to
its own surface while the host cursor is clamped to the desktop, so once the
client saturates an axis it keeps sending the same value with the cursor still
mid-screen — movement stops at invisible "walls" until you push back the other
way. v2 keeps an estimate of the host cursor (anchor + sum of sent deltas,
clamped to the touch port like the compositor does) and, when the client
saturates an axis, slides the cursor to the matching edge with relative motion
(idempotent). Closes the loop without reading the host cursor position.

**v3–v6 (2026-08-25):** v2 still left walls because saturation was measured on
mapped touch-port values. v3 closed the loop on the REAL cursor position (the
KMS capture now publishes the cursor-plane CRTC_X/Y + output extents every
frame via platf::kms_cursor_* atomics) at full strength — the cursor jumped,
since frame feedback lags behind in-flight moves. v4's partial (deadband +
gain) corrections during flight added distance proportional to speed (felt
like pointer acceleration). v5 used feedback only while the client was idle,
but its one-shot wall fix computed a zero slide when the estimate was already
pegged at the edge while the real cursor sat mid-screen. **v6** is the final
shape: dead reckoning is the only thing in the smooth motion path (1:1); the
estimate snaps to the real cursor while the client is idle (>150 ms without a
raw coordinate change); a raw-saturated axis (client coordinate within 8 px of
its own surface edge) simply TARGETS the matching host edge — the primary
loop itself drives the cursor there, continuously and idempotently — and on a
saturated axis fresh cursor feedback is authoritative even during motion (lag
only overshoots into the edge, where the compositor clamps). Env
SUNSHINE_DEBUG_ABSMOUSE=1 enables rate-limited diagnostics
(raw/sat/target/est/real).

## Build (Arch/CachyOS)

The PKGBUILD (static FFmpeg 9 + VAAPI build recipe + setcap via `.install`)
lives in
[`danalec/cachyos-rebuilds` → `sunshine/`](http://192.168.0.10:3000/danalec/cachyos-rebuilds).
The package is published on the `[custom]` pacman repo (`http://192.168.0.3/`).

### `patches/copy-framebuffer-reuse.patch`

Reuses the persistent copy framebuffer in the EGL conversion pipeline instead of
creating/destroying one per frame (avoids `glGenFramebuffers`/`glDeleteFramebuffers`
per frame on the cropped-stream path).

### `patches/kms-prop-cache.patch`

Caches DRM property IDs once at display init (HDR_OUTPUT_METADATA + the 8 cursor
plane properties) and reads only the current values per frame. Removes ~30-40
`drmModeGetProperty` ioctls per captured frame from the KMS capture path.

### `tools/set-accel-flat/`

**Accel-drift fix (2026-08-20):** the abs→rel mouse conversion feeds the
compositor's pointer-acceleration curve, so the touchpad cursor drifts from the
finger over time (it can end up stuck on the top edge, near the COSMIC bar,
moving only horizontally). Setting the **FLAT (1:1) libinput accel profile** on
the Sunshine virtual mouse removes the drift.

- `set-accel-flat.c` — libinput watcher that applies `ACCEL_PROFILE_FLAT` to
  the `libvirtualhid Mouse` device whenever it appears (compile: `cc -O2 -o
  set-accel-flat set-accel-flat.c -linput -ludev`).
- `set-accel-flat.service` — systemd user unit (install to
  `~/.config/systemd/user/`, `systemctl --user enable --now`).
