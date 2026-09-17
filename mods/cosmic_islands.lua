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

-- v65.8 POCKET UNIVERSE: a wide flat meadow far away from the cosmic
-- noise, reached only through a warp gate. The zone box REPLACES all
-- cosmic density inside it, so no island, ore or structure shares the
-- pocket sky. The lawn sits at y=154 - above every structure max_y
-- (150), so worldgen decorations can never intrude.
-- NOTE: POCKET_CX / POCKET_CZ / 96.5 zone half must match the client-side
-- sky swap in client/src/pocketfx.c.
local POCKET_CX, POCKET_CZ = 1200, -1200
local POCKET_HALF = 64           -- the field spans 129x129 blocks
local POCKET_TOP  = 154          -- lawn surface cell
local pocket_zone  = f.lt(f.abs(x - POCKET_CX), 96.5) * f.lt(f.abs(z - POCKET_CZ), 96.5)
local pocket_field = f.lt(f.abs(x - POCKET_CX), POCKET_HALF + 0.5) *
                     f.lt(f.abs(z - POCKET_CZ), POCKET_HALF + 0.5) *
                     f.lt(y, POCKET_TOP + 0.5) * f.lt(POCKET_TOP - 6.5, y)
local pocket_gate_cell = f.eq(x, POCKET_CX) * f.eq(z, POCKET_CZ) * f.eq(y, POCKET_TOP)
-- turf wears the top cell (the centre one is the return gate), warm loam
-- fills the six-block slab below. IMPORTANT: the material field IS the
-- placed block (Worldgen_Generate writes it per cell, density never
-- gates it), so the slab mask must be multiplied in - outside the field
-- the pocket material is AIR (0), same as the material chain expects.
local pocket_material = f.select(pocket_gate_cell, 80,
                        f.select(f.eq(y, POCKET_TOP), 78, 79)) * pocket_field

-- v65.29: THE FOUNDRY LOWERED 36 BLOCKS (floor 154 -> 118): worldgen is
--   bounded at max_y=160, and the 154-floor course built its upper half
--   (walls 164+, chimney 181, finish 184) ABOVE the bound - the engine
--   clipped it out of existence and the instance looked broken/empty.
--   Everything now tops out at 151 (the beacon). Also: a square fused
--   inside the meadow announces itself as a FOUNDRY gate, and the
--   barrier/rescue net learned per-zone floor heights.
-- v65.28 SECOND POCKET: "the Foundry" - a brutalist parkour course, the
-- reward instance behind the Warden. Sync contract: client/src/pocketfx.h
-- POCKETFX2_CX / POCKETFX2_CZ / POCKETFX_ZONE_HALF / POCKETFX2_TOP.
local P2_CX, P2_CZ = -1200, 1200
-- v65.29: the Foundry floor sits at 118, NOT 154: worldgen is bounded at
-- max_y=160, so a 154 floor left the whole upper course (walls 164+,
-- chimney 181, finish 184) clipped out of existence - the instance looked
-- empty. Lowered by 36, the beacon tops out at 151, safely under 160.
local P2_TOP = 118
local pocket_zone2 = f.lt(f.abs(x - P2_CX), 96.5) * f.lt(f.abs(z - P2_CZ), 96.5)

-- course helper: a solid box centred on (x0,y0,z0) with half extents;
-- half-integer centres + (n-1)/2 half extents give exact n-cell spans
local function pbox(x0, y0, z0, hx, hy, hz)
    return f.lt(f.abs(x - x0), hx + 0.5) *
           f.lt(f.abs(y - y0), hy + 0.5) *
           f.lt(f.abs(z - z0), hz + 0.5)
end
-- v65.30: the plaza (and the gate) sit west of the zone centre so the whole
-- 130-long course fits inside the zone box (the pocket_zone2 mask is +-96;
-- the v65.29 course ran to +118 and was silently masked back into void)
local p2_gate_cell = f.eq(x, P2_CX - 40) * f.eq(z, P2_CZ + 8) * f.eq(y, P2_TOP)

-- v65.30: THE FOUNDRY, REBUILT AS A PARKOUR ARENA. The v65.28 course read
-- as clutter: bare pads floating over a horizon-wide slab, no bounds, no
-- visible line. Research (KZ/CS climb maps, Mirror's Edge flow, Titanfall
-- wall-run levels, VHOLUME, Minecraft parkour guides) says: compact arena
-- with VISIBLE bounds, one readable chain of obstacles with rest beats,
-- every mass SUPPORTED (floating blocks read as mistakes), light as the
-- only signposting, one shortcut that rewards route knowledge. Spacings
-- follow Midless physics (jump ~5 blocks up, ~9-10 across at sprint):
-- flow gaps 4 with +1 rises, precision gaps 4 onto 3x3 pads.
-- the plaza slab (same 7-cell thickness as the meadow), now 57x57 with a
-- two-cell rim wall so the arena HAS an edge instead of a fake horizon
local parkour_field = pbox(P2_CX - 40, P2_TOP - 3, P2_CZ + 8, 28, 3, 28)
local function padd(b) parkour_field = f.max(parkour_field, b) end
padd(pbox(P2_CX - 40, 119.5, P2_CZ - 20, 28, 0.5, 0))     -- rim north
padd(pbox(P2_CX - 40, 119.5, P2_CZ + 36, 28, 0.5, 0))     -- rim south
padd(pbox(P2_CX - 68, 119.5, P2_CZ + 8, 0, 0.5, 28))      -- rim west
padd(pbox(P2_CX - 12, 119.5, P2_CZ - 8.5, 0, 0.5, 11.5))  -- rim east, gap z+4..z+12 = the route out
padd(pbox(P2_CX - 12, 119.5, P2_CZ + 24.5, 0, 0.5, 11.5))
-- M1: the landmark monolith in the plaza's south-west corner (climbable)
padd(pbox(P2_CX - 62, 128, P2_CZ - 12, 1.5, 9, 1.5))
-- chain 1: the flow steps - three 5x5 towers out of the slab, gap 4, +1
-- rise each (a tower reads as SUPPORT, a floating pad reads as a mistake)
padd(pbox(P2_CX - 6, 115.5, P2_CZ + 8, 2, 3.5, 2))
padd(pbox(P2_CX + 3, 116, P2_CZ + 8, 2, 4, 2))
padd(pbox(P2_CX + 12, 116.5, P2_CZ + 8, 2, 4.5, 2))
-- the rest beat: a 5x7 pad between chains (Mirror's Edge rhythm)
padd(pbox(P2_CX + 22, 116, P2_CZ + 8, 2, 5, 3))
-- chain 2: the wall-run - a 17-long face over open void; run it east and
-- step off onto the ledge. The wall grows straight out of the slab.
padd(pbox(P2_CX + 35, 119, P2_CZ + 4, 8, 7, 0))
padd(pbox(P2_CX + 47, 119, P2_CZ + 6, 1, 7, 1))          -- exit ledge tower
-- chain 3: the chimney - two 23-high faces 2 apart; drop in from the ledge
-- and wall-kick the zigzag to the crown, step out onto the back shelf
padd(pbox(P2_CX + 51, 121, P2_CZ + 7, 0, 11, 2))
padd(pbox(P2_CX + 54, 121, P2_CZ + 7, 0, 11, 2))
padd(pbox(P2_CX + 52.5, 122.5, P2_CZ + 11.5, 2.5, 10.5, 1.5))
-- chain 4: precision hops - 3x3 pad towers, gap 4
padd(pbox(P2_CX + 60, 122.5, P2_CZ + 12, 1, 10.5, 1))
padd(pbox(P2_CX + 67, 122.5, P2_CZ + 12, 1, 10.5, 1))
padd(pbox(P2_CX + 74, 122.5, P2_CZ + 12, 1, 10.5, 1))
-- the finish plateau: a rimmed monolith with a beacon, entry gap west
padd(pbox(P2_CX + 84, 122.5, P2_CZ + 12, 4, 10.5, 4))
padd(pbox(P2_CX + 84, 134, P2_CZ + 8, 4, 0, 0))
padd(pbox(P2_CX + 84, 134, P2_CZ + 16, 4, 0, 0))
padd(pbox(P2_CX + 88, 134, P2_CZ + 12, 0, 0, 4))
padd(pbox(P2_CX + 80, 134, P2_CZ + 8.5, 0, 0, 0.5))
padd(pbox(P2_CX + 80, 134, P2_CZ + 15.5, 0, 0, 0.5))
-- the shortcut: a stepped monolith south of the wall-run - base 124 from
-- the rest pad, column 128, then drop onto the wall-run exit ledge and
-- skip the traverse entirely (route knowledge pays, VHOLUME-style)
padd(pbox(P2_CX + 32, 118.5, P2_CZ + 10, 1.5, 5.5, 1.5))
padd(pbox(P2_CX + 32, 126.5, P2_CZ + 10, 0.5, 1.5, 0.5))

-- path lamps (83): the route is signposted by LIGHT, not waypoints
local function lamp(x0, y0, z0) return pbox(x0, y0, z0, 0, 0, 0) end
local lamps = lamp(P2_CX - 37, 118, P2_CZ + 11)
for k = 34, 28, -3 do lamps = f.max(lamps, lamp(P2_CX - k, 118, P2_CZ + 11 + (34 - k))) end
-- the route-out gap glows on both lips
lamps = f.max(lamps, lamp(P2_CX - 12, 120, P2_CZ + 3))
lamps = f.max(lamps, lamp(P2_CX - 12, 120, P2_CZ + 13))
-- flow steps: a lamp on the far corner of each tower reads as "next"
lamps = f.max(lamps, lamp(P2_CX - 4, 120, P2_CZ + 10))
lamps = f.max(lamps, lamp(P2_CX + 5, 121, P2_CZ + 10))
lamps = f.max(lamps, lamp(P2_CX + 14, 122, P2_CZ + 10))
-- rest pad corners
lamps = f.max(lamps, lamp(P2_CX + 20, 122, P2_CZ + 5))
lamps = f.max(lamps, lamp(P2_CX + 24, 122, P2_CZ + 11))
-- the traverse: a light strip along the wall top, and the ledge
lamps = f.max(lamps, lamp(P2_CX + 28, 127, P2_CZ + 4))
lamps = f.max(lamps, lamp(P2_CX + 34, 127, P2_CZ + 4))
lamps = f.max(lamps, lamp(P2_CX + 40, 127, P2_CZ + 4))
lamps = f.max(lamps, lamp(P2_CX + 48, 127, P2_CZ + 7))
-- chimney crown
lamps = f.max(lamps, lamp(P2_CX + 51, 133, P2_CZ + 5))
lamps = f.max(lamps, lamp(P2_CX + 54, 133, P2_CZ + 9))
-- precision pads: one lamp on the far edge of each
lamps = f.max(lamps, lamp(P2_CX + 61, 134, P2_CZ + 13))
lamps = f.max(lamps, lamp(P2_CX + 68, 134, P2_CZ + 13))
lamps = f.max(lamps, lamp(P2_CX + 75, 134, P2_CZ + 13))
-- finish ring + the beacon column over the plateau
lamps = f.max(lamps, lamp(P2_CX + 80, 134, P2_CZ + 8))
lamps = f.max(lamps, lamp(P2_CX + 88, 134, P2_CZ + 16))
lamps = f.max(lamps, pbox(P2_CX + 84, 135.5, P2_CZ + 12, 0, 1.5, 0))
parkour_field = f.max(parkour_field, lamps)

local parkour_material = f.select(p2_gate_cell, 80,
                          f.select(lamps, 83,
                          f.select(f.lt(y, P2_TOP), 82, 81))) * parkour_field

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
                   f.lt(0.05, patch_n) * f.lt(0.1, fine_n) *
                   (1 - f.max(pocket_zone, pocket_zone2))  -- v65.8/v65.28: both pockets stay pure
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

-- v65.8 POCKET UNIVERSE blocks. Fresh ids on purpose: every cosmic flora
-- gate keys off the old ground ids, so the pocket lawn stays a pure,
-- quiet meadow - no bells, no mushrooms, no embers.
midless.define_block(78, {
    name = "Pocket Turf",
    textures = { top = 78, sides = 78, bottom = 79 },
})
midless.define_block(79, {
    name = "Pocket Loam",
    textures = { all = 79 },
})
-- 80 warp_gate: the BIG warp core. Four warp cores (22) placed in a 2x2
-- square fuse into one of these (hook at the end of the file); step onto
-- it and you cross into the pocket universe.
midless.define_block(80, {
    name = "Warp Gate",
    textures = { all = 80 },
    light = block.light.EMIT,
    light_level = 10,
})
-- v65.28 the Foundry: raw brutalist concrete + a calm path lamp
midless.define_block(81, {
    name = "Concrete",
    textures = { all = 81 },
})
midless.define_block(82, {
    name = "Concrete Base",
    textures = { all = 82 },
})
midless.define_block(83, {
    name = "Path Lamp",
    textures = { all = 83 },
    light = block.light.EMIT,
    light_level = 6,
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

-- v65.8: inside the pocket zone NOTHING cosmic survives - the box is the
-- meadow slab (with the return gate in its centre) and empty sky around it
material = f.select(pocket_zone, pocket_material, material)
-- v65.28: the Foundry zone swaps in the parkour course
material = f.select(pocket_zone2, parkour_material, material)

wg.configure({
    id = "midless:cosmic", version = 24,
    min_y = 0, max_y = 160, bounded = true,
    sea_level = -1, fill_oceans = false,
    -- v65: density is the ISLANDS only. It used to include flora_cell, so
    -- FindSurfaceHeight returned the plant cell instead of the ground: every
    -- structure then stood one block ABOVE the lawn (floating gates/trees)
    -- and the new ground filter compared against a flower, never the turf.
    material = material,
    -- v65.8: the pocket zone swaps cosmic density for the flat meadow slab
    density = f.select(pocket_zone2, parkour_field, f.select(pocket_zone, pocket_field, inside)),
    skylight = f.max(f.max(f.max(inside, flora_cell), pocket_field), parkour_field),
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
    avoid_boxes = POCKET_BOXES,   -- v65.30
    spacing = 44, chance = 0.55, min_y = 20, max_y = 150,
    max_slope = 4, rotate = false, air_only = false,
    foundation = 19, foundation_depth = 4,
    blocks = basin,
})

wg.define_structure("midless:crystal_spire", {
    avoid_boxes = POCKET_BOXES,   -- v65.30
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
    avoid_boxes = POCKET_BOXES,   -- v65.30
    spacing = 22, chance = 0.55, min_y = 20, max_y = 150,
    max_slope = 5, rotate = true, air_only = true,
    ground = { 3, 2 },
    tree = { height = 5, radius = 2, trunk = 10, leaves = 11 },
})

-- v65.30: overworld decor must never scatter inside the pocket instances.
-- The meadow only escaped by luck (its floor sits above their max_y); the
-- Foundry's lower floor did not, and basins/spires/totems/cocoons/floats
-- littered the plaza - the "clutter" of the v65.29 screenshot.
local POCKET_BOXES = {
    { x0 = POCKET_CX - 96, z0 = POCKET_CZ - 96, x1 = POCKET_CX + 96, z1 = POCKET_CZ + 96 },
    { x0 = P2_CX - 96,     z0 = P2_CZ - 96,     x1 = P2_CX + 96,     z1 = P2_CZ + 96 },
}

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
    avoid_boxes = POCKET_BOXES,   -- v65.30
    spacing = 36, chance = 0.42, min_y = 20, max_y = 150,
    max_slope = 3, rotate = true, air_only = true,
    blocks = gate,
})

-- Chrome totem: polished stack with a levitating crystal cap. Pure Y2K
-- antenna aesthetics, catches the nebula light from far away.
wg.define_structure("midless:chrome_totem", {
    avoid_boxes = POCKET_BOXES,   -- v65.30
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
    avoid_boxes = POCKET_BOXES,   -- v65.30
    spacing = 52, chance = 0.5, min_y = 20, max_y = 150,
    max_slope = 2, rotate = true, air_only = false,
    foundation = 19, foundation_depth = 3,
    blocks = shrine,
})

-- Alien cocoons: eggs incubating on quiet islands. Approach and they open.
wg.define_structure("midless:void_cocoon", {
    avoid_boxes = POCKET_BOXES,   -- v65.30
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
    avoid_boxes = POCKET_BOXES,   -- v65.30
    spacing = 24, chance = 0.3, min_y = 20, max_y = 150,
    max_slope = 6, rotate = true, air_only = true,
    blocks = {
        { x = 0, y = 3, z = 0, block = 23 },
        { x = 1, y = 4, z = 1, block = 20 },
        { x = -1, y = 5, z = 0, block = 24 },
        { x = 0, y = 6, z = 1, block = 23 },
    },
})

--------------------------------------------- pocket universe hooks (v65.8) --
-- 1) Four warp cores (22) in a 2x2 square FUSE into one warp gate (80):
--    the anchor cell (lowest x/z of the square) becomes the gate, the
--    other three cores are consumed. Everyone hears about it.
-- 2) Stepping onto a gate swaps worlds:
--      cosmic side -> pocket meadow spawn (a few blocks off the centre)
--      pocket side -> back beside the gate you arrived from
--    The spawn points are never ON a gate, and a 2.5s cooldown per player
--    keeps the crossing one-shot.
local WARP_CORE_ID, WARP_GATE_ID = 22, 80
local pocket_clock = 0.0
local pocket_origin, pocket_cooldown = {}, {}
local morphing_gate = false

midless.register_on_block_update(function(pos, newId, oldId)
    if morphing_gate or newId ~= WARP_CORE_ID then return end
    local gx, gy, gz = math.floor(pos.x), math.floor(pos.y), math.floor(pos.z)
    local offs = { { 0, 0 }, { -1, 0 }, { 0, -1 }, { -1, -1 } }
    for o = 1, 4 do
        local ax, az = gx + offs[o][1], gz + offs[o][2]
        local square = true
        for dx = 0, 1 do
            for dz = 0, 1 do
                if midless.get_block({ x = ax + dx, y = gy, z = az + dz }) ~= WARP_CORE_ID then
                    square = false
                end
            end
        end
        if square then
            -- the gate rises WHERE THE PLAYER COMPLETED THE SQUARE (the
            -- cell they just placed); the other three cores are consumed
            morphing_gate = true
            for dx = 0, 1 do
                for dz = 0, 1 do
                    local cx, cz = ax + dx, az + dz
                    local id = (cx == gx and cz == gz) and WARP_GATE_ID or 0
                    midless.set_block({ x = cx, y = gy, z = cz }, id)
                end
            end
            morphing_gate = false
            -- v65.29: a square completed INSIDE the meadow opens the Foundry,
            -- so say so - the generic line read as "nothing new happened"
            if math.abs(gx - POCKET_CX) < 96 and math.abs(gz - POCKET_CZ) < 96 then
                midless.broadcast("The cores fuse into a Foundry Gate! Step onto it - a parkour course awaits.")
            else
                midless.broadcast("The warp cores fuse into a Warp Gate! Step onto it to cross over.")
            end
            return
        end
    end
end)

-- v65.19: the peristyle is a fence in marble only - nothing stopped
-- a walker (or a flyer) from stepping between the columns and off the
-- island. An invisible cylinder just inside the colonnade clamps the
-- position every tick, from the lawn up to fly-mode heights.
local BARRIER_R = 63.0   -- v65.20: widened from 60 - walk up to the columns
-- v65.28/v65.29: one barrier+rescue rule, both pocket zones; the Foundry
-- floor is LOWER (118), so each zone carries its own top
local pocket_zones = {
    { cx = POCKET_CX, cz = POCKET_CZ, top = POCKET_TOP, catch = "The meadow catches you." },
    { cx = P2_CX,     cz = P2_CZ,     top = P2_TOP,     catch = "The slab catches you." },
}
midless.register_on_step(function(dt)
    local players = midless.get_players()
    for i = 1, #players do
        local p = players[i]
        local pos = p:get_position()
        for zi = 1, #pocket_zones do
            local zone = pocket_zones[zi]
            -- v65.25: the barrier used to test only HEIGHT against the whole
            -- world: anyone above y=150 anywhere (the tallest islands reach
            -- that) was yanked to r=63 from the pocket centre - straight
            -- INTO the lawn slab, and the physics squeezed them out under
            -- the floor. The clamp and the rescue only ever look at players
            -- actually inside a pocket zone footprint.
            local dx = pos.x - (zone.cx + 0.5)
            local dz = pos.z - (zone.cz + 0.5)
            if math.abs(dx) < 96 and math.abs(dz) < 96 then
                if pos.y < zone.top - 4 then
                    -- rescue net: however you ended up UNDER the pocket
                    -- floor, the pocket catches you and puts you back on top
                    p:teleport({ x = pos.x, y = zone.top + 2, z = pos.z })
                    p:send_message(zone.catch)
                elseif pos.y < zone.top + 200 then
                    local r = math.sqrt(dx * dx + dz * dz)
                    if r > BARRIER_R then
                        local k = BARRIER_R / r
                        p:teleport({ x = zone.cx + 0.5 + dx * k, y = pos.y, z = zone.cz + 0.5 + dz * k })
                    end
                end
            end
        end
    end
end)

midless.register_on_step(function(dt)
    pocket_clock = pocket_clock + dt
    local players = midless.get_players()
    for i = 1, #players do
        local p = players[i]
        local id = p:get_id()
        local last = pocket_cooldown[id]
        if not last or pocket_clock - last > 2.5 then
            local pos = p:get_position()
            -- NOTE: get_block coerces coords to integers, so floor here -
            -- a fractional y made the binding error out (v65.8 fix)
            local fx, fz = math.floor(pos.x), math.floor(pos.z)
            local gy = math.floor(pos.y - 0.5)
            local below = midless.get_block({ x = fx, y = gy, z = fz })
            if below == WARP_GATE_ID then
                pocket_cooldown[id] = pocket_clock
                local in_p1 = math.abs(fx - POCKET_CX) < 96 and math.abs(fz - POCKET_CZ) < 96
                local in_p2 = math.abs(fx - P2_CX) < 96 and math.abs(fz - P2_CZ) < 96
                -- the meadow's CENTRE gate is the way home; any gate BUILT
                -- elsewhere in the meadow (v65.28: from the Warden's cores)
                -- opens the Foundry instead
                local home_gate = in_p1 and math.abs(fx - POCKET_CX) <= 1 and math.abs(fz - POCKET_CZ) <= 1
                if in_p1 and not home_gate then
                    pocket_origin[id] = { x = fx, y = gy, z = fz }
                    p:teleport({ x = P2_CX - 35.5, y = P2_TOP + 2, z = P2_CZ + 12.5 })
                    p:send_message("You cross into the Foundry: raw concrete, long gaps, and a clock. Run.")
                elseif in_p1 or in_p2 then
                    local o = pocket_origin[id]
                    if o then
                        -- land BESIDE the gate you arrived from (its square is clear)
                        p:teleport({ x = o.x + 1.5, y = o.y + 1.0, z = o.z + 1.5 })
                    else
                        p:teleport({ x = 8.5, y = 80.0, z = 8.5 })
                    end
                    p:send_message("The pocket universe folds away - welcome back.")
                else
                    pocket_origin[id] = { x = fx, y = gy, z = fz }
                    p:teleport({ x = POCKET_CX + 4.5, y = POCKET_TOP + 2, z = POCKET_CZ + 4.5 })
                    p:send_message("You cross into the pocket universe: a meadow adrift in a sea of pastel clouds.")
                end
            end
        end
    end
end)

-- v65.28: the Foundry time trial. Crossing the lamp ring on the start pad
-- arms the clock; the finish plateau stops it and keeps a personal best
-- per player (VHOLUME lives and dies by the clock, so the course does too).
local course_start, course_best = {}, {}
midless.register_on_step(function(dt)
    local players = midless.get_players()
    for i = 1, #players do
        local p = players[i]
        local pos = p:get_position()
        if math.abs(pos.x - P2_CX) < 96 and math.abs(pos.z - P2_CZ) < 96 then
            local id = p:get_id()
            if math.abs(pos.x - (P2_CX + 3)) < 12 and math.abs(pos.z - (P2_CZ + 8)) < 4
               and pos.y < P2_TOP + 6 then
                if not course_start[id] then
                    course_start[id] = pocket_clock
                    p:send_message("Course armed. The clock is running.")
                end
            elseif math.abs(pos.x - (P2_CX + 84)) < 5 and math.abs(pos.z - (P2_CZ + 12)) < 5
               and pos.y > P2_TOP + 14 then
                local t0 = course_start[id]
                if t0 then
                    course_start[id] = nil
                    local t = pocket_clock - t0
                    local best = course_best[id]
                    if not best or t < best then
                        course_best[id] = t
                        p:send_message("Course cleared in " .. string.format("%.1f", t) ..
                                       " s - a new personal best!")
                    else
                        p:send_message("Course cleared in " .. string.format("%.1f", t) ..
                                       " s (best: " .. string.format("%.1f", best) .. " s).")
                    end
                end
            end
        end
    end
end)
