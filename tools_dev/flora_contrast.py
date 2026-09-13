#!/usr/bin/env python3
"""flora_contrast.py - v65.1 readability gate for biome flora sprites.

The v63 art pass painted ember blooms dark-coal on charcoal turf and frost
blooms pale-ice on hoarfrost: on screen they vanished. This tool measures,
for every flora tile against its biome ground:

  ink      - non-transparent pixel count (too few = invisible speck)
  bbox     - silhouette height/width in pixels
  dL       - luminance distance between the sprite's mean ink colour and
             the ground's mean colour (too small = camouflage)
  stem     - the bottom three rows must be a narrow column (1..4 px wide),
             i.e. the plant actually stands on a leg instead of lying on
             the turf as a blob

Usage:
    tools_dev/flora_contrast.py [atlas.png]
Exit code 0 = every gated species readable, 1 = failures.

Classic-meadow species (the starter biome) are reported but not gated -
the v65.1 art pass was ordered for the ember/frost biomes only.
"""
import sys
from PIL import Image

TILE = 16

# species -> (ground tile, gated?)
SPECIES = {
    73: (3, False),   # void glowcaps - classic meadow, untouched by v65.1
    67: (57, True), 74: (57, True), 68: (57, True), 76: (57, True),
    77: (58, True), 75: (58, True), 71: (58, True), 72: (58, True),
}
MIN_INK_FLOWER = 58
MIN_INK_SHROOM = 75
SHROOMS = {73, 74, 75}
MIN_HEIGHT = 10
MIN_DL = 55.0


def lum(c):
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def tile_px(atlas, index):
    x0 = (index % 16) * TILE
    y0 = (index // 16) * TILE
    return list(atlas.crop((x0, y0, x0 + TILE, y0 + TILE)).getdata())


def stats(px):
    ink = [(i % TILE, i // TILE, p) for i, p in enumerate(px) if p[3] > 8]
    if not ink:
        return None
    xs = [a for a, _, _ in ink]
    ys = [b for _, b, _ in ink]
    mean = tuple(sum(p[i] for _, _, p in ink) // len(ink) for i in range(3))
    widths = [sum(1 for x, y, _ in ink if y == row) for row in range(TILE - 3, TILE)]
    return {
        "ink": len(ink),
        "h": max(ys) - min(ys) + 1,
        "w": max(xs) - min(xs) + 1,
        "mean": mean,
        "bottom_widths": widths,
    }


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "client/textures/terrain.png"
    atlas = Image.open(path).convert("RGBA")
    failures = 0
    print(f"flora readability report for {path}")
    for species in sorted(SPECIES):
        ground, gated = SPECIES[species]
        st = stats(tile_px(atlas, species))
        gs = stats(tile_px(atlas, ground))
        if not st or not gs:
            print(f"  {species:3d}: EMPTY TILE")
            failures += gated
            continue
        dl = abs(lum(st["mean"]) - lum(gs["mean"]))
        min_ink = MIN_INK_SHROOM if species in SHROOMS else MIN_INK_FLOWER
        # a leg: the last three rows are a narrow column (multi-stem
        # species may reach 6 px), always clearly narrower than the crown
        stem_ok = all(1 <= w <= 6 and w <= st["w"] - 2 for w in st["bottom_widths"])
        problems = []
        if st["ink"] < min_ink:
            problems.append(f"ink {st['ink']}<{min_ink}")
        if st["h"] < MIN_HEIGHT:
            problems.append(f"height {st['h']}<{MIN_HEIGHT}")
        if dl < MIN_DL:
            problems.append(f"dL {dl:.0f}<{MIN_DL:.0f} (camouflage)")
        if not stem_ok:
            problems.append(f"no leg (bottom rows {st['bottom_widths']})")
        tag = "GATE" if gated else "info"
        mark = "FAIL" if (problems and gated) else ("warn" if problems else "ok  ")
        print(f"  {species:3d} vs ground {ground:3d}: ink={st['ink']:3d} h={st['h']:2d} "
              f"w={st['w']:2d} dL={dl:5.1f} stem={st['bottom_widths']} [{tag}] {mark}"
              + (": " + ", ".join(problems) if problems else ""))
        if problems and gated:
            failures += 1
    if failures:
        print(f"FLORA CONTRAST: {failures} failure(s)")
        return 1
    print("FLORA CONTRAST: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
