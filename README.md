![Image](https://i.imgur.com/4Ku3xak.png)
[![Chat](https://img.shields.io/discord/908871478576033832?label=%20chat%20on%20discord)](https://discord.gg/tZthSbpUcV)

# Midless: Cosmic Edition

Midless is a free and open-source voxel game made in C.

This fork turns the world into a starlit void of floating islands. The sun is a still black hole. Water exists only inside crystal basins so it cannot flood the map (the old “water wall” is gone). Clouds are asteroids.

After updating, delete the `world` folder next to the executable and start Singleplayer so a new cosmic world generates.

![Cosmic style reference](docs/cosmic_style_ref.png)

## Cosmic Edition highlights

- **Black hole sun**: rotating gold accretion disk, photon ring, violet halo, and real screen-space gravity lensing (post-FX bends the starfield around it).
- **Living sky**: a galaxy band of stars, drifting nebulae, shooting stars, slow celestial rotation.
- **Cosmic textures**: indigo void rock, teal crystal turf, glowing cyan water, gold / crystal / void-shard ores, warp-core blocks that emit light.
- **Hands with fingers**: the explorer model gained hands whose fingers curl on swings and settle with idle motion (also visible in first person).
- **Island worldgen v6**: cone-tapered islands, guaranteed starter island with launch pad, crystal arch, warp-core obelisks and a glass water basin; three new ores.
- **Game feel**: coyote time, jump buffering, procedural crystal dig/place sounds, jump & teleport SFX, ambient void wind, UI clicks, void-rescue fade.
- **Cosmic menus**: animated starfield title screen with drifting island silhouettes.

## Controls

| Input                        | Action                |
|-------------------------------|----------------------|
| W A S D             | Move                           |
| Space               | Jump (fly up)                  |
| Shift               | Fly down                       |
| Left Click          | Break block                    |
| Right Click         | Place block                    |
| Mouse wheel         | Block selection (starts on stone) |
| T                   | Chat (`/help /where /tp /time /giveme`, arrow history) |
| ESC                 | Menu (New World / Regenerate World) |
| M                   | Map                            |
| Tab                 | Fly mode                       |
| F3                  | Debug overlay (includes birds) |
| F5                  | Camera view                    |
| F11                 | Fullscreen                     |

Falling into the void returns you to the starter island.

## Dependencies

| Dependency    | Version | Type      | Used By|
|---------------|---------|-----------|--------|
| [Raylib](https://github.com/raysan5/raylib/)        | 4.5     | Single-File | Client / Server
| [Zpl-c/ENet](https://github.com/zpl-c/enet)    | 2.3.6   | Single-File | Client / Server
| [FastNoiseLite](https://github.com/Auburn/FastNoiseLite) | -       | Single-File | Client / Server
| [stb_ds](https://github.com/nothings/stb/blob/master/stb_ds.h) | -       | Single-File | Client / Server
| [MiniLua](https://github.com/edubart/minilua) | -       | Single-File | Server
| For Optional Server's Websocket Support:
| [mongoose](https://github.com/cesanta/mongoose/) | 7.8       | Single-Files (.c, .h) | Server
| [OpenSSL](https://github.com/openssl/openssl) | -       | Linked | Server


## Compiling for Windows using MinGW

1. [Download and Build Raylib](https://github.com/raysan5/raylib/wiki/Working-on-Windows)
2. Place single-files dependencies inside /libs
4. Edit the makefile's properties if needed
3. Run mingw32-make inside the Midless folder where the MakeFile is located.

Make arguments:
```
BUILD_SERVER=TRUE       - Build Midless Server (Doesn't build the client)
SERVER_HEADLESS=TRUE    - Compile server without graphics
SERVER_WEB_SUPPORT=TRUE - Compile server with websocket support

DEBUG=TRUE              - Debug build

PLATFORM=PLATFORM_WEB   - Build for the web (Client only)
```

Windows CI builds (MSYS2 + Raylib) run from `.github/workflows/build-windows.yml` and publish a zip under Releases.

## License

All code in this repository is licensed under the [MIT License](https://github.com/Sirvoid/Midless/blob/main/LICENSE).

## Lua scripting

See the [complete Lua API reference](docs/LUA_API.md)
