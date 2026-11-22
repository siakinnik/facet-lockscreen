# lockscreen — Facet module

A lock screen for [Facet](https://github.com/siakinnik/facet-core): a PIN, a
password or a pattern. Set it up in Settings > Modules > Screen lock.

## How it works

- Facet locks the device at start and whenever the screen goes off, and shows
  nothing but this module's lock screen until the module says unlock. Home,
  Back and the other buttons, banners, the shade and other modules' screens
  are out of reach while locked; incoming calls and the power menu still work.
- If this module crashes or is restarting, the device stays locked (Facet
  shows "Locked" and a button to restart it).
- The PIN, password or pattern is checked here and never leaves this process.
  Only a salted PBKDF2-HMAC-SHA256 hash (20 000 rounds) is stored, in
  `lock.json` in the module's data folder, which only the module's own user
  can read.
- After 5 wrong attempts each next one waits: 30 s, then twice as long every
  time, at most an hour. The count survives a restart of the module.
- Changing or removing the lock asks for the current one first.

Forgot it? With administrator access (SSH), delete `lock.json` in the module's
data folder (`/var/lib/facet/data/lockscreen/` on a standard install) and
restart Facet.

## Requirements

Facet 0.9 or newer (`system.lock`). Permissions: `system.lock` (special) and
`background`.

## Install

```bash
curl -fsSL https://raw.githubusercontent.com/siakinnik/facet-core/main/scripts/get.sh | sudo bash -s -- --plugin siakinnik/facet-lockscreen
```

## Build

```bash
cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build
FACET_PLUGIN_PATH=$PWD/build facet   # the build directory is a plugin directory
```

The SDK comes from `../facet-core` when it is checked out next to this
repository, otherwise from facet-core's `FACET_CORE_REF` tag.

## Licence

GPL-3.0, see [LICENSE](LICENSE).
