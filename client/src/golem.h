/*
 * Midless: Cosmic Edition - THE WARDEN OF THE MEADOW (v65.21).
 *
 * The pocket's giant cube golem: a client-side boss in the lineage of
 * the hunters and mobs (all combat in this game is client-side). The
 * rig is a small forward-kinematics chain of cuboid parts; the poses
 * (kneel, rise, walk, aim, volley, death) are our own keyframes -
 * public-domain by construction, no external asset pipeline needed.
 *
 * Fight script: it kneels dormant until seen, rises, walks you down,
 * stops, aims, volleys orbs from its hands. Blade swings and blasts
 * tear the ARMS off first; only then the chest core becomes vulnerable.
 * On death the wreckage condenses into four warp cores - enough for
 * another 2x2 square, another gate, another pocket (v65.22).
 */
#ifndef GOLEM_H
#define GOLEM_H

#include "raylib.h"

void Golem_Init(void);
void Golem_Update(float dt);
void Golem_Draw(void);
void Golem_DrawHUD(void);

/* player offence hooks (blade swing / blast) */
bool Golem_MeleeHit(Vector3 origin, Vector3 dir, float maxDist);
void Golem_ExplosionDamage(Vector3 center, float radius, int damage);

bool Golem_Active(void);        /* inside the pocket & not finished */

#endif
