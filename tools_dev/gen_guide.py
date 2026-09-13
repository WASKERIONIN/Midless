"""v63b: world guide poster - every world object rendered from the real
atlas with labels. Output: docs/world_guide.png (+ docs/WORLD_GUIDE.md).
Rerun after texture changes:  python3 tools_dev/gen_guide.py"""
from PIL import Image, ImageDraw, ImageFont

TILE = 16
SCALE = 3
CELL = 64      # art box (48px + padding)
PITCH = 100    # horizontal cell pitch
SLOTS = 5      # row capacity in slots (trees take 2)
PAD = 18

GROUND = [
    (1,  1,  "Stone",        "Камень"),
    (2,  2,  "Dirt",         "Земля"),
    (3,  18, "Crystal Turf", "Кристальный дёрн"),
    (19, 21, "Void Rock",    "Пустотная порода"),
    (20, 22, "Cosmic Crystal", "Космический кристалл"),
    (14, 17, "Glass",        "Стекло"),
    (56, 56, "Ember Rock",   "Угольный камень"),
    (57, 57, "Ember Turf",   "Угольный дёрн"),
    (58, 58, "Frost Turf",   "Морозный дёрн"),
]
ORES = [
    (5,  5,  "Gold Ore",     "Золотая руда"),
    (6,  6,  "Shard Gravel", "Осколочный гравий"),
    (7,  7,  "Gloom Amber",  "Мрачный янтарь"),
    (26, 26, "Volatile Barrel", "Летучая бочка"),
    (24, 5,  "Gold Plate",   "Золотая плита"),
    (23, 24, "Chrome Plate", "Хромовая плита"),
    (22, 20, "Warp Core",    "Варп-ядро"),
    (21, 23, "Launch Pad",   "Стартовая площадка"),
]
FLORA = [
    (12, 12, "Rose",          "Роза"),
    (13, 13, "Dandelion",     "Одуванчик"),
    (28, 28, "Void Bellflower", "Пустотный колокольчик"),
    (29, 29, "Starbloom",     "Звездроцвет"),
    (30, 30, "Spiral Fern",   "Спиральный папоротник"),
    (31, 31, "Twin Tulip",    "Двойной тюльпан"),
    (32, 32, "Glow Grass",    "Светящаяся трава"),
    (33, 33, "Lanternberry",  "Фонарная ягода"),
    (47, 47, "Void Orchid",   "Пустотная орхидея"),
    (48, 48, "Frostfern",     "Морозник"),
    (39, 39, "Void Tuft",     "Пустотная метёлка"),
    (41, 41, "Void Sedge",    "Пустотная осока"),
    (52, 52, "Void Puff",     "Пустотная пуховка"),
    (59, 59, "Ember Tuft",    "Угольная трава"),
    (60, 60, "Frost Tuft",    "Морозная трава"),
    (73, 73, "Void Glowcaps", "Пустотные светлогрибы"),
    (74, 74, "Cinder Trumpet", "Угольная труба"),
    (75, 75, "Frost Puffball", "Морозный дождевик"),
    (67, 67, "Smolderhead",   "Тлеголовка"),
    (68, 68, "Cinder Buds",   "Угольные бутоны"),
    (71, 71, "Glacier Dewdrop", "Ледяная снегокапля"),
    (72, 72, "Ringbloom",     "Кольцецвет"),
    (76, 76, "Ember Lantern", "Угольный фонарик"),
    (77, 77, "Frost Burst",   "Морозный салют"),
]
TREES = [
    (49, 49, "Void Tree (big)", "Пустотное дерево"),
    (50, 53, "Lantern Tree",    "Фонарное дерево"),   # id 50 renders from tile 53

    (37, 37, "Star Reed",       "Звёздный тростник"),
    (38, 38, "Moon Bell",       "Лунный колокол"),
    (51, 51, "Crystal Stalk",   "Кристальный стебель"),
]
CREATURES = [
    (55, 55, "Grazer coat",    "Шкурка гризера"),
    (54, 54, "Spider carapace", "Панцирь паука"),
]
BIOME_NOTE = [
    ("Классический архипелаг", "кристальный дёрн всегда в пустотной траве; все классические цветы, пустотные деревья; на траве - пустотные светлогрибы"),
    ("Угольные острова", "угольный дёрн всегда в угольной траве; тлеголовка, угольные бутоны, угольный фонарик, двойной тюльпан, фонарная ягода, фонарные деревья; на траве - угольная труба"),
    ("Морозные острова", "морозный дёрн всегда в морозной траве; морозный салют, ледяная снегокапля, кольцецвет, морозник, пуховки, пустотные деревья; на траве - морозный дождевик"),
]
SECTIONS = [
    ("БЛОКИ И ЗЕМЛЯ / GROUND BLOCKS", GROUND),
    ("РУДЫ И ОСОБЫЕ БЛОКИ / ORES & SPECIAL", ORES),
    ("РАСТЕНИЯ / FLORA", FLORA),
    ("ДЕРЕВЬЯ И ГИГАНТЫ / TREES & GIANTS", TREES),
    ("СУЩЕСТВА (шкурки) / CREATURE SKINS", CREATURES),
]


def try_font(size):
    for p in ("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
              "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"):
        try:
            return ImageFont.truetype(p, size)
        except Exception:
            continue
    return ImageFont.load_default()


def tile_img(atlas, index, span=1):
    x = (index % 16) * TILE
    y = (index // 16) * TILE
    c = atlas.crop((x, y, x + TILE * span, y + TILE * span))
    return c.resize((TILE * SCALE * span, TILE * SCALE * span), Image.NEAREST)


def draw_section(img, d, atlas, x, y, title, items, font, f_small):
    d.text((x, y), title, fill=(240, 230, 200), font=font)
    y += 40
    cx, row = x, 0
    for bid, t, en, ru in items:
        span = 2 if bid in (49, 50) else 1
        art = tile_img(atlas, t, span)
        d.rectangle([cx - 4, y - 4, cx + 48 * span + 4, y + 48 * span + 4],
                    fill=(10, 10, 18), outline=(70, 70, 90))
        img.paste(art, (cx, y))
        words = ru.split()
        l1 = words[0] if words else ""
        l2 = " ".join(words[1:])
        d.text((cx, y + 48 * span + 8), l1, fill=(215, 215, 225), font=f_small)
        if l2:
            d.text((cx, y + 48 * span + 26), l2, fill=(215, 215, 225), font=f_small)
        d.text((cx, y + 48 * span + 44), f"id {bid}", fill=(130, 130, 150), font=f_small)
        cx += PITCH * span
        row += span
        if row >= SLOTS:
            cx, row = x, 0
            y += CELL + 58
    if row:
        y += CELL + 58
    return y + 12


def main():
    atlas = Image.open("client/textures/terrain.png").convert("RGBA")
    W = SLOTS * PITCH + PAD * 2
    canvas = Image.new("RGB", (W, 4000), (16, 16, 26))
    d = ImageDraw.Draw(canvas)
    f_big = try_font(30)
    font = try_font(21)
    f_small = try_font(14)
    d.text((PAD, 20), "MIDLESS — ГИД ПО МИРУ (v63)",
           fill=(255, 224, 130), font=f_big)
    y = 78
    for title, items in SECTIONS:
        y = draw_section(canvas, d, atlas, PAD, y, title, items, font, f_small)
    d.text((PAD, y), "БИОМЫ / BIOMES", fill=(240, 230, 200), font=font)
    y += 40
    import textwrap
    for name, desc in BIOME_NOTE:
        line = f"• {name}: {desc}"
        for part in textwrap.wrap(line, 74):
            d.text((PAD, y), part, fill=(195, 195, 210), font=f_small)
            y += 24
        y += 6
    y += 10
    for part in textwrap.wrap(
            "Новый мир: главное меню -> New Game (мир создастся заново с новым сидом).", 74):
        d.text((PAD, y), part, fill=(150, 150, 170), font=f_small)
        y += 24
    y += 40
    canvas = canvas.crop((0, 0, W, y))
    canvas.save("docs/world_guide.png")

    md = ["# Гид по миру Midless: Cosmic Edition (v63)\n",
          "Постер с картинками: `docs/world_guide.png` (генерируется `tools_dev/gen_guide.py`).\n"]
    for title, items in SECTIONS:
        md.append(f"\n## {title}\n")
        for bid, t, en, ru in items:
            md.append(f"- **{ru}** ({en}) — id блока {bid}")
    md.append("\n## Биомы\n")
    for name, desc in BIOME_NOTE:
        md.append(f"- **{name}**: {desc}")
    open("docs/WORLD_GUIDE.md", "w").write("\n".join(md) + "\n")
    print(f"guide written ({W}x{y}): docs/world_guide.png + docs/WORLD_GUIDE.md")


if __name__ == "__main__":
    main()
