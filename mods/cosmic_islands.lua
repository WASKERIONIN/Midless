-- Discrete floating islands in a void. Water only in glass basins.

local wg = midless.worldgen
local f = wg.field
local x, y, z = f.x(), f.y(), f.z()

-- 2D footprints: only noise *peaks* become islands, so the rest is empty sky.
local function layer(seed, freq, thresh, base_y, amp, thick)
    local n = f.noise2d({
        type = "opensimplex2s", fractal = "fbm", frequency = freq,
        octaves = 4, gain = 0.52, lacunarity = 2.1, seed_offset = seed,
    })
    local mask = n - thresh
    local h = f.noise2d({
        type = "opensimplex2s", fractal = "fbm", frequency = freq * 2.3,
        octaves = 2, seed_offset = seed + 9,
    })
    local surface = base_y + h * amp
    local thickness = 5 + f.max(mask, 0) * thick
    local warp = f.noise3d({
        type = "opensimplex2s", fractal = "fbm", frequency = 0.055,
        octaves = 2, seed_offset = seed + 21,
        x = x, y = y, z = z,
    })
    local depth = surface - y
    return f.lt(0, mask) * f.lt(-0.4, depth) * f.lt(depth, thickness + warp * 2.5)
end

local function layer_at(seed, freq, thresh, base_y, amp, thick, yy)
    local n = f.noise2d({
        type = "opensimplex2s", fractal = "fbm", frequency = freq,
        octaves = 4, gain = 0.52, lacunarity = 2.1, seed_offset = seed,
    })
    local mask = n - thresh
    local h = f.noise2d({
        type = "opensimplex2s", fractal = "fbm", frequency = freq * 2.3,
        octaves = 2, seed_offset = seed + 9,
    })
    local surface = base_y + h * amp
    local thickness = 5 + f.max(mask, 0) * thick
    local warp = f.noise3d({
        type = "opensimplex2s", fractal = "fbm", frequency = 0.055,
        octaves = 2, seed_offset = seed + 21,
        x = x, y = yy, z = z,
    })
    local depth = surface - yy
    return f.lt(0, mask) * f.lt(-0.4, depth) * f.lt(depth, thickness + warp * 2.5)
end

-- Three stacked belts of islands, plus a guaranteed starter island.
local mid  = layer(1,  0.010, 0.40, 74,  7, 38)
local high = layer(40, 0.012, 0.52, 118, 6, 24)
local low  = layer(90, 0.011, 0.48, 40,  6, 22)

local starter = f.lt(f.abs(x - 8), 11) * f.lt(f.abs(z - 8), 11) * f.lt(y, 76) * f.lt(68, y)
local inside = f.max(f.max(mid, high), f.max(low, starter))

local mid1  = layer_at(1,  0.010, 0.40, 74,  7, 38, y + 1)
local high1 = layer_at(40, 0.012, 0.52, 118, 6, 24, y + 1)
local low1  = layer_at(90, 0.011, 0.48, 40,  6, 22, y + 1)
local starter1 = f.lt(f.abs(x - 8), 11) * f.lt(f.abs(z - 8), 11) * f.lt(y + 1, 76) * f.lt(68, y + 1)
local inside1 = f.max(f.max(mid1, high1), f.max(low1, starter1))
local surface = inside * (1 - inside1)

local body = f.select(surface, 3, f.select(f.lt(y, 50), 1, 2))
local material = f.select(inside, body, 0)

wg.configure({
    id = "midless:cosmic", version = 5,
    min_y = 0, max_y = 160, bounded = true,
    sea_level = -1, fill_oceans = false,
    material = material, density = inside, skylight = inside,
})

wg.define_rule({
    match = 3, when = f.lt(256, f.local_index()), offset_y = -1, block = 2,
})

local basin = {}
local function add(bx, by, bz, id)
    basin[#basin + 1] = { x = bx, y = by, z = bz, block = id }
end
for dz = -3, 3 do
    for dx = -3, 3 do
        add(dx, -1, dz, 1)
        local edge = (math.abs(dx) == 3) or (math.abs(dz) == 3)
        if edge then
            add(dx, 0, dz, 14)
            add(dx, 1, dz, 14)
        else
            add(dx, 0, dz, 5)
        end
    end
end
add(0, 2, 3, 14)
add(0, 3, 3, 14)

wg.define_structure("midless:crystal_basin", {
    spacing = 48, chance = 0.55, min_y = 20, max_y = 150,
    max_slope = 4, rotate = false, air_only = false,
    foundation = 1, foundation_depth = 4,
    blocks = basin,
})

wg.define_structure("midless:crystal_spire", {
    spacing = 28, chance = 0.4, min_y = 20, max_y = 150,
    max_slope = 5, rotate = true, air_only = true,
    blocks = {
        { x = 0, y = 0, z = 0, block = 14 },
        { x = 0, y = 1, z = 0, block = 14 },
        { x = 0, y = 2, z = 0, block = 14 },
        { x = 0, y = 3, z = 0, block = 15 },
    },
})

wg.define_structure("midless:cosmic_tree", {
    spacing = 28, chance = 0.28, min_y = 20, max_y = 150,
    max_slope = 4, rotate = true, air_only = true,
    tree = { height = 5, radius = 2, trunk = 10, leaves = 11 },
})
