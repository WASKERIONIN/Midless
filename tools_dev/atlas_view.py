#!/usr/bin/env python3
"""atlas_view.py - dev helper: crop + upscale tiles from the terrain atlas so
sprite art can actually be inspected (16px tiles are unreadable at 1:1).

    tools_dev/atlas_view.py build/client/textures/terrain.png 49,50,65,66 out.png
    tools_dev/atlas_view.py build/client/textures/terrain.png --grid out.png

Writes a PNG (nearest-neighbour, x8 by default) and prints a per-tile
"ink" summary: how many non-transparent pixels, bounding box and the mean
colour - enough to spot an empty or half-painted tile without opening it.
"""
import sys
from PIL import Image

TILE = 16


def tile_rect(index):
    return ((index % 16) * TILE, (index // 16) * TILE,
            (index % 16) * TILE + TILE, (index // 16) * TILE + TILE)


def summary(atlas, index):
    t = atlas.crop(tile_rect(index))
    px = list(t.getdata())
    ink = [p for p in px if p[3] > 8]
    if not ink:
        return f"tile {index:3d}: EMPTY"
    xs = [i % TILE for i, p in enumerate(px) if p[3] > 8]
    ys = [i // TILE for i, p in enumerate(px) if p[3] > 8]
    mean = tuple(sum(c[i] for c in ink) // len(ink) for i in range(3))
    # row profile: how wide is the sprite at each height (bottom row first)
    widths = []
    for y in range(TILE - 1, -1, -1):
        w = sum(1 for x in range(TILE) if px[y * TILE + x][3] > 8)
        widths.append(w)
    return (f"tile {index:3d}: ink={len(ink):3d}/256 bbox=x[{min(xs)},{max(xs)}] "
            f"y[{min(ys)},{max(ys)}] mean={mean} rows(bottom-up)={widths}")


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 1
    src, spec, out = sys.argv[1], sys.argv[2], sys.argv[3]
    scale = int(sys.argv[4]) if len(sys.argv) > 4 else 8
    atlas = Image.open(src).convert("RGBA")

    if spec == "--grid":
        img = atlas.resize((atlas.width * scale, atlas.height * scale), Image.NEAREST)
        img.save(out)
        print(f"wrote {out} ({atlas.width}x{atlas.height} @ x{scale})")
        for i in range(256):
            print(summary(atlas, i))
        return 0

    idx = [int(v) for v in spec.split(",") if v.strip() != ""]
    w = max((i % 16) for i in idx) - min((i % 16) for i in idx) + 1
    h = max((i // 16) for i in idx) - min((i // 16) for i in idx) + 1
    x0 = min((i % 16) for i in idx) * TILE
    y0 = min((i // 16) for i in idx) * TILE
    crop = atlas.crop((x0, y0, x0 + w * TILE, y0 + h * TILE))
    # checkerboard backdrop so transparent texels are visible
    back = Image.new("RGBA", crop.size, (40, 40, 48, 255))
    for yy in range(crop.height):
        for xx in range(crop.width):
            if ((xx // 4) + (yy // 4)) % 2 == 0:
                back.putpixel((xx, yy), (58, 58, 68, 255))
    back.alpha_composite(crop)
    back = back.resize((crop.width * scale, crop.height * scale), Image.NEAREST)
    back.save(out)
    print(f"wrote {out} tiles={idx}")
    for i in idx:
        print(summary(atlas, i))
    return 0


if __name__ == "__main__":
    sys.exit(main())
