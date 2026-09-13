# Midless: Cosmic Edition v63.7

Floating islands drifting through a starlit void. The sun is a black hole wearing a gold ring.

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

