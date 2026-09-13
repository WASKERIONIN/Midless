#!/usr/bin/env python3
"""v65.2 height rule gate: no plant or mushroom may stand in grass as tall
as itself, and mushrooms must get the LOWEST lawn level.

The rule lives in client/src/world.c (World_FloraBillboardAt): every plant
rings itself with a grass skirt (World_GrassSkirt) whose blades clamp to
`grassCap = h * (isShroom ? SHROOM_CAP : FLOWER_CAP)`. This gate parses the
real constants back out of world.c and proves, for every species in the
billboard switch:

  1. min instance height (h * kmin)  >  tallest lawn blade it can ever be
     planted beside (lawn tuft h * kmax, or the void lawn's fixed sedge) -
     a bloom must clear the carpet of its own biome, not just its skirt;
  2. its own skirt ceiling (cap * h)  <  h for every possible instance -
     grass strictly below the plant top, never equal;
  3. SHROOM_CAP < FLOWER_CAP (mushrooms get the lowest grass level) and
     SHROOM_CAP <= 0.35.

Exit 0 = rule holds, 1 = some species drowns in grass.
"""
import re
import sys
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORLD_C = os.path.join(ROOT, "client", "src", "world.c")

LAWN_IDS = {32, 39, 41, 59, 60}          # ground cover, not plants
TREE_IDS = {49, 50}                      # 32x32 atlas regions, own rules
EPS = 1e-4

src = open(WORLD_C, encoding="utf-8").read()

# --- per-species billboard silhouette: case NN: halfW = ..f; h = ..f; -------
cases = {}
for m in re.finditer(r"case\s+(\d+):\s*halfW\s*=\s*([0-9.]+)f;\s*h\s*=\s*([0-9.]+)f;", src):
    cases[int(m.group(1))] = (float(m.group(2)), float(m.group(3)))

# --- per-instance height variation: kmin / kspan ---------------------------
mk = re.search(r"kmin\s*=\s*\(id == 49 \|\| id == 50\)\s*\?\s*([0-9.]+)f\s*:\s*([0-9.]+)f;", src)
kt = re.search(r"kspan\s*=\s*\(id == 49 \|\| id == 50\)\s*\?\s*([0-9.]+)f\s*:\s*([0-9.]+)f;", src)
if not (mk and kt):
    print("flora heights: cannot parse kmin/kspan from world.c")
    sys.exit(1)
KMIN, KMAX = float(mk.group(2)), float(mk.group(2)) + float(kt.group(2))

# --- skirt grass caps: isShroom ? A f : B f --------------------------------
mc = re.search(r"isShroom\s*\?\s*([0-9.]+)f\s*:\s*([0-9.]+)f", src)
if not mc:
    print("flora heights: cannot parse grassCap fractions from world.c")
    sys.exit(1)
SHROOM_CAP, FLOWER_CAP = float(mc.group(1)), float(mc.group(2))

# --- the void lawn's fixed sedge strand (drawn beside every tuft) ----------
ms = re.search(r"Mobs_DrawBillboard\(at, 0\.20f, ([0-9.]+)f, 41,", src)
VOID_SEDGE = float(ms.group(1)) if ms else 0.0

if not cases:
    print("flora heights: no billboard cases parsed")
    sys.exit(1)

# tallest blade of the lawn carpet itself (tuft h * kmax, or fixed sedge)
lawn_top = VOID_SEDGE
for lid in (39, 59, 60):
    if lid in cases:
        lawn_top = max(lawn_top, cases[lid][1] * KMAX)

fails = []
print("flora height rule report (client/src/world.c)")
print("  lawn carpet top = %.3f  |  grass caps: shroom %.2f / flower %.2f"
      % (lawn_top, SHROOM_CAP, FLOWER_CAP))
for pid in sorted(cases):
    if pid in LAWN_IDS or pid in TREE_IDS:
        continue
    halfW, h = cases[pid]
    min_h = h * KMIN
    cap = SHROOM_CAP if pid in (73, 74, 75) else FLOWER_CAP
    skirt_top = cap * min_h            # worst (dwarf) instance
    kind = "shroom" if pid in (73, 74, 75) else "bloom "
    ok = True
    if min_h <= lawn_top + EPS:
        fails.append("%d min height %.3f <= lawn top %.3f" % (pid, min_h, lawn_top))
        ok = False
    if skirt_top >= min_h - EPS:
        fails.append("%d skirt top %.3f >= plant min height %.3f" % (pid, skirt_top, min_h))
        ok = False
    print("  %2d %s h=%.2f min=%.3f  grass_under<= %.3f  lawn_top=%.3f  %s"
          % (pid, kind, h, min_h, skirt_top, lawn_top, "ok" if ok else "FAIL"))

if SHROOM_CAP >= FLOWER_CAP:
    fails.append("shroom cap %.2f must be below flower cap %.2f (mushrooms get the lowest grass)"
                 % (SHROOM_CAP, FLOWER_CAP))
if SHROOM_CAP > 0.35 + EPS:
    fails.append("shroom cap %.2f above the lowest-lawn-level bound 0.35" % SHROOM_CAP)

if fails:
    print("FLORA HEIGHTS: %d failure(s)" % len(fails))
    for f in fails:
        print("  -", f)
    sys.exit(1)
print("FLORA HEIGHTS: OK")
