-- maxY=896 like the shipped classic mod, but WITHOUT a skylight field, so
-- Worldgen_SkyMask falls back to regenerating every chunk above.
local wg = midless.worldgen
local f = wg.field
local hills = f.noise2d({ type = "opensimplex2s", fractal = "fbm", frequency = 0.01,
                          octaves = 3, gain = 0.5, lacunarity = 2 })
wg.configure({ id = "test:tall", version = 1, preset = "native",
               min_y = -128, max_y = 896, bounded = true, sea_level = 48 })
wg.define_biome("test:plains", { height = 64, height_variation = 24, height_noise = hills,
                                 top = 3, filler = 2, stone = 1, underwater = 6, filler_depth = 3 })
