# SystemApp

A native, dependency-free C++20 CLI for Android system administration — root
detection, partition/mount inspection, property listing, and basic file
inspection — built as a git-style tool (`systemapp <command> [flags]`) with
a plugin-style command registry so new subcommands are one new `.cpp` file.

No Java. No Android Studio project. Builds as a standalone ARM64 binary via
the NDK, a Termux `.deb`, or a flashable Magisk module zip.

## ⚠️ Status: early foundation, not the full spec

This repo currently implements the **architecture and a first slice of
commands** — enough to build, run, and extend. It does **not** yet implement
the full scope of a BusyBox/Magisk-class toolkit (native APK installer,
ZIP library, SELinux context management, AVB/dm-verity control, A/B slot
switching, debloat database, backup/restore, ELF/APK-signature inspection,
interactive shell). Those are large subsystems in their own right and are
listed below as a concrete roadmap, not vague hand-waving — each maps to a
specific `src/<module>/` directory that already exists and is empty on
purpose, ready for the corresponding command file(s).

Building all of that to a "never brick the device" standard needs real
device testing against actual A/B / dynamic-partition / vendor-blob
combinations (like the beryllium Android-16-framework-on-Android-10-vendor
setup) — not something that can be responsibly claimed as finished from a
single offline build pass. Treat this as the scaffold to build the rest
onto incrementally, testing each command on-device before trusting it with
`remount rw` or a system-partition write.

## What's implemented

| Command  | Does |
|----------|------|
| `info`   | Device/build props, root provider, partition summary (text or `--json`) |
| `root`   | Detects su / Magisk / KernelSU / APatch; exit code doubles as a scriptable "has root" check |
| `mounts` | Parses `/proc/mounts`, reports system/vendor/product/odm/etc. mount state, fs type, ro/rw, overlay |
| `props`  | Lists/filters system properties (wraps `getprop`; native trie reader is on the roadmap) |
| `ls`     | POSIX `dirent`/`stat`-based directory listing, no shelling out |

All support `--json`. Global flags: `-h/--help`, `-v/--version`, `--json`,
`--verbose`, `--force`, `--dry-run`, `--no-reboot` (the last three are wired
into `CommandContext` and ready for write-operations to consume as they're
built).

## Architecture

```
include/systemapp/
  command.hpp       ICommand interface + CommandRegistry (plugin architecture)
  root_detector.hpp  su/Magisk/KernelSU/APatch detection
  partition_detector.hpp   /proc/mounts-based partition enumeration
  json.hpp           dependency-free JSON value builder for --json output
  logger.hpp         leveled logger (VERBOSE..ERROR), optional file sink
  color.hpp          ANSI color, auto-disabled on non-TTY / NO_COLOR

src/
  main.cpp           CLI parsing + dispatch (knows nothing about individual commands)
  core/               RootDetector impl
  mount/              PartitionDetector impl
  commands/           one .cpp per subcommand, self-registers via SYSTEMAPP_REGISTER_COMMAND
  fs/ pm/ package/ system/ selinux/ zip/ utils/   empty on purpose — see roadmap
```

Adding a command: create `src/commands/cmd_foo.cpp`, implement `ICommand`,
call `SYSTEMAPP_REGISTER_COMMAND(FooCommand)` at file scope. `main.cpp` and
`CMakeLists.txt` (which globs `src/**/*.cpp`) need no changes.

## Building

Standalone binary via NDK (defaults to `arm64-v8a`, override with `ABI=`):
```bash
export ANDROID_NDK_HOME=/path/to/ndk
./build.sh binary                    # -> release/systemapp-arm64-v8a
ABI=armeabi-v7a ./build.sh binary    # -> release/systemapp-armeabi-v7a
```
Supported `ABI` values: `arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64`.

Termux `.deb` (packages the binaries built above for all four architectures;
build them first, or set `ABI=` to build just one):
```bash
./build.sh termux          # all four ABIs
# -> release/systemapp-aarch64.deb  (arm64-v8a)
# -> release/systemapp-arm.deb      (armeabi-v7a)
# -> release/systemapp-i686.deb     (x86)
# -> release/systemapp-x86_64.deb   (x86_64)

ABI=arm64-v8a ./build.sh termux
# -> release/systemapp-aarch64.deb  (single ABI)
```
Termux/Debian architecture mapping: `arm64-v8a` → `aarch64`,
`armeabi-v7a` → `arm`, `x86` → `i686`, `x86_64` → `x86_64`.

Magisk module (packages one ABI at a time; build that ABI's binary first):
```bash
./build.sh binary
ABI=arm64-v8a ./build.sh magisk
# -> release/SystemApp-Magisk-arm64-v8a.zip
```

CI (`.github/workflows/build.yml`) builds all four ABIs in a matrix, packages
a Magisk module and a Termux `.deb` per ABI (four `.deb` packages, one per
Termux/Debian architecture), and on pushes to `main`/`master` attaches
everything to a GitHub Release tagged from `include/systemapp/version.hpp`.

Host build for iterating on non-Android-specific logic:
```bash
./build.sh host
# -> build-host/systemapp
```

## Roadmap (maps to existing empty directories)

- **`src/pm/`, `src/package/`** — native APK install/uninstall as system or
  priv-app: manifest/package-name parsing, ownership/SELinux context set,
  restorecon, `pm install`-equivalent verification. This is the biggest
  remaining subsystem and the one with the highest brick-risk — needs a
  transaction/rollback layer (per the spec's "never brick the device"
  requirement) before it touches `/system`.
- **`src/selinux/`** — `restorecon`, `chcon`, `getfilecon`/`setfilecon`
  wrappers; currently only readable via shelling to the platform tools.
- **`src/zip/`** — native ZIP read/write (backup archives, Magisk module
  zips are currently produced via the `zip` CLI in `build-magisk.sh`, not
  natively).
- **`src/system/`** — AVB/vbmeta info, dm-verity status + force-enable/
  disable, A/B slot info and switching, boot image inspection. All
  higher-risk, kernel/bootloader-facing operations that need careful,
  device-specific validation.
- **`src/fs/`** — `cp/mv/rm/mkdir/touch/cat/stat/find/du/df/tree` (only `ls`
  exists today).
- **Remount engine** — `mounts` (read-only, implemented) has a write-side
  counterpart, `remount rw|ro`, still to build: overlayfs → `mount -o rw` →
  bind-mount → tmpfs fallback chain, A/B and dynamic-partition aware.
- **Debloat database**, **backup/restore + transaction/rollback**, **APK
  signature (v1–v4) + ELF inspection**, **interactive `systemapp shell`**
  with history/completion — all not started.

## Safety notes for what exists today

- `info`, `root`, `mounts`, `props`, `ls` are **read-only** — safe to run
  on any device, rooted or not.
- Nothing in this build modifies `/system`, remounts anything, or writes to
  a partition. The safety machinery described in the original spec
  (dry-run, confirmation prompts, transactional rollback) needs to land
  *before* any write command does, not after.
