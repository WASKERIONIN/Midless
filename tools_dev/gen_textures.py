#!/usr/bin/env python3
"""Generate the Cosmic Edition texture atlas (terrain.png) and humanoid skin.

Style reference: docs/cosmic_style_ref.png — floating indigo islands with teal
glowing grass, cyan crystal water, purple nebula, gold-ringed black hole.
Deterministic (seeded) so the output is reproducible.
"""
import math
import random
from PIL import Image, ImageDraw

TILE = 16
ATLAS_TILES = 16
ATLAS = TILE * ATLAS_TILES

# ---------------------------------------------------------------- palette --
DEEP_SPACE   = (13, 5, 24)
ROCK_DARK    = (28, 22, 48)
ROCK_MID     = (38, 30, 64)
ROCK_LIGHT   = (52, 42, 86)
ROCK_EDGE    = (66, 54, 106)
DIRT_DARK    = (46, 32, 66)
DIRT_MID     = (58, 42, 80)
# v43.4: dialed back toward the moody cosmic palette - green must read as
# green without glowing (overbright report).
GRASS_DEEP   = (24, 108, 82)
GRASS_MID    = (36, 142, 104)
GRASS_LIGHT  = (64, 176, 128)
GRASS_TIP    = (118, 214, 164)
WATER_DEEP   = (10, 92, 148)
WATER_MID    = (24, 150, 210)
WATER_LIGHT  = (64, 214, 255)
WATER_TIP    = (170, 244, 255)
SAND_DEEP    = (148, 130, 178)
SAND_LIGHT   = (198, 182, 222)
CRYSTAL_PALE = (206, 244, 255)
CRYSTAL_MID  = (120, 222, 248)
WOOD_DARK    = (58, 38, 78)
WOOD_MID     = (82, 54, 106)
LEAF_DEEP    = (16, 96, 104)
LEAF_MID     = (24, 140, 138)
LEAF_LIGHT   = (56, 196, 172)
LEAF_TIP     = (130, 240, 220)
LAVA_DARK    = (96, 22, 60)
LAVA_MID     = (196, 60, 40)
LAVA_LIGHT   = (255, 160, 60)
MAGENTA      = (232, 84, 240)
VIOLET       = (148, 64, 255)
GOLD         = (255, 190, 84)
AMBER        = (255, 138, 40)


def rng(seed):
    return random.Random(seed)


def blank_tile():
    return Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))


def paste(atlas, index, tile):
    x = (index % ATLAS_TILES) * TILE
    y = (index // ATLAS_TILES) * TILE
    atlas.paste(tile, (x, y))


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def with_a(c, a):
    return (c[0], c[1], c[2], a)


# ------------------------------------------------------------- generators --
def value_noise(seed, scale=4):
    """Coarse per-tile value noise, returns 16x16 floats 0..1."""
    r = rng(seed)
    grid = [[r.random() for _ in range(scale + 1)] for _ in range(scale + 1)]
    out = [[0.0] * TILE for _ in range(TILE)]
    for y in range(TILE):
        for x in range(TILE):
            fx, fy = x / TILE * scale, y / TILE * scale
            x0, y0 = int(fx), int(fy)
            tx, ty = fx - x0, fy - y0
            tx = tx * tx * (3 - 2 * tx)
            ty = ty * ty * (3 - 2 * ty)
            a = grid[y0][x0]
            b = grid[y0][x0 + 1]
            c = grid[y0 + 1][x0]
            d = grid[y0 + 1][x0 + 1]
            out[y][x] = (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty
    return out


def speckle(img, seed, color, count, bright_var=0.5, alpha=255, twinkle=True):
    r = rng(seed)
    px = img.load()
    for _ in range(count):
        x, y = r.randrange(TILE), r.randrange(TILE)
        b = 1.0 - bright_var * r.random()
        px[x, y] = with_a(lerp((16, 10, 30), color, max(0.35, b)), alpha)
        if twinkle and r.random() < 0.3:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < TILE and 0 <= ny < TILE:
                    px[nx, ny] = with_a(lerp((16, 10, 30), color, b * 0.55), alpha)


def crack_lines(img, seed, color, alpha=140, count=3):
    r = rng(seed)
    d = ImageDraw.Draw(img)
    for _ in range(count):
        x, y = r.randrange(2, 14), r.randrange(2, 14)
        pts = [(x, y)]
        for _ in range(r.randrange(3, 6)):
            x = max(0, min(15, x + r.choice((-1, 0, 1))))
            y = max(0, min(15, y + r.choice((-1, 1))))
            pts.append((x, y))
        d.line(pts, fill=with_a(color, alpha), width=1)


# tiles --------------------------------------------------------------------
def t_stone(index):
    """Cosmic basalt — indigo rock with faint star speckles."""
    n = value_noise(index * 7 + 1, 4)
    img = blank_tile()
    px = img.load()
    for y in range(TILE):
        for x in range(TILE):
            v = n[y][x]
            c = lerp(ROCK_DARK, ROCK_LIGHT, v)
            if v > 0.72:
                c = lerp(c, ROCK_EDGE, (v - 0.72) * 3)
            px[x, y] = with_a(c, 255)
    crack_lines(img, index * 3 + 2, (18, 14, 34), 110, 4)
    speckle(img, index * 5 + 3, (150, 130, 210), 5, 0.5)
    return img


def t_void_rock(index):
    """Deeper, darker void rock with purple shimmer."""
    n = value_noise(index * 11 + 5, 3)
    img = blank_tile()
    px = img.load()
    for y in range(TILE):
        for x in range(TILE):
            v = n[y][x]
            base = lerp((16, 12, 32), ROCK_DARK, v)
            px[x, y] = with_a(base, 255)
    crack_lines(img, index * 7 + 4, (10, 6, 22), 130, 5)
    speckle(img, index * 9 + 7, (120, 70, 190), 4, 0.6)
    speckle(img, index * 13 + 9, (60, 210, 220), 2, 0.4)
    return img


def t_dirt(index):
    n = value_noise(index * 17 + 2, 5)
    img = blank_tile()
    px = img.load()
    for y in range(TILE):
        for x in range(TILE):
            v = n[y][x]
            c = lerp(DIRT_DARK, DIRT_MID, v)
            if v < 0.3:
                c = lerp(c, (34, 24, 50), 0.6)
            px[x, y] = with_a(c, 255)
    speckle(img, index * 19 + 6, (96, 70, 130), 5, 0.5)
    speckle(img, index * 23 + 8, (40, 170, 150), 2, 0.35)
    return img


def t_grass_top(index):
    n = value_noise(index * 29 + 3, 5)
    img = blank_tile()
    px = img.load()
    for y in range(TILE):
        for x in range(TILE):
            v = n[y][x]
            c = lerp(GRASS_DEEP, GRASS_MID, v)
            if v > 0.55:
                c = lerp(c, GRASS_LIGHT, (v - 0.55) * 2.2)
            if v > 0.82:
                c = lerp(c, GRASS_TIP, (v - 0.82) * 4)
            px[x, y] = with_a(c, 255)
    speckle(img, index * 31 + 10, GRASS_TIP, 5, 0.3)
    return img


def t_grass_side(index):
    img = t_dirt(index)
    px = img.load()
    n = value_noise(index * 37 + 4, 4)
    for x in range(TILE):
        depth = 4 + int(n[0][x] * 3.4)
        for y in range(depth):
            v = n[y][x]
            if y == 0:
                c = lerp(GRASS_LIGHT, GRASS_TIP, v * 0.85)
            elif y < depth - 2:
                c = lerp(GRASS_MID, GRASS_LIGHT, v)
            elif y < depth - 1:
                c = lerp(GRASS_DEEP, GRASS_MID, v)
            else:
                c = lerp(GRASS_DEEP, DIRT_MID, 0.45)
            px[x, y] = with_a(c, 255)
    return img


def t_water(index):
    n = value_noise(index * 41 + 5, 3)
    img = blank_tile()
    px = img.load()
    for y in range(TILE):
        for x in range(TILE):
            v = n[y][x] * 0.6 + 0.4 * math.sin((x + y * 0.5) * 0.8)
            v = max(0.0, min(1.0, v))
            c = lerp(WATER_MID, WATER_DEEP, v)
            if v > 0.66:
                c = lerp(c, WATER_LIGHT, (v - 0.66) * 2.2)
            if v < 0.22:
                c = lerp(WATER_DEEP, (6, 52, 100), 0.5)
            px[x, y] = with_a(c, 255)
    for _sx in range(3):
        r = rng(index * 43 + _sx)
        x0, y0 = r.randrange(2, 12), r.randrange(2, 13)
        for i in range(3 + r.randrange(3)):
            if x0 + i < TILE:
                px[x0 + i, y0] = with_a(WATER_TIP, 210)
    return img


def t_sand(index):
    n = value_noise(index * 47 + 6, 6)
    img = blank_tile()
    px = img.load()
    for y in range(TILE):
        for x in range(TILE):
            v = n[y][x]
            c = lerp(SAND_DEEP, SAND_LIGHT, v)
            px[x, y] = with_a(c, 255)
    speckle(img, index * 53 + 11, (230, 218, 246), 4, 0.35)
    return img


def t_log_side(index):
    n = value_noise(index * 59 + 7, 8)
    img = blank_tile()
    px = img.load()
    for y in range(TILE):
        for x in range(TILE):
            v = n[y][x] * 0.5 + 0.5 * ((math.sin(x * 1.9 + math.sin(y * 0.7) * 2.0) + 1) / 2)
            c = lerp(WOOD_DARK, WOOD_MID, v)
            px[x, y] = with_a(c, 255)
    crack_lines(img, index * 61 + 12, (40, 24, 56), 120, 3)
    return img


def t_log_top(index):
    img = blank_tile()
    px = img.load()
    cx, cy = 7.5, 7.5
    for y in range(TILE):
        for x in range(TILE):
            d = math.sqrt((x - cx) ** 2 + (y - cy) ** 2)
            ring = (math.sin(d * 2.2) + 1) / 2
            c = lerp(WOOD_DARK, WOOD_MID, ring * 0.8)
            if d > 7.2:
                c = lerp(c, (44, 28, 62), 0.7)
            if d < 1.2:
                c = lerp(c, (120, 200, 190), 0.4)
            px[x, y] = with_a(c, 255)
    return img


def t_leaves(index):
    n = value_noise(index * 67 + 8, 5)
    img = blank_tile()
    px = img.load()
    r = rng(index * 71)
    holes = {(r.randrange(16), r.randrange(16)) for _ in range(5)}
    for y in range(TILE):
        for x in range(TILE):
            if (x, y) in holes:
                continue
            v = n[y][x]
            c = lerp(LEAF_DEEP, LEAF_MID, v)
            if v > 0.6:
                c = lerp(c, LEAF_LIGHT, (v - 0.6) * 2)
            if v > 0.85:
                c = lerp(c, LEAF_TIP, (v - 0.85) * 4)
            px[x, y] = with_a(c, 255)
    speckle(img, index * 73 + 13, LEAF_TIP, 3, 0.3)
    return img


def t_planks(index):
    img = blank_tile()
    px = img.load()
    for y in range(TILE):
        band = y // 4
        shade = (band * 37 + index * 13) % 3 / 6.0
        for x in range(TILE):
            v = ((x * 3 + band * 5) % 7) / 7.0 * 0.25 + shade
            c = lerp(WOOD_MID, WOOD_DARK, 0.3 + v * 0.5)
            if y % 4 == 3:
                c = lerp(c, (40, 26, 58), 0.6)
            if y % 4 == 0:
                c = lerp(c, (110, 76, 140), 0.35)
            px[x, y] = with_a(c, 255)
    return img


def ore_tile(index, gem_a, gem_b, gem_c):
    img = t_stone(index)
    d = ImageDraw.Draw(img)
    r = rng(index * 79 + 14)
    for _ in range(4):
        cx, cy = r.randrange(2, 13), r.randrange(2, 13)
        sz = r.choice((1, 1, 2))
        d.rectangle([cx - sz, cy - sz, cx + sz, cy + sz],
                    fill=with_a(lerp(gem_a, gem_b, r.random()), 255))
        d.point((cx, cy), fill=with_a(gem_c, 255))
    return img


def t_water_glow_tile(index):
    return t_water(index)


def t_crystal(index):
    n = value_noise(index * 83 + 15, 3)
    img = blank_tile()
    px = img.load()
    for y in range(TILE):
        for x in range(TILE):
            v = n[y][x]
            d = abs(x - 7.5) + abs(y - 7.5)
            c = lerp(CRYSTAL_MID, CRYSTAL_PALE, v)
            if d > 11:
                c = lerp(c, (70, 140, 210), 0.5)
            px[x, y] = with_a(c, 175)
    d = ImageDraw.Draw(img)
    for i in range(3):
        x0 = 2 + i * 4
        d.line([(x0, 13), (x0 + 3, 2)], fill=with_a(CRYSTAL_PALE, 200), width=1)
    return img


def t_glass(index):
    img = blank_tile()
    px = img.load()
    for i in range(TILE):
        px[i, 0] = with_a(CRYSTAL_PALE, 235)
        px[i, 15] = with_a(CRYSTAL_PALE, 235)
        px[0, i] = with_a(CRYSTAL_PALE, 235)
        px[15, i] = with_a(CRYSTAL_PALE, 235)
        px[i, i] = with_a((180, 240, 255), 60)
        px[i, 15 - i] = with_a((180, 240, 255), 40)
    return img


def t_launch_pad_top(index):
    img = blank_tile()
    px = img.load()
    for y in range(TILE):
        for x in range(TILE):
            d = max(abs(x - 7.5), abs(y - 7.5))
            c = lerp((24, 18, 46), ROCK_MID, (d / 8))
            px[x, y] = with_a(c, 255)
    d = ImageDraw.Draw(img)
    for k in range(3):
        r = 7 - k * 2
        col = lerp(AMBER, WATER_LIGHT, k / 2.0)
        d.ellipse([8 - r, 8 - r, 8 + r - 1, 8 + r - 1], outline=with_a(col, 255 - k * 30))
    d.point((7, 7), fill=with_a(WATER_TIP, 255))
    d.point((8, 7), fill=with_a(WATER_TIP, 255))
    d.point((7, 8), fill=with_a(WATER_TIP, 255))
    d.point((8, 8), fill=with_a(WATER_TIP, 255))
    return img


def t_launch_pad_side(index):
    img = t_void_rock(index + 40)
    px = img.load()
    for x in range(TILE):
        px[x, 0] = with_a(WATER_LIGHT, 255)
        px[x, 1] = with_a(lerp(WATER_MID, WATER_LIGHT, (x % 4) / 4), 255)
    return img


def t_warp_core(index):
    img = blank_tile()
    px = img.load()
    cx, cy = 7.5, 7.5
    for y in range(TILE):
        for x in range(TILE):
            d = math.sqrt((x - cx) ** 2 + (y - cy) ** 2)
            ang = math.atan2(y - cy, x - cx)
            swirl = (math.sin(ang * 3 + d * 2.4) + 1) / 2
            if d < 3.2:
                c = lerp((10, 4, 18), (30, 12, 44), swirl)
            else:
                c = lerp(lerp(VIOLET, MAGENTA, swirl), GOLD, max(0.0, 1.0 - abs(d - 4.6) * 0.8) * 0.55)
            px[x, y] = with_a(c, 255)
    d = ImageDraw.Draw(img)
    d.ellipse([4, 4, 11, 11], outline=with_a(GOLD, 230))
    return img


def t_lava(index):
    n = value_noise(index * 89 + 16, 4)
    img = blank_tile()
    px = img.load()
    for y in range(TILE):
        for x in range(TILE):
            v = n[y][x]
            c = lerp(LAVA_MID, LAVA_DARK, v)
            if v < 0.4:
                c = lerp(c, LAVA_LIGHT, (0.4 - v) * 1.8)
            px[x, y] = with_a(c, 255)
    speckle(img, index * 97 + 17, LAVA_LIGHT, 3, 0.3)
    return img


def t_fire(index):
    img = blank_tile()
    px = img.load()
    r = rng(index * 101 + 18)
    for x in range(TILE):
        h = 5 + int(r.random() * 6)
        for y in range(TILE):
            if y >= TILE - h:
                t = (y - (TILE - h)) / max(1, h)
                c = lerp(MAGENTA, AMBER, t)
                if t > 0.7:
                    c = lerp(c, (255, 240, 200), (t - 0.7) * 2)
                px[x, y] = with_a(c, 235 if t < 0.5 else 180)
    return img


# --------------------------------------------------------------- flora ----
# v54: every species gets its own silhouette (no recolour clones). All art
# is centred on the tile axis x=8 so the crossed quads line up exactly.
def _stem(d, pts, color_a, color_b=None):
    """draw a polyline stem; color_b = optional gradient toward the tip"""
    n = len(pts)
    for i in range(n - 1):
        x0, y0 = pts[i]
        x1, y1 = pts[i + 1]
        steps = max(abs(x1 - x0), abs(y1 - y0)) or 1
        for s in range(steps + 1):
            t = s / steps
            x = round(x0 + (x1 - x0) * t)
            y = round(y0 + (y1 - y0) * t)
            c = color_a if color_b is None else lerp(color_a, color_b, i / max(1, n - 1))
            d.point((x, y), fill=with_a(c, 255))


def t_rose(index):
    """cosmic rose: layered magenta petals around a gold heart."""
    img = blank_tile()
    d = ImageDraw.Draw(img)
    _stem(d, [(8, 15), (8, 12), (7, 9), (8, 6)], GRASS_DEEP, GRASS_MID)
    d.point((6, 12), fill=with_a(GRASS_MID, 255))
    d.point((7, 11), fill=with_a(GRASS_LIGHT, 255))
    d.point((10, 10), fill=with_a(GRASS_MID, 255))
    # outer petals ring
    for x, y in ((5, 4), (6, 3), (8, 2), (10, 3), (11, 4),
                 (4, 6), (5, 6), (11, 6), (12, 6), (5, 8), (11, 8)):
        d.point((x, y), fill=with_a((232, 84, 240), 255))
    # mid petals
    for x, y in ((6, 5), (10, 5), (6, 7), (10, 7), (8, 3), (7, 8), (9, 8)):
        d.point((x, y), fill=with_a((255, 150, 250), 255))
    # gold heart
    d.point((8, 5), fill=with_a((255, 230, 120), 255))
    d.point((8, 6), fill=with_a((255, 190, 84), 255))
    d.point((7, 6), fill=with_a((255, 150, 250), 255))
    d.point((9, 6), fill=with_a((255, 150, 250), 255))
    return img


def t_dandelion(index):
    """glowing puffball: white-teal orb shedding tiny seeds."""
    img = blank_tile()
    d = ImageDraw.Draw(img)
    _stem(d, [(8, 15), (8, 10)], GRASS_DEEP, GRASS_MID)
    cx, cy = 8, 5
    for dx, dy in ((-2, 0), (2, 0), (0, -2), (0, 2), (-1, -1), (1, -1), (-1, 1), (1, 1)):
        d.point((cx + dx, cy + dy), fill=with_a((64, 214, 255), 255))
    for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1), (0, 0)):
        d.point((cx + dx, cy + dy), fill=with_a((206, 244, 255), 255))
    # drifting seeds
    for x, y in ((3, 2), (12, 3), (4, 8), (13, 8), (8, 1)):
        d.point((x, y), fill=with_a((170, 244, 255), 255))
    d.point((8, 8), fill=with_a((24, 150, 210), 255))
    return img


def t_bellflower(index):
    """void bellflower: arched stem, hanging teal bell, gold clapper."""
    img = blank_tile()
    d = ImageDraw.Draw(img)
    _stem(d, [(7, 15), (7, 11), (8, 8), (10, 6), (11, 5)], GRASS_DEEP, GRASS_MID)
    # bell hangs from (11,5)
    d.point((10, 5), fill=with_a((24, 150, 210), 255))
    d.point((10, 6), fill=with_a((24, 150, 210), 255))
    for x, y in ((9, 6), (12, 6), (9, 7), (12, 7), (9, 8), (12, 8)):
        d.point((x, y), fill=with_a((64, 214, 255), 255))
    for x, y in ((10, 6), (11, 6), (10, 7), (11, 7), (10, 8), (11, 8)):
        d.point((x, y), fill=with_a((10, 92, 148), 255))
    for x, y in ((9, 9), (10, 9), (11, 9), (12, 9)):
        d.point((x, y), fill=with_a((24, 150, 210), 255))
    d.point((10, 10), fill=with_a((255, 190, 84), 255))
    d.point((11, 10), fill=with_a((255, 190, 84), 255))
    d.point((6, 12), fill=with_a(GRASS_MID, 255))
    return img


def t_starbloom(index):
    """starbloom: a five-point gold star with violet sparks, dead centre."""
    img = blank_tile()
    d = ImageDraw.Draw(img)
    _stem(d, [(8, 15), (8, 9)], GRASS_DEEP, GRASS_MID)
    cx, cy = 8, 5
    star = [(cx, cy - 3), (cx + 1, cy - 1), (cx + 3, cy - 1), (cx + 1, cy),
            (cx + 2, cy + 2), (cx, cy + 1), (cx - 2, cy + 2), (cx - 1, cy),
            (cx - 3, cy - 1), (cx - 1, cy - 1)]
    for x, y in star:
        d.point((x, y), fill=with_a((255, 190, 84), 255))
    d.point((cx, cy), fill=with_a((255, 240, 190), 255))
    for x, y in ((4, 1), (12, 2), (5, 9), (12, 9)):
        d.point((x, y), fill=with_a((148, 64, 255), 255))
    return img


def t_spiralfern(index):
    """spiral fern: a fiddlehead frond curling into a violet-tipped spiral."""
    img = blank_tile()
    d = ImageDraw.Draw(img)
    _stem(d, [(8, 15), (8, 10), (9, 7), (10, 5), (10, 4)], GRASS_DEEP, GRASS_MID)
    spiral = [(10, 3), (9, 3), (8, 3), (8, 4), (9, 4), (9, 5)]
    for i, (x, y) in enumerate(spiral):
        c = (56, 196, 172) if i < 3 else (130, 240, 220)
        d.point((x, y), fill=with_a(c, 255))
    # side fronds
    for x, y in ((7, 12), (6, 11), (9, 11), (10, 10), (7, 9)):
        d.point((x, y), fill=with_a((24, 140, 138), 255))
    d.point((6, 13), fill=with_a(GRASS_MID, 255))
    return img


def t_tulip(index):
    """twin tulip: two heads on a forked stem, magenta and amber."""
    img = blank_tile()
    d = ImageDraw.Draw(img)
    _stem(d, [(7, 15), (7, 10), (6, 7)], GRASS_DEEP)
    _stem(d, [(8, 15), (8, 10), (10, 7)], GRASS_DEEP)
    # left head (magenta)
    for x, y in ((5, 5), (7, 5), (4, 6), (8, 6), (4, 7), (8, 7), (5, 8), (7, 8), (6, 8)):
        d.point((x, y), fill=with_a((232, 84, 240), 255))
    d.point((6, 6), fill=with_a((255, 150, 250), 255))
    d.point((6, 5), fill=with_a((255, 150, 250), 255))
    # right head (amber)
    for x, y in ((9, 5), (11, 5), (9, 6), (12, 6), (9, 7), (12, 7), (10, 8), (11, 8)):
        d.point((x, y), fill=with_a((255, 138, 40), 255))
    d.point((10, 6), fill=with_a((255, 190, 84), 255))
    d.point((11, 6), fill=with_a((255, 190, 84), 255))
    d.point((5, 12), fill=with_a(GRASS_MID, 255))
    d.point((10, 11), fill=with_a(GRASS_MID, 255))
    return img


def t_glowgrass(index):
    """glow grass tuft: five blades with cold cyan tips."""
    img = blank_tile()
    d = ImageDraw.Draw(img)
    _stem(d, [(4, 15), (4, 11), (3, 8)], GRASS_DEEP, (56, 196, 172))
    _stem(d, [(6, 15), (6, 10), (7, 6)], GRASS_DEEP, (118, 214, 164))
    _stem(d, [(8, 15), (8, 9), (8, 4)], GRASS_DEEP, (64, 214, 255))
    _stem(d, [(10, 15), (10, 10), (9, 6)], GRASS_DEEP, (118, 214, 164))
    _stem(d, [(12, 15), (12, 11), (13, 8)], GRASS_DEEP, (56, 196, 172))
    d.point((3, 7), fill=with_a((170, 244, 255), 255))
    d.point((8, 3), fill=with_a((170, 244, 255), 255))
    d.point((13, 7), fill=with_a((170, 244, 255), 255))
    return img


def t_lanternberry(index):
    """lanternberry: arched stem bearing three glowing orange lanterns."""
    img = blank_tile()
    d = ImageDraw.Draw(img)
    _stem(d, [(8, 15), (8, 11), (9, 8), (10, 7)], GRASS_DEEP, GRASS_MID)
    _stem(d, [(8, 12), (6, 10), (5, 9)], GRASS_DEEP, GRASS_MID)
    _stem(d, [(9, 9), (11, 9)], GRASS_DEEP, GRASS_MID)
    # three lanterns: gold core, orange ring, dim halo
    for cx, cy in ((5, 8), (8, 6), (11, 8)):
        d.point((cx, cy), fill=with_a((255, 240, 190), 255))
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            d.point((cx + dx, cy + dy), fill=with_a((255, 138, 40), 255))
        for dx, dy in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
            d.point((cx + dx, cy + dy), fill=with_a((196, 60, 40), 255))
    d.point((8, 3), fill=with_a((255, 160, 60), 255))
    return img


def t_void_shard(index):
    """v55: inventory icon - a teal void shard crystal."""
    img = blank_tile()
    d = ImageDraw.Draw(img)
    TEAL = (96, 231, 214)
    TEAL_L = (200, 255, 246)
    TEAL_D = (24, 120, 110)
    d.polygon([(8, 1), (12, 8), (8, 15), (4, 8)], fill=TEAL)
    d.polygon([(8, 1), (12, 8), (8, 15)], fill=TEAL_D)
    d.line([(8, 1), (8, 15)], fill=TEAL_L)
    d.line([(4, 8), (8, 3)], fill=TEAL_L)
    d.point((7, 4), fill=TEAL_L)
    d.point((9, 9), fill=(170, 244, 235))
    return img


def t_sedge(index):
    """v59: thin sedge blades - wispy strands sticking in different
    directions, slightly taller than the tuft; layered over it."""
    img = blank_tile()
    px = img.load()
    D = (30, 104, 88)
    M = (62, 168, 128)
    L = (120, 226, 176)
    H = (188, 255, 214)
    strands = [
        (8, 15, 12, 1, L), (7, 15, 3, 2, M), (8, 15, 10, 0, M),
        (6, 15, 1, 5, D), (9, 15, 14, 4, D), (7, 15, 5, 1, L),
        (8, 15, 7, 2, H), (6, 15, 8, 3, M), (9, 15, 13, 1, L),
    ]
    for x0, y0, x1, ytop, col in strands:
        steps = y0 - ytop
        for s in range(steps + 1):
            xx = x0 + (x1 - x0) * s // max(steps, 1)
            yy = y0 - s
            px[xx, yy] = col
    px[2, 4] = H
    px[13, 3] = H
    return img


def t_undersky(index):
    """v59.2: the other world's sky, seen through the shell's underside -
    nothing like our purple void: near-black green, gold and ice-white
    stars, one alien emerald nebula streak."""
    img = Image.new("RGBA", (TILE, TILE), (6, 10, 8, 255))
    px = img.load()
    rndg = rng(index * 311 + 73)
    # deep green-black wash with faint banding
    for y in range(TILE):
        shade = 10 + (y % 4) * 2
        for x in range(TILE):
            px[x, y] = (4 + shade // 2, 8 + shade, 7 + shade // 2, 255)
    # emerald nebula streak across the middle
    for x in range(TILE):
        cyN = 7 + int(2.2 * __import__("math").sin(6.2832 * x / 16.0))
        for dy in (-1, 0, 1):
            yy = cyN + dy
            if 0 <= yy < 16 and rndg.random() < 0.75:
                px[x, yy] = (16, 92, 60, 255) if dy == 0 else (10, 46, 34, 255)
    # alien stars: gold / ice / magenta-white
    for _ in range(26):
        sx, sy = rndg.randint(0, 15), rndg.randint(0, 15)
        kind = rndg.random()
        if kind < 0.4: c = (255, 214, 120, 255)
        elif kind < 0.75: c = (215, 245, 255, 255)
        else: c = (255, 170, 235, 255)
        px[sx, sy] = c
    # two bright crosses
    for cx0, cy0 in ((4, 3), (12, 11)):
        px[cx0, cy0] = (255, 255, 255, 255)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            xx, yy = (cx0 + dx) % 16, (cy0 + dy) % 16
            px[xx, yy] = (180, 230, 210, 255)
    return img

def t_moth(index):
    """v59: glowmoth sprite - teal wings with magenta eyespots, tiny body,
    faint trailing shimmer; drawn as a small billboard."""
    img = blank_tile()
    px = img.load()
    WING = (64, 224, 208)
    WING_D = (30, 150, 150)
    SPOT = (255, 110, 240)
    BODY = (240, 250, 240)
    # upper wings
    for x in range(3, 7):
        px[x, 5] = WING
        px[16 - x, 5] = WING
    for x in range(2, 8):
        px[x, 6] = WING if 3 <= x <= 6 else WING_D
        px[15 - x, 6] = WING if 3 <= x <= 6 else WING_D
    # lower wings
    for x in range(3, 8):
        px[x, 7] = WING_D
        px[15 - x, 7] = WING_D
    for x in range(4, 7):
        px[x, 8] = WING_D
        px[15 - x, 8] = WING_D
    # eyespots
    px[4, 6] = SPOT
    px[11, 6] = SPOT
    # body
    px[7, 5] = BODY
    px[8, 5] = BODY
    px[7, 6] = BODY
    px[8, 6] = BODY
    px[7, 7] = BODY
    px[8, 7] = BODY
    px[7, 8] = (200, 215, 205)
    px[8, 8] = (200, 215, 205)
    # antennae
    px[6, 4] = BODY
    px[9, 4] = BODY
    px[5, 3] = (220, 255, 250)
    px[10, 3] = (220, 255, 250)
    # glow dust around
    px[2, 9] = (150, 255, 235)
    px[13, 9] = (150, 255, 235)
    px[12, 4] = (255, 160, 245)
    px[3, 4] = (255, 160, 245)
    return img


def t_cloud(index):
    """v59.2: shell texture - puffy violet cumulus, seamless wrapping,
    built from 2 octaves of wrapped value noise; light comes from above,
    tiny teal glints catch the eye. Reads as a real cloud from any side."""
    img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 255))
    px = img.load()
    rndg = rng(index * 613 + 7)
    g8 = [[rndg.random() for _ in range(9)] for _ in range(9)]
    for j in range(9):
        g8[j][8] = g8[j][0]
    for i in range(9):
        g8[8][i] = g8[0][i]
    g16 = [[rndg.random() for _ in range(17)] for _ in range(17)]
    for j in range(17):
        g16[j][16] = g16[j][0]
    for i in range(17):
        g16[16][i] = g16[0][i]

    def val(g, n, fx, fy):
        x, y = fx * n, fy * n
        x0, y0 = int(x) % n, int(y) % n
        x1, y1 = (x0 + 1) % n, (y0 + 1) % n
        tx, ty = x - int(x), y - int(y)
        tx = tx * tx * (3 - 2 * tx)
        ty = ty * ty * (3 - 2 * ty)
        a = g[y0][x0] * (1 - tx) + g[y0][x1] * tx
        b = g[y1][x0] * (1 - tx) + g[y1][x1] * tx
        return a * (1 - ty) + b * ty

    SHADES = [
        (44, 32, 78),     # deep folds
        (78, 58, 122),    # body
        (114, 92, 162),   # lit body
        (158, 138, 202),  # crest light
    ]
    for y in range(TILE):
        for x in range(TILE):
            fx, fy = x / TILE, y / TILE
            n = val(g8, 8, fx, fy) * 0.68 + val(g16, 16, fx, fy) * 0.32
            # light from the top edge (the sky above the shell)
            n += (1.0 - fy) * 0.22
            band = int(n * 3.6)
            if band < 0: band = 0
            if band > 3: band = 3
            r, gg, b = SHADES[band]
            px[x, y] = (r, gg, b, 255)
    # crest highlights along the top rows
    for x in range(TILE):
        for y in range(2):
            if px[x, y][0] > 100:
                px[x, y] = (210, 196, 240, 255)
    # sparse teal glints
    for _ in range(6):
        gx, gy = rndg.randint(0, 15), rndg.randint(2, 15)
        px[gx, gy] = (110, 225, 210, 255)
    return img

def t_void_tuft(index):
    """v58: short grass tuft - dense teal-green blades anchored to the
    ground; the billboard is only a third of a block tall."""
    img = blank_tile()
    px = img.load()
    D = (26, 92, 74)
    M = (52, 148, 108)
    L = (98, 214, 156)
    H = (168, 255, 205)
    blades = [
        (7, 15, 7, 3, L), (8, 15, 9, 1, M), (6, 15, 5, 6, M),
        (9, 15, 11, 4, D), (5, 15, 3, 9, D), (10, 15, 12, 8, M),
        (7, 15, 6, 2, H), (9, 15, 10, 3, L),
    ]
    for x0, y0, x1, ytop, col in blades:
        steps = y0 - ytop
        for s in range(steps + 1):
            xx = x0 + (x1 - x0) * s // max(steps, 1)
            yy = y0 - s
            px[xx, yy] = col
    px[4, 13] = D
    px[11, 13] = D
    px[8, 4] = H
    px[5, 8] = L
    return img


def t_spell_scroll(index):
    """v58: spell scroll - rolled parchment with a teal seal ribbon and
    gold rune sparks; the 'gaze true' charm against the black hole."""
    img = blank_tile()
    px = img.load()
    PAP = (238, 222, 178)
    PAP_D = (198, 176, 128)
    PAP_S = (168, 142, 96)
    SEAL = (64, 224, 208)
    SEAL_D = (30, 150, 150)
    GOLD = (255, 208, 110)
    # unrolled middle sheet
    for y in range(4, 12):
        for x in range(4, 12):
            c = PAP
            if y in (4, 11) or x in (4, 11):
                c = PAP_D
            px[x, y] = c
    # rolled ends
    for y in range(3, 13):
        for x in (2, 3, 12, 13):
            c = PAP_D if x in (3, 12) else PAP_S
            if y in (3, 12):
                c = PAP_S
            px[x, y] = c
    px[2, 4] = PAP_D
    px[13, 4] = PAP_D
    px[2, 11] = PAP_D
    px[13, 11] = PAP_D
    # teal seal band across the middle
    for x in range(2, 14):
        px[x, 7] = SEAL
        px[x, 8] = SEAL_D
    px[7, 7] = (210, 255, 250)
    # rune sparks
    px[6, 5] = GOLD
    px[9, 5] = GOLD
    px[6, 10] = GOLD
    px[9, 10] = PAP_S
    return img

def t_star_reed(index):
    """v57: star reed - tall teal stalk topped with a gold four-point star.
    Drawn for a billboard twice as tall as wide-ish (1.1 x 1.9 blocks)."""
    img = blank_tile()
    px = img.load()
    ST_D = (24, 78, 96)
    ST = (46, 138, 150)
    ST_L = (96, 226, 220)
    GOLD = (255, 206, 96)
    GOLD_L = (255, 240, 170)
    # stalk: 2 px wide, brighter toward the top
    for y in range(5, 16):
        for x in (7, 8):
            c = ST
            if y >= 12:
                c = ST_D
            elif y <= 7:
                c = ST_L if x == 8 else ST
            px[x, y] = c
    # node rings
    px[6, 9] = ST_D
    px[9, 9] = ST_D
    px[6, 12] = ST_D
    px[9, 12] = ST_D
    # little side leaves
    px[5, 10] = ST
    px[4, 9] = ST_D
    px[10, 13] = ST
    px[11, 12] = ST_D
    # gold four-point star bulb
    star_rows = {
        1: (7, 8),
        2: (6, 9),
        3: (4, 11),
        4: (6, 9),
    }
    for y, (x0, x1) in star_rows.items():
        for x in range(x0, x1 + 1):
            px[x, y] = GOLD
    px[5, 3] = GOLD_L
    px[10, 3] = GOLD_L
    px[7, 3] = GOLD_L
    px[8, 3] = GOLD_L
    px[7, 2] = GOLD_L
    px[8, 2] = GOLD_L
    return img


def t_moon_bell(index):
    """v57: moon bell - arching stem with a heavy drooping violet bell,
    gold stamens and teal glow dots (1.4 x 1.7 blocks)."""
    img = blank_tile()
    px = img.load()
    STEM = (172, 150, 205)
    STEM_D = (128, 108, 165)
    BELL = (198, 74, 235)
    BELL_D = (146, 40, 185)
    BELL_L = (232, 150, 255)
    GOLD = (255, 208, 110)
    GLOW = (120, 255, 230)
    # arched stem: bottom right -> upper left
    stem_path = [(11, 15), (11, 14), (10, 13), (10, 12), (9, 11), (8, 10),
                 (7, 9), (6, 8), (6, 7), (5, 6), (5, 5), (5, 4)]
    for i, (x, y) in enumerate(stem_path):
        px[x, y] = STEM if i > 2 else STEM_D
    # drooping bell: attaches at (5,4), flares toward the bottom
    bell_rows = {
        4: (4, 7),
        5: (3, 8),
        6: (3, 9),
        7: (2, 10),
        8: (2, 10),
        9: (3, 9),
    }
    for y, (x0, x1) in bell_rows.items():
        for x in range(x0, x1 + 1):
            c = BELL
            if x in (x0, x1) or y >= 8:
                c = BELL_D
            px[x, y] = c
    # highlights and glow
    px[4, 5] = BELL_L
    px[6, 5] = BELL_L
    px[5, 7] = GLOW
    px[8, 7] = GLOW
    # gold stamens peeking under the rim
    px[4, 10] = GOLD
    px[6, 10] = GOLD
    px[8, 10] = GOLD
    px[5, 11] = GOLD
    px[7, 11] = GOLD
    # drifting moon dust
    px[12, 4] = BELL_L
    px[3, 13] = GLOW
    px[13, 9] = GOLD
    return img

def t_spider_hide(index):
    """v55: spider carapace - dark violet chitin plates, magenta veins."""
    img = blank_tile()
    px = img.load()
    rnd = rng(index * 131 + 17)
    TOP = (86, 44, 130)
    MID = (56, 28, 92)
    BOT = (34, 16, 62)
    PLATE = (110, 62, 160)
    VEIN = (196, 60, 140)
    for y in range(16):
        t = y / 15.0
        base = lerp(TOP, MID, min(1.0, t * 1.4)) if t < 0.5 else lerp(MID, BOT, (t - 0.5) / 0.5)
        for x in range(16):
            n = rnd.random()
            c = base
            if n > 0.82:
                c = lerp(base, PLATE, 0.6)
            elif n < 0.06:
                c = lerp(base, (230, 200, 255), 0.3)
            px[x, y] = (c[0], c[1], c[2], 255)
    for vy in (3, 8, 12):
        for x in range(16):
            if (x + vy) % 3 != 0:
                continue
            px[x, vy] = VEIN
            if x % 2 == 0 and vy + 1 < 16:
                px[x, vy + 1] = lerp(VEIN, MID, 0.4)
    for x in range(16):
        px[x, 6] = lerp(PLATE, (255, 255, 255), 0.25)
    for x, y in ((3, 2), (4, 2), (10, 9), (11, 9)):
        px[x, y] = (214, 180, 255, 255)
    return img


def t_egg_shell(index):
    """v54: alien egg shell - violet mottled hide with dark veins and a wet
    highlight; used by the 3D cocoon mesh (opaque surface tile)."""
    img = blank_tile()
    px = img.load()
    rnd = rng(index * 977 + 5)
    TOP = (216, 156, 255)
    MID = (176, 84, 255)
    BOT = (110, 40, 190)
    VEIN = (84, 26, 150)
    for y in range(16):
        t = y / 15.0
        base = lerp(TOP, MID, min(1.0, t * 1.6)) if t < 0.55 else lerp(MID, BOT, (t - 0.55) / 0.45)
        for x in range(16):
            n = rnd.random()
            c = base
            if n > 0.86:
                c = lerp(base, VEIN, 0.55)
            elif n < 0.07:
                c = lerp(base, (255, 230, 255), 0.45)
            px[x, y] = (c[0], c[1], c[2], 255)
    # dark branching veins
    for vx, vy in ((3, 9), (11, 6), (7, 13)):
        x, y = vx, vy
        for _ in range(6):
            px[x, y] = VEIN
            x = max(0, min(15, x + rnd.choice((-1, 0, 1))))
            y = max(0, min(15, y + rnd.choice((-1, 1))))
    # wet highlight
    for x, y in ((4, 3), (5, 3), (4, 4), (5, 4), (6, 4)):
        px[x, y] = (240, 214, 255, 255)
    return img


def t_explosive(index):
    """v51: volatile void barrel - black with hazard-orange stripes."""
    img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 15, 15], fill=(22, 20, 26))
    d.rectangle([0, 0, 15, 15], outline=(10, 9, 14))
    # diagonal hazard stripes
    for y in range(-16, 17, 8):
        for i in range(16):
            yy = y + i
            if 0 <= yy < 16:
                d.point((i, yy), fill=(255, 140, 30))
                d.point((i, yy + 1), fill=(255, 140, 30))
                d.point(((i + 8) % 16, yy), fill=(255, 140, 30))
                d.point(((i + 8) % 16, yy + 1), fill=(255, 140, 30))
    # rivets / glow dots
    d.rectangle([3, 3, 4, 4], fill=(255, 200, 60))
    d.rectangle([11, 11, 12, 12], fill=(255, 200, 60))
    d.rectangle([11, 3, 12, 4], fill=(120, 60, 10))
    d.rectangle([3, 11, 4, 12], fill=(120, 60, 10))
    return img


def t_void_mushroom(index):
    """v52: void mushroom sprite - pale stem, violet cap, gold specks."""
    img = blank_tile()
    px = img.load()
    STEM = (226, 210, 250)
    STEM_D = (176, 156, 220)
    CAP = (176, 84, 255)
    CAP_D = (138, 52, 224)
    CAP_L = (216, 156, 255)
    SPECK = (255, 208, 120)
    # stem
    for y in range(8, 15):
        for x in (7, 8):
            px[x, y] = STEM_D if x == 7 else STEM
    px[7, 15] = STEM_D
    px[8, 15] = STEM
    # cap dome
    cap_rows = {
        2: (5, 10), 3: (4, 11), 4: (3, 12), 5: (3, 12), 6: (4, 11), 7: (5, 10),
    }
    for y, (x0, x1) in cap_rows.items():
        for x in range(x0, x1 + 1):
            c = CAP
            if y >= 6 or x in (x0, x1):
                c = CAP_D
            if y <= 3 and x in (x0 + 2, x0 + 3, x1 - 2):
                c = CAP_L
            px[x, y] = c
    px[5, 4] = SPECK
    px[9, 3] = SPECK
    px[7, 6] = SPECK
    return img


def t_white(index):
    img = blank_tile()
    for y in range(TILE):
        for x in range(TILE):
            px = img.load()
            px[x, y] = (255, 255, 255, 255)
    return img


def t_glassbell(index):
    """v59.5: glassbell - a translucent teal bell on a thin stalk (1.55
    blocks tall). Bioluminescent aqua glass with a pale rim (the alien-
    flora palette: electric blues/teals glowing on dark)."""
    img = blank_tile()
    px = img.load()
    D = (16, 62, 74)      # deep teal stalk
    T = (44, 150, 158)    # teal glass
    A = (110, 232, 226)   # aqua glow
    P = (214, 252, 248)   # pale rim
    G = (255, 214, 110)   # gold clapper
    # stalk
    for y in range(9, 16):
        px[7, y] = D
        px[8, y] = T if y < 12 else D
    px[6, 11] = D
    px[9, 13] = D
    # glass bell: dome rows 1-7, open mouth at row 7
    dome = {
        1: (7, 8), 2: (6, 9), 3: (5, 10), 4: (5, 10),
        5: (4, 11), 6: (4, 11), 7: (4, 11),
    }
    for y, (x0, x1) in dome.items():
        for x in range(x0, x1 + 1):
            edge = x in (x0, x1) or y in (1, 7)
            px[x, y] = P if edge else (A if (x + y) % 3 else T)
    # highlight + clapper
    px[5, 2] = P
    px[6, 2] = P
    px[7, 8] = G
    px[8, 8] = (200, 160, 70)
    return img


def t_embercup(index):
    """v59.5: embercup - a warm ember-gold cup on a dark stem (1.35
    blocks). The one warm accent in the meadow, like a coal that took
    root; rim glows as if smouldering."""
    img = blank_tile()
    px = img.load()
    D = (52, 30, 22)      # charred stem
    S = (94, 52, 30)      # stem light
    E = (232, 96, 40)     # ember
    E2 = (255, 160, 60)   # ember light
    G = (255, 232, 150)   # gold rim
    for y in range(9, 16):
        px[7, y] = D
        px[8, y] = S if y < 12 else D
    px[6, 12] = D
    px[9, 14] = D
    # cup: two leaf-curves meeting upward, rows 2-8
    cup = {
        2: (7, 8), 3: (6, 9), 4: (5, 10), 5: (5, 10),
        6: (4, 11), 7: (5, 10), 8: (6, 9),
    }
    for y, (x0, x1) in cup.items():
        for x in range(x0, x1 + 1):
            if y in (6, 7, 8):
                px[x, y] = E
            else:
                px[x, y] = E2 if x in (x0, x1) else E
    # glowing rim
    for x in range(4, 12):
        px[x, 6] = G
    px[5, 5] = G
    px[10, 5] = G
    px[7, 3] = G
    px[8, 3] = (255, 248, 200)
    # a spark above the cup
    px[8, 0] = (255, 200, 90)
    return img


def t_voidorchid(index):
    """v59.5: void orchid - deep magenta orchid over near-black foliage
    (1.7 blocks). Real 'black plant' botany (dark foliage catches light)
    plus the palette's magenta: petals edged pale violet, heart almost
    black."""
    img = blank_tile()
    px = img.load()
    K = (18, 12, 24)      # black foliage
    K2 = (30, 20, 40)     # foliage light
    M = (196, 44, 150)    # magenta petal
    M2 = (238, 110, 200)  # petal light
    V = (240, 214, 246)   # pale violet edge
    # dark leaves around the stem base
    for (x, y) in [(4, 14), (5, 13), (3, 15), (11, 14), (10, 13), (12, 15),
                   (5, 11), (10, 11), (6, 15), (9, 15)]:
        px[x, y] = K
    px[5, 12] = K2
    px[10, 12] = K2
    # stem
    for y in range(7, 16):
        px[7, y] = K2
        px[8, y] = K
    # orchid bloom rows 1-6: five petals
    petals = [(5, 3), (10, 3), (4, 5), (11, 5), (7, 1), (8, 1)]
    for (x, y) in petals:
        px[x, y] = V
    body = [(6, 2), (9, 2), (6, 3), (7, 3), (8, 3), (9, 3),
            (5, 4), (6, 4), (7, 4), (8, 4), (9, 4), (10, 4),
            (6, 5), (7, 5), (8, 5), (9, 5), (7, 6), (8, 6)]
    for (x, y) in body:
        px[x, y] = M
    highlights = [(6, 3), (9, 4), (7, 5)]
    for (x, y) in highlights:
        px[x, y] = M2
    # near-black heart
    px[7, 4] = K
    px[8, 4] = K
    px[7, 5] = (40, 16, 44)
    return img


def t_frostfern(index):
    """v59.5: frostfern - an icy pale-blue fern frond unrolling (1.8
    blocks). Paired pinnae climb the stalk and shrink toward the tip;
    white frost on the upper edges."""
    img = blank_tile()
    px = img.load()
    D = (58, 106, 150)    # frond base
    I = (140, 200, 240)   # ice blue
    W = (226, 246, 255)   # frost white
    S = (86, 140, 180)    # stem
    for y in range(1, 16):
        px[7, y] = S
        px[8, y] = D if y > 3 else S
    # pinnae pairs shrink toward the tip
    span = {2: 1, 3: 1, 4: 2, 5: 2, 6: 3, 7: 3, 8: 3, 9: 2, 10: 2, 11: 1, 12: 1}
    for y, half in span.items():
        for d in range(1, half + 1):
            px[7 - d, y] = I
            px[8 + d, y] = I
            if half >= 2 and d == 1:
                px[7 - d, y - 1] = W if y <= 6 else I
                px[8 + d, y - 1] = W if y <= 6 else I
    # curled tip
    px[8, 0] = W
    px[9, 1] = I
    # frost speckles
    px[5, 8] = W
    px[10, 6] = W
    px[4, 10] = I
    px[11, 9] = I
    return img


def build_atlas():
    atlas = Image.new("RGBA", (ATLAS, ATLAS), (0, 0, 0, 0))
    tiles = {
        1: t_stone(1),
        2: t_dirt(2),
        3: t_grass_side(3),
        4: t_planks(4),
        5: ore_tile(5, (255, 170, 60), GOLD, (255, 240, 190)),
        6: ore_tile(6, (150, 220, 255), CRYSTAL_MID, CRYSTAL_PALE),
        7: ore_tile(7, (60, 40, 90), (100, 60, 160), VIOLET),
        8: t_log_side(8),
        9: t_log_top(9),
        10: t_leaves(10),
        11: t_sand(11),
        12: t_rose(12),
        13: t_dandelion(13),
        28: t_bellflower(28),
        29: t_starbloom(29),
        30: t_spiralfern(30),
        31: t_tulip(31),
        32: t_glowgrass(32),
        33: t_lanternberry(33),
        34: t_egg_shell(34),
        35: t_void_shard(35),
        36: t_spider_hide(36),
        37: t_star_reed(37),
        38: t_moon_bell(38),
        39: t_void_tuft(39),
        40: t_spell_scroll(40),
        41: t_sedge(41),
        42: t_moth(42),
        43: t_cloud(43),
        44: t_undersky(44),
        45: t_glassbell(45),
        51: t_crystal_stalk(51),
        52: t_void_puff(52),
        55: t_grazer_hide(55),
        56: t_ember_rock(56),
        57: t_ember_turf(57),
        58: t_frost_turf(58),
        59: t_ember_tuft(59),
        60: t_frost_tuft(60),
        61: t_grazer_fur(61),
        46: t_embercup(46),
        47: t_voidorchid(47),
        48: t_frostfern(48),
        14: t_water(14),
        15: t_lava(15),
        16: t_fire(16),
        17: t_glass(17),
        18: t_grass_top(18),
        19: t_launch_pad_top(19),
        20: t_warp_core(20),
        21: t_void_rock(21),
        22: t_crystal(22),
        23: t_launch_pad_side(23),
        24: t_white(24),
        26: t_explosive(26),
        27: t_void_mushroom(27),
    }
    for index in sorted(tiles):
        paste(atlas, index, tiles[index])
    return atlas


# ------------------------------------------------------------- humanoid ----
def build_humanoid(path):
    """128x128 cosmic explorer skin matching the entity model UV layout."""
    img = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    SUIT = (52, 38, 92)
    SUIT_D = (38, 27, 68)
    SUIT_L = (74, 56, 124)
    SKIN = (232, 188, 152)
    SKIN_D = (198, 156, 122)
    SKIN_L = (248, 214, 182)
    VISOR = (94, 231, 255)
    VISOR_D = (36, 150, 200)
    TEAL = (60, 220, 190)
    GOLD2 = (255, 186, 92)

    def rect(x, y, w, h, c, outline=None):
        d.rectangle([x, y, x + w - 1, y + h - 1], fill=c)
        if outline:
            d.rectangle([x, y, x + w - 1, y + h - 1], outline=outline)

    def grad(x, y, w, h, c1, c2):
        for yy in range(h):
            t = yy / max(1, h - 1)
            d.rectangle([x, y + yy, x + w - 1, y + yy], fill=lerp(c1, c2, t))

    # ---- head: WEST(0,14,14x16) EAST(30,14) SOUTH(44,14) NORTH(14,14)
    def head_face(x, y, front=False):
        grad(x, y, 16, 16, SKIN, SKIN_D)
        d.rectangle([x, y, x + 15, y], fill=SKIN_L)
        if front:  # south face: visor
            grad(x + 2, y + 5, 12, 4, VISOR, VISOR_D)
            d.rectangle([x + 2, y + 4, x + 13, y + 4], fill=(180, 245, 255))
            d.point((x + 4, y + 6), fill=(230, 255, 255))
            d.rectangle([x + 2, y + 10, x + 13, y + 12], fill=SUIT)
            d.rectangle([x + 5, y + 11, x + 10, y + 11], fill=TEAL)
        else:
            d.rectangle([x, y + 12, x + 15, y + 15], fill=SUIT)
            d.rectangle([x, y + 11, x + 15, y + 11], fill=GOLD2)
        d.rectangle([x, y, x + 15, y], fill=lerp(SKIN, (255, 255, 255), 0.3))

    head_face(14, 14, front=False)   # north (back of head)
    head_face(0, 14)                 # west
    head_face(30, 14)                # east
    head_face(44, 14, front=True)    # south (face)
    # head up (30,14,-16,-14) -> mirrored rect at (14,0)? layout uses (30,0..)? keep plain skin patch
    grad(30, 0, 16, 14, SKIN, SKIN_D)
    grad(46, 0, 16, 14, SKIN_D, SUIT)

    # ---- torso: NORTH(6,36,14x18) EAST(20,36,6x18) SOUTH(26,36,14x18) WEST(0,36,6x18)
    def suit_panel(x, y, w, h, accent=False):
        grad(x, y, w, h, SUIT, SUIT_D)
        d.rectangle([x, y, x + w - 1, y], fill=SUIT_L)
        if accent:
            d.rectangle([x + w // 2 - 3, y + 3, x + w // 2 + 2, y + 6], fill=TEAL)
            d.point((x + w // 2 - 3, y + 3), fill=(210, 255, 250))
            d.rectangle([x + 2, y + h - 5, x + w - 3, y + h - 4], fill=GOLD2)

    suit_panel(6, 36, 14, 18, accent=True)
    suit_panel(26, 36, 14, 18)
    suit_panel(0, 36, 6, 18)
    suit_panel(20, 36, 6, 18)
    grad(20, 18, 14, 6, SUIT_L, SUIT)   # torso up region
    grad(34, 30, 14, 6, SUIT_D, SUIT)

    # ---- arms: 6x20 columns. right arm x=46..69, left arm x=70..93 at y=36
    def arm(x, y, glove=False):
        grad(x, y, 6, 20, SUIT, SUIT_D)
        d.rectangle([x, y, x + 5, y], fill=SUIT_L)
        if glove:
            grad(x, y + 14, 6, 6, (34, 30, 40), (24, 20, 30))
            d.rectangle([x, y + 14, x + 5, y + 14], fill=TEAL)
    arm(46, 36, glove=True)
    arm(52, 36, glove=True)
    arm(58, 36, glove=True)
    arm(64, 36, glove=True)
    arm(70, 36, glove=True)
    arm(76, 36, glove=True)
    arm(82, 36, glove=True)
    arm(88, 36, glove=True)

    # ---- legs: 6x20 at y=6, x=60..104 (right), x=60..84 (left)
    def leg(x, y, boot=True):
        grad(x, y, 6, 20, SUIT_D, (28, 20, 50))
        if boot:
            d.rectangle([x, y + 16, x + 5, y + 19], fill=(30, 26, 40))
            d.rectangle([x, y + 16, x + 5, y + 16], fill=GOLD2)
    leg(84, 6); leg(90, 6); leg(96, 6); leg(102, 6)
    leg(60, 6); leg(66, 6); leg(72, 6); leg(78, 6)

    # ---- fingers strip (custom area, y=60..75): 4 fingers x 6 wide
    fx0 = 8
    for i in range(4):
        x = fx0 + i * 8
        grad(x, 60, 6, 8, SKIN, SKIN_D)
        d.rectangle([x, 60, x + 5, 60], fill=lerp(SKIN, (255, 255, 255), 0.25))
        d.rectangle([x, 67, x + 5, 67], fill=SKIN_D)

    # ---- v43.3: fill palm faces (arm DOWN UVs) and boot soles (leg DOWN UVs).
    # These were fully transparent, punching holes through the hands/feet.
    # Palms read as gloved hands: dark glove gradient + three knuckle grooves.
    for px0 in (52, 76):
        grad(px0, 30, 6, 6, (40, 36, 48), (26, 22, 34))
        for k in range(3):
            d.rectangle([px0 + 1 + k * 2, 31, px0 + 1 + k * 2, 34], fill=(18, 15, 24))
        d.rectangle([px0, 30, px0 + 5, 30], fill=TEAL)
    # boot soles: dark sole with a gold toe edge, matching the boots
    for px0 in (78, 102):
        d.rectangle([px0, 0, px0 + 5, 5], fill=(30, 26, 40))
        d.rectangle([px0, 0, px0 + 5, 0], fill=GOLD2)
        d.rectangle([px0, 5, px0 + 5, 5], fill=(20, 17, 28))

    img.save(path)


# v61: tiles that belong to CUBE blocks must be fully opaque. Decoration
# passes (crack_lines with alpha=140, dim speckle) used to leave alpha
# 110-230 pixels inside stone/ore/log tiles; the chunk shader discards
# texels below a=0.5 and the GL blender always blends, so those pixels
# punched visible holes and see-through specks into the cubes.
CUBE_TILES = {1, 2, 4, 5, 6, 7, 8, 9, 11, 15, 18, 19, 20, 21, 23, 26, 56, 57, 58}
# (intentionally translucent/cutout tiles stay untouched: 10 leaves,
# 14 water, 17 glass, 22 crystal, every SPRITE/flora tile)


def enforce_opaque_cube_tiles(atlas):
    px = atlas.load()
    for tile in CUBE_TILES:
        x0 = (tile % 16) * 16
        y0 = (tile // 16) * 16
        for y in range(y0, y0 + 16):
            for x in range(x0, x0 + 16):
                r, g, b, a = px[x, y]
                if a != 255:
                    px[x, y] = (r, g, b, 255)


def _px_ell(px, cx, cy, rx, ry, col):
    """filled ellipse helper (float centers, crisp pixel edges)"""
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            dx = (x + 0.5 - cx) / rx
            dy = (y + 0.5 - cy) / ry
            if dx * dx + dy * dy <= 1.0:
                if 0 <= x < 32 and 0 <= y < 32:
                    px[x, y] = col


def _px_disc(px, cx, cy, r, col):
    _px_ell(px, cx, cy, r, r, col)


def t_void_tree_big():
    """v61.3: void tree, 32x32 - a real silhouette: lobed teal canopy
    with dithered depth bands, carved gaps, aerial glow-roots, an
    indigo trunk with a fork and flaring roots. 5+ blocks tall live."""
    img = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    px = img.load()
    TD = (26, 17, 42)
    T  = (52, 36, 78)
    TL = (88, 66, 122)
    CD = (11, 70, 64)
    C  = (24, 126, 110)
    CL = (72, 200, 184)
    CP = (196, 255, 246)
    A  = (110, 240, 224)

    # ---- canopy: lobe mass ----
    lobes = [(15.5, 8.0, 11.0, 6.2), (7.0, 10.5, 5.5, 4.2), (24.0, 9.5, 6.0, 4.6),
             (11.5, 4.5, 6.0, 3.4), (20.5, 4.0, 5.0, 3.0), (15.5, 2.0, 4.0, 2.2),
             (27.0, 13.0, 3.4, 2.6), (4.5, 14.0, 3.0, 2.2)]
    for cx, cy, rx, ry in lobes:
        _px_ell(px, cx, cy, rx, ry, C)
    # carve two notch gaps into the silhouette (breaks the outline)
    _px_disc(px, 3.0, 7.0, 2.6, (0, 0, 0, 0))
    _px_disc(px, 29.0, 6.5, 2.2, (0, 0, 0, 0))
    _px_disc(px, 19.5, 14.2, 1.7, (0, 0, 0, 0))

    # ---- canopy shading: light from up-left, dithered bands ----
    for y in range(32):
        for x in range(32):
            r, g, b, a = px[x, y]
            if a == 0 or y > 16:
                continue
            dx = (x - 15.5) / 12.0
            dy = (y - 7.5) / 7.0
            d = (dx * 0.72 + dy * 0.70)          # -1 light side .. +1 shade
            if d < -0.28:
                c = CL
            elif d < 0.16:
                c = C
            else:
                c = CD
            # dither the band borders (classic 2x2 checker)
            if -0.34 < d < -0.22 or (0.10 < d < 0.22) and (x + y) % 2 == 0:
                c = CL if c == C else (C if c == CD else C)
            if d > 0.30 and (x + y) % 2 == 0:
                c = CD
            px[x, y] = c
    # pale moonlit tips on the top lobes + glow drips on the underside
    for x, y in ((10, 2), (15, 1), (21, 2), (6, 6), (24, 5)):
        px[x, y] = CP
        if (x + y) % 2 == 0:
            px[x + 1, y] = CL
    for x, y in ((7, 14), (12, 15), (17, 14), (22, 14), (26, 12), (4, 12)):
        px[x, y] = A

    # ---- trunk: bent, forked, flaring ----
    for y in range(14, 30):
        w = 2 if y < 20 else (3 if y < 27 else 4)
        x0 = 14 if y < 25 else 13
        for x in range(x0, x0 + w):
            px[x, y] = T
        px[x0, y] = TL if y < 22 else T
        px[x0 + w - 1, y] = TD
    # fork branch up-left into the canopy
    for i, (x, y) in enumerate(((13, 16), (12, 15), (11, 14), (10, 13))):
        px[x, y] = T if i < 2 else TD
    # root flare
    roots = ((10, 30, 11, 31), (12, 29, 12, 31), (17, 30, 18, 31), (16, 29, 16, 31))
    for x0, y0, x1, y1 in roots:
        for y in range(y0, y1 + 1):
            px[10 + (y - 30), 30] = TD
            px[18 - (y - 30), 31] = TD
    px[12, 30] = T; px[13, 31] = T
    px[16, 30] = TD; px[15, 31] = TD
    px[14, 29] = TD   # knot
    # aerial glow-root strands
    for x, y0, y1 in ((9, 15, 21), (22, 15, 19)):
        for y in range(y0, y1):
            px[x, y] = (60, 170, 158, 255) if (y - y0) % 2 else A
        px[x, y1] = CP
    return img


def t_lantern_tree_big():
    """v61.2: lantern tree, 32x32 - copper fork-trunk, two-and-a-half
    leafy clusters full of gold lanterns, three lanterns hanging below
    the canopy on stems."""
    img = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    px = img.load()
    TD = (32, 21, 15)
    T  = (72, 46, 27)
    TL = (110, 74, 40)
    LF = (28, 58, 46)
    LM = (46, 90, 62)
    G  = (255, 198, 84)
    G2 = (216, 146, 58)
    W  = (255, 242, 170)

    # ---- leaf clusters ----
    clusters = [(11.0, 7.5, 8.5, 5.8), (22.0, 9.0, 6.8, 5.2), (6.0, 12.0, 4.6, 3.4),
                (16.5, 3.5, 5.5, 2.8), (25.0, 13.5, 3.6, 2.4)]
    for cx, cy, rx, ry in clusters:
        _px_ell(px, cx, cy, rx, ry, LM)
    _px_disc(px, 2.0, 8.0, 2.2, (0, 0, 0, 0))
    _px_disc(px, 29.5, 10.0, 1.8, (0, 0, 0, 0))
    for y in range(32):
        for x in range(32):
            r, g, b, a = px[x, y]
            if a == 0 or y > 16:
                continue
            if (x * 5 + y * 3) % 7 < 2:
                px[x, y] = LF
    # lanterns embedded in the leaf (bright core + dim ring)
    for lx, ly, big in ((9, 5, 1), (14, 8, 0), (19, 4, 1), (23, 8, 0), (6, 10, 0),
                        (12, 11, 0), (21, 11, 1), (26, 12, 0), (16, 3, 0)):
        px[lx, ly] = G
        if big:
            px[lx, ly - 1] = G2
            px[lx - 1, ly] = G2
            px[lx + 1, ly] = G2
        px[lx, ly - (1 if big else 0)] = W if big else px[lx, ly - (1 if big else 0)]
    # wicks on the big ones
    px[9, 5] = W
    px[19, 4] = W
    px[21, 11] = W

    # ---- trunk: fork into two branches ----
    for y in range(15, 30):
        w = 2 if y < 21 else (3 if y < 27 else 4)
        x0 = 15 if y < 25 else 14
        for x in range(x0, x0 + w):
            px[x, y] = T
        px[x0, y] = TL if y < 23 else T
        px[x0 + w - 1, y] = TD
    # left branch to the big cluster
    for x, y in ((14, 16), (13, 15), (12, 14), (11, 13), (11, 12)):
        px[x, y] = T if y > 14 else TD
    # right branch
    for x, y in ((17, 16), (18, 15), (19, 14), (20, 13), (21, 12)):
        px[x, y] = T if y > 14 else TD
    px[15, 28] = TD  # knot
    # root flare
    px[13, 30] = TD; px[14, 31] = TD; px[12, 31] = TD
    px[17, 30] = TD; px[18, 31] = TD; px[19, 31] = TD
    px[15, 31] = T; px[16, 31] = T

    # ---- three hanging lanterns below the canopy ----
    hangs = [(8, 17, 4), (17, 18, 3), (24, 16, 5)]
    for hx, hy, ln in hangs:
        for y in range(hy, hy + ln):
            px[hx, y] = TD
        px[hx, hy + ln] = G2
        px[hx, hy + ln + 1] = G
        px[hx, hy + ln + 2] = W if ln == 4 else G
        if ln == 4:
            px[hx - 1, hy + ln + 1] = G2
            px[hx + 1, hy + ln + 1] = G2
    return img


def t_crystal_stalk(index):
    """v61.2: crystal stalk - a tight cluster of raw cosmic crystal
    shards rising from a rocky base (1.9 blocks). Rigid, no sway."""
    img = blank_tile()
    px = img.load()
    RK = (52, 40, 74)       # rocky base
    RK_D = (36, 26, 54)
    C_D = (40, 96, 128)     # shard deep
    C = (90, 190, 216)      # shard
    C_L = (190, 244, 252)   # shard edge
    # base rocks
    for x in range(5, 11):
        px[x, 14] = RK_D
        px[x, 15] = RK_D
    px[6, 13] = RK
    px[9, 13] = RK
    # main shard: tall kite x=7-8, rows 1-13
    kite = {1: (7, 8), 2: (7, 8), 3: (6, 9), 4: (6, 9), 5: (6, 9),
            6: (6, 9), 7: (6, 9), 8: (7, 8), 9: (7, 8), 10: (7, 8)}
    for y, (x0, x1) in kite.items():
        for x in range(x0, x1 + 1):
            px[x, y] = C_D if x == x0 or y == 10 else C
    px[7, 1] = C_L
    px[7, 3] = C_L
    px[6, 5] = C_L
    # two side shards
    for y in range(6, 13):
        px[4, y] = C if y < 11 else C_D
    px[4, 5] = C_L
    for y in range(8, 13):
        px[11, y] = C_D if y > 10 else C
    px[11, 7] = C
    return img


def t_void_puff(index):
    """v61.2: void puff - a fat ball of luminous cotton on a short stem
    (0.85 blocks). Soft lavender-white fluff over a teal stem; the one
    pale accent in the meadow."""
    img = blank_tile()
    px = img.load()
    ST = (24, 88, 78)
    F_D = (148, 134, 196)   # fluff shadow
    F = (198, 188, 232)     # fluff
    F_L = (240, 236, 255)   # fluff light
    T = (150, 255, 236)     # teal mote
    for y in range(11, 16):
        px[7, y] = ST
        px[8, y] = ST
    px[6, 13] = ST
    ball = {3: (5, 10), 4: (4, 11), 5: (3, 12), 6: (3, 12),
            7: (4, 11), 8: (5, 10), 9: (6, 9), 10: (7, 8)}
    for y, (x0, x1) in ball.items():
        for x in range(x0, x1 + 1):
            edge = x in (x0, x1) or y in (3, 10)
            if edge:
                px[x, y] = F_D
            else:
                px[x, y] = F if (x * 5 + y * 3) % 4 else F_L
    px[6, 4] = F_L
    px[9, 5] = F_L
    px[5, 7] = T
    px[10, 8] = T
    px[7, 8] = F_L
    return img


def t_grazer_hide(index):
    """v62: grazer hide - mottled warm-sand fur with soft teal rosettes
    and a pale belly gradient. Creature-only tile (never a block)."""
    img = blank_tile()
    px = img.load()
    F = (176, 158, 138)     # fur base
    F_D = (128, 110, 96)    # fur shadow
    F_L = (214, 200, 178)   # fur light
    R = (122, 142, 116)     # v63: muted sage patch (was loud teal)
    R_D = (92, 110, 88)
    # vertical soft gradient: light top -> shadow bottom
    for y in range(16):
        t = y / 15.0
        base = tuple(int(F[i] * (1 - t) + F_D[i] * t) for i in range(3))
        for x in range(16):
            jitter = ((x * 7 + y * 13) % 5) - 2
            px[x, y] = tuple(max(0, min(255, base[i] + jitter * 6)) for i in range(3)) + (255,)
    # mottling: soft blobs of darker fur
    for bx, by, br in ((3, 4, 2), (11, 3, 3), (7, 8, 3), (13, 10, 2), (2, 11, 2), (9, 13, 2)):
        for y in range(16):
            for x in range(16):
                if (x - bx) ** 2 + (y - by) ** 2 <= br * br:
                    r, g, b, a = px[x, y]
                    px[x, y] = (r * 82 // 100, g * 82 // 100, b * 82 // 100, 255)
    # v63: sparse ASYMMETRIC sage patches - a few broken crescents on
    # the back only, low contrast, so blobs never read as green rings
    crescents = [((4, 4), (7, 3)), ((11, 6), (13, 8)), ((6, 9), (8, 11))]
    for (x0, y0), (x1, y1) in crescents:
        steps = abs(x1 - x0) + abs(y1 - y0)
        for s in range(steps + 1):
            xx = x0 + (x1 - x0) * s // max(steps, 1)
            yy = y0 + (y1 - y0) * s // max(steps, 1)
            px[xx, yy] = R + (255,)
            if s % 2 == 0 and 0 <= xx + 1 < 16:
                px[xx + 1, yy] = R_D + (255,)
    # top highlight band (light from above)
    for x in range(16):
        r, g, b, a = px[x, 0]
        px[x, 0] = (min(255, r + 30), min(255, g + 28), min(255, b + 24), 255)
    return img


def t_grazer_fur(index):
    """v63: plain grazer fur - same coat as the hide, no markings.
    Used for ears, tail and legs so they never show rosettes."""
    img = t_grazer_hide(index)
    px = img.load()
    R = (122, 142, 116)
    R_D = (92, 110, 88)
    # repaint the sage pixels with the local fur tone instead
    for y in range(16):
        for x in range(16):
            r, gg, b, a = px[x, y]
            if (r, gg, b) == R or (r, gg, b) == R_D:
                base = 176 - y * 3
                jitter = ((x * 7 + y * 13) % 5) - 2
                px[x, y] = (max(0, base + jitter * 6), max(0, base - 18 + jitter * 6),
                            max(0, base - 38 + jitter * 6), 255)
    return img


def t_ember_tuft(index):
    """v63: ember biome grass - charcoal-based warm blades with
    smoldering amber tips, one teal lichen fleck."""
    img = blank_tile()
    px = img.load()
    D = (74, 52, 44)
    M = (138, 92, 52)
    L = (206, 130, 62)
    H = (248, 186, 104)
    blades = [
        (7, 15, 7, 3, L), (8, 15, 9, 1, M), (6, 15, 5, 6, M),
        (9, 15, 11, 4, D), (5, 15, 3, 9, D), (10, 15, 12, 8, M),
        (7, 15, 6, 2, H), (9, 15, 10, 3, L),
    ]
    for x0, y0, x1, ytop, col in blades:
        steps = y0 - ytop
        for s in range(steps + 1):
            xx = x0 + (x1 - x0) * s // max(steps, 1)
            yy = y0 - s
            px[xx, yy] = col
    px[4, 13] = D
    px[11, 13] = D
    px[8, 4] = H
    px[5, 8] = (110, 220, 200)   # ember lichen fleck
    return img


def t_frost_tuft(index):
    """v63: frost biome grass - pale icy blades with a cold glint."""
    img = blank_tile()
    px = img.load()
    D = (108, 130, 152)
    M = (150, 176, 196)
    L = (198, 220, 236)
    H = (240, 250, 255)
    blades = [
        (7, 15, 7, 3, L), (8, 15, 9, 1, M), (6, 15, 5, 6, M),
        (9, 15, 11, 4, D), (5, 15, 3, 9, D), (10, 15, 12, 8, M),
        (7, 15, 6, 2, H), (9, 15, 10, 3, L),
    ]
    for x0, y0, x1, ytop, col in blades:
        steps = y0 - ytop
        for s in range(steps + 1):
            xx = x0 + (x1 - x0) * s // max(steps, 1)
            yy = y0 - s
            px[xx, yy] = col
    px[4, 13] = D
    px[11, 13] = D
    px[8, 4] = H
    px[5, 8] = (88, 172, 160)   # hardy teal blade
    return img


def t_ember_rock(index):
    """v62: ember biome stone - charcoal basalt with dim ember cracks."""
    img = blank_tile()
    px = img.load()
    import random as _r
    rnd = _r.Random(77)
    B = (34, 30, 40)        # basalt
    B_D = (24, 21, 30)
    B_L = (52, 47, 62)
    E = (196, 92, 40)       # ember glow
    E_D = (120, 52, 30)
    for y in range(16):
        for x in range(16):
            n = rnd.randint(-10, 10)
            base = B if (x + y) % 7 else B_L
            px[x, y] = (max(0, base[0] + n), max(0, base[1] + n), max(0, base[2] + n), 255)
    # diagonal crack network with ember fill
    for cx, cy in ((2, 2), (9, 5), (4, 10), (12, 12), (7, 14)):
        x, y = cx, cy
        for step in range(5):
            if 0 <= x < 16 and 0 <= y < 16:
                px[x, y] = E
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    if 0 <= x + dx < 16 and 0 <= y + dy < 16 and (x + y) % 3 == 0:
                        if px[x + dx, y + dy][0] < 60:
                            px[x + dx, y + dy] = E_D
            x += rnd.choice((0, 1, 1, -1))
            y += rnd.choice((1, 0, 1))
    return img


def t_ember_turf(index):
    """v62: ember biome surface - charred turf with smoldering veins."""
    img = blank_tile()
    px = img.load()
    import random as _r
    rnd = _r.Random(913)
    T = (46, 36, 42)        # dark warm turf
    T_L = (64, 50, 54)
    T_D = (30, 24, 30)
    E = (232, 126, 52)      # smolder
    E_L = (255, 190, 110)
    A = (110, 220, 200)     # rare teal lichen
    for y in range(16):
        for x in range(16):
            n = rnd.randint(-9, 9)
            base = T if (x * 3 + y) % 5 else T_L
            if (x + y * 2) % 11 == 0:
                base = T_D
            px[x, y] = (max(0, base[0] + n), max(0, base[1] + n), max(0, base[2] + n), 255)
    # smoldering veins
    for _ in range(4):
        x, y = rnd.randint(0, 15), rnd.randint(0, 15)
        for step in range(4):
            if 0 <= x < 16 and 0 <= y < 16:
                px[x, y] = E_L if step == 0 else E
            x += rnd.choice((1, -1, 0))
            y += rnd.choice((0, 1))
    # sparse teal lichen dots (keeps the palette linked)
    for _ in range(3):
        px[rnd.randint(0, 15), rnd.randint(0, 15)] = A
    return img


def t_frost_turf(index):
    """v62: frost biome surface - pale silver-blue hoarfrost turf."""
    img = blank_tile()
    px = img.load()
    import random as _r
    rnd = _r.Random(4177)
    F = (150, 176, 196)     # frost base
    F_L = (198, 220, 236)   # frost light
    F_D = (108, 130, 152)   # shadow
    I = (240, 250, 255)     # ice sparkle
    T = (88, 172, 160)      # frozen teal grass blades
    for y in range(16):
        for x in range(16):
            n = rnd.randint(-10, 10)
            base = F if (x + 2 * y) % 5 else F_D
            if (x * 2 + y) % 9 == 0:
                base = F_L
            px[x, y] = (max(0, min(255, base[0] + n)), max(0, min(255, base[1] + n)),
                        max(0, min(255, base[2] + n)), 255)
    # tiny frost blades
    for _ in range(6):
        x, y = rnd.randint(0, 15), rnd.randint(2, 15)
        px[x, y] = T
        if rnd.random() < 0.5 and y > 0:
            px[x, y - 1] = (118, 196, 182)
    # sparkles
    for _ in range(5):
        px[rnd.randint(0, 15), rnd.randint(0, 15)] = I
    return img


def main():
    atlas = build_atlas()
    enforce_opaque_cube_tiles(atlas)
    atlas.save("client/textures/terrain.png")
    build_humanoid("client/textures/humanoid.png")
    print("terrain.png + humanoid.png written")


if __name__ == "__main__":
    main()
