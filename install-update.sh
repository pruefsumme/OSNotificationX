#!/usr/bin/env sh
set -eu

PREFIX="${PREFIX:-/usr}"
BUILD_DIR="${BUILD_DIR:-build}"

if ! command -v meson >/dev/null 2>&1; then
  echo "meson is required but was not found in PATH." >&2
  exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
  echo "ninja is required but was not found in PATH." >&2
  exit 1
fi

if [ -d "$BUILD_DIR" ] && [ ! -w "$BUILD_DIR" ]; then
  echo "$BUILD_DIR exists but is not writable. Re-run with a writable BUILD_DIR or fix its ownership." >&2
  exit 1
fi

if [ -d "$BUILD_DIR" ] && [ -f "$BUILD_DIR/meson-logs/install-log.txt" ] && [ ! -w "$BUILD_DIR/meson-logs/install-log.txt" ]; then
  echo "$BUILD_DIR/meson-logs/install-log.txt is not writable." >&2
  echo "Fix it with: sudo chown -R \"$(id -un):$(id -gn)\" \"$BUILD_DIR\"" >&2
  exit 1
fi

if [ -d "$BUILD_DIR" ]; then
  meson setup "$BUILD_DIR" --prefix="$PREFIX" --wipe
else
  meson setup "$BUILD_DIR" --prefix="$PREFIX"
fi

meson compile -C "$BUILD_DIR"
meson test -C "$BUILD_DIR" --print-errorlogs

if [ "$(id -u)" -eq 0 ]; then
  meson install -C "$BUILD_DIR"
else
  sudo meson install -C "$BUILD_DIR"
fi

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  if [ "$(id -u)" -eq 0 ]; then
    gtk-update-icon-cache -f "$PREFIX/share/icons/hicolor" || true
  else
    sudo gtk-update-icon-cache -f "$PREFIX/share/icons/hicolor" || true
  fi
fi

if command -v xfce4-panel >/dev/null 2>&1; then
  xfce4-panel --restart
else
  echo "xfce4-panel was not found; restart the XFCE panel manually after installation." >&2
fi

echo "OSNotificationX installed/updated. Add it from the XFCE panel's Add New Items dialog if needed."
