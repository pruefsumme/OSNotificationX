#!/usr/bin/env sh
set -eu

COUNT="${COUNT:-18}"
DELAY="${DELAY:-0.12}"

if ! command -v notify-send >/dev/null 2>&1; then
  echo "notify-send is required but was not found in PATH." >&2
  exit 1
fi

i=1
while [ "$i" -le "$COUNT" ]; do
  case $((i % 5)) in
    0)
      app="Calendar"
      title="Team Sync"
      body="Design review starts in $((10 + i)) minutes."
      urgency="normal"
      ;;
    1)
      app="Backup"
      title="Backup Complete"
      body="Your files have been successfully backed up to the server."
      urgency="normal"
      ;;
    2)
      app="Mail"
      title="New Message from Danielle Durr"
      body="Vacation notes and follow-up details are ready to review."
      urgency="low"
      ;;
    3)
      app="Messages"
      title="Gabe Glick"
      body="Email sent."
      urgency="normal"
      ;;
    *)
      app="System"
      title="Battery Time Remaining"
      body="$((40 + i)) minutes left ($((70 - i % 20))%)."
      urgency="critical"
      ;;
  esac

  notify-send -a "$app" -u "$urgency" "$title" "$body"
  i=$((i + 1))
  sleep "$DELAY"
done

echo "Sent $COUNT test notifications."
