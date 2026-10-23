# Desktop graphics performance

A screen recording of the graphical desktop running on **real Raspberry Pi 3B
hardware** (caches off), captured with a phone/camera pointed at the monitor.

**Videos (Google Drive):**

- [All four windows running](https://drive.google.com/file/d/126QG7_SoZEDXUpqwDwMqm8nfJbKwmFOf/view?usp=drive_link) — `term`, `basic`, `clock`, and `pong` composited together.
- [Two windows running](https://drive.google.com/file/d/1KU-MFdu487W6fIaWszLPZGokqHqxEyoU/view?usp=drive_link) — `term` and `basic` are frozen/not actively being redrawn.

The videos are shared on Google Drive with the instructors (or anyone at Brown).

## What the recording shows

- Four windowed user processes composited onto a double-buffered framebuffer:
  `term` and `basic` (text consoles), `clock`, and `pong`.
- Quest 5 damage detection: the compositor only re-copies a window whose canvas
  `changed`. Once `term` and `basic` draw their startup prompts and block on
  input, their windows stop being copied, so the per-refresh copy load drops to
  just the animating windows (`clock`, `pong`) and the refresh rate climbs.
- The two-window recording makes the same point from the other direction: with
  fewer windows to copy each refresh, the desktop runs visibly faster.
