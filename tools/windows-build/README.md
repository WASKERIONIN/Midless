# windows-build

Cross-builds Midless for 64-bit Windows from Linux or macOS.

The project's own `Makefile` assumes a Windows host with MinGW at `C:/mingw64/bin` and a
raylib checkout at `C:/raylib/raylib`. This script does the same job anywhere, using
**Zig's C compiler** (`zig cc -target x86_64-windows-gnu`), which ships the MinGW-w64
headers and import libraries. Everything is linked statically, so the output needs no
runtime DLLs beyond what Windows 10/11 already provides.

## Usage

```sh
# one-time: get a zig compiler (any of these works)
python3 -m venv ~/.cache/midless-zig && ~/.cache/midless-zig/bin/pip install ziglang
#   ...or install zig from your package manager and leave `zig` on PATH

tools/windows-build/build-windows.sh
```

Output:

```
release/Midless-Windows-x64/game.exe      client, with an embedded singleplayer server
release/Midless-Windows-x64/server.exe    dedicated server, headless, console log, port 25565
release/Midless-Windows-x64/textures/     terrain.png, humanoid.png, midless.png
release/Midless-Windows-x64/mods/         build/client/mods/*.lua
release/Midless-Windows-x64/README.txt    how to run it
release/Midless-Windows-x64.zip           the whole folder
```

`ZIG=/path/to/zig` overrides the compiler; `SKIP_ZIP=1` skips the archive.
Dependency sources are cached in `.winbuild/` and the single-file headers are copied into
`libs/` (both gitignored — the same place the project's README tells you to put them).

## Dependencies fetched

| Library | Version | Notes |
|---|---|---|
| raylib | 4.5.0 | compiled to `libraylib.a` for `x86_64-windows-gnu`, `PLATFORM_DESKTOP`, bundled GLFW win32 |
| raygui | 3.2 | the version raylib 4.5 ships in its examples; matches the call signatures in `client/src/gui/` (`GuiSlider` takes the value **by value**, raygui 4.0 changed it to a pointer) |
| zpl-c/enet | 2.7.0 (master) | **deviates from the README's 2.3.6** — see below |
| stb | master | `stb_ds.h` |
| FastNoiseLite | master | C header |
| minilua | main | `minilua.h` |

## Three deviations that were needed to make it link

1. **enet 2.3.6 → 2.7.0.** The 2.3.6 header defines a non-static `clock_gettime()` on
   `_WIN32`, which collides with the `clock_gettime()` that MinGW-w64's winpthread header
   declares. 2.7.0 renamed its shim to `_clock_gettime`. The enet API Midless uses
   (`enet_host_create` with five arguments, `enet_packet_create`, `enet_peer_send`, …) is
   unchanged between the two.
2. **FastNoiseLite implementation.** `server/src/world/worldgenerator.c` defines `FNL_IMPL`
   and includes `FastNoiseLite.h`, then `worldgen.h` includes it again; the implementation
   block sits *outside* the header's include guard, so the second include redefines
   everything. The script compiles that one file with the project's own
   `-DMIDLESS_FNL_EXTERNAL` and supplies the implementation once from a generated
   `.winbuild/fnl_impl.c`. This is the same issue recorded as problem P6 in
   `docs/WORLDGEN_AUDIT.md`.
3. **raylib's `src/external` is not on the include path.** With `-I.../src/external`,
   `#include <dirent.h>` in `server/src/scripting/luaengine.c` resolves to raylib's bundled
   copy, whose `opendir`/`readdir`/`closedir`/`rewinddir` are non-static and then collide at
   link time with the same symbols inside `libraylib.a(rcore.o)`.

## What has and has not been verified here

Verified in the sandbox: both binaries link with no undefined symbols and are valid
PE32+ x86-64 images — `game.exe` subsystem GUI (2), `server.exe` subsystem CONSOLE (3),
importing only `KERNEL32`, `USER32`, `GDI32`, `SHELL32`, `WINMM`, `WS2_32` and the UCRT
`api-ms-win-crt-*` forwarders that ship with Windows 10/11. `opengl32.dll` is deliberately
absent: rlgl resolves GL entry points at run time.

**Not verified: actually running them.** The sandbox has no Wine, so neither exe has been
executed. The world-generation code itself was exercised separately on the host
(`tools/worldgen-check`), but window creation, OpenGL and the enet sockets have not been.
If `game.exe` does not start, the most likely culprits are the OpenGL 3.3 requirement or
an antivirus blocking an unsigned executable.
