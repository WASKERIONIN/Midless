# worldgen-check

Headless self-check for Midless world generation.

It compiles the **real** server worldgen translation units from this repository —
`server/src/world/worldgen.c`, `worldgenfield.c`, `worldgenfeatures.c`,
`worldgenerator.c`, `server/src/world/chunk/chunk.c`, `shared/chunkdata.c`,
`shared/blockdefinition.c` and `server/src/scripting/luaworldgen.c` — and drives them
through a Lua mod exactly like the server does. Only the engine layer those files link
against (file IO, logging, allocation, the `serverWorld` global) is stubbed in `stubs.c`.
No world-generation logic is re-implemented here, so a PASS or FAIL describes shipping code.

## Usage

```sh
tools/worldgen-check/build.sh                      # every definition × 3 seeds
tools/worldgen-check/build.sh <mod.lua> [seed]     # one definition
tools/worldgen-check/dumpcolumn <mod.lua> <x> <z>  # dump one block column
```

`build.sh` downloads the single-file dependencies (raylib 4.5 headers, `stb_ds`,
`FastNoiseLite`, `minilua`) into `tools/worldgen-check/.deps`, which is gitignored. If you
already vendored them under `/libs`, point at them instead:

```sh
DEPS_DIR=... tools/worldgen-check/build.sh
```

The exit code is non-zero when any check fails, so it can be used as a regression gate.

## What it checks

| Check | Fails when |
|---|---|
| field cache consistency | the per-column evaluation cache in `Worldgen_Generate` disagrees with a fresh `Worldgen_Eval` context (bad `isColumnConstant` flag) |
| sky mask | `Worldgen_SkyMask` disagrees with a brute-force vertical scan of the `skylight` field |
| surface layering | `top` / `filler` / `stone` placement depends on where a chunk boundary falls |
| rule reach | a `define_rule` with `offset_y` cannot reach the neighbouring chunk |
| compression | `ChunkData_CreateCompressed` → `ChunkData_Decompress` round trip is lossy |
| order independence | generating the same region in a different order changes any block |
| throughput / sky-mask fallback cost | informational, no pass/fail |

Checks that do not apply to a definition (e.g. surface layering for a `material`-field
world, or the rule check when no rule is defined) print `[INFO] … skipped` instead of
silently passing.

## Test definitions

| File | Purpose |
|---|---|
| `mods/seam64.lua` | flat surface at y=64 (local y=0 of a chunk) + a rule with `offset_y = -1`; reproduces the chunk-boundary rule bug |
| `mods/seam65.lua` | identical but with the surface at y=65; the control case |
| `mods/rolling.lua` | two biomes, height noise, clusters ore, tree structures |
| `mods/tall.lua` | `max_y = 896`, no `skylight`, fast sky-mask path |
| `mods/tallslow.lua` | `max_y = 896`, no `skylight`, plus ores and structures; exercises the sky-mask fallback that regenerates every chunk above |

`build.sh` also runs `build/client/mods/*.lua`, i.e. the shipped terrain mod.

## Known failure

`mods/seam64.lua` currently fails the *rule reach* check on every seed: 256 of 256 grass
columns do not get the block the rule should place below them, because `ApplyMaterialRules`
(`server/src/world/worldgen.c:313`) skips any rule whose target index falls outside the
chunk. See `docs/WORLDGEN_AUDIT.md`, problem P1.
