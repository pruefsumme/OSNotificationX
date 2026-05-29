#!/usr/bin/env sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUT_DIR="$ROOT/preview/out"
OUT_FILE="${OUT_FILE:-$OUT_DIR/osnotificationx-preview.png}"
WIDTH="${WIDTH:-430}"
HEIGHT="${HEIGHT:-900}"

mkdir -p "$OUT_DIR"

if command -v chromium >/dev/null 2>&1; then
  BROWSER=chromium
elif command -v chromium-browser >/dev/null 2>&1; then
  BROWSER=chromium-browser
elif command -v google-chrome >/dev/null 2>&1; then
  BROWSER=google-chrome
else
  echo "chromium, chromium-browser, or google-chrome is required to render the preview image." >&2
  exit 1
fi

"$BROWSER" \
  --headless \
  --no-sandbox \
  --disable-gpu \
  --disable-dev-shm-usage \
  --hide-scrollbars \
  --window-size="$WIDTH,$HEIGHT" \
  --screenshot="$OUT_FILE" \
  "file://$ROOT/preview/index.html" >/dev/null 2>&1

echo "$OUT_FILE"
