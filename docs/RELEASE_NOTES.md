# Midless: Cosmic Edition v62

Floating islands drifting through a starlit void. The sun is a black hole wearing a gold ring.

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

