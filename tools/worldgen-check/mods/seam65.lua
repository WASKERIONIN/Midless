-- Identical, but the surface sits at y=65 (local y=1 of chunk y=4).
local wg = midless.worldgen
wg.configure({ id = "test:seam65", version = 1, preset = "native",
               min_y = -128, max_y = 256, bounded = true, sea_level = 48 })
wg.define_biome("test:plains", { height = 65, top = 3, filler = 2, stone = 1,
                                 underwater = 6, filler_depth = 3 })
wg.define_rule({ match = 3, block = 9, offset_y = -1 })
