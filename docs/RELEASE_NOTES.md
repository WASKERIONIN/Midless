# Midless: Cosmic Edition v65.36

Floating islands drifting through a starlit void. The sun is a black hole wearing a gold ring.

## v65.46

- LOADING BAR TELLS THE TRUTH AGAIN: the v65.45 crawl cap (~10%/s) is
  removed - the displayed progress tracks the real gate readiness 1:1
  and the gate opens on the real 95% condition as soon as it is met.
- THE GRAZER STANDS ON THE PROGRESS LINE: the turf fill and the green
  edge now end exactly at the rabbit's nose, so it walks ON the line
  instead of ahead of it, and at 0% the whole rabbit is already on the
  bar (tail at the left edge) instead of spawning in front of it.
- CORNER BLOCK PREVIEW IS SIGNED: the held-item preview in the top-right
  corner now shows the item's name under the icon in a readable 16px
  font with a dark outline (underscores become spaces, words are
  capitalised; mushrooms and the Gaze Scroll keep their proper satchel
  names). Empty hand shows no label.
- THE STALE BUILD-TEXTURE TRAP IS GONE FOR GOOD: the tracked
  build/client/textures/*.png duplicates (which twice went stale on
  workspace rollbacks and overrode the real atlas during packaging) are
  untracked and deleted; the release workflow now packages
  client/textures as the single source of truth.

## v65.45

- LOADING BAR PACING: now that the world often fills in a few seconds,
  the grazer could not finish its walk - the gate opened while the bunny
  was still before the middle of the bar. The displayed progress now
  climbs at most ~10%/s toward the real readiness, and the gate opens
  only when BOTH conditions hold: world 95% built AND the walk at 100%
  (the 45 s safety cap is unchanged). The grazer also starts ON the bar
  (tail at the left edge at 0%) instead of standing in front of it, and
  the flowers are munched when its nose actually reaches them.
- GRAZER SLEEP IS LESS EAGER: a grazer now naps only after at least
  three meals (was: 60% chance after EVERY meal, plus 22% idle doze
  after a single bite). Sleep durations unchanged.
- A SLEEPING GRAZER NO LONGER STARTLES from 5.5 blocks - it wakes and
  flees only when the player comes within 1 block (awake flee radius
  stays 3.4).
- MORE GRAZER INTERACTION: a wandering grazer passing within 2-6 blocks
  of a SLEEPING one can now wake it for the turn-based hop conversation
  (previously both had to be idle-awake); chat chance 35% -> 45% per
  decision tick, per-pair cooldown 25-50 s -> 12-27 s.
- MUSHROOMS DRY UP INSTEAD OF BLINKING: the 2 Hz expiry blink in the
  last 30 s is removed; the existing wither shrink is extended to the
  final 24 s, runs smoothly all the way to zero scale, and the cap
  darkens as it dries.

## v65.44

- WORLD LOADING IS HONEST NOW. The loading screen never actually loaded
  the world: the fill gate only checked that 185 chunks (radius-3 disc,
  ~0.7% of the visible world at draw distance 30) had their DATA arrive,
  and chunk building (light + mesh) did not run at all until after entry -
  ~25k queued chunks then assembled in-game on a 4 ms/frame budget, so
  islands popped in one by one for minutes. Now the loading screen pumps
  the build queue at 30 ms/frame, the gate covers a radius-10 disc
  (~1585 chunks) and requires every chunk to be fully BUILT
  (data + light + mesh), opening at 95% (45 s safety cap unchanged).
- PURE-AIR CHUNKS TAKE A FAST PATH. Most chunks in a floating-islands
  world are empty air, yet each ran the full emitter scan, light
  reconcile and 4096-cell mesh pass. The arriving RLE now marks all-air
  chunks; their sunlight is filled uniformly when every column is
  sky-open (with all reconcile face flags set, so shadowed island
  undersides still receive lateral light exactly as before) or falls
  back to the real flood, and the mesh pass is skipped entirely. A new
  headless test (tools_dev/airlight_test.c, `probe_build.sh
  airlight-run`) proves the fast path byte-identical to the classic
  pipeline in four scenarios: open sky, covered underside, overhang
  side-lighting and a neighbouring emitter; it also covers the
  dirty-list and block-placement mutation semantics.
- QUADRATIC LOAD PATHS REMOVED. The per-pop closest-chunk scan of the
  build queue (O(queue) on every chunk, quadratic over a full load) is
  replaced by a distance sort done once per player chunk crossing for
  big backlogs; the whole-hashmap light-dirty scan after every built
  chunk is replaced by a targeted dirty list (World_MarkLightDirty);
  the offline World_LoadChunks ring scan (~33k positions per frame from
  Player_Update) now runs only when the player crosses a chunk border.
- SERVER CHUNK STREAMING IS PIPELINED. A single chunkRequestPending
  flag stalled ALL streaming while the loader thread generated one
  chunk; a player may now keep up to 8 generation requests in flight
  and already-generated chunks keep flowing while generation runs.
- IN-GAME BUILD BUDGET IS ADAPTIVE: 10 ms/frame while the backlog
  exceeds 900 chunks, 6 ms above 200, 4 ms otherwise.
- Release hygiene: a workspace rollback briefly left a stale
  build/client/textures atlas copy in the local working tree (old teal
  mushrooms, leaves holes); per protocol both tracked atlases were
  re-audited tile-by-tile against the current generator and are
  byte-identical again. Repository and CI artefacts were never
  affected.
- Dev tooling: airlight_test added to the probe battery; pocketplay
  full loop, worldprobe, modprobe, compile (76+3) and api checks green.

## v65.43

- GILDING IS VISIBLE AGAIN: the dragon's natural-ground whitelist only
  knew the base terrain ids 1-18 and meadow turf 78, but main-world
  islands surface as Ember Turf (57) and Frost Turf (58) - every gild
  candidate was rejected and no gold plates ever spawned. Dragon_NaturalId
  now accepts 57/58.
- TRANSPARENT-PIXEL HOLES FIXED AT THE SOURCE: a full atlas transparency
  audit found exactly one cube tile with punched-through pixels - leaves
  (tile 10, 4 fully transparent texels, deliberately skipped by the
  generator since pre-v65.6). The hole-punching is removed (replaced by a
  dark speckle for depth) and tile 10 joined CUBE_TILES, so
  enforce_opaque_cube_tiles guarantees it. Atlas regenerated and copied to
  build/client/textures/; intentionally translucent tiles (water 14, fire
  16, glass 17, crystal 22) untouched.
- GRAZERS TALK IN TURNS: two idle grazers within 2-6 blocks of each other
  can enter a new social state (5): they face one another and perform up
  to 4 ballistic hops each (v0 3.0, g 9.8), strictly ALTERNATING - the
  pair starts with offset timers (0.15 s vs 0.80 s) and every landing
  adds a 0.85-1.15 s wait, so one always hops while the other waits.
  Per-pair cooldown 25-50 s; the chat dissolves if the partner wanders
  beyond 8 blocks or the player comes within the usual 3.4-block flee
  radius. Ground-snap smoothing is suspended mid-hop only.

## v65.42

- MUSHROOM COLOURS RESTORED: the shipped terrain atlas was STALE - it had
  been regenerated back in v65.34 from an outdated texture script (a
  workspace-rollback artefact), so since v65.34 the Void Glowcaps (73)
  were teal-green again and Cinder Trumpet / Frost Puffball wore the old
  ember/frost palettes. The atlas is regenerated from the current script:
  all three mushroom species are back in the violet family per the colour
  rule, and every other tile matches the generator again.
- DRAGON HOVERS LIKE BEFORE: the v65.41 circular orbit plus tangential yaw
  read as "spinning around a strange axis". The dragon now parks above its
  island, bobs gently and turns slowly on the vertical axis.
- DRAGON AND RAIN CLOUD NEVER SHARE AN ISLAND: the dragon rejects anchors
  within 80 blocks of an active mushroom rain cloud, and the cloud event
  defers 30 s when its chosen spot is within 80 blocks of a hovering
  dragon (new accessors Mobs_ShellActive/Mobs_ShellCenter and
  Dragon_GetAnchor).

## v65.41

- VOXEL RENDER FIXED: 4 of the 6 face directions were wound clockwise, so
  backface culling ate those faces - models looked like they had missing
  cube sides and came apart in slices. All quads are now wound CCW seen
  from outside; the dragon renders solid.
- The decoration layer is reduced to the dragon: character figures and the
  other sample models are removed; only models/vox/dragon.vox ships.
- WANDERING DRAGON: every 4-8 minutes (first hunt ~25 s after entering the
  world) a dragon appears over a random island 28-72 blocks from the
  player and circles it for 85 s, 12 blocks above the surface. It never
  appears near the pockets (220-block exclusion around both pocket
  centres).
- GILDED HOARD: on arrival, up to 14 natural surface blocks within +-6 of
  the anchor (base terrain ids 1-18 and turf 78; never plants, machines,
  gates or crafted blocks) turn into Gold Plate (24) through the normal
  client->server SetBlock path - co-op players see them and can mine them.
- Each gilded block carries a 10-MINUTE timer: not mined in time, it
  reverts to the exact original block; mined, it is kept. Pending
  reverts persist to dragonhoard.dat next to the executable and survive a
  restart; reverts only run while the area is loaded (within 128 blocks).
- A chat line on arrival explains the rule; new client module dragon.c/h,
  voxprobe now validates dragon.vox (PASS).

## v65.40

- SUB-BLOCK VOXEL DECORATIONS: new client modules voxparse.c (pure-C
  MagicaVoxel .vox reader: SIZE/XYZI/RGBA chunks, palette-less files get
  the embedded MagicaVoxel default palette) and voxdecor.c (bakes
  face-culled vertex-coloured meshes at ~1/12-block resolution). The world
  grid and collision stay 1 block - this is a decoration layer.
- 12 official MagicaVoxel sample models by ephtracy (models/vox/, shipped
  in the release zip, attribution in models/CREDITS.txt) are placed as
  landmarks on the starter island: six characters near the spawn, a fox
  and a slowly spinning T-Rex west, a deer, a teapot shrine east, two
  monuments, and a rotating dragon hovering 12 blocks above the ground.
- Scale is derived per model from its MEASURED content height (SIZE
  includes empty margin) against a target height in blocks: characters
  1.7 (the player is 1.5), cat/fox 1.3, T-Rex 2.4, deer 1.5, teapot 2.0,
  monuments 5-6, dragon 7 - nothing dwarf-sized or colossal. Models with
  empty margin underneath are normalized to stand ON the ground.
- Render rules: main world only (pockets keep their purity), within 200
  blocks, lazy mesh bakes at one model per frame, ground snapped by
  scanning down to the first solid block with periodic retries while
  chunks load.
- New host probe tools_dev/voxprobe.c validates every bundled model
  (magic, dims, voxel bounds, palette): PASS 12/12.

## v65.39

- WALL-KICK IS A NORMAL JUMP AGAIN, with full control: the vertical pop
  always fires; a held direction adds the sustained sideways throw ON TOP
  of the ordinary arc. Gravity, double jump, glide and air steering all
  stay available during the throw - no forced flight path, no pinned
  vertical speed (that was the v65.38 "sinks lower and lower" glide).
- Wall-chain momentum preserved: the wall-run attach speed cap is raised
  0.24 -> 0.45 blocks/frame - arriving on the opposite wall from a
  full-speed throw no longer cuts the run down to a jog, so kick chains
  keep (and build) speed instead of losing it.
- Throw sustain strengthened: horizontal feed 0.055 -> 0.07 per frame,
  equilibrium at the 0.42 clamp (~25 blocks/s) across the 0.45 s window.
- The wall-coyote throw now pops at double-jump strength (0.22) instead of
  a flat 0.05 glide.

## v65.38

- WALL-KICK WITH A HELD DIRECTION IS NOW STRICTLY SIDEWAYS: the vertical
  pop is removed entirely and the vertical speed stays pinned for the whole
  0.45 s throw window - under no circumstances can a directional wall jump
  go up or arc; the double jump is blocked while a throw is active. With no
  key held the classic vertical wall jump remains.
- Wall-coyote throw: pressing jump + direction within 0.3 s AFTER the
  wall-run ended (slid off / sank out) throws sideways too, instead of
  falling back to a vertical double jump.
- Stickier walls: attach reach 0.9 (was 0.75), min speed lowered to 0.05
  b/frame, and rising after a jump no longer blocks the grab (old gate
  rejected attach while vy > 0.10); re-grab cooldowns shortened
  (0.25 s after a drop, 0.15 s after a kick).
- FOUNDRY KILL PLANE: falling below the course (y < 110 inside the pocket
  zone) returns the runner to the START pad - the custom map start marker
  or the built-in course gate - instead of dropping out of the pocket into
  the ordinary world.
- MAP EDITOR: all dock sections are now separated - TOOLS / BLOCKS /
  MARKERS on the left, SIZES / VIEW / MAP LIBRARY / FILE NAME on the right,
  each with a header and a divider, laid out by one running cursor. The
  raygui side labels that used to draw over the neighbouring control
  ("Thick", "Grid Y", "Zoom") are gone - labels are hand-placed.

## v65.37

- MAP EDITOR field is now 384 x 128 x 384 cells (was 192 x 64 x 192) - long
  courses fit; grid draws to the new bounds, box clamps follow.
- MAP EDITOR view controls are now VISIBLE: "Fly" and "Zoom" sliders in the
  right dock; Ctrl+mouse-wheel changes FOV 20..90 (plain wheel = fly speed);
  the status line shows the current FOV. The camera starts over the field
  centre. (Anchor moved with the field - maps saved before v65.37 land
  shifted by (-96, -96) on the grid.)
- WALL-KICK actually throws now: after leaving a wall with a direction key
  held, horizontal push is RE-APPLIED every frame for 0.34 s (up to ~18
  blocks/s) instead of one decaying impulse that the air drag ate - you fly
  to the opposite wall instead of hopping. The direction indicator stays
  lit for that window.
- music/ FOLDER: extensions now match case-insensitively (Track.MP3 counts);
  the Foundry HUD label diagnoses the folder live: track name when playing,
  "Foundry (music/ folder empty)" when nothing was found, "(cannot play)"
  when a file failed to load. Pressing N inside the Foundry rescans music/
  immediately if the playlist is empty.
- The cosmic pocket zone radius grew to +-192 blocks to cover the bigger
  Foundry field.

## v65.36 - cosmic parkour sky, directional wall-kick, editor mouse fix

- **The parkour instance is cosmic now.** Zone 2 kept the warm concrete
  dusk sky, which read as empty and plain over custom maps. It now uses
  the same near-black indigo void as the main game, and the starfield
  with its nebulae plus the black-hole sun are drawn inside the zone
  (the screen-space gravity lensing stays zeroed inside pockets, ship
  traffic and overworld threats stay excluded - pocket purity rules
  intact). The meadow pocket keeps its calm blue sky.
- **Directional wall-kick.** Jumping off a wall-run while holding a
  movement key now throws the runner in that direction (across canyons
  and chimneys toward the opposite wall) instead of a vertical hop in
  place: the held WASD direction is added as a 0.30 impulse on top of
  the existing wall-normal kick, with the horizontal launch clamped to
  0.42 so chained kicks stay punchy but bounded. No key held - the
  classic straight kick off the wall is unchanged.
- **Map editor: horizontal mouse look was inverted** - moving the mouse
  right turned the view left. Sign corrected; strafe keys stay as fixed
  in v65.33 (the view/strafe geometry relationship is unchanged).

---


Floating islands drifting through a starlit void. The sun is a black hole wearing a gold ring.

## v65.35 - TEST loading hang fixed; your own mp3 playlist for parkour mode

- **TEST IN GAME no longer hangs the loading screen.** The on-join
  teleport fought the client-side spawn fill gate (the gate waits for
  the home-island spawn disc while the server had already moved the
  player into the pocket). The warp is now deferred: the loading
  screen finishes normally, and only at the switch to game does it
  hand a one-shot warp request to the server thread, which teleports
  the player onto the map's START marker. Entering through the main
  menu is untouched.
- **New: user music folder for parkour mode.** A `music/` folder is
  created next to game.exe (with a readme). Drop `.mp3`, `.ogg`,
  `.wav`, `.flac`, `.xm` or `.mod` files in - inside the parkour
  instance YOUR playlist plays instead of the built-in Foundry
  station (an empty folder keeps the built-in music). Tracks are
  sorted alphabetically, auto-advance at the end, and loop.
  - **N** - next track, **B** - previous track; the bottom-right
    station label shows the current file name.
  - The folder rescans every time you enter the parkour zone, so new
    files are picked up without a restart; an unreadable file is
    skipped with a "(cannot play)" label.
  - The music setting (on/off) and the volume slider apply to the
    user playlist as well; the stream keeps playing through pause
    and menu screens exactly like the built-in stations.
- Dev: warp request/consume split between client and server threads
  (shared parkourmap module), new client usermusic module, all probes
  re-run green (mapprobe both modes, pocketplay full loop,
  foundryprobe, modprobe/worldprobe, compile/api checks).

---


Floating islands drifting through a starlit void. The sun is a black hole wearing a gold ring.

## v65.34 - map saving fixed, TEST drops you into the instance, zone is ONLY your map, new main menu

- **Map saving rebuilt on native file I/O.** The previous raylib-based
  write/list path could silently fail on Windows (empty picker, empty
  `maps/` folder). Now: `fopen`/`opendir` directly, the folder is
  created if missing, every save is verified on disk, every failure
  records a reason, and the map list is sorted so the picker order is
  stable. The editor banner after SAVE now shows the FULL path of the
  written file (red banner with the reason on failure).
- **TEST IN GAME actually tests the map:** it arms a one-shot server
  warp, so on join the player is teleported straight onto the map's
  START marker - into the parkour instance, not the home island.
  Without a START marker the editor refuses to launch the test and says
  why. The toolbar button is separated from the file operations and
  right-aligned in its own accent style; toolbar gaps widened; the map
  name is clipped so it can never run under the button.
- **The parkour instance now contains ONLY your map.** The 190x190
  safety slab is gone: with a custom map active the zone holds your
  boxes plus small plazas under the gate/start/finish markers, and
  nothing else - no built-in course, no generic floor. Falling off the
  course resets you to your start pad with a message instead of
  bouncing on an invisible slab.
- **Main menu fully redesigned** (flat modern style, no bevelled
  panels): left column - title block with accent rule and primary
  actions (SINGLEPLAYER / PARKOUR MAP EDITOR / OPTIONS / QUIT); right
  column - sectioned flat panels for MULTIPLAYER (name/address/port/
  CONNECT), HOST (server.ini settings) and PARKOUR MODE MAP (`<` name
  `>` switcher + OPEN MAP EDITOR, long names clipped to the plate,
  `maps/` rescanned every second). All raygui controls now use the
  shared dark theme.
- World mod version 27.
- Dev: mapprobe extended (marker pads, bare-zone checks; both modes
  green), probe sandbox mods symlink auto-repaired, all probes and
  checks re-run green.

---


Floating islands drifting through a starlit void. The sun is a black hole wearing a gold ring.

## v65.33 - map editor fixes: strafe keys, layout audit, map picker, TEST IN GAME

- **Editor camera: A/D were mirrored** relative to the look direction -
  the strafe basis vector sign is corrected.
- **Editor layout audit:** tool buttons respaced (rows now have a 4px
  gap, columns clear the dock edges); the palette list got comfortable
  24px rows with the colour swatches aligned per row and moved out of
  the text column (all six blocks fit without a scrollbar); the MAP
  LIBRARY group was resized so LOAD/DELETE/SAVE no longer spill past
  its border; name box, caption and EXIT respaced below it; the toolbar
  (NEW / UNDO / REDO / GRID / TEST IN GAME / map name) keeps clear
  gaps and the name never reaches the right dock; the menu map-name
  plate clips long names instead of overflowing.
- **Map picker reworked:** the combobox is replaced by an explicit
  `[ < ]  MAP NAME  [ > ]` switcher on the main menu - the current
  choice is always visible, one click per map, the list rescans once a
  second so freshly saved maps appear immediately, and the selection
  persists to settings.ini.
- **Visible save feedback:** SAVE now raises a large centred
  `SAVED maps/<name>.pmap` banner for 3 seconds (plus the status-bar
  line); a failed save reports itself instead of staying silent.
- **New: TEST IN GAME button** in the editor toolbar - saves the map,
  marks it as the active parkour map in settings, and launches
  singleplayer straight into the game with exactly the geometry that
  is on screen.

---


Floating islands drifting through a starlit void. The sun is a black hole wearing a gold ring.

## v65.32 - parkour map editor, custom maps, and a camera-bank fix

- **Wall-run camera bank flipped.** The roll now leans INTO the wall the
  runner is on (it leaned away before) - `rollTarget` sign corrected in
  the client camera code.
- **New: parkour map editor** (`Map Editor` button on the main menu).
  A standalone workbench screen built on raygui with its own dark
  professional UI - not the in-game HUD style: top toolbar
  (NEW / UNDO / REDO / GRID), left dock (8 tools + 6-block palette +
  marker status), right dock (properties + map library), bottom status
  bar with live cursor cell and tool hints. Free-fly camera
  (RMB look, WASD/QE, Shift boost, wheel = speed).
- **Build in bulk, not block by block:** BOX CHAIN fills a whole region
  between two clicked corners at a set height; WALL draws a wall of any
  length between two clicks with Height/Thickness properties; WALL 45
  makes stepped diagonal walls; PLACE/ERASE handle single blocks;
  GATE/START/FINISH tools set the course markers. Undo/redo stack (24
  steps), Ctrl+Z / Ctrl+Y.
- **Maps live in a `maps/` folder** next to the game executable
  (`maps/<name>.pmap`, documented text format: `box x y z hx hy hz id`
  and `gate|start|finish x y z`). Save / Load / Delete from the editor's
  map library.
- **Map picker on the main menu:** `PARKOUR MODE MAP` combobox lists
  `Default Foundry` + every saved map, persists to `settings.ini`
  (`parkourmap=` key), and is applied when Singleplayer/host starts.
  The list rescans live, so maps saved in the editor appear at once.
- **Server:** a new shared `parkourmap` module (list/load/save/delete +
  session-active map). When a map is active, the chunk generator stamps
  its boxes into the second pocket zone (anchor -1296,118,1104,
  192x64x192 local space) and the Lua course switches to a plain slab;
  markers drive the gate block, spawn teleport and the run timer via
  new `wg.parkour_map_active()` / `wg.parkour_map_marker(name)`
  bindings. The active map name is part of the world fingerprint, so
  switching maps regenerates the world - no stale geometry.
- World mod version 26.
- Dev: new `mapprobe` (default-course and custom-map modes, both green),
  probe stubs for the map file surface, probes re-run (foundryprobe,
  pocketplay full loop, modprobe/worldprobe, compile/api checks).

---


Floating islands drifting through a starlit void. The sun is a black hole wearing a gold ring.

## v65.31 - the Foundry is a wall canyon now, and the invisible wall is gone

Playtest verdict on v65.30: platforms must not be the main verb - WALLS
must (the studied references all put the route ON the walls), the level
must be longer and more varied, high walls must close the space around
the route, and the path must branch. Rebuilt accordingly:

- **A long S-shaped canyon.** Start court, then a wall-run gallery: two
  continuous 68-long canyon walls with floor pits between rest islands -
  the pits are crossed ON the walls (run, or kick between faces), the
  islands are just rests. Then the route BRANCHES: the high line runs
  along the crest of a divider wall between two corridors, the low line
  wall-kicks up and over a chimney stub pair; they rejoin at a junction.
  Leg two is a 3-wide chimney slot between inner walls - a wall-kick
  staircase north. Leg three closes with wall-to-wall kicks across
  offset pits, and a final wall-kick chimney lifts you onto the finish
  plateau and its beacon. Crests, islands and the finish carry light
  strips as the only signposting. No rims, no collision traps anywhere:
  a fall costs clock, nothing else.
- **The "invisible wall before the finish" was the zone barrier.** Its
  63-block radius cylinder sliced the course at x+63 - right before the
  precision section. The second zone now uses a BOX safety net at the
  zone boundary (it only ever catches you at the boundary or under the
  floor); the meadow keeps its circle. The barrier constant is gone.
- Worldgen version 25 - old worlds regenerate on first launch.
- Dev probes updated to the canyon: foundryprobe validates 20 course
  coordinates plus the region leak scan; pocketplay runs the full loop
  headless - all green.


## v65.30 - the parkour instance, rebuilt on parkour-game rules

The second instance was rebuilt after studying how FPS parkour games build
levels (KZ/climb maps, Mirror's Edge flow chains, Titanfall's wall-run
levels, Red Eclipse, VHOLUME): a compact arena with visible bounds instead
of a horizon-wide slab; one readable chain of obstacles with rest beats
between them; every mass carried on a support tower (floating pads read as
mistakes); light strips as the only signposting; one shortcut monolith that
pays for route knowledge. Spacings follow the game's own jump physics
(flow gaps 4 with +1 rises, precision gaps 4 onto 3x3 pads).

- **Wall-run feel, fixed at the root.** Contact now gives an instant
  Titanfall-style boost (~1.45x sprint, clamped) along the wall, and the
  run holds a pinned speed curve: flat and fast for the first 55% of the
  run (vertical velocity decays to zero - you run FORWARD), then gravity
  ramps in quadratically while speed eases to 75% - the run closes with a
  downward arc, exactly the shape asked for in playtest. The global
  horizontal damping no longer applies mid-run (it was eating the speed:
  "runs slow along the wall"), and a wall kick carries the run speed plus
  a 12% jump-off boost.
- **The instance is decor-free now.** Overworld scatter structures
  (crystal basins, spires, dream gates, chrome totems, shrines, cocoons,
  memory floats) had no zone mask at all: the meadow only escaped them
  because its floor sits above their placement ceiling - the lower
  Foundry floor did not, and they littered the plaza (the clutter in the
  last screenshot). Structures accept `avoid_boxes` now, and every decor
  structure keeps out of both pocket zones.
- **Wireframe rocks no longer render inside the instance**: the asteroid
  belt's DRAW gate tested zone 1 only, so stale overworld asteroids kept
  drawing in the second pocket (their update was already gated).
- **The old course was half-erased by its own zone mask**: the pocket zone
  box is +-96 blocks, and the v65.29 finish plateau sat at +118 - outside
  the mask, so the material select silently returned void there. The
  course is re-centred to live inside its zone.
- Worldgen field budget 1024 -> 2048 nodes (the arena's rim, towers and
  lamp signage needed it). Worldgen version 24 - old worlds regenerate.
- Dev probes: `foundryprobe` validates all 22 course coordinates plus a
  full-region leak scan; `pocketplay` runs the whole loop headless
  (fuse -> cross -> spawn -> chunk leak scan -> return) - all green.


## v65.29 - the second instance fixed: it was being clipped out of the world

All three reported bugs, diagnosed with headless probes against the real
server stack (no guessing this time):

- **The instance looked empty/broken.** World generation is bounded at
  y=160, and the v65.28 course was laid out on a floor at y=154 - so its
  upper half (walls, chimney, finish) sat above the bound and the engine
  simply clipped it out of existence. The whole zone is lowered 36 blocks
  (floor now 118, highest point 151, safely under 160) and the pocket sky
  height follows. Verified cell-by-cell: `foundryprobe` (new dev tool)
  checks the frozen material field at 18 course coordinates - all pass.
- **White/invisible blocks ("like last time").** The outdated-atlas guard
  healed tiles 78-80 but the v65.28 blocks are 81-83, so an old
  `terrain.png` left the new cubes textureless. The guard now covers
  78-83 and repaints concrete, base and lamp tiles itself.
- **Wall run "pulls you down".** Two physics fixes: (1) touching the wall
  head-on used to leave you stuck in place while the sink dragged you
  down - contact now redirects your velocity ALONG the wall (+8%, min
  0.16), so the grab launches the run instead of stalling it; (2) the sink
  clamp was 0.045/frame (~2.7 blocks/s), now 0.012 (~0.7 blocks/s) - a
  whisper. The wall probe is slightly longer (0.68 -> 0.75) and the
  vertical attach window wider (0.06 -> 0.10), so the grab is easier to
  hold at speed.
- **Gate messages no longer ambiguous.** Fusing a square inside the first
  pocket now announces itself as a gate to the second instance, distinct
  from the overworld crossing.
- **Barrier + rescue net are per-zone now** (the two instances have
  different floor heights). Worldgen version 23.
- Dev tooling: `pocketplay` harness runs the full loop headless - build a
  square inside the first pocket, fuse, cross, spawn-check the second
  instance, return through its centre gate - all green.

## v65.28 - wall running, and a second pocket instance

- **Wall run + wall kick.** Airborne, moving along a wall, it sticks:
  near-zero gravity with a floaty sink, the camera banks into the
  surface, and Space kicks off it - refreshing the double jump and
  dash charges so kick chains build speed instead of spending it.
  The grab never triggers while rising hard, so ordinary jumps past
  walls do not snag. Runs hold up to 2.8 s; abuses are left to level
  design, not timers.
- **The second pocket instance.** The cores the Warden leaves behind
  complete a second crossing: a brutalist parkour course adrift in
  the void - a plaza, a rising platform run with deliberately
  irregular spacings, a wall-run traverse, a wall-kick chimney,
  precision hops and a finish plateau with a beacon. The route is
  signposted by light strips only; two monoliths double as climbable
  shortcuts. Crossing the start pad arms a course clock; the finish
  plateau stops it and keeps a per-player personal best (server Lua).
  Falling off lands you on the plaza - the clock is the only price.
- **Plumbing.** pocketfx generalised to two zones (per-zone chunk
  veil, factor-any gates for the cosmos/mobs/postfx, distinct sky
  palettes), music station 8 claimed inside the second pocket, the
  barrier + under-floor rescue hooks cover both zones, gate routing
  distinguishes the meadow's centre gate from gates built inside it.
- **New blocks/tiles:** Concrete (81), Concrete Base (82), Path Lamp
  (83, calm light 6). The outdated-atlas guard now covers tiles
  78-83. Worldgen version 22.

## v65.27 - depth precision at range, and flora follows its support

- **Depth precision.** The projection near plane was 0.01, so at
  100-200 blocks the 24-bit depth buffer resolved coarser (0.06-0.24
  blocks) than any geometry clearance - nested meshes shimmered at
  range even after the margins were fixed. The near plane is now 0.1
  (10x finer depth at distance), patched into the pinned raylib
  config at build time. Close-range rendering is unchanged; nothing
  in the game draws closer than 0.1 to the eye.
- **Flora never dangles.** Removing a block now also removes the
  plant standing on it (cascade included), with a leaf burst. The
  same rule runs on the client and on the server, so the authoritative
  world and every peer stay in sync without extra packets.

## v65.26 - the Warden stops flickering, and grass no longer blocks building

- **Z-fighting exorcised.** The golem's segments were nested with
  faces in the SAME plane - the knee cap's sides exactly matched the
  shin's, the heel exactly matched the shin's back, the elbow cap
  left only 0.02 against the forearm, and the head's bottom lay
  exactly on the torso's top. Coplanar faces fight for the depth
  buffer and paint a striped diagonal moire that crawls as he moves.
  Every joint now keeps a clear margin: knee cap 0.70 (thigh 0.6,
  shin 0.5), heel moved off the shin plane, elbow cap 0.54 (upper
  arm 0.45, forearm 0.36), head bites 0.12 into the chest. Same
  silhouette, steady stone.
- **Build through grass.** The ray used to land on the grass tuft
  and the block was placed ON TOP of it (or rejected) - you had to
  clear the lawn by hand first. Now a placement aimed at grass
  breaks the tuft (with a leafy burst) and takes its cell.

## v65.25 - the real under-the-floor culprit, and the pocket gets its dream back

Playtest verdict on v65.24: both fixes missed the mark. Corrected:

- **The yank is gone.** The reported bug had nothing to do with
  blasts: the invisible pocket barrier (v65.19) tested only HEIGHT
  against the whole world - anyone above y=150, anywhere, was
  teleported to 63 blocks from the pocket centre. Jump on the
  tallest island (its top clears 150) and mid-jump you were yanked
  into the pocket zone AT YOUR JUMP HEIGHT - i.e. inside the lawn
  slab itself - and the physics squeezed you out under the floor.
  The barrier and the rescue net now only ever look at players
  physically inside the pocket zone footprint; islands are yours
  again, jump as high as you like.
- **The rescue net actually works now.** In v65.24 it was nested
  inside the "above the floor" branch, so the "below the floor"
  condition could never fire - dead code. It now lives outside it:
  end up under the meadow however it happened, and it catches you
  ("The meadow catches you.").
- **The battle music was never heard because the POCKET was already
  playing it.** The pocket station was derived as "the last track"
  (MUS_NTRACKS-1); adding the battle track moved the pocket onto
  station 7 - dreamcore was silently replaced, and the Warden's
  awakening changed nothing. Stations are now explicit ids: pocket
  = 6 "Pocket of Clouds", battle = 7 "The Warden Wakes", claimed
  only while the golem fights and handed back when he falls.
- Kept from v65.24 as extra insurance: the pocket's bottom shell
  is blast-proof (a crater can no longer be punched through the
  thin slab either).

## v65.24 - battle music for the Warden, and the floor holds

- **The Warden Wakes.** When the golem opens its eyes the radio
  hands over to an eighth station: a dungeon-synth battle march in
  the same generative family - fast Dm-Bb-F-C bars, pulsing bass,
  string pad, choir lead, a war bell every two bars. It holds for
  the whole fight and hands back to the pocket dream when he falls
  (the overworld radio never touches stations 6-7).
- **The floor holds.** A blast column could punch clean through the
  thin pocket slab - a crater straight through the bottom - and a
  jump into that crater put you under the floor in the void (the
  reported bug, screenshot confirmed). Two fixes: the pocket's
  bottom shell (lawn-3 and below) is now blast-proof, and a
  server-side rescue net teleports anyone who ends up under the
  pocket floor back on top ("The meadow catches you."), for LAN
  guests too.

## v65.23 - the Warden learns to fear the laser

Second playtest, second pass:

- **Shoulders out of the torso.** Arm pivots moved from +-1.6 to
  +-1.95 (the torso wall is at 1.3, so the upper arm and shoulder
  cap used to live INSIDE the chest), and the arm ends slimmed
  down (forearm 0.44 -> 0.36, hand 0.6 -> 0.48): lowered arms no
  longer sweep through the body.
- **The laser rifle now hits him.** The beam chain in player.c
  (hunter -> mobs -> cocoons) never asked the Warden, so shots
  passed clean through. Golem_LaserHit joins the chain: sparks at
  the beam stop, white flash, recoil flinch, clang/core sounds
  rate-limited for the laser's tick rate.
- **A detonation you can see.** The v65.22 death burst was a fistful
  of tiny sparks - invisible. Now: 40+ textured gold core chunks fly
  (block-break debris), two shockwave bursts, the boom, the cores
  condense in the blast, and the husk sinks into the meadow
  afterwards (death 2.0 -> 2.6 s to let it all read).

## v65.22 - the Warden polish pass, from playtest footage

Every note from the first boss fight landed:

- **A real kneel.** Shins folded back, knees in the grass, hands
  forward - the v65.21 pose read as a squat. Standing heels now sit
  exactly on the turf (pelvis 5.2 -> 5.6) instead of sinking half a
  block.
- **The blue/pale glitch quads are gone.** The glow boost multiplied
  colour past 255 and the unsigned char WRAPPED (255*1.25 -> 62),
  which is exactly what turned hands and chest into blue-yellow
  confetti. All channels clamp now.
- **Arms lower gently** after the volley on an eased blend instead
  of snapping down.
- **Hit feedback.** The struck part flashes white and recoils
  (arm jerks back, torso staggers), sparks fly at the exact hit
  point, and hit volumes are fatter (capsule 1.1 -> 1.35, torso
  +0.25). Shoulder and knee caps keep the joints solid mid-stride.
- **Sound.** Stone footsteps per stride, a zap per volley orb, a
  metallic clang per arm hit, a deep boom on core hits, a crack-tear
  when a limb goes, and a full explosion at the end.
- **The death blow.** The Warden kneels and DETONATES; the four warp
  cores condense in that blast, not after you walk over the empty
  spot (death also shortened 2.6 -> 2.0 s).

## v65.21 - THE WARDEN OF THE MEADOW: a cube golem boss

The pocket had a peristyle but no guardian. Now it has one: a giant
cube golem, client-side like all combat in this game (hunters and
mobs always were), with a small forward-kinematics rig of cuboid
parts and hand-keyed poses - our own public-domain "animation
asset", because the cube is this world's native format.

- **The script.** It kneels dormant in the north-east of the meadow.
  Come within 26 blocks and it opens its eyes and RISES (2.8 s).
  Then it walks you down, stops, aims, and volleys three hot orbs
  from its hands (they hurt, they break on blocks), cools down,
  repeats. It turns to face you and its walk cycle swings legs and
  arms.
- **The fight.** Blade swings (6-block reach) and blasts tear the
  ARMS off first - 60 hp each, stump left behind, chat line per arm.
  Only with both arms gone does the chest plate open and the torso
  (160 hp) become vulnerable. A boss bar shows the arms, then the
  core.
- **The reward.** On death it kneels again and crumbles; approach
  the wreckage and four warp cores condense in the grass (2x2 worth
  - exactly a new gate square). The second pocket dimension those
  cores open is v65.22.
- Drawn through the proven immediate-batch path (culling off, flush,
  restore); frozen while you are outside the pocket.

## v65.20 - culling off at FLUSH time, barrier widened

v65.19 disabled backface culling around the colonnade batch - and it
changed nothing for the arch legs: rlgl's batch is deferred, rlEnd
only closes the CPU side, and the GPU draw happens at the next flush.
The flush of the stream tail landed AFTER culling was re-enabled, so
the last arches in the build order drew single-sided and their legs
went transparent at grazing angles (exactly what the close-up
screenshot showed).

- The colonnade now forces rlDrawRenderBatchActive() while culling is
  still off, then restores it - every marble vertex draws
  double-sided, deterministically.
- Invisible barrier widened from radius 60 to 63: you can walk right
  up to the columns and stand in the colonnade, but not past it.

## v65.19 - solid marble and an invisible fence

Close up, the arches read as inverted sails: rlgl culls backfaces by
default and the merged builder's winding is not uniform, so front
faces vanished at grazing angles. And nothing but marble style
stopped a walker from stepping between the columns into the void.

- **Double-sided marble.** Backface culling is switched off for the
  colonnade draw only (the baked shading was two-sided from the
  start), so every face renders from every angle - no more holes or
  inverted caps up close.
- **The invisible barrier.** A server-side cylinder of radius 60,
  just inside the colonnade, clamps every player position each tick
  from the lawn up to +200 blocks (fly mode included). Enforced in
  the mod's step hook, so it holds for LAN guests too. The warp gate
  at the centre is far inside the ring - crossing is unaffected.

## v65.18 - the peristyle leaves the DrawMesh path entirely

v65.17's batch flush did not stop the crossing crash, so the whole
VAO/custom-shader/DrawMesh path is gone - it was the only new GL
surface in v65.16 and it is the only thing the crash could touch.

- The colonnade now draws through rlgl's IMMEDIATE batch, exactly
  like the terrain has since v1: CPU-side positions with two-sided
  lambert baked into per-vertex colours, pushed with
  rlColor4ub/rlVertex3f inside rlBegin/rlEnd. No UploadMesh, no VAO,
  no shader object, no DrawMesh - nothing left to fault.
- Same geometry, same marble look (the shading is baked per vertex
  at build time instead of in the fragment shader).

## v65.17 - the crossing crash: batch flush around the peristyle

v65.16 crashed the client at the moment of crossing into the pocket.
The peristyle was the only new draw on that path, and it drew through
DrawMesh - a VAO+shader switch - while the world's chunk vertices were
still pending in rlgl's deferred render batch (world.c flushes that
batch explicitly around its own special draws; the colonnade did not).
Pending vertices meeting a foreign VAO is driver-side state
corruption - a segfault on the first frame inside the pocket.

- The colonnade now flushes the render batch before AND after its
  DrawMesh call.
- Client log breadcrumbs: COLONNADE init line (vert/tri/vao/shader
  counts) and a first-draw line, so any future crash on this path is
  attributable from midless_client.log.

## v65.16 - the cloud dream is over: blue sky and a marble peristyle

Walking the meadow settled it: cloud billboards cannot work here -
the camera pokes the sprites (screen glitches up close) and the
quads flash through at grazing angles. The dreamcore attempt is
retired with respect; the pocket keeps its calm in architecture
instead:

- **Blue sky is back.** The v65.12 soft blue (124,178,214) returns;
  the gradient and every cloud billboard are gone.
- **The peristyle.** A ring of 64 round marble columns (plinth,
  shaft, echinus, abacus) at radius 62 carries 64 ROUND arches -
  true semicircular arcs, smooth mesh geometry, not voxel steps and
  not square lintels - over a low stylobate ring hugging the lawn
  edge. One merged mesh, one draw call, its own tiny two-sided
  marble shader lit by the pocket sun.
- The perimeter now reads as a built horizon: colonnade against the
  blue, the void edge framed by marble instead of hidden by sprites.
- Music, purity gates and the black-hole lensing fix all stay.

## v65.15 - the dream gets a SKY: gradient, cloud collar, no panels

Screenshots from the rim showed why it read as "ужасно": a flat mauve
bedsheet of a sky, and the near-opaque rim wall up close was a blinding
white panel with hard edges - from fly mode the same wall lay across
the lawn in streaks (it stood INSIDE the platform footprint).

- **Gradient sky.** The pocket sky is no longer one flat colour: a
  screen-space gradient paints the reference top to bottom -
  periwinkle (150,156,208) melting through pink (214,178,204) into a
  peach horizon glow (246,204,190) and cream - blended by the pocket
  factor under the 3D world.
- **Cloud collar, not a wall.** The rim ring moved OUTSIDE the
  platform (radius 69-95 vs the lawn's 64) and dropped below the turf
  line: from above it is a collar hugging the island, from eye level
  its tops peek over the rim and cover the cut line - no streaks on
  the lawn, no panels in your face.
- **Softer puffs.** The cloud texture lost its boosted alpha core
  (smoothstep falloff), alphas came down (collar 205, sea 175,
  drifters 110) and the sea sank deeper (y ~108) and farther out
  (radius 95-245): clouds read as clouds, not glowing paper.
- Crossing flavour line now says "a meadow adrift in a sea of pastel
  clouds".

## v65.14 - the dream gets contrast: real cloud sea, no rim, no lensing

v65.13's pocket read as "white sky, white smudges": the pale wash
swallowed the pastel clouds, and the cloud ring floated far outside the
lawn edge, leaving the platform rim fully visible. Rebuilt:

- **Deeper sky.** The pocket sky is now the saturated upper sky of the
  references - a dusty mauve-periwinkle (168,132,178) - so the pale
  pink/lavender/peach clouds finally have something to stand against.
- **The rim wall.** 34 near-opaque cloud billboards now straddle the
  lawn edge itself (radius 62 vs the platform's 64), rooted in the
  turf by the depth test: the platform's cut edge dissolves into
  cloud instead of ending in a hard line over the void.
- **The under-sea.** A second cloud layer drifts BELOW the platform
  (y 122-138): look over the edge and there is no void drop - the
  island floats in a pastel sea, the dreamcore staple.
- **Denser puffs.** The cloud texture core went from a faint falloff
  to a packed cumulus (alpha 245 at the wall), so they read as
  clouds, not haze.
- **No black-hole lensing in the pocket.** The postfx distortion was
  still warping the screen after the crossing; bhStrength now fades
  out with the pocket factor. (The hole itself was already hidden.)

## v65.13 - the pocket dreams: dreamcore sky, cloud sea, its own music

The meadow is no longer "the overworld, quieted down" - it is somewhere
else entirely, and nothing from home follows you in:

- **Purity.** Glowmoths, wireframe asteroids, the VOID TIDE and its
  crawler/wisp waves are all client-side systems, and every one of them
  now checks the pocket factor: inside the pocket they neither update
  nor draw. No hunt can start there, no threat HUD glows there.
- **Dreamcore.** Reference-hunting paid off: the pocket sky is now a
  dusty pink-lavender (sampled from liminal-space photography), and a
  slow sea of pastel clouds - pink, lavender, peach - rings the meadow
  at lawn level, swallowing its rim. A few pale drifters ride higher.
  The platform dissolves into cloud: an empty field floating in a soft
  nowhere, exactly the liminal feeling.
- **Its own station.** The generative radio learned a seventh track,
  "Pocket of Clouds": a slow F-Am-C-G drift, an airy breath pad, a
  music-box lullaby and a far bell under a dark filter. Crossing the
  gate swaps to it automatically; crossing home restores whatever you
  had. The overworld radio never plays it, and auto-switch never
  sweeps it away.

## v65.12 - the pocket becomes a POCKET: veiled, calm, gate-only

Screenshots from a real run showed the meadow reading as "a platform in
the same world": the cosmic islands were visible from inside the pocket,
and the whole scene sat under a paper-white, bloom-blown sky on acid
grass. Fixed on all three fronts:

- **The veil.** Chunk rendering is now side-aware: inside the pocket the
  cosmos is not drawn at all; outside it the meadow platform is not
  drawn. No islands on the pocket horizon, no platform speck from the
  islands - the warp gate is the only door in either direction (wire
  auras and flora respect the veil too).
- **The glare is gone.** Pocket daylight dropped from 1.0 to 0.82, the
  sky deepened from paper-white to a soft blue, bloom inside the pocket
  cut to a third, and the meadow turf repainted from neon to a calm
  emerald with muted gold flecks (atlas heal placeholders match).

## v65.11 - the client heals an outdated terrain atlas by itself

The client log from a real run showed the truth: definitions for blocks
78/79/80 were REJECTED because the terrain.png on disk predated the
pocket universe - every pocket block (the gate included) meshed as
nothing, while the teleport itself worked fine and switched the sky to
the pocket palette (the "white world, islands remain" sight).

- **Atlas self-heal**: at startup the client probes tiles 78-80 of
  terrain.png; blank tiles get synthesized placeholder pixels patched
  straight into the GPU atlas, so the gate and the meadow render even
  from an old install. The real art arrives with a proper zip extract
  (the red banner keeps saying so).
- Rejected block definitions now log the offending tile; the atlas
  dimensions on disk are logged at startup.

## v65.10 - diagnostics release: the client finally leaves a paper trail

The pocket-gate reports ("invisible cube", "white sky") only reproduce on
a real Windows box, so this build instruments the client:

- **midless_client.log** is written next to the executable on every run:
  block-definition rejections (with the block id), pocket-atmosphere
  transitions (with the player position), world/chunk warnings - exactly
  what remote debugging needs.
- **Outdated-atlas banner**: if textures/terrain.png predates the pocket
  universe (tiles 78-80 blank - the classic "invisible blocks" cause when
  a zip is unpacked over an old folder without overwrite), a red banner
  says so on screen instead of leaving you with ghost blocks.
- pocketfx guards its smoothing against NaN frame times and logs every
  crossing into / out of the pocket mood.

## v65.9 - the warp gate actually carries you (pocket-universe hotfix)

- **Teleport fixed at the root.** The step-on-gate hook probed the block
  under your feet with a FRACTIONAL coordinate; the get_block binding
  coerces coordinates to integers and errored out on every tick, so the
  hook died silently and nothing ever happened. The hook now floors the
  probe itself. Verified end to end by a new headless harness
  (tools_dev/pocketplay_harness.c) that drives the real server stack
  with a synthetic player: four cores fuse, standing on the gate lands
  you at the pocket spawn, the centre gate brings you back beside your
  home gate.
- **The gate rises where you finish the square.** It used to appear at
  the low corner of the 2x2, which read as "the cores vanished into an
  invisible cube". Now the cell you placed fourth becomes the gate -
  finish the square while standing on it and you cross over immediately.
- **Mod block glow travels over the wire.** The define-block packet never
  carried lightLevel, so the client ran every emissive mod block at the
  legacy full blast. The packet grew by one byte (83) and the client
  reads the graded level now.

## v65.8 - the pocket universe: four cores, one gate, another world

- **Build the gate.** Mine Warp Core blocks (they cap the obelisks, the
  crystal spires and the ruined shrines) and set FOUR of them in a 2x2
  square. They fuse into a single Warp Gate - the server announces it in
  chat the moment the square closes.
- **Step onto it.** You cross into the pocket universe: a wide flat
  129x129 meadow of emerald turf over warm loam, floating far from the
  cosmic archipelago under a bright DAY sky. The whole mood swaps with
  it - no nebulae, no black-hole sun, no Void Runner traffic; the light
  is full daylight and the colour grade turns warm and clean (sky,
  ambient, postfx vignette and grain all follow you in).
- **The pocket keeps its own rules.** Pure lawn by design: no cosmic
  flora, no ores, no structures intrude (the meadow sits at y=154, above
  everything worldgen decorates). A return Warp Gate stands dead centre
  and drops you back beside the gate you came from. Friends in
  multiplayer cross the same gates - positions are server-authoritative.
- **A stage for what's next.** The pocket is an isolated, empty arena on
  purpose: bosses and activities will move in in later releases.
- One-time world regeneration on this update (the worldgen fingerprint
  changed with the new fields).

## v65.7 - multiplayer you can hand to a friend: server config, host panel, address on screen

## v65.7 - multiplayer you can hand to a friend: server config, host panel, address on screen

- **The server answers "is it okay?" with yes, and now says so.** Audited and live-tested headless:
  it binds 0.0.0.0:25565 (ENet/UDP), survives junk packets, loads the cosmic mod, and logs a
  readable startup: config line, mod load, and "Listening on 0.0.0.0:port as 'name' (max N
  players)". The logger also stopped printf-ing arbitrary strings (a server name from server.ini
  could have been a format string) and gained line endings.
- **server.ini - one config for both servers.** Written as a commented template on first run next
  to the executable (game folder or server.exe folder): port, max_players (1..64), name. The
  dedicated server.exe AND the in-game host read the same file, so editing it once works either
  way.
- **HOST SETTINGS on the login screen**: server name / port / max players boxes; pressing
  Singleplayer saves them to server.ini and starts the hosted server with exactly those values.
  Before this, creating a server showed nothing and tuned nothing.
- **The address lives on screen while hosting**: a HUD line under the shard counter
  (HOST ip:port players N/M) plus the F6 host panel - server name, the big LAN address to send to
  friends, the port-forwarding note for internet play, player count, and the server.ini hint.
  A chat line at host start repeats the address. The LAN IP is detected locally (a UDP route
  probe - nothing is sent anywhere).
- **Player counting exists now**: the server tracks connected peers (the host panel shows
  remote players + you).

## v65.6 - solid plants, six quick slots, no G key, named items, and the purple mushrooms are back

## v65.6 - solid plants, six quick slots, no G key, named items, and the purple mushrooms are back

- **Plants no longer see through.** The flora sprite batch ran with the depth mask off (a v59.6
  over-correction), so trees, flowers and mushrooms wrote no depth at all: everything drawn later
  painted straight through them and overlaps depended on draw order ("the world shows through the
  plants, it glitches"). The batch now writes depth; the alpha-cutout shader still discards
  transparent corners, so quads occlude like solid geometry and sprite-vs-sprite overlap is
  depth-correct.
- **Six quick slots** (was four), and the old junk is gone: pins are the real species tiles now
  (glowcap / trumpet / puffball art in the slot, per-species counts), the legacy generic mushroom
  icon is accepted only as an alias, and discovery order fills one slot per carried species.
- **The G key is deleted.** Eating a mushroom lives only on the quick slots (pin one, press its
  number) - the hotbar already had use-bindings, the extra key was legacy debris. Hints and the
  satchel footer say so.
- **The satchel names what you point at**: hover any occupied cell and the item name appears under
  the grid (localised: Void Glowcaps / Cinder Trumpet / Frost Puffball / Gaze Scroll).
- **Mushrooms wear their own violet family - no biome colours, no world colours.** The glowcap
  cluster was teal (both the "disgusting green" and the world's crystal colour); the trumpet wore
  ember amber; the puffball wore frost magenta. Now: glowcaps are bright violet with lavender
  spots (the original purple look, back on dirt blocks), the cinder trumpet is a violet funnel with
  a hot-pink rim, the puffball a deep-violet dome with pale warts. Contrast gate still passes
  (dL 63-88 against their grounds), and species still sprout only on their own biome ground.

## v65.5 - the mushroom scheme, an honest satchel, a currency section, bigger type, dimmer lights

- **Mushrooms follow the scheme and nothing else.** A rain cloud drifts in, its rain grows the
  biome's own mushrooms, they live ten minutes - and now they end visibly, withering over the last
  12 seconds (the cap shrinks into the lawn) instead of popping out of existence. Worldgen plants
  NO mushrooms at all anymore: the old p99+ lawn band is gone, and modprobe now FAILS the build if
  worldgen ever plants one again. Rain is the only source, exactly as intended.
- **The satchel shows only what you actually carry.** The grid used to pre-display shard, mushroom
  and scroll icons with zero counts - now a row appears only once you hold one. Collected mushroom
  species keep SEPARATE stacks (glowcaps / cinder trumpets / puffballs, own icon and count each -
  nothing folds into one generic mushroom), and the grid grew from 8 slots to 12.
- **Shards are currency, not luggage.** They left the item grid for a dedicated CURRENCY section at
  the top of the satchel (the HUD counter stays where it was).
- **UI type is 25% larger everywhere** - one scale in the I18n layer, so every panel, HUD line and
  hint grows together and centered text stays centered. The satchel panel is taller, with a
  roomier footer.
- **Every emitter dimmed**: warp core 15 -> 8, fire 15 -> 10, lava 15 -> 9. Full-strength emission
  was bleaching the launch pad and whatever stood near it.
- Progress saves keep the per-species mushroom stacks (`shrooms73/74/75` keys; the legacy single
  key still restores old saves).

## v65.4 - emission reverted: the v65.2 look returns, light_level stays dormant

- **The v65.3 glow pass is reverted by player verdict.** From afar the bioluminescent meadows read
  fine, but up close the flood light from glowing flora added onto the sunlit turf and overexposed
  it - green dirt grass looked bleached, and the pale frost ground clipped straight to white
  ("the ice cuts the eyes"). No block except the warp core, fire and lava emits light again -
  exactly the v65.2 picture.
- **The mechanism stays, dormant.** The graded `light_level` (1..15) in the block definition, the
  flood fill that honours it and the mod field all remain in place, compile-checked and
  probe-verified, with zero blocks using them: a future hazard-glow pass can bring selective glows
  back at tuned strengths without rebuilding the plumbing.
- **The F4 work view and the F3 flora-work HUD line stay** - debug-only, off by default, they never
  touched the picture.

## v65.3 - Grimorium ideas landed: the flora work view (F4) and graded emission

- **Flora work view (F4)** - a debug pass in the spirit of Grimorium's "colour every pixel by the
  work the ray did there". Every flora cell wears a tinted ground patch by how loaded its chunk's
  flora list is against the 1024 cap (green < 256, yellow < 512, orange < 768, red above), and the
  F3 HUD gains a line: plants drawn, billboard quads submitted this frame (a bloom costs its own
  quad plus its grass skirt), and the heaviest chunk in view. The whole bug class v65 fixed - a
  silent cap eating half the lawn - now shows up on screen at a glance instead of in a probe log.
- **Graded emission (`light_level`)** - emission used to be binary: EMIT flooded at full 15, so
  nothing could glow softly. Blocks now carry a 1..15 strength and the flood fill honours it.
  Applied by Grimorium's readability rule ("hazards and living light announce themselves"):
  volatile barrels glow dull orange (7) so nobody steps on one in the dark; mushrooms glow soft
  bioluminescence (6); biome blooms and lanternberries (4); launch pad sigils (4); cosmic crystal
  hums at (3). Warp cores, fire and lava keep the legacy full-strength 15.
- **Mod API**: new optional `light_level` field on `midless.define_block` (validated 0..15, and
  rejected on a block that does not set `light = block.light.EMIT`); the client Block carries the
  level and re-triggers chunk lighting when it changes.

## v65.2 - the height rule: no plant stands in grass as tall as itself, mushrooms get the lowest lawn

- **New world rule: every bloom and mushroom now rises above the grass growing under it.** The
  grass skirt ringed around each stem base used to grow independently of the plant - blades up to
  0.44 blocks and a sedge strand up to 0.60 - while a frost puffball at its smallest instance
  stands 0.40 tall: the grass literally swallowed the mushroom whole, which is why mushrooms
  stayed "invisible" even after the v65.1 repaint. Skirt blades now clamp to the plant's own
  height: blooms keep their grass at <= 60% of themselves, and **mushrooms get the lowest lawn
  level - 35%** - a short fuzz at the base with the cap fully in view. The cap travels with the
  per-instance size, so a dwarf plant gets dwarf grass.
- **Sedge strands skip** when the clamp would bring them down to the tuft line (no doubled quads
  at the cap height).
- **New static gate** `tools_dev/flora_heights.py`: parses the billboard heights, the per-instance
  variation span, the lawn carpet top and both grass caps straight out of `world.c` and proves for
  every species that (a) its smallest instance clears the tallest lawn blade of any biome,
  (b) its skirt ceiling sits strictly below its own top, and (c) the mushroom cap is below the
  flower cap. Exits non-zero when any species drowns in grass.

## v65.1 - the readability pass: mushrooms grow stems, flowers get their own colours per biome

- **Biome mushrooms repainted (ember + frost).** The old caps were a few stemless pixels tinted
  the same hue as the ground they stood on - frost puffballs were pale-on-pale snow, cinder
  trumpets soot-on-soot, both effectively invisible at play distance. Now: cinder trumpets are
  molten-gold funnel caps with a glowing rim and gill underside on a bone stem; frost puffballs
  are deep-magenta domes with pale spots on a violet stem. Both stand ~14 px tall with a real
  2-3 px leg, and both contrast hard against their turf (luminance delta 63-123 across the
  repainted set, was 15-25).
  The classic glowcap clusters of the starter meadow are untouched by design.
- **Biome flowers repainted so every biome reads at a glance.** Each set used to be painted in a
  single ground-tone colour ("everything in one colour"), so ember blooms vanished into charcoal
  turf and frost blooms into pale snow. Now the ember set glows bright gold/amber on bone stems
  (smolderhead dome, triple cinder buds, ember lantern spike with teal sparks) and the frost set
  burns deep magenta/orchid on violet stems (frost burst star, drooping glacier dewdrop,
  ringbloom wreath) - six distinct silhouettes, none of them the colour of its ground.
- **New automated readability gate** `tools_dev/flora_contrast.py`: per species it measures ink
  coverage, silhouette height, luminance contrast against that biome's own ground tile and
  demands a stem (narrow "leg") in the bottom rows of the sprite; it exits non-zero when any
  species camouflages. All eight repainted species pass; the gate runs standalone in tools_dev
  and joins the probe family.

## v65.0 - the biome audit: every cube grows its own, the mushroom cloud rains again, flat clouds deleted

- **Every biome surface now really wears its own grass AND its own plants.** The per-chunk
  billboard list still capped flora at 80 plants (a number from before the lawn existed), so each
  chunk silently threw away everything after its first five rows - that is why only part of every
  island had grass and the blooms/mushrooms were invisible. The cap is now 1024 compact entries
  and the whole lawn draws.
- **Biome purity enforced**: the lawn "pepper" bands were two-way (ember, else frost), so classic
  meadows grew frost bursts, ringblooms and puffballs; the starter island inherited whatever the
  biome noise said over spawn (frost plants on crystal turf); ember patches planted classic twin
  tulips. Every layer now selects per biome: classic - bellflower/starbloom/lanternberry + glowcap
  clusters; ember - smolderhead/cinder buds/ember lantern + cinder trumpets; frost - frost
  burst/glacier dewdrop/ringbloom + frost puffballs.
- **The mushroom rain cloud works again.** It never could sprout anything since the lawn rule
  landed: its spore scan demanded bare dirt/grass with air above, and every column now starts with
  a grass tuft. The scan understands lawns now, and the drops grow the biome's OWN mushroom art
  (glowcap cluster / cinder trumpet / frost puffball) instead of one generic cap.
- **Green log-and-leaf trees stay in the classic meadow.** Structures gained a `ground = { ... }`
  surface filter (the cosmic biomes are noise regions, not registered biomes, so nothing else
  could gate them). Trees also no longer float: `density` counted flower cells as terrain, so
  every gate/totem/tree was placed one block above the ground, and air-only structures silently
  dropped their base blocks - structures now settle into the grass and skip placement entirely
  when their anchor cell is occupied (no more canopies hovering over the launch pad).
- **Trees have plain trunks**: the billboard tree sprites ended in a widened root-flare "base" at
  the bottom of the trunk - repainted as straight constant-width trunks into the soil.
- **The flat vanilla cloud layer is gone for good** (cloud.c + its four shaders). It was dead code
  left behind when clouds became asteroids; nothing draws it, nothing misses it.
- **Grazers replant again**: their seed scatter had the same "cell must be air" bug as the cloud
  (lawns occupy those cells), and it still offered the v63.6 recolor twins; seeds now replace a
  lawn tuft with that biome's own bloom.
- **Cocoons return to the meadows**: the egg placement flora-list had frozen at v61 and treated the
  new lawns as flowers, so since v63.9 no cocoon could spawn anywhere; eggs now avoid blooms and
  mushrooms but may park on grass.
- **Generator headroom**: the field-graph ceiling (512) made the mod fail to LOAD once the biome
  gates grew three-way selects - raised to 1024 with a readable error.
- **Dev tooling kept in-repo** (tools_dev/): `worldprobe` generates real chunks headless and audits
  lawn coverage, biome purity, per-chunk flora caps, stacked/floating plants, tree biomes and
  undefined blocks; `probe_build.sh` builds/runs it; `atlas_view.py` zooms atlas tiles for sprite
  inspection; `fetch_raylib.sh` restores the pinned raylib headers compile_check needs.

## v64.0 - mushrooms back on the lawns, blooms pepper every island, clouds removed, menu polish

- **The experimental rain clouds are gone** (they never worked right). Mushrooms are simply lawn residents again: each biome grows only its own species right on its turf - glowcap clusters on classic turf, cinder trumpets on ember turf, frost puffballs on frost turf. Every biome lawn now has them scattered around, always.
- **The biome blooms are now impossible to miss**: instead of rare patches they are peppered cell-by-cell across the whole lawn (the spawn bands were re-tuned to the MEASURED noise percentiles - the old thresholds sat in a range the noise almost never reaches, which is exactly why you could not find the new plants).
- **Every biome block always wears its own grass** (the rule stands): classic turf - void tufts, ember turf - ember blades, frost turf - icy blades; blooms and mushrooms replace single lawn cells, never stack.
- **Menu polish**: button hover/click areas are now 3px forgiving on every side, button labels are centered by their true measured height (no more visually off-center captions), and the options panel no longer keeps an empty dead row below the last button.
- Generator safety net: the world probe now refuses any block id that is placed without a definition (this class of invisible-flora bug cannot return).

## v63.9 - the lawn rule: every biome block always wears its own grass, biome blooms actually appear

- **New rule applied**: every surface of every biome now ALWAYS carries its own grass - classic turf is tufted with void grass, ember turf with ember blades, frost turf with icy blades. No more bare charcoal or bare hoarfrost; the lawn is everywhere, and flowers claim individual cells on top of it (never stacking).
- **The six new biome blooms are finally encounterable**: the spawn patches were tuned far too rare (and two species silently shipped without block definitions - fixed, with a new generator check that forbids undefined blocks forever). Patch coverage roughly tripled: walk any ember isle and you will meet the Smolderhead, Cinder Buds and Ember Lantern; frost isles carry Frost Burst, Glacier Dewdrop and Ringbloom.
- Rain mushrooms can sprout straight through their own biome's lawn - the grass politely parts, so a cloud still mushrooms everywhere it waters.

## v63.8 - the atlas is whole again, stemmed biome blooms, one proper mushroom cloud

- **Sprite-sheet duplication fixed at the root**: the two big trees live in 2x2 sprite regions (tiles 49/50+65/66 and 53/54+69/70), and they were painted outside the generator - every atlas rebuild wiped them, and other plants ghosted into the free halves (that is exactly the "doubled/stretched" flora you spotted). The trees are now painted inside the generator itself and can never vanish again.
- **The biome blooms are redrawn with stems and varied heights** (no more egg shapes):
  - ember biome: **Smolderhead** (a coal rosette on a curved stem, ~0.9 blocks), **Cinder Buds** (three pods on stems of three different heights), **Ember Lantern** (~1.2 blocks - a tall arched stem with a hanging glow-lampion);
  - frost biome: **Frost Burst** (~1 block - an icy starburst on a straight stem), **Glacier Dewdrop** (an arched stem with one hanging teardrop bud), **Ringbloom** (a stem topped with a hollow halo of frost orbs).
- **Rain is one proper mushroom cloud again**: a single cloud drifts in, rains visibly, and mushrooms of exactly that biome sprout where drops land (glowcap cluster / cinder trumpet / frost puffball). No cloud flocks.
- Grazers snack on all the new blooms. World guide poster in the zip updated.

## v63.7 - rain clouds grow the mushrooms, three real mushroom body plans, zero recolors

- **Rain clouds are here**: every so often a small slate cloud drifts in near you, hovers and rains - you can see the drops fall under it. Where the drops land on living ground, **mushrooms sprout** - and only that biome's own species. A world starts mushroom-free: no rain, no mushrooms.
- **Three completely different mushrooms** (drawn from real fungal body plans, not recolors - the old dome-cap twins are gone for good):
  - **Void Glowcaps** (classic turf) - a gregarious *cluster* of three small teal umbrellas with lavender spots, like real agarics that sprout in groups;
  - **Cinder Trumpet** (ember turf) - a hollow *chanterelle-style funnel* rising from the ground with a smoldering glow deep in the cup;
  - **Frost Puffball** (frost turf) - a stemless *lycoperdon-style ball* sitting right on the turf, with a cold spore pore cracking open on its crown.
- The stretched/duplicated mushroom look is fixed: the old cap twins (64-66) no longer exist in generation at all, and old worlds regenerate fresh.
- Grazers snack on all the new blooms and mushrooms too. World guide poster (in the zip) updated with the rain mushrooms.

## v63.6 - unique biome flora (no recolors), the twin-flower fix, world guide in the zip

- **The "twin flower" is gone**: the orange Embercup and the white Glassbell were the same silhouette painted twice (a v59.5 recolor pair), and they could stand side by side on frost turf looking like a glitch. Both are removed from world generation completely.
- **No more recolors - six genuinely new plants**, all low and round (no tall stems - those are everywhere already):
  - **Smolderhead** (ember turf) - a squat layered coal bulb with a glowing ember core split.
  - **Cinder Cluster** (ember turf) - three charred nodes huddled on the ground, each smoldering.
  - **Ember Lens** (ember turf) - a flat molten disc lying right on the lawn, hot ring and all.
  - **Frost Star** (frost turf) - a thick six-ray ice star sprawled flat, white-tipped.
  - **Glacier Bulb** (frost turf) - a fat ice onion half-buried in the turf with a seam of cold light.
  - **Ringbloom** (frost turf) - a fairy ring of small frost orbs with a spark in the hollow.
- Grazers happily snack on the new blooms too. The world guide poster (now also shipped **inside the release zip** as `world_guide.png` + `world_guide.md`) shows every species with ids and biome notes.

## v63.5 - loading-screen polish, draw distance 34, bigger safe ship, biome grass + mushrooms, radio settings

- **Loading screen fixed**: the dirt bar no longer sticks out of its frame (edges align pixel-perfect at every window size); the bunny now faces the direction it walks and *eats each flower right in front of its nose*; and the progress bar finally reaches 100% exactly when the world is ready - the bunny always finishes its stroll. Flowers pop with petals at the spot where they are eaten.
- **Draw distance**: default is now 30 for everyone (one-time migration, your later choice is kept). The setting gained a bigger step: **18 / 22 / 26 / 30 / 34**. Fair warning for 34: with the classic single-thread loader the world fills at the same ~20 chunks/s, so a full disc at 34 is roughly 40+ minutes of standing still - it is there for the view, walking fills it as you go.
- **Grazers have no shared mind**: every grazer decides on its own (verified in code - reactions are strictly per-animal; two grazing side by side can both bolt simply because you were close to both). On top of that, two grazers no longer pick the same flower - each flower can be claimed by only one grazer now.
- **The sky freighter got a real refit**: 1.6x bigger hull, engines and trail, and its approach lane is now probed *along the actual flight path* - the ship climbs above the tallest island under the route (30 m clearance) instead of clipping through peaks.
- **Biome lawns under every plant**: the grass skirt at each stem base is picked from the ground it grows on - teal lawn on classic turf, warm ember blades on ember turf, icy pale blades on frost turf. No more green lawn on frost.
- **Mushrooms!** Each biome grows its own species on its own ground: **Void Glowcap** (glowing teal cap, classic turf), **Ember Cap** (smoldering orange cap, ember turf), **Frost Cap** (pale icy cap, frost turf). Per-mushroom crafting properties are planned for a future round.
- **Radio settings**: the game now always starts with **"The Abyss"**, and track auto-switching is a separate option ("Auto Tracks", default OFF - press N to skip manually). The Wanderer's March still opens the options screen vibe... from the second track on, if you enable auto mode.

## v63.4 - loading screen with a grazing bunny, smarter grazers, draw distance up to 30

## v63.4 - loading screen with a grazing bunny, smarter grazers, draw distance up to 30

- **Real loading screen**: entering a world now shows a dusk-sky screen where a little rosette grazer strolls along the progress bar and *eats flowers as it passes them* - like shader-compilation screens in big games. The game only drops you in once the chunk disc around spawn has actually streamed in (or after a 45 s safety timeout), so you never spawn into an empty sky and the moth never falls through unloaded terrain. Worlds with saved chunks skip through almost instantly - the bunny just sprints.
- **Grazers sleep like rabbits now**: idle choices run on a slow decision tick - a fed grazer dozes off within seconds (naps 8-22 s), and after every meal it naps 60% of the time. Before, they almost never slept.
- **Snack stolen mid-chew**: destroy the flower a grazer is eating - it notices instantly, never chews on void, and bolts (away from you if you are close, otherwise away from the vanishing flower).
- **Draw distance setting now cycles 18 / 22 / 26 / 30** (30 is back as a choice for maximum horizon; note that with the classic v62 loader a bigger value simply means the world takes longer to fill - the fog follows the value either way). Fresh settings default to 26.

## v63.3 - draw distance is a real setting (faster world fill), sprite editor mirror toggle

- **The chunk loader stays exactly as v62** (as requested) - verified byte-for-byte in this release. Standing still, the game fills a disc of ~23,000 chunks (draw distance 26) one chunk at a time at ~20 chunks/s, which takes many minutes - that is the original v62 pace, not a regression; the mod generation cost was measured identical to v62 (25.3 s vs 24.9 s on the same workload). Walking re-centers the fill, which is why islands appear along your path.
- **New: "Draw Distance" is now a clickable setting** in the pause menu (18 / 22 / 26 chunks). Smaller value = the world fills up to 2.4x faster; the fog follows the value automatically, so islands still fade out before the load edge. The choice is saved to settings.ini and applied live. Old settings files are migrated to 18 (fastest fill).
- **Sprite editor**: the mirror toggle now shows its state explicitly ("Симметрия: ВКЛ/ВЫКЛ"), draws a dashed guide line down the canvas center while on, and pops a hint when switched.

## v63.2 - world loading fully reverted to the proven v62 system + sprite editor

- **World loading reverted to exactly the v62 system** (single generator thread, one chunk request in flight, chunks delivered the moment they are generated - no neighbor waiting, no deferral). The v63/v63.1 experiments made loading stall until you moved; that whole experiment is gone. If islands still pop in at the fog edge, that is the v62 behavior you had before - tell me and we tune the fog, not the loader.
- **Sprite Editor** ships in the release: open `sprite_editor/sprite_editor.html` in any browser (double-click). Draw plants/creatures on a 16/32/48 grid with pencil, fill, line, rect, symmetry mirror, game palettes (cosmic/ember/frost/fur), undo/redo; open an existing PNG to repaint it; save as PNG (1x exact-size or 8x preview). Send me the saved PNG and I will wire it into the game atlas.
- Grazer sleep, leg-rooting, biome lawns, terraced ember isles, unstacked flora, calmed frost/ember textures, muted trees and the world guide poster from v63/v63.1 are all still in.

## v63.1 - world-loading hotfix, clean flora, calmer palette, world guide

- **Fixed the world-loading regression**: the "show a chunk only with neighbors" rule is now applied only near the player (a 5-chunk horizon around you), so the world never waits for the whole 33-chunk disk to generate. The old full-radius wait could stall the start island and pile up a giant memory queue on new worlds - both are gone. Draw distance is back to the proven 26 chunks (~416 m).
- **No more plants stacked on plants**: island-edge cells could pass the flora check twice in a row (two flowers in a column). The offset fields used by the flora gates are now terraced exactly like the terrain itself, and the near-open-sky gate was tightened - verified on a scanned map: stacked pairs went from 52 to 0.
- **Calmer biome palette**: frost turf lost its harsh white patches (softer base, dim glints), ember cracks and smolder veins glow dimmer, trees repainted in muted jade/slate and warm green/gold tones with branches only beneath the crown (no texture crossings).
- **World guide**: `docs/world_guide.png` + `docs/WORLD_GUIDE.md` - every ground block, ore, flower, tree and creature skin rendered from the real atlas with ids, plus a biome summary.

## v63 - smarter grazers, biome lawns, Doom-style trees, faster world loading

- **Grazers grew a brain**: they no longer overlap each other or step onto plants, they refuse to eat a flower they can't actually reach (no more nibbling through a ledge), they wander with a proper footstep-driven gait - legs freeze when they stand - and they occasionally curl up for a **nap** (tucked legs, drooping ears, closed eyes, slow breathing). Legs are no longer floating dots: they are rooted under the body in seamless "trouser" stubs, and ears/tail use plain fur instead of the rosette coat.
- **Every biome keeps its own lawn**: the crystal meadow stays classic, ember isles grow a charcoal-and-amber grass with embercup/tulip/lanternberry patches, frost isles grow pale icy grass with frostfern/glassbell/puff patches. The old flowers no longer leak into foreign biomes; grazers replant species native to the ground they stand on.
- **Doom-style trees**: both big trees were redrawn from scratch in the spirit of Doom's community tree sprites (proctree/DOOMTREE) - visible forked branches against the sky, asymmetric clumped canopy with dithered depth, dark under-lip, root flare, leaf tufts on branch tips. A big dead center gap was filled in.
- **The world loads much faster**: chunk generation now runs on **4 parallel worker threads** (was 1), the client keeps up to 8 chunk requests in flight (was 1), and a chunk is only revealed once its horizontal neighbors exist - islands appear whole instead of quarter-slices.
- **Draw distance raised again**: 26 -> 30 chunks (~480 m) and chunks now stack deeper vertically (6 levels around you), so high islands above and below load in time.
- **The Void Runner never clips islands anymore**: before starting a flyby it probes the highest terrain along its whole corridor and rides at least 26 m above it.

## v62 - further horizons: the Void Runner, meadow grazers, ember & frost isles

- **Islands no longer pop out of nowhere**: draw distance raised 20 -> 26 chunks (~416 m) and the distance fog rebuilt - distant isles now dissolve into the void *before* the load edge instead of snapping into view.
- **The Void Runner**: a real starship occasionally warps past in the deep background. Not blocks, not wireframes - proper faceted hull with panel seams, a self-lit glass canopy, swept wings with red/green navigation lights, twin glowing nacelles, a warp flash on entry/exit and a soft ion trail. Pure scenery: it cruises far beyond reach.
- **Grazers**: small rosette-furred meadow creatures. They wander, seek out flowers, actually *eat* them (the flower block disappears in a puff of petals), plant a fresh flower now and then, and scamper off if you approach. Fully textured and animated - walk gait, chewing bob, ear flicks.
- **Ember & Frost biomes**: huge-scale climate noise sorts the archipelago into coherent clusters in **all directions**. *Ember isles* - charcoal basalt with smoldering cracks, stepped mesa silhouettes, warm flora (embercups, twin tulips, lanternberries, lantern groves). *Frost isles* - pale hoarfrost turf with ice sparkle, frost flora (frostferns, glassbells, void puffs, void trees). Three new ground textures; the palette stays soft on the eyes.
- **More islands**: spawn thresholds loosened and a rare *deep layer* added - lonely rocks drifting far below the main roads.
- The worldgen fingerprint changed, so old chunk saves are re-generated automatically on first launch of this version.

## v61.3 - grounded stems, real trees, clean cocoons

- **Stems disappear into the lawn**: every plant now paints first and its grass skirt paints AFTER it - the base of every stem is hidden behind grass blades, never floating on the dirt. Mushrooms got the same treatment.
- **Real trees**: 5.2 / 4.4 blocks tall (from 3.4/2.9), drawn as detailed 32x32 sprites - lobed canopies with dithered depth bands, carved silhouette gaps, forked trunks with flaring roots, aerial glow-root strands on the void tree, embedded + hanging lanterns on the lantern tree.
- **No cocoons on the starter island** (server-side, 34 m around the pad) and **no cocoons on or next to flowers**: placement probes the material field and skips any flora-adjacent cell. On the starter island any leftover egg opens into shard loot with nothing hostile waking.
- Instance size variation widened for trees (0.75x..1.35x).

Floating islands drifting through a starlit void. The sun is a black hole wearing a gold ring.

## v61.2 - a cloud worth looking at, and a living meadow

- **The violet shell's top is completely new**: a plateau-cumulus in the world's own palette - big lavender lobes with moonlit crests over indigo valleys, a lit pole cap and a dusting of faint stars. No more stretched white mush.
- **The alien sky under the dome lost its checkerboard**: smooth deep green-black with soft noise drift.
- **Four new plants**: slender **void trees** (3.4 blocks, teal canopy dripping glow), warm **lantern trees** (gold canopy), raw **crystal stalks** and soft luminous **void puffs**. Trees root into big underbrush clumps; rare groves and lone giants appear in worldgen (regenerate the world to see them).
- **The lawn is no longer uniform**: every grass clump rolls its own stable dice - blade count, heights and widths vary per plant, and every plant of a species varies in size (+-30% height).

## v61.1 - polish pass on player feedback

- Pollen: slimmer motes laid behind/below the flyer - the moth is never hidden by its own dust.
- Cocoon hatch sound rebuilt (soft rustle + warm bloom).
- Starter island is a sanctuary (no mob spawns within 30 m of the pad).
- Enemy AI pass: crawler flanking + last-seen memory, spider facing-gated leading leaps.

Floating islands drifting through a starlit void. The sun is a black hole wearing a gold ring.

## v61.1 - polish pass on player feedback

- **Pollen**: slimmer motes, and the trail is laid BEHIND and slightly UNDER the flyer - the moth is never hidden behind its own dust anymore.
- **The violet shell**: rebuilt 512x256 cumulus texture with distinct lobe rims (no more stretched blur), bilinear filtering, the crooked pentagram circle on the crest is gone, and the alien sky under the dome got a smooth nebula glow instead of a hard bent line.
- **Starter island sanctuary**: no crawlers, wisps, hunter or cocoon hatches within 30 m of the spawn pad - new players get to learn walking first.
- **Enemy AI pass**: crawlers now curve around you at close range instead of forming a conga line, remember where they last saw you for a couple of seconds, and spiders only leap when actually facing you, leading your movement (sidestep-baiting jumps is harder now).
- **Cocoon hatch sound**: rebuilt from scratch - dark fibrous rustle + warm low bloom, a fraction of the old harshness.

Floating islands drifting through a starlit void. The sun is a black hole wearing a gold ring.

## v61 - pollen you can actually see, solid cubes, an honest hatchery

- **Pollen rewritten.** The v59.2 trail drew every mote as a single-wound camera quad while the game leaves backface culling on session-wide - the GPU silently culled 100% of the motes (the "pollen is gone" bug; the earlier "rainbow lines" were the same quads mis-indexed). The trail now renders on the plain pipeline with a double-wound quad on the default white texture: immune to atlas swaps, texture packs, the cutout discard and face culling. The RGB ring split and the bright-core fade stay.
- **No more holes in cubes.** The texture generator drew crack decorations with alpha 110-140 inside stone/ore/log/void-rock tiles; the chunk shader discards texels below half alpha and GL always blends, so cubes showed transparent pixels and pinholes. All cube tiles are now forced opaque (leaves, glass, water and crystal keep their intentional translucency); a lint test guards it.
- **Cocoons always hatch something.** The spider hatchery rotates: four hunt at once, and when all slots are busy the oldest hatchling collapses into collectible shards so a fresh one always climbs out of the shell - no more "empty" cocoons.

## v60 - the glowmoth pollen reads again

- Pollen motes keep a bright core all their life and only ease out at the very end.
- Bigger, longer-lived motes; the trail is fed by time AND distance, so it never gaps - even when a moth hovers in place.
- New World / Regenerate World / disconnect reset the whole fauna session.
- Web builds render the trail at half the particle cost.

## Earlier Cosmic Edition features

- Black hole with rotating accretion disk, photon ring and gravity lensing
- Galaxy band, nebulae and shooting stars
- Cosmic texture set: void rock, crystal turf, glowing water, new ores
- Explorer hands with animated fingers
- Island fauna: crawlers, wisps, cocoon spiders, glowmoths, void mushrooms, the violet shell event
- New worldgen: cone islands, starter island with launch pad, obelisks and crystal arch
- Coyote time, jump buffering, procedural sounds, ambient void wind

Extract, run `game.exe`, click Singleplayer.
Delete the old `world` folder if you played a previous version.

