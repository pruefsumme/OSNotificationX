# OSNotificationX

OSNotificationX is a native XFCE panel applet that presents the existing `xfce4-notifyd` notification log in an old OS X Notification Center style drawer.

## Build

```sh
meson setup build --prefix=/usr
meson compile -C build
meson test -C build
```

## Install

```sh
meson install -C build
```

For the normal update/install flow on XFCE, use:

```sh
./install-update.sh
```

Optional overrides:

```sh
PREFIX=/usr/local BUILD_DIR=build-local ./install-update.sh
```

After installation, add **OSNotificationX** from the XFCE panel's “Add New Items” dialog. The plugin installs:

- `libosnotificationx.so` into the XFCE panel plugin directory.
- `osnotificationx.desktop` into the XFCE panel plugin descriptor directory.
- `cloth_bg.png` and `applet_icon.png` into the package data directory.
- `osnotificationx.png` into the hicolor app icon theme.

The applet reads notifications from `org.xfce.Notifyd.Log` and uses Xfconf’s `xfce4-notifyd:/do-not-disturb` setting for the top alerts switch.

Group clear buttons hide notifications from the open center without deleting rows from XFCE notifyd's log database. Clicking a notification attempts to open its source app through the matching `.desktop` entry; whether an existing window is raised depends on that app and the window manager.

## Notification Test

Send a batch of sample notifications with:

```sh
./scripts/send-test-notifications.sh
```

Optional knobs:

```sh
COUNT=24 DELAY=0.08 ./scripts/send-test-notifications.sh
```

Check whether XFCE notifyd is logging notifications:

```sh
./scripts/check-notification-log.sh
```

## Visual Preview

Open `preview/index.html` in a browser or run `./preview/render-preview.sh` to export a PNG preview with example notifications before reinstalling the panel plugin.

If you install to `/usr/local`, restart the panel after installation so XFCE reloads panel plugin descriptors:

```sh
xfce4-panel --restart
```
