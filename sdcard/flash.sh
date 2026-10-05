#!/bin/sh
# Rebuild and push just the kernel to the Pi SD card, then eject.
# Usage: sh sdcard/flash.sh        (run from the repo root)
set -e
CARD=/Volumes/ALPINE

make
cp kernel8.img "$CARD/kernel8.img"
sync
echo "kernel8.img flashed to $CARD"
diskutil eject "$CARD" && echo "ejected -> pull the card and power-cycle the Pi"
