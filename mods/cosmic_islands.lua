-- Midless: Cosmic Edition worldgen (v12)
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
    -- v65.5: full-15 emission bleached everything near the pad obelisks;
    -- the core hums at 8 now - visible, not a floodlight
    light_level = 8,
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

-- 25 cocoon: alien egg (invisible cube; the client draws a proper egg over it)
midless.define_block(25, {
    name = "Void Cocoon",
    textures = { all = 21 },
    model = block.model.GAS,
    collider = block.collider.NONE,
    -- v55: transparent for light - the egg must not cast a shadow column
    -- (v54 made it glow instead; the pulsing light pop on removal felt wrong)
    render = block.render.TRANSPARENT,
})

-- 26 volatile barrel: black-orange hazard, explodes on contact
midless.define_block(26, {
    name = "Volatile Barrel",
    textures = { all = 26 },
})

-- v54: island flora species - each has its own silhouette art
midless.define_block(28, {
    name = "Void Bellflower",
    textures = { all = 28 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(29, {
    name = "Starbloom",
    textures = { all = 29 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(30, {
    name = "Spiral Fern",
    textures = { all = 30 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(31, {
    name = "Twin Tulip",
    textures = { all = 31 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(32, {
    name = "Glow Grass",
    textures = { all = 32 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(33, {
    name = "Lanternberry",
    textures = { all = 33 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})

-- v57: tall flora - one block footprint, the client draws the billboard
-- two blocks high; laser-searable like every other flower
midless.define_block(37, {
    name = "Star Reed",
    textures = { all = 37 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(38, {
    name = "Moon Bell",
    textures = { all = 38 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})

-- v58: void tuft - low ground cover (one third of a block tall), it hides
-- the bare dirt under flowers and mushrooms
midless.define_block(39, {
    name = "Void Tuft",
    textures = { all = 39 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(41, {
    name = "Void Sedge",
    textures = { all = 41 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})

-- v59.5: four new tall flora species for variety - bioluminescent teals,
-- a warm ember accent, near-black foliage with magenta, and ice blue
midless.define_block(45, {
    name = "Glassbell",
    textures = { all = 45 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(46, {
    name = "Embercup",
    textures = { all = 46 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(47, {
    name = "Void Orchid",
    textures = { all = 47 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(48, {
    name = "Frostfern",
    textures = { all = 48 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})

midless.define_block(49, {
    name = "Void Tree",
    textures = { all = 49 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})

-- v61.2: lantern tree - gold canopy twin of the void tree
midless.define_block(50, {
    name = "Lantern Tree",
    textures = { all = 50 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})

-- v61.2: crystal stalk - raw shard cluster
midless.define_block(51, {
    name = "Crystal Stalk",
    textures = { all = 51 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})

-- v61.2: void puff - luminous cotton ball
midless.define_block(52, {
    name = "Void Puff",
    textures = { all = 52 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})

--------------------------------------------------------------- biomes -----
-- v62: two new island biomes, sampled on a huge-scale noise so whole
-- clusters of islands share a character. Ember: charcoal basalt with
-- smoldering veins, warm flora. Frost: pale hoarfrost turf, ice flora.
midless.define_block(56, {
    name = "Ember Rock",
    textures = { all = 56 },
})

midless.define_block(57, {
    name = "Ember Turf",
    textures = { all = 57 },
})

midless.define_block(58, {
    name = "Frost Turf",
    textures = { all = 58 },
})

-- v63: biome ground cover - every biome keeps its own grass
midless.define_block(59, {
    name = "Ember Tuft",
    textures = { all = 59 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})

midless.define_block(60, {
    name = "Frost Tuft",
    textures = { all = 60 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
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
    -- v62: in the ember biome the mid islands rise as stepped mesas -
    -- quantized surface height gives the terrain a deliberate, terraced
    -- silhouette instead of smooth cones
    if seed == 1 then
        local biome_n_t = f.noise2d({
            type = "opensimplex2s", fractal = "fbm", frequency = 0.006,
            octaves = 2, seed_offset = 4242,
        })
        local ember_b = f.lt(0.22, biome_n_t)
        local terr = f.floor(h * 3.0) / 3.0 + 0.15
        h = f.select(ember_b, terr, h)
    end
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
    -- v63b: keep the ember terraces in sync with layer() - the flora
    -- gates sample these offset fields, so a mismatch made plants
    -- stack or float around terrace edges
    if seed == 1 then
        local biome_n_t = f.noise2d({
            type = "opensimplex2s", fractal = "fbm", frequency = 0.006,
            octaves = 2, seed_offset = 4242,
        })
        local ember_b = f.lt(0.22, biome_n_t)
        local terr = f.floor(h * 3.0) / 3.0 + 0.15
        h = f.select(ember_b, terr, h)
    end
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
-- v52: frequencies up / thresholds down - a denser archipelago
-- v62: thresholds loosened a touch (more islands), plus a rare DEEP
-- layer far below the others - lonely rocks drifting in the black
local MID_F, MID_T   = 0.0112, 0.32
local HIGH_F, HIGH_T = 0.0144, 0.45
local LOW_F, LOW_T   = 0.0130, 0.41
local DEEP_F, DEEP_T = 0.0090, 0.52
local mid  = layer(1,  MID_F,  MID_T,  74,  8, 46)
local high = layer(40, HIGH_F, HIGH_T, 116, 7, 28)
local low  = layer(90, LOW_F,  LOW_T,  38,  6, 24)
local deep = layer(777, DEEP_F, DEEP_T, 20, 6, 18)

-- the guaranteed starter island: a rounded cone slab centred on (8, 8)
local sd = f.max(f.abs(x - 8), f.abs(z - 8))
local wobble = f.noise2d({
    type = "opensimplex2s", fractal = "fbm", frequency = 0.09,
    octaves = 2, seed_offset = 7,
})
local starter_bottom = 66 + sd * 0.9 + wobble * 1.2
local starter = f.lt(f.abs(x - 8), 10.5) * f.lt(f.abs(z - 8), 10.5) *
                f.lt(y, 76) * f.lt(starter_bottom, y)

-- v62: biome noise - huge scale, so island CLUSTERS share a biome
local biome_n = f.noise2d({
    type = "opensimplex2s", fractal = "fbm", frequency = 0.006,
    octaves = 2, seed_offset = 4242,
})
local inside = f.max(f.max(f.max(mid, high), f.max(low, deep)), starter)

local mid1  = layer_at(1,  MID_F,  MID_T,  74,  8, 46, y + 1)
local high1 = layer_at(40, HIGH_F, HIGH_T, 116, 7, 28, y + 1)
local low1  = layer_at(90, LOW_F,  LOW_T,  38,  6, 24, y + 1)
local deep1 = layer_at(777, DEEP_F, DEEP_T, 20, 6, 18, y + 1)
local starter1 = f.lt(f.abs(x - 8), 10.5) * f.lt(f.abs(z - 8), 10.5) *
                 f.lt(y + 1, 76) * f.lt(starter_bottom, y + 1)
local inside1 = f.max(f.max(f.max(mid1, high1), f.max(low1, deep1)), starter1)
local surface = inside * (1 - inside1)

-- v52: island flora - cosmic roses (12) and crystal dandelions (13) patch
-- the meadows. A flower cell is empty air with island density right below,
-- gated by patch + fine noise; keeps off the launch pad area.
local mid_m1  = layer_at(1,  MID_F,  MID_T,  74,  8, 46, y - 1)
local high_m1 = layer_at(40, HIGH_F, HIGH_T, 116, 7, 28, y - 1)
local low_m1  = layer_at(90, LOW_F,  LOW_T,  38,  6, 24, y - 1)
local deep_m1 = layer_at(777, DEEP_F, DEEP_T, 20, 6, 18, y - 1)
local starter_m1 = f.lt(f.abs(x - 8), 10.5) * f.lt(f.abs(z - 8), 10.5) *
                   f.lt(y - 1, 76) * f.lt(starter_bottom, y - 1)
local solid_below = f.max(f.max(f.max(mid_m1, high_m1), f.max(low_m1, deep_m1)), starter_m1)
-- v57: the cell above must ALSO be open - on slopes the old rule stacked
-- flower blocks two or three high (and cocoons ended up under a flower)
local mid_a1  = layer_at(1,  MID_F,  MID_T,  74,  8, 46, y + 1)
local high_a1 = layer_at(40, HIGH_F, HIGH_T, 116, 7, 28, y + 1)
local low_a1  = layer_at(90, LOW_F,  LOW_T,  38,  6, 24, y + 1)
local deep_a1 = layer_at(777, DEEP_F, DEEP_T, 20, 6, 18, y + 1)
local starter_a1 = f.lt(f.abs(x - 8), 10.5) * f.lt(f.abs(z - 8), 10.5) *
                   f.lt(y + 1, 76) * f.lt(starter_bottom, y + 1)
local open_above = 1 - f.max(f.max(f.max(mid_a1, high_a1), f.max(low_a1, deep_a1)), starter_a1)
local patch_n = f.noise2d({
    type = "opensimplex2s", fractal = "fbm", frequency = 0.045,
    octaves = 2, seed_offset = 313,
})
local fine_n = f.noise2d({
    type = "opensimplex2s", fractal = "fbm", frequency = 0.4,
    octaves = 1, seed_offset = 777,
})
-- v63b: the near-open-sky gate kills stacked plants: on a soft island
-- edge two cells in a row could both pass the product threshold and
-- grow a flower on top of a flower. Real ground always has clear sky.
local flora_cell = f.lt(3.0, f.max(f.abs(x - 8), f.abs(z - 8))) *
                   f.lt(0.4, solid_below * (1 - inside) * open_above) *
                   f.lt(0.92, open_above) *
                   (1 - f.lt(0.2, inside)) *
                   f.lt(0.05, patch_n) * f.lt(0.1, fine_n)
local which_n = f.noise2d({
    type = "opensimplex2s", fractal = "fbm", frequency = 0.09,
    octaves = 2, seed_offset = 555,
})
-- v63: biome gates for flora. Every biome keeps its own plant set:
-- the classic crystal meadow no longer leaks into ember or frost.
local ember_f = f.lt(0.22, biome_n)
local frost_f = f.lt(biome_n, -0.22)
local classic_f = 1 - f.max(ember_f, frost_f)
-- v65: the starter island is ALWAYS classic meadow (its ground blocks are
-- forced below), so its flora gates must be forced too - otherwise a frost
-- noise pocket over the spawn pad grew frost bursts and puffballs on
-- classic turf (the old code mixed a frost lawn into the starter meadow).
local starter_flora = f.lt(f.abs(x - 8), 13) * f.lt(f.abs(z - 8), 13)
ember_f = ember_f * (1 - starter_flora)
frost_f = frost_f * (1 - starter_flora)
classic_f = f.max(classic_f, starter_flora)

-- classic meadow: v54 species bands. NOTE: fold must run from the
-- LOWEST threshold up: the last select that fires wins, so higher
-- thresholds (checked later) claim higher which_n.
local classic_id = 33                                 -- lanternberry default
for _, band in ipairs({
    { -0.58, 12 }, -- rose
    { -0.36, 13 }, -- dandelion
    { -0.25, 39 }, -- v58: void tuft ground cover
    { -0.20, 41 }, -- v59: sedge strands
    { -0.14, 28 }, -- bellflower
    { 0.08, 29 },  -- starbloom
    { 0.30, 30 },  -- spiral fern
    { 0.41, 47 },  -- v59.5: void orchid
    { 0.52, 31 },  -- twin tulip
    { 0.575, 52 }, -- v65: void puff (48 frostfern was frost-native: leak)
    { 0.71, 32 },  -- glow grass
    { 0.80, 50 },  -- v61.2: lantern tree groves (very rare)
}) do
    classic_id = f.select(f.lt(band[1], which_n), band[2], classic_id)
end
-- v64.0: mushrooms are lawn residents - each biome grows only its own
-- species right on its turf (bands in the lawn layer below). Three
-- completely different body plans, zero recolors:
--   73 Void Glowcaps - a gregarious CLUSTER of small teal umbrellas
--   74 Cinder Trumpet - a hollow chanterelle-style funnel with a
--     smoldering glow deep in the cup
--   75 Frost Puffball - a stemless lycoperdon-style ball with a cold
--     spore pore cracking open on the crown
midless.define_block(73, {
    name = "Void Glowcaps",
    textures = { all = 73 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(74, {
    name = "Cinder Trumpet",
    textures = { all = 74 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(75, {
    name = "Frost Puffball",
    textures = { all = 75 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})

-- v63.9: stemmed biome blooms, one silhouette each, varied heights.
-- Ember: Smolderhead (coal rosette on a curved stem), Cinder Buds
-- (three pods on stems of three heights), Ember Lantern (tall arch
-- with a hanging glow-lampion).
-- Frost: Frost Burst (icy starburst on a straight stem), Glacier
-- Dewdrop (arched stem with one hanging teardrop bud), Ringbloom
-- (stem topped with a hollow halo of frost orbs).
midless.define_block(67, {
    name = "Smolderhead",
    textures = { all = 67 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(68, {
    name = "Cinder Buds",
    textures = { all = 68 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(71, {
    name = "Glacier Dewdrop",
    textures = { all = 71 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(72, {
    name = "Ringbloom",
    textures = { all = 72 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(76, {
    name = "Ember Lantern",
    textures = { all = 76 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
midless.define_block(77, {
    name = "Frost Burst",
    textures = { all = 77 },
    model = block.model.SPRITE,
    render = block.render.TRANSPARENT,
    collider = block.collider.NONE,
})
-- v61.2 giant ladder (classic biome only) - puffs, moon bells, star
-- reeds, crystal stalks and (the rarest) void trees
classic_id = f.select(f.lt(0.60, fine_n) * f.lt(fine_n, 0.70), 52, classic_id)
classic_id = f.select(f.lt(0.70, fine_n) * f.lt(fine_n, 0.80), 38, classic_id)
classic_id = f.select(f.lt(0.80, fine_n) * f.lt(fine_n, 0.90), 37, classic_id)
classic_id = f.select(f.lt(0.90, fine_n) * f.lt(fine_n, 0.965), 51, classic_id)
classic_id = f.select(f.lt(0.965, fine_n), 49, classic_id)

-- ember isles: smoldering grass lawn with flower PATCHES inside it
-- (patch_n masks where blooms may appear) and rare lantern groves
-- v65: biome patches grow ONLY that biome's own blooms - the old ember
-- patches planted classic twin tulips / lanternberries on charcoal turf
local ember_patch = f.lt(0.40, patch_n)  -- v63.9: actually encounterable (f.lt(a,b) = a < b!)
local ember_id = 59                                   -- ember tuft lawn
ember_id = f.select(ember_patch * f.lt(-0.30, which_n) * f.lt(which_n, 0.00), 67, ember_id)
ember_id = f.select(ember_patch * f.lt(0.00, which_n) * f.lt(which_n, 0.30), 68, ember_id)
ember_id = f.select(ember_patch * f.lt(0.30, which_n), 76, ember_id)
ember_id = f.select(f.lt(0.90, fine_n), 50, ember_id) -- rare lantern tree

-- frost isles: hoarfrost lawn with pale bloom patches, rare void groves
local frost_patch = f.lt(0.40, patch_n)  -- v63.9: actually encounterable (f.lt(a,b) = a < b!)
local frost_id = 60                                   -- frost tuft lawn
frost_id = f.select(frost_patch * f.lt(-0.30, which_n) * f.lt(which_n, 0.15), 77, frost_id)
frost_id = f.select(frost_patch * f.lt(0.15, which_n) * f.lt(which_n, 0.45), 71, frost_id)
frost_id = f.select(frost_patch * f.lt(0.45, which_n), 72, frost_id)
frost_id = f.select(f.lt(0.90, fine_n), 49, frost_id) -- rare void tree

local flower_id = f.select(classic_f, classic_id,
                   f.select(ember_f, ember_id, frost_id))

-- stratified bodies: crystal turf over dirt over void rock over stone.
-- v62: the biome picks the skin - ember turf over ember rock, frost
-- turf over void rock; the starter island keeps the classic look.
local starter_zone = f.lt(f.abs(x - 8), 13) * f.lt(f.abs(z - 8), 13)
local ember_b = f.lt(0.22, biome_n)
local frost_b = f.lt(biome_n, -0.22)
local top_block = f.select(ember_b, 57, 3)
top_block = f.select(frost_b, 58, top_block)
top_block = f.select(starter_zone, 3, top_block)
local mid_body = f.select(ember_b, 56, 2)
mid_body = f.select(frost_b, 19, mid_body)
mid_body = f.select(starter_zone, 2, mid_body)
local body = f.select(surface, top_block, f.select(f.lt(y, 46), 1, mid_body))
local material = f.select(inside, body, 0)
-- v64.0 RULE: every biome surface ALWAYS wears its own grass - the
-- air cell resting on the ground carries the biome lawn: classic
-- turf -> void tufts, ember turf -> ember blades, frost turf -> icy
-- blades. ON that lawn, scattered cell by cell (fine noise, so they
-- pepper the whole island instead of clumping), grow each biome's
-- own blooms and mushrooms - mushrooms simply live on the lawn now
-- (the rain-cloud experiment is gone for good).
local lawn_spot = solid_below * (1 - inside)
local lawn_open = lawn_spot * open_above
local lawn_id = f.select(starter_zone, 39,
                 f.select(ember_b, 59,
                 f.select(frost_b, 60, 39)))
material = f.select(lawn_spot, lawn_id, material)
-- three interleaved fine-noise bands pepper each biome's lawn with
-- its three blooms. Bands sit on MEASURED percentiles of the fine
-- noise (p95=0.467, p97=0.511, p99=0.577 in a representative region)
-- so every island gets a visible scatter of all three species.
-- v65: same starter-zone rule for the lawn layer (the ground skin is
-- forced classic there, so the lawn species must be too)
local lawn_ember = ember_b * (1 - starter_flora)
local lawn_frost = frost_b * (1 - starter_flora)
-- v65: the pepper bands were two-way (ember else FROST), so classic meadows
-- grew frost bursts and ringblooms. Every biome now peppers its OWN three:
--   classic - bellflower / starbloom / lanternberry
--   ember   - smolderhead / cinder buds / ember lantern
--   frost   - frost burst / glacier dewdrop / ringbloom
local bloom1 = f.select(lawn_ember, 67, f.select(lawn_frost, 77, 28))
local bloom2 = f.select(lawn_ember, 68, f.select(lawn_frost, 71, 29))
local bloom3 = f.select(lawn_ember, 76, f.select(lawn_frost, 72, 33))
material = f.select(lawn_open * f.lt(0.460, fine_n) * f.lt(fine_n, 0.495), bloom1, material)
material = f.select(lawn_open * f.lt(0.495, fine_n) * f.lt(fine_n, 0.530), bloom2, material)
material = f.select(lawn_open * f.lt(0.530, fine_n) * f.lt(fine_n, 0.577), bloom3, material)
-- v65.5: MUSHROOMS ARE RAIN-ONLY. The cloud event in mobs.c grows them,
-- they live ten minutes and wither; worldgen plants NONE by design
-- (the old p99+ lawn band made them default flora, which broke the scheme).
material = f.select(flora_cell, flower_id, material)

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
    id = "midless:cosmic", version = 21,
    min_y = 0, max_y = 160, bounded = true,
    sea_level = -1, fill_oceans = false,
    -- v65: density is the ISLANDS only. It used to include flora_cell, so
    -- FindSurfaceHeight returned the plant cell instead of the ground: every
    -- structure then stood one block ABOVE the lawn (floating gates/trees)
    -- and the new ground filter compared against a flower, never the turf.
    material = material, density = inside,
    skylight = f.max(inside, flora_cell),
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

-- volatile barrels: rare near the surface where careless feet find them
wg.define_ore("volatile_barrels", {
    block = 26, replaces = { 3, 2, 19, 1 },
    min_y = 60, max_y = 130, size = 2, spacing = 18, chance = 0.22,
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

-- v65: the leafy green tree belongs to the CLASSIC crystal meadow only -
-- it used to sprout on ember basalt and hoarfrost alike. The ground filter
-- is the new engine-side biome gate (the cosmic biomes are noise regions,
-- not registered WGBiomes, so `biome = ...` cannot express this).
wg.define_structure("midless:cosmic_tree", {
    spacing = 22, chance = 0.55, min_y = 20, max_y = 150,
    max_slope = 5, rotate = true, air_only = true,
    ground = { 3, 2 },
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

-- Alien cocoons: eggs incubating on quiet islands. Approach and they open.
wg.define_structure("midless:void_cocoon", {
    spacing = 8, chance = 0.5, min_y = 20, max_y = 150,
    max_slope = 3, rotate = false,
    air_only = true,   -- v57: a flower cell is skipped, never turned into an egg
    blocks = {
        { x = 0, y = 0, z = 0, block = 25 },
    },
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
