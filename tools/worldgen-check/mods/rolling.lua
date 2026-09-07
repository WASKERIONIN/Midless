-- Heightmap worldgen (no material field) with two biomes and a height field.
local wg = midless.worldgen
local f = wg.field
local hills = f.noise2d({ type = "opensimplex2s", fractal = "fbm", frequency = 0.02,
                          octaves = 3, gain = 0.5, lacunarity = 2 })
local temp = f.noise2d({ type = "value", fractal = "none", frequency = 0.004,
                         octaves = 1, seed_offset = 99 })
wg.configure({ id = "test:rolling", version = 1, preset = "native",
               min_y = -128, max_y = 256, bounded = true, sea_level = 48,
               temperature = temp })
wg.define_biome("test:plains", { temperature = -1, spread = 1, height = 56,
                                 height_variation = 12, height_noise = hills,
                                 top = 3, filler = 2, stone = 1, underwater = 6, filler_depth = 3 })
wg.define_biome("test:hills", { temperature = 1, spread = 1, height = 72,
                                height_variation = 20, height_noise = hills,
                                top = 6, filler = 2, stone = 1, underwater = 6, filler_depth = 4 })
wg.define_ore("test:coal", { block = 16, min_y = -64, max_y = 48, size = 3,
                             spacing = 16, chance = 0.5, distribution = "clusters" })
wg.define_structure("test:oak", { spacing = 32, chance = 0.5, min_y = 40, max_y = 200,
                                  max_slope = 6, rotate = true,
                                  tree = { height = 7, radius = 3, trunk = 10, leaves = 11 } })
