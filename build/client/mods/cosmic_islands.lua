-- cosmic_islands.lua  —  Floating islands drifting through a starlit void.
-- No ocean, no flat horizon.  The world is a scatter of rocky isles with
-- crystal basins, luminescent flora, and asteroid rubble above and below.

local wg = midless.worldgen
local f  = wg.field
local x, y, z    = f.x(), f.y(), f.z()
local ox, oy, oz = f.origin_x(), f.origin_y(), f.origin_z()

----------------------------------------------------------------------
-- Helpers
----------------------------------------------------------------------

local function noise2(px, pz, octaves, gain, lacunarity, freq)
    return f.noise2d({
        type = "opensimplex2s", fractal = "fbm",
        frequency = freq or 0.01,
        octaves = octaves, gain = gain, lacunarity = lacunarity,
        x = px, z = pz,
    })
end

local function noise3(px, py, pz, octaves, gain, lacunarity, fractal, freq)
    return f.noise3d({
        type = "opensimplex2s", fractal = fractal or "fbm",
        frequency = freq or 0.01,
        octaves = octaves, gain = gain, lacunarity = lacunarity,
        x = px, y = py, z = pz,
    })
end

----------------------------------------------------------------------
-- Island placement — jittered grid of cells, one island per cell.
----------------------------------------------------------------------
local CELL_XZ = 56
local CELL_Y  = 36

local function cell_hash(cx, cy, cz, salt)
    return f.random({
        seed = f.trunc(cx * 73856093 + cy * 19349663 + cz * 83492791) % 2048,
        salt = salt, seed_scale = 1024, salt_scale = 1024,
    })
end

local cx = f.floor(x / CELL_XZ)
local cy = f.floor(y / CELL_Y)
local cz = f.floor(z / CELL_XZ)

-- Guarantee a starter island in the cell containing the spawn point
-- (0, 80, 0) so the player always has solid ground to land on.
-- Cell (0, 2, 0) spans x:0-56, y:72-108, z:0-56.
-- Place the island centre at (0, 80, 0) exactly.
local is_starter_cell = f.eq(cx, 0) * f.eq(cy, 2) * f.eq(cz, 0)

local jitter_x = f.select(is_starter_cell, 0, (cell_hash(cx, cy, cz, 1) % (CELL_XZ - 20)) + 10)
local jitter_y = f.select(is_starter_cell, 0, (cell_hash(cx, cy, cz, 2) % (CELL_Y  - 10)) + 5)
local jitter_z = f.select(is_starter_cell, 0, (cell_hash(cx, cy, cz, 3) % (CELL_XZ - 20)) + 10)

local ix = cx * CELL_XZ + jitter_x
local iy = cy * CELL_Y  + jitter_y
local iz = cz * CELL_XZ + jitter_z

-- Per-island radius: 8..22 blocks — big enough to walk on.
-- Starter island is always large so the player has room to explore.
local base_radius = f.select(is_starter_cell, 18, 8 + (cell_hash(cx, cy, cz, 4) % 15))

-- Oblate ellipsoid distance (wider than tall).
-- dx, dz scaled by radius, dy scaled differently for top and bottom.
local dx = (x - ix) / base_radius
local dz = (z - iz) / base_radius

-- Upper hemisphere: flattened top (scale factor 0.45 of radius)
local dy_top = (y - iy) / (base_radius * 0.45)
-- Lower hemisphere: tapered stalactite (scale factor 1.4 of radius)
local dy_bot = (y - iy) / (base_radius * 1.4)

-- hdist2 = (dx^2 + dz^2)
local hdist2 = dx * dx + dz * dz

-- Warp the distance field with 3-D noise for organic silhouettes.
local warp = noise3(x * 0.08, y * 0.08, z * 0.08, 2, 0.6, 2) * 3

-- Upper: only for blocks above centre
local upper_dy = (y + warp - iy) / (base_radius * 0.45)
local upper_d2 = hdist2 + upper_dy * upper_dy
local inside_upper = f.select(f.lt(y, iy), 0, f.lt(upper_d2, 1))

-- Lower: only for blocks below centre
local lower_dy = (y + warp - iy) / (base_radius * 1.4)
local lower_d2 = hdist2 + lower_dy * lower_dy
local inside_lower = f.select(f.lt(iy, y), 0, f.lt(lower_d2, 1))

local inside_island = f.max(inside_upper, inside_lower)

----------------------------------------------------------------------
-- Surface detection & material assignment
----------------------------------------------------------------------
-- "surface" = block is inside island, but y+1 is not.
-- We need to re-evaluate "inside" for y+1.
local y1 = y + 1
local dy_top1 = (y1 + warp - iy) / (base_radius * 0.45)
local upper_d2_y1 = hdist2 + dy_top1 * dy_top1
local inside_upper_y1 = f.select(f.lt(y1, iy), 0, f.lt(upper_d2_y1, 1))

local dy_bot1 = (y1 + warp - iy) / (base_radius * 1.4)
local lower_d2_y1 = hdist2 + dy_bot1 * dy_bot1
local inside_lower_y1 = f.select(f.lt(iy, y1), 0, f.lt(lower_d2_y1, 1))

local inside_y1 = f.max(inside_upper_y1, inside_lower_y1)
local surface = f.eq(inside_island, 1) * f.eq(inside_y1, 0)

-- Material selection:
--   surface       → grass (3) - starlit moss
--   shallow (0-3) → dirt (2)  - regolith
--   deep          → stone (1) - cosmic rock
--   underside tip → sand (6)  - crystal dust
local depth = f.max(0, iy - y)
local deep = f.select(f.lt(3, depth), 1, 2)
local body = f.select(surface, 3, deep)
-- Tip of underside stalactite
local tip_zone = f.lt(y, iy - base_radius * 0.9)
local island_block = f.select(tip_zone, 6, body)

-- Material: only solid inside islands, air outside.
local material = f.select(inside_island, island_block, 0)
local solid = inside_island

----------------------------------------------------------------------
-- Crystal pools — water ONLY in contained bowls on island tops.
----------------------------------------------------------------------
local has_pool = f.eq(cell_hash(cx, cy, cz, 7) % 4, 0)
local bowl_r = base_radius * 0.5
local bowl_dx = (x - ix) / bowl_r
local bowl_dz = (z - iz) / bowl_r
local in_bowl = f.lt(bowl_dx * bowl_dx + bowl_dz * bowl_dz, 1)

-- Bowl is carved from solid terrain: air in a cylindrical region
-- from iy-2 to iy+3, inside the bowl radius.
local bowl_height = f.lt(iy - 2, y) * f.lt(y, iy + 3)
local bowl_carve = f.eq(has_pool, 1) * f.eq(in_bowl, 1) * bowl_height

-- Carve the bowl out of solid terrain
material = f.select(f.eq(bowl_carve, 1), 0, material)
solid = solid * (1 - bowl_carve)

-- Water fills the carved bowl at iy to iy+1 (only 1-2 blocks deep).
local water_height = f.lt(iy - 1, y) * f.lt(y, iy + 2)
local water_cell = f.eq(has_pool, 1) * f.eq(in_bowl, 1) * water_height
-- Only place water where it's air (already carved)
water_cell = water_cell * f.eq(solid, 0)

-- Override carved air with water
material = f.select(f.eq(water_cell, 1), 5, material)

----------------------------------------------------------------------
-- Configure generator
----------------------------------------------------------------------
wg.configure({
    id = "midless:cosmic", version = 1,
    min_y = -256, max_y = 512, bounded = false,
    material = material,
    density  = f.max(solid, f.eq(water_cell, 1)),
    skylight = solid,
})

----------------------------------------------------------------------
-- Flora & decoration
----------------------------------------------------------------------
local function random_at(px, py, pz, salt)
    return f.random({
        seed = f.trunc(px * 1135 + py * 1307 + pz * 1479) % 2048,
        salt = salt, seed_scale = 1024, salt_scale = 1024,
    })
end
local function random_here(salt) return random_at(x, y, z, salt) end
local function random_origin(salt) return random_at(ox, oy, oz, salt) end

-- Grass → dirt rule
wg.define_rule({
    match = 3, when = f.lt(256, f.local_index()), offset_y = -1, block = 2,
})

-- Sparse luminescent flowers on moss
local flower = random_here(6) % 96
wg.define_rule({ match = 3, when = f.eq(flower, 0), offset_y = 1, block = 12, descending = true })
wg.define_rule({ match = 3, when = f.eq(flower, 1), offset_y = 1, block = 13, descending = true })

----------------------------------------------------------------------
-- Cosmic trees — slim trunks with luminous canopy
----------------------------------------------------------------------
local step, steps = f.step(), f.steps()

local function stroke(from, to, thickness, upwardness, length, salt, when)
    local angle = (random_here(10 + salt) % 360) * (math.pi / 180)
    return {
        op = "stroke", from = from, to = to, when = when, block = 10,
        steps = length * 4,
        dx = f.cos(angle) / upwardness, dy = 0.25,
        dz = f.sin(angle) / upwardness,
        bounds = thickness,
        radius = thickness / (2 + step / (steps / 1.5)),
    }
end

local commands = {
    stroke(0, 1, 4, 24, 6 + random_origin(1) % 3, 10),
}
local count = 3 + random_origin(2) % 2
for i = 0, 3 do
    local enabled = f.lt(i, count)
    commands[#commands + 1] = stroke(1, 2, 3, 4,
        4 + random_origin(3 * i) % 2, i, enabled)
    local thickness = 7 + random_origin(4 * i) % 3
    commands[#commands + 1] = {
        op = "sphere", from = 2, to = 2, when = enabled, block = 11,
        bounds = thickness, radius = thickness / 2,
    }
end

local allowed_column = (1 - f.eq(f.floor(x / 16), 0))
    * (1 - f.eq(f.floor(z / 16), 0))
wg.define_feature("midless:cosmic_tree", {
    when = f.select(allowed_column,
        f.select(surface, f.eq(random_here(5) % 384, 0), 0), 0),
    origin_min = {-1, -2, -1}, origin_max = {1, 1, 1},
    commands = commands,
})
