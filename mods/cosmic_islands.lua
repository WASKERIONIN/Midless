-- Floating islands. Water only in glass basins. A solid starter pad at spawn.

local wg = midless.worldgen
local f = wg.field
local x, y, z = f.x(), f.y(), f.z()

local n = f.noise3d({
    type = "opensimplex2s", fractal = "fbm", frequency = 0.018,
    octaves = 3, gain = 0.5, lacunarity = 2.0,
    x = x, y = y * 1.4, z = z,
})

-- Soft vertical band so islands float around y = 72 instead of filling the void.
local band = 1 - f.abs(y - 72) / 26
local blob = f.lt(0.28, n + band * 0.85)

-- Guaranteed pad under the player (8, 77, 8).
local starter = f.lt(f.abs(x - 8), 12) * f.lt(f.abs(z - 8), 12) * f.lt(y, 76) * f.lt(68, y)
local inside = f.max(blob, starter)

local n1 = f.noise3d({
    type = "opensimplex2s", fractal = "fbm", frequency = 0.018,
    octaves = 3, gain = 0.5, lacunarity = 2.0,
    x = x, y = (y + 1) * 1.4, z = z,
})
local band1 = 1 - f.abs((y + 1) - 72) / 26
local blob1 = f.lt(0.28, n1 + band1 * 0.85)
local starter1 = f.lt(f.abs(x - 8), 12) * f.lt(f.abs(z - 8), 12) * f.lt(y + 1, 76) * f.lt(68, y + 1)
local inside1 = f.max(blob1, starter1)
local surface = inside * (1 - inside1)

local body = f.select(surface, 3, f.select(f.lt(y, 70), 1, 2))
local material = f.select(inside, body, 0)

wg.configure({
    id = "midless:cosmic", version = 4,
    min_y = 0, max_y = 192, bounded = true,
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
for dz = -2, 2 do
    for dx = -2, 2 do
        add(dx, -1, dz, 1)
        local edge = (math.abs(dx) == 2) or (math.abs(dz) == 2)
        if edge then
            add(dx, 0, dz, 14)
            add(dx, 1, dz, 14)
        else
            add(dx, 0, dz, 5)
        end
    end
end

wg.define_structure("midless:crystal_basin", {
    spacing = 64, chance = 0.4, min_y = 40, max_y = 160,
    max_slope = 3, rotate = false, air_only = false,
    foundation = 1, foundation_depth = 3,
    blocks = basin,
})

wg.define_structure("midless:cosmic_tree", {
    spacing = 32, chance = 0.35, min_y = 40, max_y = 160,
    max_slope = 3, rotate = true, air_only = true,
    tree = { height = 5, radius = 2, trunk = 10, leaves = 11 },
})
