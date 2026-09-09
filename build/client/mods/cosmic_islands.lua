-- cosmic_islands.lua
-- Floating islands in a starlit void. Water exists only inside crystal basins.
-- Islands are padded inside their cells so they never clip at chunk/cell seams.

local wg = midless.worldgen
local f = wg.field
local x, y, z = f.x(), f.y(), f.z()
local ox, oy, oz = f.origin_x(), f.origin_y(), f.origin_z()

local function noise3(px, py, pz, octaves, gain, lacunarity, fractal)
    return f.noise3d({
        type = "opensimplex2s", fractal = fractal or "fbm", frequency = 0.01,
        octaves = octaves, gain = gain, lacunarity = lacunarity,
        x = px, y = py, z = pz,
    })
end

local CELL_XZ = 80
local CELL_Y = 64

local function cell_hash(cx, cy, cz, salt)
    return f.random({
        seed = f.trunc(cx * 73856093 + cy * 19349663 + cz * 83492791) % 2048,
        salt = salt, seed_scale = 1024, salt_scale = 1024,
    })
end

local cx = f.floor(x / CELL_XZ)
local cy = f.floor(y / CELL_Y)
local cz = f.floor(z / CELL_XZ)

-- Starter island lives in cell (0, 1, 0): x 0..80, y 64..128, z 0..80.
-- Centre (24, 88, 24), radius 18 — covers spawn (24, 97, 24) with margin.
local is_starter = f.eq(cx, 0) * f.eq(cy, 1) * f.eq(cz, 0)

local jitter_x = f.select(is_starter, 24, 22 + (cell_hash(cx, cy, cz, 1) % 36))
local jitter_y = f.select(is_starter, 24, 22 + (cell_hash(cx, cy, cz, 2) % 30))
local jitter_z = f.select(is_starter, 24, 22 + (cell_hash(cx, cy, cz, 3) % 36))

local ix = cx * CELL_XZ + jitter_x
local iy = cy * CELL_Y + jitter_y
local iz = cz * CELL_XZ + jitter_z

local base_radius = f.select(is_starter, 18, 10 + (cell_hash(cx, cy, cz, 4) % 7))

local dx = (x - ix) / base_radius
local dz = (z - iz) / base_radius
local hdist2 = dx * dx + dz * dz

local warp = noise3(x * 0.07, y * 0.07, z * 0.07, 2, 0.55, 2) * 2.4

local upper_dy = (y + warp - iy) / (base_radius * 0.40)
local inside_upper = f.select(f.lt(y, iy), 0, f.lt(hdist2 + upper_dy * upper_dy, 1))

local lower_dy = (y + warp - iy) / (base_radius * 1.15)
local inside_lower = f.select(f.lt(iy, y), 0, f.lt(hdist2 + lower_dy * lower_dy, 1))

local inside_island = f.max(inside_upper, inside_lower)

local y1 = y + 1
local upper_dy1 = (y1 + warp - iy) / (base_radius * 0.40)
local inside_upper_y1 = f.select(f.lt(y1, iy), 0, f.lt(hdist2 + upper_dy1 * upper_dy1, 1))
local lower_dy1 = (y1 + warp - iy) / (base_radius * 1.15)
local inside_lower_y1 = f.select(f.lt(iy, y1), 0, f.lt(hdist2 + lower_dy1 * lower_dy1, 1))
local inside_y1 = f.max(inside_upper_y1, inside_lower_y1)
local surface = f.eq(inside_island, 1) * f.eq(inside_y1, 0)

local depth = f.max(0, iy - y)
local body = f.select(surface, 3, f.select(f.lt(3, depth), 1, 2))
local tip = f.lt(y, iy - base_radius * 0.85)
local island_block = f.select(tip, 6, body)

-- No oceans. Air outside islands. Water is added later as crystal-basin structures.
local material = f.select(inside_island, island_block, 0)
local solid = inside_island

wg.configure({
    id = "midless:cosmic", version = 3,
    min_y = 0, max_y = 256, bounded = true,
    sea_level = -1,
    fill_oceans = false,
    material = material,
    density = solid,
    skylight = solid,
})

local function random_at(px, py, pz, salt)
    return f.random({
        seed = f.trunc(px * 1135 + py * 1307 + pz * 1479) % 2048,
        salt = salt, seed_scale = 1024, salt_scale = 1024,
    })
end
local function random_here(salt) return random_at(x, y, z, salt) end
local function random_origin(salt) return random_at(ox, oy, oz, salt) end

wg.define_rule({
    match = 3, when = f.lt(256, f.local_index()), offset_y = -1, block = 2,
})

local flower = random_here(6) % 80
wg.define_rule({ match = 3, when = f.eq(flower, 0), offset_y = 1, block = 12, descending = true })
wg.define_rule({ match = 3, when = f.eq(flower, 1), offset_y = 1, block = 13, descending = true })

wg.define_ore("midless:iron_vein", {
    block = 7, min_y = 0, max_y = 256, size = 3, spacing = 24, chance = 0.35,
    distribution = "clusters", replaces = {1},
})
wg.define_ore("midless:gold_vein", {
    block = 9, min_y = 0, max_y = 256, size = 2, spacing = 32, chance = 0.18,
    distribution = "clusters", replaces = {1},
})
wg.define_ore("midless:coal_vein", {
    block = 8, min_y = 0, max_y = 256, size = 3, spacing = 20, chance = 0.4,
    distribution = "clusters", replaces = {1},
})

-- Crystal basin: stone floor + glass walls. Water cannot spill into the void.
local basin = {}
local function add(bx, by, bz, id)
    basin[#basin + 1] = { x = bx, y = by, z = bz, block = id }
end
local r = 3
for dz = -r, r do
    for dx = -r, r do
        add(dx, -1, dz, 1)
        local edge = (math.abs(dx) == r) or (math.abs(dz) == r)
        if edge then
            add(dx, 0, dz, 14)
            add(dx, 1, dz, 14)
        else
            add(dx, 0, dz, 5)
        end
    end
end

wg.define_structure("midless:crystal_basin", {
    spacing = 48, chance = 0.55, min_y = 48, max_y = 220,
    max_slope = 2, rotate = false, air_only = false,
    foundation = 1, foundation_depth = 4,
    blocks = basin,
})

local crystal = {}
local function addc(bx, by, bz, id)
    crystal[#crystal + 1] = { x = bx, y = by, z = bz, block = id }
end
addc(0, 0, 0, 14)
addc(0, 1, 0, 14)
addc(0, 2, 0, 14)
addc(1, 0, 0, 14)
addc(-1, 0, 0, 14)
addc(0, 0, 1, 14)
addc(0, 0, -1, 14)
addc(0, 3, 0, 14)

wg.define_structure("midless:crystal_spire", {
    spacing = 40, chance = 0.28, min_y = 48, max_y = 220,
    max_slope = 3, rotate = true, air_only = true,
    blocks = crystal,
})

wg.define_structure("midless:cosmic_tree", {
    spacing = 28, chance = 0.45, min_y = 48, max_y = 220,
    max_slope = 2, rotate = true, air_only = true,
    tree = { height = 5, radius = 2, trunk = 10, leaves = 11 },
})
