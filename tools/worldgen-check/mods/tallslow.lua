-- maxY=896, no skylight field, and ores+structures present: this forces the
-- Worldgen_SkyMask fallback that regenerates every chunk above the target.
local wg = midless.worldgen
local f = wg.field
local hills = f.noise2d({ type = "opensimplex2s", fractal = "fbm", frequency = 0.01,
                          octaves = 3, gain = 0.5, lacunarity = 2 })
wg.configure({ id = "test:tallslow", version = 1, preset = "native",
               min_y = -128, max_y = 896, bounded = true, sea_level = 48 })
wg.define_biome("test:plains", { height = 64, height_variation = 24, height_noise = hills,
                                 top = 3, filler = 2, stone = 1, underwater = 6, filler_depth = 3 })
wg.define_ore("test:coal", { block = 16, min_y = -64, max_y = 48, size = 3,
                             spacing = 16, chance = 0.5, distribution = "clusters" })
wg.define_structure("test:oak", { spacing = 32, chance = 0.5, min_y = 40, max_y = 200,
                                  max_slope = 6, rotate = true,
                                  tree = { height = 7, radius = 3, trunk = 10, leaves = 11 } })
