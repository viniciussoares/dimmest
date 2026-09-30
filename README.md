# dimmest

dim → dimmer → **dimmest**.

A tiny, no-frills screen dimmer for Windows 10/11 that lives in the system tray.
It's a spiritual successor to [dimmer](https://github.com/clangen/dimmer) by Casey Langen.

> **Status:** early work in progress. Nothing to download yet.

## Why a successor?

dimmer was great, but it's from 2017. It dims the screen by putting a
semi-transparent window on top of everything, and that approach has limits:

- the overlay sometimes disappears (monitor sleep, waking the PC, other "always on top" windows)
- popup menus, the taskbar and the Start menu are not dimmed unless you turn on a hack that polls 100 times a second
- color temperature uses gamma ramps, which Windows resets after sleep, UAC prompts and games

dimmest uses the Windows Magnification API's full-screen color effect instead.
The goal: dim **everything** (popups, taskbar, Start, notifications) with no
polling and no overlay window to lose. `spikes/magnifier-test` checks that on real hardware.

## Planned features

- [ ] Dim everything via the Magnification color effect
- [ ] Per-monitor overlay mode as a fallback (event-driven, no polling)
- [ ] Survives sleep, monitor power-off, lock/unlock and explorer restarts
- [ ] Color temperature from 1900K to 6500K
- [ ] Color tints: amber, red night mode, grayscale, custom color
- [ ] Monitor names in the menu ("LS27A600U" instead of "DISPLAY1")
- [ ] Start with Windows, global hotkeys
- [ ] Optional: real backlight control via DDC/CI

## Building

Requires Visual Studio 2022 (MSVC, x64). Build instructions will land with the first version.

## License

BSD 3-Clause, same as the original dimmer. See [LICENSE](LICENSE).
