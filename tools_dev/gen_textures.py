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
GRASS_DEEP   = (34, 148, 112)
GRASS_MID    = (48, 186, 136)
GRASS_LIGHT  = (88, 224, 164)
GRASS_TIP    = (168, 250, 208)
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


def flower_tile(index, petal_a, petal_b, core):
    img = blank_tile()
    d = ImageDraw.Draw(img)
    d.line([(8, 15), (8, 8)], fill=with_a(GRASS_DEEP, 255))
    d.point((7, 11), fill=with_a(GRASS_MID, 255))
    d.point((9, 12), fill=with_a(GRASS_MID, 255))
    cx, cy = 8, 6
    for dx, dy in ((-1, -1), (1, -1), (-1, 1), (1, 1), (0, -2), (0, 2), (-2, 0), (2, 0)):
        d.point((cx + dx, cy + dy), fill=with_a(lerp(petal_a, petal_b, 0.5), 255))
    for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1), (0, 0)):
        d.point((cx + dx, cy + dy), fill=with_a(petal_a if (dx or dy) else core, 255))
    return img


def t_white(index):
    img = blank_tile()
    for y in range(TILE):
        for x in range(TILE):
            px = img.load()
            px[x, y] = (255, 255, 255, 255)
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
        12: flower_tile(12, MAGENTA, (255, 150, 250), (255, 230, 120)),
        13: flower_tile(13, CRYSTAL_MID, (150, 240, 255), (255, 255, 220)),
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
    }
    for index in range(1, 25):
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


def main():
    atlas = build_atlas()
    atlas.save("client/textures/terrain.png")
    build_humanoid("client/textures/humanoid.png")
    print("terrain.png + humanoid.png written")


if __name__ == "__main__":
    main()
