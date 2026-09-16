=== Midless: Cosmic Edition v65.26 ===

A voxel world adrift in space: smooth floating islands hanging in a starlit
void, a black hole wearing a gold photon ring instead of a sun, glowing
crystal water pooled in glass basins, and asteroids where clouds used to be.

WHAT'S NEW IN v65.26
  - The Warden no longer flickers: every nested segment pair that
    shared a plane (knees, heels, elbows, neck) now keeps a clear
    depth margin - the striped moire is gone.
  - Building on grass: placing a block at a grass tuft breaks the
    tuft and takes its cell instead of demanding you clear it first.

WHAT'S NEW IN v65.25
  - The under-the-floor bug, for real this time: the pocket barrier
    used to yank ANY player above y=150 - including jumpers on the
    tallest islands - into the pocket slab. It now only touches
    players inside the pocket zone, and the under-floor rescue net
    actually fires (in v65.24 it was dead code).
  - Battle music fixed: the pocket was wrongly claiming the new
    battle station, so the Warden's awakening changed nothing. The
    pocket plays "Pocket of Clouds" again, and "The Warden Wakes"
    starts when he rises and ends when he falls.

WHAT'S NEW IN v65.24
  - Battle music: the Warden fight now plays "The Warden Wakes", a
    dungeon-synth march from the same generative radio, until he
    falls.
  - The under-the-floor bug is dead: blasts can no longer punch
    through the pocket slab, and anyone who ends up under the
    pocket floor is caught and put back on top.

WHAT'S NEW IN v65.23
  - Warden pass two: arm pivots moved outside the torso and the arm
    ends slimmed (no more interpenetration), the laser rifle now
    stops in him with sparks/flash/recoil/sound, and his death is a
    visible detonation - gold core chunks flying - before the husk
    sinks into the meadow.

WHAT'S NEW IN v65.22
  - Warden polish from playtest: a true kneel (not a squat), heels on
    the turf, no more colour-wrap glitch quads on hands/chest, eased
    arm lowering, white hit flash + recoil + sparks at the hit point,
    fatter hit volumes, footsteps/volley/clang/tear/explosion sounds,
    and the cores condense in the death explosion itself.

WHAT'S NEW IN v65.21
  - BOSS: the Warden of the Meadow - a giant cube golem kneeling in
    the pocket. It rises when seen, walks you down, volleys orbs
    from its hands. Blade and blasts tear its arms off first, then
    the chest core; on death it drops four warp cores - enough for
    another 2x2 gate square (the second pocket comes in v65.22).

WHAT'S NEW IN v65.20
  - Arch legs no longer go transparent up close: the colonnade batch
    is now flushed while backface culling is still off (the deferred
    flush used to land after culling was restored).
  - The invisible barrier widened to radius 63 - you can stand in the
    colonnade itself, just not past it.

WHAT'S NEW IN v65.19
  - The marble now renders double-sided (no more inverted/see-through
    arches up close), and an invisible server-side cylinder just
    inside the colonnade stops anyone from leaving the meadow - on
    foot or in fly mode, up to +200 blocks.

WHAT'S NEW IN v65.18
  - The crossing crash, attempt two: the colonnade no longer uses
    DrawMesh/VAO/custom shader at all - it draws through the same
    rlgl immediate-batch path as the terrain, with lighting baked
    into per-vertex colours. Nothing new on the GL path anymore.

WHAT'S NEW IN v65.17
  - Fixes the v65.16 crash on crossing into the pocket: the colonnade
    draw now flushes rlgl's render batch before and after its DrawMesh
    (pending chunk vertices vs a foreign VAO was the segfault), plus
    client-log breadcrumbs on the colonnade path.

WHAT'S NEW IN v65.16
  - The cloud experiment is retired (sprites glitched at the camera).
    The pocket gets its calm blue sky back and a marble PERISTYLE:
    64 round columns carrying 64 round arches around the lawn edge -
    real smooth mesh geometry with its own marble shader.

WHAT'S NEW IN v65.15
  - The pocket sky is now a dreamcore GRADIENT (periwinkle -> pink ->
    peach horizon glow) instead of a flat mauve; the cloud ring moved
    outside the platform edge and softened - no more white panels at
    the rim or streaks on the lawn, just a cloud collar and a sea
    below the island.

WHAT'S NEW IN v65.14
  - The pocket dream got CONTRAST: a deeper mauve sky, a near-opaque
    cloud wall straddling the lawn edge (no more visible platform rim)
    and a cloud sea drifting below the island. The black-hole lens
    distortion no longer warps the screen inside the pocket.

WHAT'S NEW IN v65.13
  - The pocket universe now DREAMS: a pink-lavender dreamcore sky, a
    pastel cloud sea ringing the meadow, and its own music station
    ("Pocket of Clouds", a music-box lullaby) that swaps in when you
    cross the gate and out when you come home. Moths, wireframe rocks
    and the VOID TIDE do not exist inside the pocket.

WHAT'S NEW IN v65.12
  - The pocket universe is now properly pocketed: from inside it the
    cosmic islands are not rendered, from outside the meadow is not
    rendered - the gate is the only door. Daylight, sky and bloom are
    calmed down and the turf repainted from neon to calm emerald.

WHAT'S NEW IN v65.11
  - The client now SELF-HEALS an outdated terrain.png: missing pocket
    tiles 78-80 get placeholder pixels patched into the GPU atlas, so
    the warp gate and the meadow are visible even from an old install.

WHAT'S NEW IN v65.10
  - DIAGNOSTICS: the client writes midless_client.log next to the exe
    (block rejections, pocket sky transitions, positions) and shows a
    red banner if textures/terrain.png is older than the pocket
    universe. If anything still misbehaves, send that log.

WHAT'S NEW IN v65.9
  - WARP GATE HOTFIX: stepping on the gate now really teleports you
    (the hook used to die on a fractional coordinate probe), the gate
    appears exactly where you placed the fourth core, and mod block
    glow levels reach the client properly.

WHAT'S NEW IN v65.8
  - THE POCKET UNIVERSE. Place four Warp Core blocks (mine them from
    obelisks / spires / ruined shrines) in a 2x2 square: they fuse into
    a Warp Gate. Step on it and you cross into a wide flat meadow under
    a bright day sky - a whole different palette, no nebulae, no black
    hole. The gate in the meadow centre brings you home. The world
    regenerates ONCE on this update.

WHAT'S NEW IN v65.7
  - Multiplayer made hand-off-able: server.ini (port / max players /
    name) beside the exe, HOST SETTINGS boxes on the login screen,
    a HUD line and F6 host panel showing the address friends type,
    player count, and the port-forward note. server.exe ships in the
    zip and reads the same server.ini.

WHAT'S NEW IN v65.6
  - Plants are solid: the sprite batch writes depth again, so nothing
    shows through trees, flowers or mushrooms.
  - Six quick slots; pins show the real mushroom species art; the G key
    is gone - use a quick slot to eat.
  - The satchel shows the name of the item you hover.
  - Mushrooms repainted into their own violet family (purple glowcaps on
    dirt are back): no biome colours, no standard world colours.

WHAT'S NEW IN v65.5
  - Mushroom scheme: rain cloud -> mushrooms grow -> live 10 minutes ->
    wither away. Worldgen plants none; the probe forbids it.
  - Satchel: only carried items show; mushroom species keep separate
    stacks; 12 slots; shards moved to a CURRENCY section.
  - UI type 25% larger everywhere; satchel panel roomier.
  - Emitters dimmed (warp core 8, fire 10, lava 9) - no more bleaching.

WHAT'S NEW IN v65.4
  - Emission reverted to the v65.2 look: up close the glowing flora
    overexposed the turf and the ice, so only warp cores, fire and lava
    emit again. The light_level mechanism stays dormant for a future,
    better-tuned hazard-glow pass.
  - F4 work view and the F3 flora-work line remain (debug, off by default).

WHAT'S NEW IN v65.3
  - F4 flora work view: ground patches tinted by chunk flora load
    (green/yellow/orange/red against the 1024 cap) + F3 HUD line with
    plants, submitted quads and the heaviest chunk in view.
  - Graded emission: blocks glow at their own strength (light_level
    1..15). Barrels warn dull orange, mushrooms bioluminesce, blooms
    and lanternberries glimmer, warp cores/fire/lava stay full blaze.
  - Mod API: optional light_level field on define_block.

WHAT'S NEW IN v65.2
  - Height rule: no plant or mushroom stands in grass as tall as itself.
    Skirt grass clamps to 60% of a bloom's height; mushrooms get the
    lowest lawn level (35%) so their caps always clear the carpet.
  - New tools_dev/flora_heights.py gate: parses world.c and proves the
    rule for every species (min instance vs lawn top, skirt vs own top,
    mushroom cap below flower cap).

WHAT'S NEW IN v65.1
  - Readability pass: ember + frost mushrooms repainted with real stems and
    caps that contrast their ground (gold funnels on bone stems; magenta
    domes on violet stems). Starter-meadow glowcaps untouched.
  - Biome flowers repainted per biome: ember blooms glow gold/amber on
    charcoal turf, frost blooms burn magenta/orchid on pale snow - six
    distinct silhouettes instead of one camouflaged colour per biome.
  - New tools_dev/flora_contrast.py gate: measures ink, height, ground
    contrast and stem presence per species; fails the build-style check if
    any plant camouflages into its biome ground.

WHAT'S NEW IN v65
  - Biome audit: every island cube grows its own grass and its own plants;
    the hidden 80-plants-per-chunk cap that ate half of every lawn is gone.
  - The mushroom rain cloud rains and sprouts again (biome-correct species).
  - Green log-and-leaf trees grow only in the classic meadow and stand in
    the grass; billboard trees got plain straight trunks.
  - The leftover flat cloud layer was deleted; the sky keeps its asteroids.
  - Grazers replant biome blooms; cocoons spawn on meadows again.

ARCHIVE (v43)WHAT'S NEW IN v43
  - Black hole rebuilt: rotating gold accretion disk, photon ring, violet
    halo, and real gravity lensing that bends the view around it.
  - Deeper sky: galaxy band of stars, drifting nebulae, shooting stars,
    slow celestial rotation.
  - New cosmic textures: indigo rock, teal crystal turf, glowing water,
    gold / crystal / void-shard ores, warp cores, launch pads.
  - Your explorer now has hands with individual fingers that curl when you
    swing, grip when you idle.
  - New world: island cone-taper, starter island with launch pad, crystal
    arch, warp-core obelisks and a glass water basin; 3 new ores.
  - Game feel: coyote time, jump buffering, jump/teleport/UI sounds,
    ambient void wind, void rescue fade.
  - Options (as in v42): draw distance, debug, max FPS, fullscreen (F11),
    resolution, volume.

CONTROLS:
  WASD          Move
  Space         Jump (fly up in fly mode)
  Shift         Fly down
  Left Click    Break block
  Right Click   Place block
  Mouse Wheel   Select block
  T             Chat  (arrows = history)
  /help /where /tp /spawn /fly /time /giveme
  ESC           Menu  (New World / Regenerate World in singleplayer)
  M             Map
  Tab           Fly mode
  F3            Debug
  F5            Camera view
  F11           Fullscreen

HOW TO PLAY:
  1. Extract this folder anywhere.
  2. Run game.exe
  3. Click Singleplayer
  4. If you played an older build, the world upgrades automatically:
     the game detects the old/legacy save and regenerates it as a fresh
     cosmic world (your seed is kept). You can also delete the "world"
     folder next to game.exe for a completely clean start.

Falling into the void returns you to the starter island.
The warp-core obelisks on the starter island are light sources - build
your own beacons with blocks 20 (crystal) and 22 (warp core).

Multiplayer / dedicated server: run server.exe (port 25565).

Files in this folder:
  game.exe        the game
  server.exe      dedicated server (optional)
  README.txt      this file
  world_guide.png illustrated guide: every block, plant and creature
  world_guide.md  the same guide as text (ids + biome notes)
  textures\       terrain, skin and icon (used by the game)
  mods\           world generation (cosmic_islands.lua)


============================================================
SPRITE EDITOR / РЕДАКТОР СПРАЙТОВ
============================================================
Open sprite_editor/sprite_editor.html in any browser.
Запустите sprite_editor/sprite_editor.html в любом браузере.

Draw a plant or creature, save the PNG (1x for the game, 8x for preview),
and send it to the developer - it will be wired into the game atlas.
Нарисуйте растение или существо, сохраните PNG (1x - точный размер,
8x - превью) и пришлите разработчику - спрайт будет добавлен в игру.

Other pixel editors work too (Aseprite, Libresprite, Piskel, Pixilart,
Paint.net): canvas exactly 16x16 or 32x32, transparent background,
save PNG at scale 1:1.
Подходят и другие пиксель-редакторы: холст ровно 16x16 или 32x32,
прозрачный фон, PNG без увеличения.
