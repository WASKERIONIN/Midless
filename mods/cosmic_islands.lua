-- Midless: Cosmic Edition worldgen (v7)
-- Floating islands adrift in a starlit void, cone-tapered like hanging
-- gardens. Water exists only inside glass basins. The starter island carries
-- a launch pad, four warp-core obelisks and a glowing crystal basin.

local wg = midless.worldgen
local f = wg.field
local x, y, z = f.x(), f.y(), f.z()

---------------------------------------------------------------- blocks ----
-- 19 void_rock: deep indigo rock that makes up island cores
midless.define_block(19, {
    name = "Void Rock",
    textures = { all = 21 },
})

-- 20 crystal: translucent cosmic crystal (buildable glass-like block)
midless.define_block(20, {
    name = "Cosmic Crystal",
    textures = { all = 22 },
    render = block.render.TRANSPARENT,
    collider = block.collider.SOLID,
})

-- 21 launch_pad: dark star-forged pad with a glowing teal sigil
midless.define_block(21, {
    name = "Launch Pad",
    textures = { top = 19, sides = 23, bottom = 21 },
})

-- 22 warp_core: humming gold/violet core that emits light
midless.define_block(22, {
    name = "Warp Core",
    textures = { all = 20 },
    light = block.light.EMIT,
})

-- 23 chrome: mirror-bright white plate, pure Y2K
midless.define_block(23, {
    name = "Chrome Plate",
    textures = { all = 24 },
})

-- 24 gold_plate: warm polished gold
midless.define_block(24, {
    name = "Gold Plate",
    textures = { all = 5 },
})

------------------------------------------------------------- utilities ----
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
    -- cones: thickness grows where the island mask is strongest, so islands
    -- taper into hanging points, like the reference art
    local thickness = 4 + f.max(mask, 0) * thick
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
    local thickness = 4 + f.max(mask, 0) * thick
    local warp = f.noise3d({
        type = "opensimplex2s", fractal = "fbm", frequency = 0.055,
        octaves = 2, seed_offset = seed + 21,
        x = x, y = yy, z = z,
    })
    local depth = surface - yy
    return f.lt(0, mask) * f.lt(-0.4, depth) * f.lt(depth, thickness + warp * 2.5)
end

local function column(cx, cz, y0, y1)
    return f.eq(x, cx) * f.eq(z, cz) * f.lt(y0 - 0.5, y) * f.lt(y, y1 + 0.5)
end

--------------------------------------------------------------- islands ----
-- three belts of drifting islands
local mid  = layer(1,  0.0088, 0.37, 74,  8, 46)
local high = layer(40, 0.0115, 0.50, 116, 7, 28)
local low  = layer(90, 0.0105, 0.46, 38,  6, 24)

-- the guaranteed starter island: a rounded cone slab centred on (8, 8)
local sd = f.max(f.abs(x - 8), f.abs(z - 8))
local wobble = f.noise2d({
    type = "opensimplex2s", fractal = "fbm", frequency = 0.09,
    octaves = 2, seed_offset = 7,
})
local starter_bottom = 66 + sd * 0.9 + wobble * 1.2
local starter = f.lt(f.abs(x - 8), 10.5) * f.lt(f.abs(z - 8), 10.5) *
                f.lt(y, 76) * f.lt(starter_bottom, y)

local inside = f.max(f.max(mid, high), f.max(low, starter))

local mid1  = layer_at(1,  0.0088, 0.37, 74,  8, 46, y + 1)
local high1 = layer_at(40, 0.0115, 0.50, 116, 7, 28, y + 1)
local low1  = layer_at(90, 0.0105, 0.46, 38,  6, 24, y + 1)
local starter1 = f.lt(f.abs(x - 8), 10.5) * f.lt(f.abs(z - 8), 10.5) *
                 f.lt(y + 1, 76) * f.lt(starter_bottom, y + 1)
local inside1 = f.max(f.max(mid1, high1), f.max(low1, starter1))
local surface = inside * (1 - inside1)

-- stratified bodies: crystal turf over dirt over void rock over stone
local body = f.select(surface, 3, f.select(f.lt(y, 46), 1, 19))
local material = f.select(inside, body, 0)

------------------------------------------- starter island decorations ----
-- launch pad: 5x5 dark pads at y=76 framing the spawn point
local pad = f.lt(f.max(f.abs(x - 8), f.abs(z - 8)), 2.5) * f.eq(y, 76)

-- crystal basin east of the pad, centred on (13, 8)
local bm = f.max(f.abs(x - 13), f.abs(z - 8))
local basinRing = f.lt(2.5, bm) * f.lt(bm, 3.5)
local basinIn = f.lt(bm, 2.6)
local basinWall = basinRing * f.eq(y, 76)
local basinWallTop = basinRing * f.eq(y, 77)
local basinWater = basinIn * f.eq(y, 76)
local basinFloor = basinIn * f.eq(y, 75)

-- warp-core obelisks on the pad corners: crystal body, glowing core cap
local obelisk = column(6, 6, 77, 78) + column(10, 6, 77, 78) +
                column(6, 10, 77, 78) + column(10, 10, 77, 78)
local cores = column(6, 6, 79, 79) + column(10, 6, 79, 79) +
              column(6, 10, 79, 79) + column(10, 10, 79, 79)
-- a welcoming crystal arch over the pad
local arch = (column(5, 8, 77, 80) + column(11, 8, 77, 80)) +
             (column(6, 8, 80, 80) + column(7, 8, 81, 81) +
              column(8, 8, 81, 81) + column(9, 8, 81, 81) + column(10, 8, 80, 80))

material = f.select(basinFloor, 14, material)
material = f.select(basinWater, 5, material)
material = f.select(basinWall, 14, material)
material = f.select(basinWallTop, 14, material)
material = f.select(obelisk, 20, material)
material = f.select(cores, 22, material)
material = f.select(arch, 20, material)
material = f.select(pad, 21, material)

wg.configure({
    id = "midless:cosmic", version = 7,
    min_y = 0, max_y = 160, bounded = true,
    sea_level = -1, fill_oceans = false,
    material = material, density = inside, skylight = inside,
})

-- turf keeps a skin of dirt, void rock holds the cones together
wg.define_rule({ match = 3, when = f.lt(256, f.local_index()), offset_y = -1, block = 2 })
wg.define_rule({ match = 2, when = f.lt(256, f.local_index()), offset_y = -1, block = 19 })

------------------------------------------------------------------ ores ----
wg.define_ore("cosmic_gold", {
    block = 5, replaces = { 1, 19 },
    min_y = 4, max_y = 70, size = 4, spacing = 12, chance = 0.5,
    distribution = "clusters",
})

wg.define_ore("void_shard", {
    block = 6, replaces = { 19, 1 },
    min_y = 10, max_y = 120, size = 5, spacing = 16, chance = 0.6,
    distribution = "clusters",
})

wg.define_ore("gloom_amber", {
    block = 7, replaces = { 19, 1 },
    min_y = 20, max_y = 130, size = 4, spacing = 22, chance = 0.3,
    distribution = "clusters",
})

----------------------------------------------------------- structures ----
local basin = {}
local function add(bx, by, bz, id)
    basin[#basin + 1] = { x = bx, y = by, z = bz, block = id }
end
for dz = -3, 3 do
    for dx = -3, 3 do
        add(dx, -1, dz, 14)  -- glass floor: peer through into the void
        local edge = (math.abs(dx) == 3) or (math.abs(dz) == 3)
        if edge then
            add(dx, 0, dz, 14)
            add(dx, 1, dz, 14)
        else
            add(dx, 0, dz, 5)  -- still, glowing water
        end
    end
end

wg.define_structure("midless:crystal_basin", {
    spacing = 44, chance = 0.55, min_y = 20, max_y = 150,
    max_slope = 4, rotate = false, air_only = false,
    foundation = 19, foundation_depth = 4,
    blocks = basin,
})

wg.define_structure("midless:crystal_spire", {
    spacing = 30, chance = 0.4, min_y = 20, max_y = 150,
    max_slope = 5, rotate = true, air_only = true,
    blocks = {
        { x = 0, y = 0, z = 0, block = 20 },
        { x = 0, y = 1, z = 0, block = 20 },
        { x = 0, y = 2, z = 0, block = 20 },
        { x = 0, y = 3, z = 0, block = 22 },  -- lit core cap
    },
})

wg.define_structure("midless:cosmic_tree", {
    spacing = 28, chance = 0.28, min_y = 20, max_y = 150,
    max_slope = 4, rotate = true, air_only = true,
    tree = { height = 5, radius = 2, trunk = 10, leaves = 11 },
})

------------------------------------------------- dream structures (v7) ----
-- Dream gate: a Y2K chrome doorway that hums on island edges. A floating
-- crystal gem hangs in the opening - dreamcore's "remember this?" landmark.
local gate = {}
local function g(bx, by, bz, id)
    gate[#gate + 1] = { x = bx, y = by, z = bz, block = id }
end
for py = 0, 2 do
    g(-1, py, 0, 23)  -- chrome pillar left
    g(1, py, 0, 23)   -- chrome pillar right
end
g(-1, 3, 0, 20)       -- crystal arch left
g(0, 3, 0, 24)        -- gold keystone
g(1, 3, 0, 20)        -- crystal arch right
g(0, 2, 0, 20)        -- the gem floating inside the gate

wg.define_structure("midless:dream_gate", {
    spacing = 36, chance = 0.42, min_y = 20, max_y = 150,
    max_slope = 3, rotate = true, air_only = true,
    blocks = gate,
})

-- Chrome totem: polished stack with a levitating crystal cap. Pure Y2K
-- antenna aesthetics, catches the nebula light from far away.
wg.define_structure("midless:chrome_totem", {
    spacing = 30, chance = 0.35, min_y = 20, max_y = 150,
    max_slope = 3, rotate = false, air_only = true,
    blocks = {
        { x = 0, y = 0, z = 0, block = 23 },
        { x = 0, y = 1, z = 0, block = 24 },
        { x = 0, y = 2, z = 0, block = 23 },
        { x = 0, y = 3, z = 0, block = 24 },
        { x = 0, y = 5, z = 0, block = 20 },  -- cap floats one block above
    },
})

-- Ruined shrine: dungeon synth ruins. A cracked void-rock platform with
-- broken columns and a still-lit warp core in the middle.
local shrine = {}
local function s(bx, by, bz, id)
    shrine[#shrine + 1] = { x = bx, y = by, z = bz, block = id }
end
for dz = -2, 2 do
    for dx = -2, 2 do
        local edge = (math.abs(dx) == 2) or (math.abs(dz) == 2)
        s(dx, 0, dz, edge and 19 or 21)  -- pad ring on void rock
    end
end
-- broken columns (deliberately uneven heights)
s(-2, 1, -2, 19); s(-2, 2, -2, 19)
s(2, 1, -2, 19)
s(-2, 1, 2, 19)
s(2, 1, 2, 19); s(2, 2, 2, 19); s(2, 3, 2, 20)
-- the core
s(0, 1, 0, 22)

wg.define_structure("midless:ruined_shrine", {
    spacing = 52, chance = 0.5, min_y = 20, max_y = 150,
    max_slope = 2, rotate = true, air_only = false,
    foundation = 19, foundation_depth = 3,
    blocks = shrine,
})

-- Memory float: little levitating clusters of chrome and crystal drifting
-- above open ground. The dreamcore signature - geometry that shouldn't fly,
-- flying anyway.
wg.define_structure("midless:memory_float", {
    spacing = 24, chance = 0.3, min_y = 20, max_y = 150,
    max_slope = 6, rotate = true, air_only = true,
    blocks = {
        { x = 0, y = 3, z = 0, block = 23 },
        { x = 1, y = 4, z = 1, block = 20 },
        { x = -1, y = 5, z = 0, block = 24 },
        { x = 0, y = 6, z = 1, block = 23 },
    },
})
