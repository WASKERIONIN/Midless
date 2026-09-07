-- Flat surface exactly at y=64 (which is local y=0 of chunk y=4) plus a rule
-- that rewrites the block *below* every grass block.
local wg = midless.worldgen
wg.configure({ id = "test:seam64", version = 1, preset = "native",
               min_y = -128, max_y = 256, bounded = true, sea_level = 48 })
wg.define_biome("test:plains", { height = 64, top = 3, filler = 2, stone = 1,
                                 underwater = 6, filler_depth = 3 })
wg.define_rule({ match = 3, block = 9, offset_y = -1 })
