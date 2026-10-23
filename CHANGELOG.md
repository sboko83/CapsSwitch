# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
[Semantic Versioning](https://semver.org/).

## [Unreleased]

## [1.1.1] - 2026-10-23

### Fixed

- The elevated sign-in option now starts the app when it is installed for all users in Program
  Files, runs on battery power, and keeps the app running past 72 hours at normal priority.
  Rerun the setup with the option kept to repair an existing installation.

## [1.1.0] - 2026-10-22

### Added

- "Restart as administrator" in the tray menu: relaunches the app elevated, so Caps Lock keeps
  switching layouts in windows that run as administrator.
- "Start it elevated" sign-in option in the setup: a Task Scheduler job with highest rights
  starts the app silently at every sign-in (a Run entry cannot elevate).

## [1.0.0] - 2026-10-03

### Added

- Switch the Windows keyboard layout with Caps Lock, once per press.
- Keep normal Caps Lock available with Shift + Caps Lock.
- Pause, resume, and exit from the tray, with English and Russian menus.
- Run a single portable instance on Windows 10/11, with x64 and ARM64 packages.
- Stop the running app with `--stop` before replacing its files.
- Install per user or for all users with a setup that can start the app at sign-in.
