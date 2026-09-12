# Midless: Cosmic Edition v60.1

Floating islands drifting through a starlit void. The sun is a black hole wearing a gold ring.

## v60 - the glowmoth pollen reads again

- Pollen motes keep a bright core all their life and only ease out at the very end (v59.8 faded them to invisibility within the first second).
- Bigger, longer-lived motes; the trail is fed by time AND distance, so it never gaps - even when a moth hovers in place.
- The RGB split is now a slow spinning ring (radial chromatic aberration) instead of a flat horizontal fringe.
- New World / Regenerate World / disconnect reset the whole fauna session: no more moths flying on stale timers or dust left from the old world.
- Web builds render the trail at half the particle cost.
- New regression harness under `tests/`: an exact model of raylib 4.5 batching guards the trail geometry (13k+ checks per run).

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
