#!/usr/bin/env sh
set -eu

DB="${XDG_CACHE_HOME:-$HOME/.cache}/xfce4/notifyd/log.sqlite"
LIMIT="${LIMIT:-10}"

if ! command -v sqlite3 >/dev/null 2>&1; then
  echo "sqlite3 is required but was not found in PATH." >&2
  exit 1
fi

if [ ! -f "$DB" ]; then
  echo "No XFCE notifyd log DB found at: $DB" >&2
  exit 1
fi

echo "DB: $DB"
echo
sqlite3 "$DB" "SELECT COUNT(*) || ' notifications logged' FROM notifications;"
echo
sqlite3 "$DB" \
  "SELECT datetime(timestamp / 1000000, 'unixepoch', 'localtime') || ' | ' ||
          COALESCE(NULLIF(app_name, ''), NULLIF(app_id, ''), 'Unknown') || ' | ' ||
          COALESCE(summary, '') || ' | ' ||
          COALESCE(body, '')
   FROM notifications
   ORDER BY timestamp DESC
   LIMIT $LIMIT;"
