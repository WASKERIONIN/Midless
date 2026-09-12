/**
 * v50: island fauna.
 *
 * Crawlers - hostile wireframe skitterers that patrol island surfaces,
 * chase along the ground (gravity-bound, hop up single blocks) and sting
 * in melee range. Wisps - shy teal spirits that flee from the player and
 * carry extra shards; the laser is the honest way to take one.
 */
#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "mobs.h"
#include "player.h"
#include "i18n.h"
#include "world.h"
#include "block.h"
#include "hunter.h"
#include "soundfx.h"
#include "particle.h"
#include "chat.h"

/* ------------------------------------------------------------------ common */
#define MOB_BODY_R      0.35f
#define MOB_BODY_HALF_H 0.35f

static bool Mob_IsAirAt(Vector3 p) {
    return World_GetBlock(p) == 0;
}

static bool Mob_BodyBlocked(Vector3 center) {
    int minX = (int)floorf(center.x - MOB_BODY_R), maxX = (int)floorf(center.x + MOB_BODY_R);
    int minY = (int)floorf(center.y - MOB_BODY_HALF_H), maxY = (int)floorf(center.y + MOB_BODY_HALF_H);
    int minZ = (int)floorf(center.z - MOB_BODY_R), maxZ = (int)floorf(center.z + MOB_BODY_R);
    for (int y = minY; y <= maxY; y++)
        for (int z = minZ; z <= maxZ; z++)
            for (int x = minX; x <= maxX; x++) {
                int id = World_GetBlock((Vector3){ x, y, z });
                if (id == 0) continue;
                const Block *b = Block_GetDefinition(id);
                if (b->colliderType != BLOCK_COLLIDER_SOLID) continue;
                float bx0 = x + b->minBB.x / 16.0f, bx1 = x + b->maxBB.x / 16.0f;
                float by0 = y + b->minBB.y / 16.0f, by1 = y + b->maxBB.y / 16.0f;
                float bz0 = z + b->minBB.z / 16.0f, bz1 = z + b->maxBB.z / 16.0f;
                if (center.x + MOB_BODY_R > bx0 && center.x - MOB_BODY_R < bx1 &&
                    center.y + MOB_BODY_HALF_H > by0 && center.y - MOB_BODY_HALF_H < by1 &&
                    center.z + MOB_BODY_R > bz0 && center.z - MOB_BODY_R < bz1) return true;
            }
    return false;
}

static bool Mob_HasLOS(Vector3 from, Vector3 to) {
    Vector3 delta = Vector3Subtract(to, from);
    float dist = Vector3Length(delta);
    if (dist < 0.01f) return true;
    Vector3 dir = Vector3Scale(delta, 1.0f / dist);
    for (float t = 0.7f; t < dist; t += 0.6f) {
        if (!Mob_IsAirAt(Vector3Add(from, Vector3Scale(dir, t)))) return false;
    }
    return true;
}

static Vector3 Mob_PlayerCenter(void) {
    return (Vector3){ player.position.x + 0.5f, player.position.y + 0.9f, player.position.z + 0.5f };
}

/* v62: small constructor used all over the fauna code */
static Vector3 V3_(float x, float y, float z) { return (Vector3){ x, y, z }; }

/* v61.1: the starter island is a SANCTUARY - no mob spawns, no cocoon
 * hatches (and no hunter materializations; see hunter.c) around the
 * cosmic spawn pad. New players get to learn walking before dying. */
#define STARTER_SAFE_R 30.0f
bool Mobs_InStarterSanctuary(Vector3 p) {
    float dx = p.x - COSMIC_SPAWN_X;
    float dz = p.z - COSMIC_SPAWN_Z;
    return dx * dx + dz * dz < STARTER_SAFE_R * STARTER_SAFE_R;
}

/* ---------------------------------------------------------------- crawlers */
#define CRAWLER_MAX 4
#define CRAWLER_HP 2
#define CRAWLER_SPEED 1.7f
#define CRAWLER_SIGHT 9.0f
#define CRAWLER_SIGHT_MEM 2.0f

typedef struct Crawler {
    bool active;
    Vector3 pos;
    Vector3 vel;
    float hp;
    float phase;
    float aggroTimer;
    float retreatUntil;
    float wanderDir;
    float wanderTimer;
    float age;
    bool grounded;
    float stuck;   /* v56: how long the body has been fighting a wall */
    /* v61.1 AI pass: flanking curve around the target at close range and
     * a short memory of the last seen position */
    float flankSide;
    float flankTimer;
    Vector3 lastSeen;
    float memoryTimer;
} Crawler;
static Crawler crawlers[CRAWLER_MAX];
static bool crawlerAnnounced;

/* ------------------------------------------------------------------- wisps */
#define WISP_MAX 2
#define WISP_HP 1
#define WISP_SPEED 2.2f
#define WISP_FLEE_RANGE 7.0f

typedef struct Wisp {
    bool active;
    Vector3 pos;
    Vector3 vel;
    float phase;
    float age;
    float fleeUntil;
} Wisp;
static Wisp wisps[WISP_MAX];



int Mobs_CrawlerCount(void) {
    int n = 0;
    for (int i = 0; i < CRAWLER_MAX; i++) if (crawlers[i].active) n++;
    return n;
}

int Mobs_WispCount(void) {
    int n = 0;
    for (int i = 0; i < WISP_MAX; i++) if (wisps[i].active) n++;
    return n;
}

/* ------------------------------------------------------------- spawning */
static bool Mob_FindSurfaceSpot(Vector3 center, float minDist, float maxDist, Vector3 *out) {
    /* sample a loaded chunk ring around the player, drop to the surface */
    for (int attempt = 0; attempt < 10; attempt++) {
        float ang = (float)GetRandomValue(0, 3599) * 0.001745f;
        float dist = minDist + (float)GetRandomValue(0, 900) / 100.0f * (maxDist - minDist) / 9.0f;
        float px = center.x + cosf(ang) * dist;
        float pz = center.z + sinf(ang) * dist;
        int bx = (int)floorf(px), bz = (int)floorf(pz);
        for (int y = 150; y >= 2; y--) {
            int id = World_GetBlock((Vector3){ bx, y, bz });
            if (id == 0) continue;
            const Block *b = Block_GetDefinition(id);
            if (b->colliderType != BLOCK_COLLIDER_SOLID) continue;
            Vector3 spot = { bx + 0.5f, y + 1 + MOB_BODY_HALF_H + 0.02f, bz + 0.5f };
            if (Mob_BodyBlocked(spot)) break;   /* v52: try the next attempt */
            *out = spot;
            return true;
        }
    }
    return false;
}

static void Crawler_SpawnTry(bool surge) {
    int cap = surge ? 3 : 1;
    if (Mobs_CrawlerCount() >= cap) return;
    Vector3 center = Mob_PlayerCenter();
    Vector3 spot;
    if (!Mob_FindSurfaceSpot(center, 10.0f, 24.0f, &spot)) return;
    if (Mobs_InStarterSanctuary(spot)) return;   /* v61.1: spawn sanctuary */
    for (int i = 0; i < CRAWLER_MAX; i++) {
        Crawler *c = &crawlers[i];
        if (c->active) continue;
        c->active = true;
        c->pos = spot;
        c->vel = (Vector3){ 0 };
        c->hp = CRAWLER_HP;
        c->phase = (float)GetRandomValue(0, 628) / 100.0f;
        c->aggroTimer = 0.0f;
        c->retreatUntil = 0.0f;
        c->wanderDir = (float)GetRandomValue(0, 3599) * 0.001745f;
        c->wanderTimer = 2.0f;
        c->age = 0.0f;
        c->grounded = false;
        c->stuck = 0.0f;
        c->flankSide = (GetRandomValue(0, 1) == 0) ? 1.0f : -1.0f;
        c->flankTimer = 1.5f + GetRandomValue(0, 150) / 100.0f;
        c->lastSeen = (Vector3){ 0 };
        c->memoryTimer = 0.0f;
        if (!crawlerAnnounced) {
            crawlerAnnounced = true;
            Chat_AddLine(Tr("Something skitters across the island..."));
        }
        return;
    }
}

static void Wisp_SpawnTry(void) {
    if (Mobs_WispCount() >= WISP_MAX) return;
    Vector3 center = Mob_PlayerCenter();
    for (int attempt = 0; attempt < 6; attempt++) {
        float ang = (float)GetRandomValue(0, 3599) * 0.001745f;
        float dist = 12.0f + (float)GetRandomValue(0, 800) / 100.0f;
        Vector3 p = { center.x + cosf(ang) * dist,
                      center.y - 3.0f + (float)GetRandomValue(0, 600) / 100.0f,
                      center.z + sinf(ang) * dist };
        if (!Mob_BodyBlocked(p) && !Mobs_InStarterSanctuary(p)) {
            for (int i = 0; i < WISP_MAX; i++) {
                Wisp *w = &wisps[i];
                if (w->active) continue;
                w->active = true;
                w->pos = p;
                w->vel = (Vector3){ 0 };
                w->phase = (float)GetRandomValue(0, 628) / 100.0f;
                w->age = 0.0f;
                w->fleeUntil = 0.0f;
                return;
            }
        }
    }
}

/* --------------------------------------------------------------- updates */
static void Crawler_Update(float deltaTime, bool surge, double now) {
    Vector3 center = Mob_PlayerCenter();
    for (int i = 0; i < CRAWLER_MAX; i++) {
        Crawler *c = &crawlers[i];
        if (!c->active) continue;
        c->age += deltaTime;

        Vector3 toPlayer = Vector3Subtract(center, c->pos);
        toPlayer.y = 0;
        float hdist = Vector3Length(toPlayer);
        float dist3 = Vector3Distance(center, c->pos);
        if (c->age > 120.0f || dist3 > 40.0f) { c->active = false; continue; }

        /* sight-based aggro like the hunters.
         * v59.3: a surge no longer wakes the whole island - the tide
         * raises temper only for crawlers actually NEAR the player
         * (12 m); far ones keep roaming instead of piling under you. */
        if (!player.flying && hdist < CRAWLER_SIGHT &&
            Mob_HasLOS(c->pos, center)) {
            c->aggroTimer = CRAWLER_SIGHT_MEM;
            c->lastSeen = center;          /* v61.1: remember the spot */
            c->memoryTimer = 2.5f;
        } else if (c->aggroTimer > 0.0f) {
            c->aggroTimer -= deltaTime;
        } else if (c->memoryTimer > 0.0f) {
            c->memoryTimer -= deltaTime;
        }
        bool aggro = ((surge && hdist < 12.0f) || c->aggroTimer > 0.0f) &&
                     now >= c->retreatUntil;

        Vector3 desired;
        if (aggro && hdist > 0.05f) {
            Vector3 chase = Vector3Scale(Vector3Scale(toPlayer, 1.0f / hdist),
                                         CRAWLER_SPEED * (surge ? 1.2f : 1.0f));
            /* v61.1: at close range curve AROUND the target instead of
             * forming a conga line straight up the player's nose */
            c->flankTimer -= deltaTime;
            if (c->flankTimer <= 0.0f) {
                c->flankSide = -c->flankSide;
                c->flankTimer = 1.5f + GetRandomValue(0, 150) / 100.0f;
            }
            if (hdist < 4.5f && hdist > 0.9f) {
                Vector3 tangent = Vector3Normalize((Vector3){ -toPlayer.z, 0, toPlayer.x });
                float curve = CRAWLER_SPEED * 0.55f * c->flankSide * (1.0f - hdist / 4.5f);
                chase = Vector3Add(Vector3Scale(chase, 0.72f), Vector3Scale(tangent, curve));
            }
            desired = chase;
        } else if (!aggro && c->memoryTimer > 0.0f) {
            /* v61.1: the trail went cold - walk to where the player was
             * last seen before drifting back to wandering */
            Vector3 toSeen = Vector3Subtract(c->lastSeen, c->pos);
            toSeen.y = 0;
            float sd = Vector3Length(toSeen);
            if (sd > 0.6f) {
                desired = Vector3Scale(Vector3Scale(toSeen, 1.0f / sd), CRAWLER_SPEED * 0.8f);
            } else {
                c->memoryTimer = 0.0f;
                desired = (Vector3){ 0 };
            }
        } else {
            c->wanderTimer -= deltaTime;
            if (c->wanderTimer <= 0.0f) {
                c->wanderTimer = 2.5f + (float)GetRandomValue(0, 250) / 100.0f;
                c->wanderDir = (float)GetRandomValue(0, 3599) * 0.001745f;
            }
            desired = (Vector3){ cosf(c->wanderDir) * 0.6f, 0, sinf(c->wanderDir) * 0.6f };
        }
        /* back off after a sting */
        if (now < c->retreatUntil) {
            Vector3 away = Vector3Scale(toPlayer, -1.0f / (hdist > 0.05f ? hdist : 1.0f));
            desired = Vector3Scale(away, CRAWLER_SPEED * 0.8f);
        }
        /* v59.3: separation - step aside from the nearest fellow crawler
         * so packs spread out instead of stacking into one column */
        for (int o = 0; o < CRAWLER_MAX; o++) {
            if (o == i || !crawlers[o].active) continue;
            Vector3 away = Vector3Subtract(c->pos, crawlers[o].pos);
            float d = Vector3Length(away);
            if (d > 0.01f && d < 1.3f)
                desired = Vector3Add(desired, Vector3Scale(away, (1.3f - d) * 2.4f / d));
        }

        c->vel.x = c->vel.x + (desired.x - c->vel.x) * (1.0f - powf(0.05f, deltaTime));
        c->vel.z = c->vel.z + (desired.z - c->vel.z) * (1.0f - powf(0.05f, deltaTime));
        c->vel.y -= 0.02f * deltaTime * 60.0f;
        if (c->vel.y < -0.5f) c->vel.y = -0.5f;

        /* horizontal move with step-up hop */
        Vector3 want = Vector3Add(c->pos, Vector3Scale((Vector3){ c->vel.x, 0, c->vel.z }, deltaTime));
        if (!Mob_BodyBlocked(want)) {
            c->pos = want;
            c->stuck = 0.0f;
        } else if (c->grounded) {
            Vector3 stepUp = c->pos;
            stepUp.y += 1.05f;
            Vector3 stepOver = Vector3Add(stepUp, Vector3Scale((Vector3){ c->vel.x, 0, c->vel.z }, deltaTime * 2.0f));
            if (!Mob_BodyBlocked(stepOver)) {
                c->pos = stepOver;
                c->vel.y = 0.1f;
                c->stuck = 0.0f;
            } else if (c->stuck > 0.7f) {
                /* v56: cornered against a two-block wall - leap over it */
                c->vel.y = 2.1f;
                c->grounded = false;
                c->stuck = -1.5f;   /* cooldown before the next leap */
            } else if (c->stuck >= 0.0f) {
                c->stuck += deltaTime;
                c->vel.x *= 0.6f;
                c->vel.z *= 0.6f;
            } else {
                c->stuck += deltaTime;   /* negative = mid-cooldown */
            }
        } else if (c->stuck < 0.0f || c->vel.y > 0.5f) {
            /* v56: mid-leap - keep the horizontal push to clear the wall */
        } else {
            c->vel.x *= 0.4f;
            c->vel.z *= 0.4f;
        }

        /* vertical move + ground snap */
        Vector3 down = Vector3Add(c->pos, Vector3Scale((Vector3){ 0, c->vel.y, 0 }, deltaTime));
        if (!Mob_BodyBlocked(down)) {
            c->pos = down;
            c->grounded = false;
        } else {
            if (c->vel.y < 0) c->grounded = true;
            c->vel.y = 0;
        }

        /* sting */
        if (aggro && dist3 < 1.15f && now >= c->retreatUntil) {
            Vector3 push = Vector3Scale(Vector3Scale(toPlayer, -1.0f / (hdist > 0.05f ? hdist : 1.0f)), 1.0f);
            Player_Damage(2, push);
            c->retreatUntil = (float)now + 1.2f;
        }
    }
}

static void Wisp_Update(float deltaTime, double now) {
    Vector3 center = Mob_PlayerCenter();
    for (int i = 0; i < WISP_MAX; i++) {
        Wisp *w = &wisps[i];
        if (!w->active) continue;
        w->age += deltaTime;
        if (w->age > 60.0f) { w->active = false; continue; }

        Vector3 toPlayer = Vector3Subtract(center, w->pos);
        float dist = Vector3Length(toPlayer);
        if (dist > 45.0f) { w->active = false; continue; }

        if (dist < WISP_FLEE_RANGE && w->age > 1.0f) w->fleeUntil = (float)now + 2.0f;

        Vector3 desired;
        if ((double)w->fleeUntil > now && dist > 0.05f) {
            desired = Vector3Scale(Vector3Scale(toPlayer, -1.0f / dist), WISP_SPEED);
        } else {
            float t = (float)now * 0.4f + w->phase;
            desired = (Vector3){ sinf(t) * 0.5f, sinf(t * 1.9f + w->phase) * 0.25f, cosf(t * 0.77f) * 0.5f };
        }
        w->vel = Vector3Lerp(w->vel, desired, 1.0f - powf(0.15f, deltaTime));
        Vector3 next = Vector3Add(w->pos, Vector3Scale(w->vel, deltaTime));
        if (!Mob_BodyBlocked(next)) w->pos = next;
        else w->vel = Vector3Scale(w->vel, -0.5f);
    }
}

/* ---------------------------------------------------------------- spiders */
/* v51: hatch from void cocoons. Two legs and a segmented tail it whips
 * around; attacks by leaping at the player, then bounces back.
 * v61: the hatchery rotates. The old cap of 2 made every cocoon past the
 * first two hatch "empty" until a spider died or despawned (the player
 * read that as cocoons releasing nothing). Now four hunt at once, and a
 * fifth hatch collapses the OLDEST spider into collectible shards so a
 * fresh one always climbs out of the shell. */
#define SPIDER_MAX 4
#define SPIDER_HP 3
#define SPIDER_SPEED 2.1f
#define SPIDER_SIGHT 10.0f
#define SPIDER_LEAP_RANGE 3.4f

typedef struct Spider {
    bool active;
    Vector3 pos;
    Vector3 vel;
    float hp;
    float phase;
    float faceAng;
    float aggroTimer;
    float jumpCd;
    float stingCd;
    float bounceBackUntil;
    bool airborne;
    bool grounded;
    float age;
    float stuck;   /* v56: how long the body has been fighting a wall */
} Spider;
static Spider spiders[SPIDER_MAX];
static bool spiderAnnounced;

/* v55: returns false only if the spawn is truly impossible.
 * v61: with all four slots busy the OLDEST hatchling collapses into
 * shards and the newborn takes its place - a hatched cocoon always
 * releases a spider. */
static bool Spider_SpawnInto(Spider *s, Vector3 pos);   /* v61: shared body */

bool Mobs_SpawnSpider(Vector3 pos) {
    for (int i = 0; i < SPIDER_MAX; i++) {
        Spider *s = &spiders[i];
        if (s->active) continue;
        return Spider_SpawnInto(s, pos);
    }
    /* hatchery full: rotate the oldest one out */
    Spider *oldest = &spiders[0];
    for (int i = 1; i < SPIDER_MAX; i++) {
        if (spiders[i].age > oldest->age) oldest = &spiders[i];
    }
    /* v61: the rotated-out body bursts into shard loot on the ground */
    Hunter_WireBurst(oldest->pos);
    Particle_SpawnImpact(oldest->pos);
    Hunter_DropShards(oldest->pos, 2);
    return Spider_SpawnInto(oldest, pos);
}

static bool Spider_SpawnInto(Spider *s, Vector3 pos) {
    s->active = true;
    s->pos = pos;
    s->vel = (Vector3){ 0, 1.2f, 0 };   /* bursts out of the cocoon */
    s->hp = SPIDER_HP;
    s->phase = (float)GetRandomValue(0, 628) / 100.0f;
    s->faceAng = (float)GetRandomValue(0, 3599) * 0.001745f;
    s->aggroTimer = 8.0f;               /* freshly hatched: furious */
    s->jumpCd = 0.8f;
    s->stingCd = 0.0f;
    s->bounceBackUntil = 0.0f;
    s->airborne = false;
    s->grounded = false;
    s->age = 0.0f;
    s->stuck = 0.0f;
    SoundFx_PlayExplosion();
    Particle_SpawnImpact(pos);
    if (!spiderAnnounced) {
        spiderAnnounced = true;
        Chat_AddLine(Tr("The cocoon splits open. Something many-legged rises."));
    }
    return true;
}

static void Spider_Damage(Spider *s, Vector3 rd) {
    s->hp -= 1.0f;
    s->vel = Vector3Add(Vector3Scale(rd, 2.4f), (Vector3){ 0, 0.4f, 0 });
    Particle_SpawnImpact(s->pos);
    s->aggroTimer = 4.0f;
    if (s->hp <= 0.0f) {
        s->active = false;
        Hunter_WireBurst(s->pos);
        Particle_SpawnImpact(s->pos);
        Hunter_DropShards(s->pos, 2);
        Player_Heal(1);
        SoundFx_PlayHunterDie();
        Chat_AddLine(Tr("The hatchling collapses into shards."));
    } else {
        SoundFx_PlayHunterHit();
    }
}

static void Spider_Update(float deltaTime, double now) {
    Vector3 center = Mob_PlayerCenter();
    for (int i = 0; i < SPIDER_MAX; i++) {
        Spider *s = &spiders[i];
        if (!s->active) continue;
        s->age += deltaTime;
        if (s->jumpCd > 0.0f) s->jumpCd -= deltaTime;
        if (s->stingCd > 0.0f) s->stingCd -= deltaTime;

        Vector3 toPlayer = Vector3Subtract(center, s->pos);
        toPlayer.y = 0;
        float hdist = Vector3Length(toPlayer);
        float dist3 = Vector3Distance(center, s->pos);
        /* v55: hatchlings roam for 90 s at most, so cocoons never feel dead */
        if (s->age > 90.0f || dist3 > 32.0f) { s->active = false; continue; }

        /* v59.3: spiders ignore a player hovering high above the ground */
        if (!player.flying && hdist < SPIDER_SIGHT && Mob_HasLOS(s->pos, center)) s->aggroTimer = 3.0f;
        else if (s->aggroTimer > 0.0f) s->aggroTimer -= deltaTime;
        bool aggro = s->aggroTimer > 0.0f ||
                     (Hunter_GetSurgeLevel() > 0.5f &&
                      Vector3Distance(center, s->pos) < 12.0f);

        Vector3 desired = (Vector3){ 0 };
        if (aggro && (double)s->bounceBackUntil > now) {
            /* just landed from a leap: bounce back out of reach */
            if (hdist > 0.05f) desired = Vector3Scale(toPlayer, -SPIDER_SPEED * 1.3f / hdist);
        } else if (aggro && hdist > 0.05f) {
            desired = Vector3Scale(toPlayer, SPIDER_SPEED / hdist);
        } else if (hdist > 0.05f) {
            float t = (float)now * 0.35f + s->phase;
            desired = (Vector3){ cosf(t) * 0.5f, 0, sinf(t * 0.8f) * 0.5f };
        }
        /* v59.3: separation from fellow spiders - no more clumping */
        for (int o = 0; o < SPIDER_MAX; o++) {
            if (o == i || !spiders[o].active) continue;
            Vector3 away = Vector3Subtract(s->pos, spiders[o].pos);
            float d = Vector3Length(away);
            if (d > 0.01f && d < 1.6f)
                desired = Vector3Add(desired, Vector3Scale(away, (1.6f - d) * 2.6f / d));
        }

        /* leap attack: from up to three blocks away it jumps at the player.
         * v61.1: it leaps only once actually FACING the target (no more
         * mid-turn backward hops) and leads the aim by half the player's
         * motion during the flight - sidestep-dodging straight lines no
         * longer trivially baits every jump. */
        if (aggro && s->grounded && s->jumpCd <= 0.0f &&
            dist3 > 1.3f && dist3 < SPIDER_LEAP_RANGE && now >= s->bounceBackUntil) {
            Vector3 facing = (Vector3){ cosf(s->faceAng), 0, sinf(s->faceAng) };
            Vector3 pn = (hdist > 0.05f) ? Vector3Scale(toPlayer, 1.0f / hdist) : (Vector3){ 0, 0, 1 };
            if (pn.x * facing.x + pn.z * facing.z > 0.25f) {
                float flight = dist3 / 3.6f;
                Vector3 aim = (Vector3){ center.x + player.velocity.x * 0.5f * flight,
                                         center.y,
                                         center.z + player.velocity.z * 0.5f * flight };
                Vector3 toAim = Vector3Subtract(aim, s->pos);
                toAim.y = 0;
                float ad = Vector3Length(toAim);
                Vector3 dir = (ad > 0.05f) ? Vector3Scale(toAim, 1.0f / ad) : pn;
                s->vel = (Vector3){ dir.x * 3.3f, 1.55f, dir.z * 3.3f };
                s->airborne = true;
                s->grounded = false;
                s->jumpCd = 1.7f;
                SoundFx_PlayJump();
            }
        }

        if (!s->airborne) {
            s->vel.x = s->vel.x + (desired.x - s->vel.x) * (1.0f - powf(0.05f, deltaTime));
            s->vel.z = s->vel.z + (desired.z - s->vel.z) * (1.0f - powf(0.05f, deltaTime));
        }
        s->vel.y -= 0.02f * deltaTime * 60.0f;
        if (s->vel.y < -0.5f) s->vel.y = -0.5f;

        /* face where we are going */
        float spd2 = s->vel.x * s->vel.x + s->vel.z * s->vel.z;
        if (spd2 > 0.02f) {
            float want = atan2f(s->vel.z, s->vel.x);
            float d = want - s->faceAng;
            while (d > 3.1416f) d -= 6.2832f;
            while (d < -3.1416f) d += 6.2832f;
            s->faceAng += d * (1.0f - powf(0.001f, deltaTime));
        }

        if (!s->airborne) {
            Vector3 want = Vector3Add(s->pos, Vector3Scale((Vector3){ s->vel.x, 0, s->vel.z }, deltaTime));
            if (!Mob_BodyBlocked(want)) {
                s->pos = want;
                s->stuck = 0.0f;
            } else if (s->grounded) {
                Vector3 stepUp = s->pos;
                stepUp.y += 1.05f;
                Vector3 stepOver = Vector3Add(stepUp, Vector3Scale((Vector3){ s->vel.x, 0, s->vel.z }, deltaTime * 2.0f));
                if (!Mob_BodyBlocked(stepOver)) { s->pos = stepOver; s->vel.y = 0.1f; s->stuck = 0.0f; }
                else if (s->stuck > 0.7f) {
                    s->vel.y = 2.2f;          /* v56: pounce over the wall */
                    s->airborne = true;
                    s->grounded = false;
                    s->stuck = -1.5f;
                }
                else if (s->stuck >= 0.0f) { s->stuck += deltaTime; s->vel.x *= 0.6f; s->vel.z *= 0.6f; }
                else s->stuck += deltaTime;
            }
        } else {
            Vector3 fly = Vector3Add(s->pos, Vector3Scale(s->vel, deltaTime));
            if (!Mob_BodyBlocked(fly)) s->pos = fly;
            else { s->airborne = false; s->vel.x = 0; s->vel.z = 0; }
        }

        /* vertical */
        Vector3 down = Vector3Add(s->pos, Vector3Scale((Vector3){ 0, s->vel.y, 0 }, deltaTime));
        if (!Mob_BodyBlocked(down)) {
            s->pos = down;
            s->grounded = false;
        } else {
            if (s->airborne) {
                /* landing after a leap -> bounce back */
                s->airborne = false;
                s->bounceBackUntil = now + 0.7;
            }
            if (s->vel.y < 0) s->grounded = true;
            s->vel.y = 0;
        }

        /* sting on contact */
        if (aggro && dist3 < 1.2f && s->stingCd <= 0.0f) {
            Vector3 push = Vector3Scale(toPlayer, -1.0f / (hdist > 0.05f ? hdist : 1.0f));
            Player_Damage(2, push);
            s->stingCd = 1.2f;
            s->bounceBackUntil = now + 0.6;
        }
    }
}

/* ---------------------------------------------------------------- cocoons */
static void Cocoon_Hatch(Vector3 cell) {
    Vector3 spawn = { cell.x + 0.5f, cell.y + 0.35f, cell.z + 0.5f };
    /* v59.8: the cocoon ALWAYS cracks open now. The old rule kept it
     * sealed while both hatch slots were busy, so with two spiders
     * around every nearby cocoon was a dead prop forever. The egg is
     * consumed either way; a spider climbs out when there is room, and
     * when there is not - the hatchling collapses into drifting shards
     * so the shell is never wasted.
     * v61.3: on the starter island the shell still opens (no frozen
     * props), but nothing hostile climbs out - the sanctuary never
     * spawns enemies, and shards are the consolation. */
    World_SetBlock(cell, 0, true);
    SoundFx_PlayCocoonOpen();
    Hunter_WireBurst((Vector3){ cell.x + 0.5f, cell.y + 0.5f, cell.z + 0.5f });
    Particle_SpawnImpact((Vector3){ cell.x + 0.5f, cell.y + 0.5f, cell.z + 0.5f });
    if (Mobs_InStarterSanctuary(cell)) {
        Hunter_DropShards((Vector3){ cell.x + 0.5f, cell.y + 0.6f, cell.z + 0.5f }, 2);
        Chat_AddLine(Tr("The cocoon cracks open - the hatchling never wakes. Shards drift out."));
        return;
    }
    if (!Mobs_SpawnSpider(spawn)) {
        Hunter_DropShards((Vector3){ cell.x + 0.5f, cell.y + 0.6f, cell.z + 0.5f }, 2);
        Chat_AddLine(Tr("The cocoon cracks open - empty. Shards drift out."));
    }
}

static void Cocoon_Scan(float deltaTime) {
    /* proximity collapse: any cocoon within three blocks bursts open */
    static double scanTimer = 0.0;
    scanTimer -= deltaTime;
    if (scanTimer > 0.0f) return;
    scanTimer = 0.5;
    Vector3 pc = Mob_PlayerCenter();
    int px = (int)floorf(pc.x), py = (int)floorf(pc.y), pz = (int)floorf(pc.z);
    for (int dy = 3; dy >= -3; dy--)
        for (int dz = -3; dz <= 3; dz++)
            for (int dx = -3; dx <= 3; dx++) {
                Vector3 cell = { px + dx, py + dy, pz + dz };
                if (World_GetBlock(cell) == 25) {
                    Cocoon_Hatch(cell);
                    return;   /* one hatch per scan keeps the moment readable */
                }
            }
}

/* laser pops cocoons from a distance; also detonates barrels in the path */
bool Mobs_CocoonLaser(Vector3 origin, Vector3 dir, float maxDist, Vector3 *hitPoint) {
    float rayLen = Vector3Length(dir);
    if (rayLen < 0.0001f) return false;
    Vector3 rd = Vector3Scale(dir, 1.0f / rayLen);
    for (float t = 1.0f; t < maxDist; t += 0.7f) {
        Vector3 p = Vector3Add(origin, Vector3Scale(rd, t));
        int id = World_GetBlock(p);
        if (id != 0) {
            const Block *b = Block_GetDefinition(id);
            /* v54: the beam sears island flora - cut it down */
            if (b->modelType == BLOCK_MODEL_SPRITE && id != 15) {
                Vector3 cell = { floorf(p.x), floorf(p.y), floorf(p.z) };
                World_SetBlock(cell, 0, true);
                Particle_SpawnImpact((Vector3){ cell.x + 0.5f, cell.y + 0.35f, cell.z + 0.5f });
                Particle_SpawnImpact((Vector3){ cell.x + 0.5f, cell.y + 0.6f, cell.z + 0.5f });
                SoundFx_PlayHunterHit();
                if (hitPoint) *hitPoint = (Vector3){ cell.x + 0.5f, cell.y + 0.4f, cell.z + 0.5f };
                return true;
            }
            if (b->colliderType == BLOCK_COLLIDER_SOLID) return false;  /* wall stops the beam */
        }
        if (id == 25) {
            Vector3 cell = { floorf(p.x), floorf(p.y), floorf(p.z) };
            World_SetBlock(cell, 0, true);
            Hunter_WireBurst((Vector3){ cell.x + 0.5f, cell.y + 0.5f, cell.z + 0.5f });
            Particle_SpawnImpact((Vector3){ cell.x + 0.5f, cell.y + 0.5f, cell.z + 0.5f });
            SoundFx_PlayHunterHit();
            Chat_AddLine(Tr("The cocoon bursts under the beam. Silence... for now."));
            if (hitPoint) *hitPoint = (Vector3){ cell.x + 0.5f, cell.y + 0.5f, cell.z + 0.5f };
            return true;
        }
        if (id == 26) {
            Vector3 cell = { floorf(p.x), floorf(p.y), floorf(p.z) };
            if (hitPoint) *hitPoint = (Vector3){ cell.x + 0.5f, cell.y + 0.5f, cell.z + 0.5f };
            World_ExplodeAt(cell);
            return true;
        }
    }
    return false;
}

/* v51: any mob standing on / flying through a volatile barrel sets it off */
static void Mob_BarrelCheck(Vector3 pos, bool flying) {
    int bx = (int)floorf(pos.x), bz = (int)floorf(pos.z);
    int feetY = (int)floorf(pos.y - (flying ? 0.0f : 0.45f));
    /* body cell, and for flyers also the cells below: passing even one
     * block above a barrel cooks it off */
    int maxY = feetY, minY = flying ? feetY - 2 : feetY - 1;
    for (int y = minY; y <= maxY; y++) {
        if (World_GetBlock((Vector3){ bx, y, bz }) == 26) {
            World_ExplodeAt((Vector3){ bx, y, bz });
            return;
        }
    }
}

/* -------------------------------------------------------------- mushrooms */
#define MUSH_MAX 24
typedef struct Mushroom {
    bool active;
    Vector3 pos;
    double expireAt;   /* 10 real minutes */
    float scale;
} Mushroom;
static Mushroom mushrooms[MUSH_MAX];
static int mushroomsStored = 0;   /* inventory count */
static bool mushHintShown;

/* ------------------- v59.3: shell textures (runtime) --------------------
 * v61.1: the atlas tiles were never going to read over a 9 m dome, so the
 * cap wears RUNTIME textures. 512x256 now (the first 256x128 pass read
 * stretched and mushy up close), with distinct cumulus lobes: cell rims
 * come from the noise gradient, the pole stays lit, a touch of dither
 * kills banding - and both shells use bilinear filtering. */
#define SHELL_TW 512
#define SHELL_TH 256
static Texture2D shellTopTex;
static Texture2D shellUndTex;
static bool shellTexReady;

static float Shell_WrappedNoiseN(const float *g, int n, float fx, float fy) {
    float x = fx * n, y = fy * n;
    int x0 = (int)x % n, y0 = (int)y % n;
    int x1 = (x0 + 1) % n, y1 = (y0 + 1) % n;
    float tx = x - (int)x, ty = y - (int)y;
    tx = tx * tx * (3 - 2 * tx);
    ty = ty * ty * (3 - 2 * ty);
    float a = g[y0 * n + x0] * (1 - tx) + g[y0 * n + x1] * tx;
    float b = g[y1 * n + x0] * (1 - tx) + g[y1 * n + x1] * tx;
    return a * (1 - ty) + b * ty;
}

static Texture2D Shell_MakeTopTexture(void) {
    /* v61.2b: PLATEAU CUMULUS. Noise soup failed twice on the dome, so
     * this is a cartoon-cloud construction instead: the low-frequency
     * field is threshold-smoothed into big flat lobes with soft skirts,
     * a directional light brush paints the crest of every lobe, and the
     * valleys sink into indigo with a teal wash. Reads as ONE cloud from
     * any distance, in the world's own palette. */
    Color *px = (Color *)MemAlloc(SHELL_TW * SHELL_TH * sizeof(Color));
    unsigned int seed = 987654321u;
    float g8[9][9], g16[17][17];
    for (int j = 0; j < 9; j++)
        for (int i = 0; i < 9; i++) {
            seed = seed * 1664525u + 1013904223u;
            g8[j][i] = (float)(seed >> 16) / 65535.0f;
        }
    for (int j = 0; j < 9; j++) { g8[j][8] = g8[j][0]; g8[8][j] = g8[0][j]; }
    g8[8][8] = g8[0][0];
    for (int j = 0; j < 17; j++)
        for (int i = 0; i < 17; i++) {
            seed = seed * 1664525u + 1013904223u;
            g16[j][i] = (float)(seed >> 16) / 65535.0f;
        }
    for (int j = 0; j < 17; j++) { g16[j][16] = g16[j][0]; g16[16][j] = g16[0][j]; }
    g16[16][16] = g16[0][0];

    #define SHELL_SSTEP(a, b, v) (Clamp(((v) - (a)) / ((b) - (a)), 0.0f, 1.0f))

    for (int y = 0; y < SHELL_TH; y++) {
        float fy = (float)y / SHELL_TH;
        /* pole cap: near the pinch everything settles to one lit tone */
        float capMix = 1.0f - SHELL_SSTEP(0.06f, 0.20f, fy);
        for (int x = 0; x < SHELL_TW; x++) {
            float fx = (float)x / SHELL_TW;
            float n = Shell_WrappedNoiseN(&g8[0][0], 8, fx, fy) * 0.72f +
                      Shell_WrappedNoiseN(&g16[0][0], 16, fx, fy) * 0.28f;
            /* big smooth lobes: plateau where the field is high */
            float plateau = SHELL_SSTEP(0.42f, 0.58f, n);
            /* directional brush: light falls from up-left */
            float e = 0.018f;
            float ns = Shell_WrappedNoiseN(&g8[0][0], 8, fx + e, fy + e * 0.6f) * 0.72f +
                       Shell_WrappedNoiseN(&g16[0][0], 16, fx + e, fy + e * 0.6f) * 0.28f;
            float ps = SHELL_SSTEP(0.42f, 0.58f, ns);
            float crest = Clamp((plateau - ps) * 9.0f, 0.0f, 1.0f) * plateau;
            float valley = 1.0f - plateau;

            float valley3 = valley * valley * valley;
            float r = 48.0f + 88.0f * plateau + 64.0f * crest - 6.0f * valley + 14.0f * valley3;
            float g = 38.0f + 76.0f * plateau + 58.0f * crest + 8.0f * valley;
            float b = 86.0f + 112.0f * plateau + 38.0f * crest - 4.0f * valley + 8.0f * valley3;
            /* teal wash pooling in the valleys, faint magenta in the deepest */
            g += 16.0f * valley * SHELL_SSTEP(0.15f, 0.55f, Shell_WrappedNoiseN(&g16[0][0], 16, fx, fy));
            /* pole cap */
            r = r * (1.0f - capMix) + 132.0f * capMix;
            g = g * (1.0f - capMix) + 122.0f * capMix;
            b = b * (1.0f - capMix) + 168.0f * capMix;
            /* soft dither vs banding */
            seed = seed * 1664525u + 1013904223u;
            float dith = ((int)(seed >> 12) % 32) / 32.0f - 0.5f;
            r += dith * 3.0f; g += dith * 3.0f; b += dith * 3.0f;
            if (r > 255.0f) { r = 255.0f; }
            if (r < 0.0f) { r = 0.0f; }
            if (g > 255.0f) { g = 255.0f; }
            if (g < 0.0f) { g = 0.0f; }
            if (b > 255.0f) { b = 255.0f; }
            if (b < 0.0f) { b = 0.0f; }
            px[y * SHELL_TW + x] = (Color){ (unsigned char)r, (unsigned char)g,
                                            (unsigned char)b, 255 };
        }
    }
    #undef SHELL_SSTEP
    /* a sparse dusting of faint stars, never near the pinch */
    for (int k = 0; k < 18; k++) {
        seed = seed * 1664525u + 1013904223u;
        int sx = (int)((seed >> 10) % SHELL_TW);
        seed = seed * 1664525u + 1013904223u;
        int sy = (int)((seed >> 10) % (SHELL_TH / 2)) + SHELL_TH / 3;
        seed = seed * 1664525u + 1013904223u;
        unsigned char tint = (unsigned char)(180 + (seed >> 14) % 60);
        px[sy * SHELL_TW + sx] = (Color){ (unsigned char)(tint * 9 / 10), tint, 255, 255 };
    }
    Image img = { .data = px, .width = SHELL_TW, .height = SHELL_TH,
                  .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    Texture2D t = LoadTextureFromImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    MemFree(px);
    return t;
}

static Texture2D Shell_MakeUnderTexture(void) {
    Color *px = (Color *)MemAlloc(SHELL_TW * SHELL_TH * sizeof(Color));
    unsigned int seed = 192837465u;
    float bg8[9][9], bg16[17][17];
    for (int j = 0; j < 9; j++)
        for (int i = 0; i < 9; i++) {
            seed = seed * 1664525u + 1013904223u;
            bg8[j][i] = (float)(seed >> 16) / 65535.0f;
        }
    for (int j = 0; j < 9; j++) { bg8[j][8] = bg8[j][0]; bg8[8][j] = bg8[0][j]; }
    bg8[8][8] = bg8[0][0];
    for (int j = 0; j < 17; j++)
        for (int i = 0; i < 17; i++) {
            seed = seed * 1664525u + 1013904223u;
            bg16[j][i] = (float)(seed >> 16) / 65535.0f;
        }
    for (int j = 0; j < 17; j++) { bg16[j][16] = bg16[j][0]; bg16[16][j] = bg16[0][j]; }
    bg16[16][16] = bg16[0][0];
    /* v61.2: a smooth deep green-black sky - the old (x/24+y/24)%3 base
     * pattern read as a checkerboard. Now: vertical depth gradient plus
     * soft wrapped-noise drift, no visible tiling. */
    for (int y = 0; y < SHELL_TH; y++) {
        float fy = (float)y / SHELL_TH;
        for (int x = 0; x < SHELL_TW; x++) {
            float fx = (float)x / SHELL_TW;
            float n = Shell_WrappedNoiseN(&bg8[0][0], 8, fx, fy) * 0.7f +
                      Shell_WrappedNoiseN(&bg16[0][0], 16, fx, fy) * 0.3f;
            float dith = (((x * 7 + y * 13) % 8) - 4) / 4.0f;
            float r = 3.0f + 7.0f * n + 3.0f * fy + dith;
            float g = 8.0f + 15.0f * n + 6.0f * fy + dith;
            float b = 6.0f + 11.0f * n + 5.0f * fy + dith;
            if (r < 0.0f) r = 0.0f;
            if (g < 0.0f) g = 0.0f;
            if (b < 0.0f) b = 0.0f;
            px[y * SHELL_TW + x] = (Color){ (unsigned char)r,
                                            (unsigned char)g,
                                            (unsigned char)b, 255 };
        }
    }
    /* emerald nebula glow snaking across - a smooth gradient wash, not a
     * hard line (the old 3-row band read as a crooked circle overhead) */
    for (int x = 0; x < SHELL_TW; x++) {
        float cyN = SHELL_TH / 2.0f + sinf(6.2832f * x / SHELL_TW) * SHELL_TH * 0.16f;
        for (int yy = 0; yy < SHELL_TH; yy++) {
            float d = (yy - cyN) / (SHELL_TH * 0.10f);
            float glow = expf(-d * d) * 0.8f;
            if (glow < 0.02f) continue;
            Color *c = &px[yy * SHELL_TW + x];
            c->r = (unsigned char)(c->r * (1.0f - glow) + 26.0f * glow);
            c->g = (unsigned char)(c->g * (1.0f - glow) + 112.0f * glow);
            c->b = (unsigned char)(c->b * (1.0f - glow) + 70.0f * glow);
        }
    }
    /* alien stars: gold / ice / magenta-white, plus two bright crosses */
    for (int k = 0; k < 460; k++) {
        seed = seed * 1664525u + 1013904223u;
        int sx = (int)((seed >> 10) % SHELL_TW);
        seed = seed * 1664525u + 1013904223u;
        int sy = (int)((seed >> 10) % SHELL_TH);
        seed = seed * 1664525u + 1013904223u;
        unsigned int kind = (seed >> 12) % 100u;
        Color c = kind < 40 ? (Color){ 255, 214, 120, 255 }
                : kind < 78 ? (Color){ 215, 245, 255, 255 }
                            : (Color){ 255, 170, 235, 255 };
        px[sy * SHELL_TW + sx] = c;
    }
    for (int cross = 0; cross < 2; cross++) {
        int cx0 = cross == 0 ? 60 : 190, cy0 = cross == 0 ? 30 : 88;
        px[cy0 * SHELL_TW + cx0] = (Color){ 255, 255, 255, 255 };
        int dxs[4] = { 1, -1, 0, 0 }, dys[4] = { 0, 0, 1, -1 };
        for (int d = 0; d < 4; d++) {
            int xx = cx0 + dxs[d], yy = cy0 + dys[d];
            if (xx >= 0 && xx < SHELL_TW && yy >= 0 && yy < SHELL_TH)
                px[yy * SHELL_TW + xx] = (Color){ 180, 230, 210, 255 };
        }
    }
    Image img = { .data = px, .width = SHELL_TW, .height = SHELL_TH,
                  .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    Texture2D t = LoadTextureFromImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    MemFree(px);
    return t;
}

static void Shell_EnsureTextures(void) {
    if (shellTexReady) return;
    shellTexReady = true;
    shellTopTex = Shell_MakeTopTexture();
    shellUndTex = Shell_MakeUnderTexture();
}

/* ---------------------------- v59: glowmoths ----------------------------
 * v60: the trail budget is platform-aware. Browsers often run WebGL on
 * weaker drivers, so the web build gets half the motes with a slightly
 * wider drop gap - the look stays, the fill cost halves. */
#define MOTH_MAX 10
#ifdef PLATFORM_WEB
#define POLLEN_MAX       144
#define POLLEN_MIN_GAP   0.16    /* seconds between drops (idle moth) */
#define POLLEN_DIST_GAP  0.30f   /* meters between drops (flying moth) */
#else
#define POLLEN_MAX       288
#define POLLEN_MIN_GAP   0.10
#define POLLEN_DIST_GAP  0.22f
#endif
typedef struct Moth {
    bool active;
    Vector3 pos;
    Vector3 vel;
    Vector3 target;
    double targetAt;
    float phase;
    double pollenAt;
    Vector3 lastDrop;      /* v60: where the previous mote was laid */
    bool hasLastDrop;
} Moth;
typedef struct Pollen {
    Vector3 pos;
    Vector3 vel;
    float life;
    float size;
    float shift;
} Pollen;
static Moth moths[MOTH_MAX];
static Pollen pollen[POLLEN_MAX];
static int pollenNext;
static bool mothAnnounced;

int Mobs_MothCount(void) {
    int n = 0;
    for (int i = 0; i < MOTH_MAX; i++) if (moths[i].active) n++;
    return n;
}

/* v59.8: is the straight path from a to b clear of blocks? Step-wise,
 * cheap - it runs a handful of times per second at most. */
static bool Moth_PathClear(Vector3 a, Vector3 b) {
    float d = Vector3Distance(a, b);
    int steps = (int)(d / 0.6f) + 1;
    for (int i = 1; i <= steps; i++) {
        float k = (float)i / steps;
        Vector3 p = { a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k,
                      a.z + (b.z - a.z) * k };
        if (World_GetBlock(p) != 0) return false;
    }
    return true;
}

static void Moth_PickTarget(Moth *m) {
    for (int attempt = 0; attempt < 8; attempt++) {
        float ang = GetRandomValue(0, 3599) * 0.001745f;
        float dist = 6.0f + GetRandomValue(0, 900) / 100.0f;
        Vector3 want = { m->pos.x + cosf(ang) * dist,
                         m->pos.y + (GetRandomValue(-400, 500) / 100.0f),
                         m->pos.z + sinf(ang) * dist };
        if (World_GetBlock(want) != 0) continue;   /* drift through open space only */
        /* v59.8: the whole straight path must be open, not just the end */
        if (!Moth_PathClear(m->pos, want)) continue;
        m->target = want;
        m->targetAt = GetTime() + 7.0 + GetRandomValue(0, 500) / 100.0;
        return;
    }
    m->target = (Vector3){ m->pos.x + GetRandomValue(-600, 600) / 100.0f,
                           m->pos.y + GetRandomValue(-200, 300) / 100.0f,
                           m->pos.z + GetRandomValue(-600, 600) / 100.0f };
    m->targetAt = GetTime() + 5.0;
}

static void Moth_Update(float deltaTime, double now) {
    Vector3 pc = Mob_PlayerCenter();
    for (int i = 0; i < MOTH_MAX; i++) {
        Moth *m = &moths[i];
        if (!m->active) {
            /* roam the islands; appear anywhere around the player */
            if (GetRandomValue(0, 100) < 6) {
                float ang = GetRandomValue(0, 3599) * 0.001745f;
                float dist = 8.0f + GetRandomValue(0, 900) / 100.0f;
                Vector3 want = { pc.x + cosf(ang) * dist, pc.y + GetRandomValue(-200, 500) / 100.0f,
                                 pc.z + sinf(ang) * dist };
                if (World_GetBlock(want) == 0) {
                    m->active = true;
                    m->pos = want;
                    m->vel = (Vector3){ 0 };
                    m->phase = GetRandomValue(0, 628) / 100.0f;
                    m->pollenAt = 0.0;
                    m->hasLastDrop = false;
                    Moth_PickTarget(m);
                    if (!mothAnnounced) {
                        mothAnnounced = true;
                        Chat_AddLine(Tr("Glowmoths shimmer between the islands."));
                    }
                }
            }
            continue;
        }
        if (Vector3Distance(m->pos, pc) > 52.0f) { m->active = false; continue; }

        /* never brush through the player */
        Vector3 away = Vector3Subtract(m->pos, pc);
        float pd = Vector3Length(away);
        if (pd < 2.2f && pd > 0.001f)
            m->vel = Vector3Add(m->vel, Vector3Scale(away, 3.5f * deltaTime / pd));

        if (now >= m->targetAt || Vector3Distance(m->pos, m->target) < 1.0f) Moth_PickTarget(m);
        Vector3 to = Vector3Subtract(m->target, m->pos);
        float td = Vector3Length(to);
        if (td > 0.01f) {
            Vector3 want = Vector3Scale(to, 1.45f / td);
            /* butterfly wobble */
            want.x += sinf(now * 2.3f + m->phase) * 0.55f;
            want.y += sinf(now * 3.1f + m->phase * 2.0f) * 0.40f;
            want.z += cosf(now * 1.9f + m->phase) * 0.55f;
            m->vel = Vector3Lerp(m->vel, want, 1.0f - powf(0.35f, deltaTime));
        }
        /* v59.8: solid air - if the next step lands inside a block, bounce
         * off and pick a fresh open-air target instead of ghosting through
         * islands */
        Vector3 step = Vector3Scale(m->vel, deltaTime);
        Vector3 np = Vector3Add(m->pos, step);
        if (World_GetBlock(np) != 0) {
            m->vel = Vector3Scale(m->vel, -0.3f);
            Moth_PickTarget(m);
        } else {
            m->pos = np;
        }

        /* glowing pollen trails behind the flight.
         * v60: the stream is fed by TWO clocks - time AND distance - so
         * it reads as one continuous line: a hovering moth keeps dropping
         * motes and a fast glide never leaves gaps (the time-only gate
         * thinned the trail whenever the wobble cancelled the chase). */
        bool timeDue = now >= m->pollenAt;
        bool distDue = !m->hasLastDrop ||
                       Vector3Distance(m->pos, m->lastDrop) >= POLLEN_DIST_GAP;
        if (timeDue || distDue) {
            m->pollenAt = now + POLLEN_MIN_GAP + GetRandomValue(0, 6) / 100.0;
            m->lastDrop = m->pos;
            m->hasLastDrop = true;
            Pollen *p = &pollen[pollenNext];
            pollenNext = (pollenNext + 1) % POLLEN_MAX;
            /* v61.1: motes are laid BEHIND and slightly UNDER the flyer -
             * the old drop right at the body center piled a glowing blob
             * on top of the moth and hid it. Slimmer motes, too. */
            Vector3 emit = m->pos;
            float spd = Vector3Length(m->vel);
            if (spd > 0.25f)
                emit = Vector3Add(emit, Vector3Scale(m->vel, -0.16f / spd));
            emit.y -= 0.18f;   /* well clear of the body/wings */
            p->pos = (Vector3){ emit.x + GetRandomValue(-6, 6) / 100.0f,
                                emit.y + GetRandomValue(-5, 5) / 100.0f,
                                emit.z + GetRandomValue(-6, 6) / 100.0f };
            p->vel = (Vector3){ GetRandomValue(-16, 16) / 100.0f,
                                -(14 + GetRandomValue(0, 12)) / 100.0f,
                                GetRandomValue(-16, 16) / 100.0f };
            p->life = 1.8f + GetRandomValue(0, 80) / 100.0f;
            p->size = 0.064f + GetRandomValue(0, 30) / 1000.0f;
            p->shift = GetRandomValue(0, 628) / 100.0f;
        }
    }
    for (int i = 0; i < POLLEN_MAX; i++) {
        Pollen *p = &pollen[i];
        if (p->life <= 0.0f) continue;
        p->life -= deltaTime;
        p->vel.y -= 0.06f * deltaTime;   /* v59.8: dust hangs, it does not sink */
        p->pos.x += p->vel.x * deltaTime * 0.4f + sinf(now * 1.7f + p->shift) * deltaTime * 0.12f;
        p->pos.y += p->vel.y * deltaTime;
        p->pos.z += p->vel.z * deltaTime * 0.4f;
    }
}

/* v59.2: a moth is a real little body - a four-sided cone - with two
 * textured wings flapping on its sides. Everything is emitted into the
 * caller's atlas batch; tile 24 (pure white) carries the vertex colors. */
static void Moth_Draw(double now) {
    Matrix view = rlGetMatrixModelview();
    Vector3 right = Vector3Normalize((Vector3){ view.m0, view.m4, view.m8 });
    for (int i = 0; i < MOTH_MAX; i++) {
        Moth *m = &moths[i];
        if (!m->active) continue;
        float pulse = 0.80f + 0.20f * sinf(now * 3.0f + m->phase);
        unsigned char br = (unsigned char)(225.0f * pulse);
        unsigned char bg = (unsigned char)(250.0f * pulse);
        unsigned char bb = (unsigned char)(240.0f * pulse);
        float u24 = (24 % 16) / 16.0f, v24 = (24 / 16) / 16.0f;
        float uW = (42 % 16) / 16.0f, vW = (42 / 16) / 16.0f;

        /* ---- body: tiny 4-sided cone, apex up ---- */
        Vector3 apex = { m->pos.x, m->pos.y + 0.10f, m->pos.z };
        Vector3 base[4];
        float br0 = 0.065f;
        for (int k = 0; k < 4; k++) {
            float an = 6.2832f * k / 4.0f + 0.7854f;
            base[k] = (Vector3){ m->pos.x + cosf(an) * br0, m->pos.y - 0.08f,
                                 m->pos.z + sinf(an) * br0 };
        }
        for (int k = 0; k < 4; k++) {
            Vector3 a = base[k], b = base[(k + 1) % 4];
            /* v59.5: each cone side is two 4-vertex quads (tri + repeated
             * corner degenerate). The old 3+3 triangles left half-quads
             * that sheared into the next emitter's vertices. */
            rlColor4ub(br, bg, bb, 255);
            rlTexCoord2f(u24, v24 + 1.0f / 16.0f); rlVertex3f(a.x, a.y, a.z);
            rlTexCoord2f(u24 + 1.0f / 16.0f, v24 + 1.0f / 16.0f); rlVertex3f(b.x, b.y, b.z);
            rlTexCoord2f(u24 + 0.5f / 16.0f, v24); rlVertex3f(apex.x, apex.y, apex.z);
            rlTexCoord2f(u24 + 0.5f / 16.0f, v24); rlVertex3f(apex.x, apex.y, apex.z);
            /* inside, darker */
            rlColor4ub((unsigned char)(br * 3 / 4), (unsigned char)(bg * 3 / 4),
                       (unsigned char)(bb * 3 / 4), 255);
            rlTexCoord2f(u24 + 0.5f / 16.0f, v24); rlVertex3f(apex.x, apex.y, apex.z);
            rlTexCoord2f(u24 + 1.0f / 16.0f, v24 + 1.0f / 16.0f); rlVertex3f(b.x, b.y, b.z);
            rlTexCoord2f(u24, v24 + 1.0f / 16.0f); rlVertex3f(a.x, a.y, a.z);
            rlTexCoord2f(u24, v24 + 1.0f / 16.0f); rlVertex3f(a.x, a.y, a.z);
        }

        /* ---- wings: two quads hinged at the body, flapping hard ---- */
        float flap = sinf(now * 11.0f + m->phase);
        float lift = flap * 0.22f;             /* tip rises/falls */
        float span = 0.26f * (0.75f + 0.25f * fabsf(flap));  /* foreshorten */
        float u0 = uW, u1 = uW + 1.0f / 16.0f, v0 = vW, v1 = vW + 1.0f / 16.0f;
        for (int side = 0; side < 2; side++) {
            float sgn = side == 0 ? 1.0f : -1.0f;
            Vector3 inLo  = { m->pos.x,                    m->pos.y - 0.015f, m->pos.z };
            Vector3 inHi  = { m->pos.x,                    m->pos.y + 0.035f, m->pos.z };
            Vector3 outLo = { m->pos.x + right.x * span * sgn, m->pos.y - 0.03f + lift * 0.4f,
                              m->pos.z + right.z * span * sgn };
            Vector3 outHi = { m->pos.x + right.x * span * 1.08f * sgn, m->pos.y + 0.05f + lift,
                              m->pos.z + right.z * span * 1.08f * sgn };
            rlColor4ub(br, bg, bb, 255);
            rlTexCoord2f(u0, v1); rlVertex3f(inLo.x, inLo.y, inLo.z);
            rlTexCoord2f(u0, v0); rlVertex3f(inHi.x, inHi.y, inHi.z);
            rlTexCoord2f(u1, v0); rlVertex3f(outHi.x, outHi.y, outHi.z);
            rlTexCoord2f(u1, v1); rlVertex3f(outLo.x, outLo.y, outLo.z);
            /* reverse winding */
            rlTexCoord2f(u1, v1); rlVertex3f(outLo.x, outLo.y, outLo.z);
            rlTexCoord2f(u1, v0); rlVertex3f(outHi.x, outHi.y, outHi.z);
            rlTexCoord2f(u0, v0); rlVertex3f(inHi.x, inHi.y, inHi.z);
            rlTexCoord2f(u0, v1); rlVertex3f(inLo.x, inLo.y, inLo.z);
        }
    }
}

/* v61: pollen - REWRITTEN to be impossible to lose again.
 *
 * History: v59.2 drew each mote as ONE camera-facing quad with a single
 * winding. World_Draw leaves backface culling enabled session-wide
 * (v43.5 water fix; see the v56 note), and that winding faced AWAY from
 * the camera - so the GPU silently culled every mote and the trail
 * read as "gone" even though the geometry, alpha and atlas were fine.
 * (The v59.0 "rainbow lines" were the same quads mis-indexed by the
 * 7-vertex emission bug - a different failure at the same spot.)
 *
 * The new draw closes every door that could hide it:
 *   - it runs on rlgl's DEFAULT white texture through the PLAIN
 *     pipeline (outside the cutout sprite batch) - immune to atlas
 *     content, server texture packs and the alpha<0.5 discard;
 *   - every quad is emitted in BOTH windings - face culling can never
 *     eat it, whichever side the driver considers front;
 *   - the mote color IS the RGB shift: three stacked translucent quads
 *     with phase-shifted cycle colors, offset around a small slowly
 *     spinning ring in the view plane (radial chromatic aberration). */
static void Moth_PollenDraw(double now) {
    Matrix view = rlGetMatrixModelview();
    Vector3 right = Vector3Normalize((Vector3){ view.m0, view.m4, view.m8 });
    Vector3 up = Vector3Normalize((Vector3){ view.m1, view.m5, view.m9 });
    for (int i = 0; i < POLLEN_MAX; i++) {
        Pollen *p = &pollen[i];
        if (p->life <= 0.0f) continue;
        /* a mote stays ITSELF until it dies: hold a bright core, ease
         * out only at the very end, keep most of the size while dying */
        float k = Clamp(p->life, 0.0f, 1.0f);
        float ease = k * k * (3.0f - 2.0f * k);
        float twinkle = 0.92f + 0.08f * sinf(now * 3.4f + p->shift * 3.0f);
        unsigned char alpha = (unsigned char)(235.0f * (0.60f + 0.40f * ease) * twinkle);
        float s = p->size * (0.85f + 0.25f * ease);
        Vector3 rx = Vector3Scale(right, s), uy = Vector3Scale(up, s);
        for (int ch = 0; ch < 3; ch++) {
            float ph = p->shift + ch * 2.094f;
            unsigned char r = (unsigned char)(127.0f + 127.0f * sinf(now * 2.6f + ph));
            unsigned char g = (unsigned char)(127.0f + 127.0f * sinf(now * 2.6f + ph + 2.094f));
            unsigned char bc = (unsigned char)(127.0f + 127.0f * sinf(now * 2.6f + ph + 4.188f));
            /* radial chromatic aberration: the three color layers ride a
             * slowly spinning ring instead of one flat horizontal split */
            float an = now * 0.7f + 6.2832f * ch / 3.0f + p->shift * 0.15f;
            Vector3 c = Vector3Add(p->pos,
                Vector3Add(Vector3Scale(right, cosf(an) * s * 0.55f),
                           Vector3Scale(up, sinf(an) * s * 0.55f)));
            Vector3 a = Vector3Subtract(Vector3Subtract(c, rx), uy);
            Vector3 b2 = Vector3Add(Vector3Subtract(c, rx), uy);
            Vector3 d2 = Vector3Add(Vector3Add(c, rx), uy);
            Vector3 e = Vector3Subtract(Vector3Add(c, rx), uy);
            rlColor4ub(r, g, bc, alpha);
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(a.x, a.y, a.z);
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(b2.x, b2.y, b2.z);
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(d2.x, d2.y, d2.z);
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(e.x, e.y, e.z);
            /* reverse winding: never a back face, whatever the state */
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(e.x, e.y, e.z);
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(d2.x, d2.y, d2.z);
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(b2.x, b2.y, b2.z);
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(a.x, a.y, a.z);
        }
    }
}


/* ---------------------------------------------------------------- grazers */
/* v62: meadow grazers - shy textured herbivores that eat flowers and
 * replant them elsewhere (the meadow slowly rearranges itself around
 * them). They are NOT wireframe: furred blob body, ears, legs, tail,
 * dark beady eyes - all atlas-shaded. Flees the player, harmless. */
#define GRAZER_MAX 3
#define GRAZER_HP 2
typedef struct Grazer {
    bool active;
    Vector3 pos;
    float velY;
    float phase;
    float faceAng;
    int state;             /* 0 wander, 1 seek flower, 2 eat, 3 flee, 4 sleep */
    float stateTimer;
    Vector3 walkTarget;
    Vector3 targetCell;    /* flower being eaten */
    int eats;
    float age;
    bool grounded;
    unsigned char tint;    /* per-instance fur tint */
    float gaitPhase;       /* v63: advances with actual distance walked */
    float speedSm;         /* v63: smoothed speed -> gait amplitude */
} Grazer;
static Grazer grazers[GRAZER_MAX];
static bool grazerAnnounced;

int Mobs_GrazerCount(void) {
    int n = 0;
    for (int i = 0; i < GRAZER_MAX; i++) if (grazers[i].active) n++;
    return n;
}

/* flowers a grazer will eat (visual flora, not plain grass) */
static bool Grazer_IsDelicacy(int id) {
    return id == 12 || id == 13 || id == 28 || id == 29 || id == 30 ||
           id == 31 || id == 33 || id == 37 || id == 38 ||
           id == 45 || id == 46 || id == 47 || id == 48 || id == 52;
}

static bool Grazer_Reachable(Vector3 from, Vector3 flower);

/* scan the neighborhood for a flower cell the grazer can actually
 * reach (not behind a ledge, not floating two blocks up) */
static bool Grazer_FindFlower(Vector3 center, Vector3 *out) {
    int cx = (int)floorf(center.x), cy = (int)floorf(center.y), cz = (int)floorf(center.z);
    for (int dy = 1; dy >= -1; dy--)
        for (int dz = -6; dz <= 6; dz++)
            for (int dx = -6; dx <= 6; dx++) {
                Vector3 cell = { cx + dx, cy + dy, cz + dz };
                if (!Grazer_IsDelicacy(World_GetBlock(cell))) continue;
                if (!Grazer_Reachable(center, cell)) continue;
                *out = cell;
                return true;
            }
    return false;
}

/* v63: solid ground only - sprite flora and other grazers are NOT
 * standable, so a grazer can never perch on a plant */
static bool Grazer_SolidAt(Vector3 p) {
    int id = World_GetBlock(p);
    return id > 0 && blockDefinitions[id].colliderType == BLOCK_COLLIDER_SOLID;
}

static float Grazer_GroundY(Vector3 p) {
    int px = (int)floorf(p.x), pz = (int)floorf(p.z);
    for (int y = (int)floorf(p.y) + 1; y >= (int)floorf(p.y) - 3; y--) {
        Vector3 probe = { px, y - 0.5f, pz };
        if (Grazer_SolidAt(probe)) return (float)y;
    }
    return p.y - 10.0f;   /* no ground: caller treats as "keep flying"? never - grounded mob */
}

/* v63: is the straight hop from the grazer to this flower free of
 * walls? Rejects eating "through" a block when the flower sits a
 * level up behind a ledge */
static bool Grazer_Reachable(Vector3 from, Vector3 flower) {
    if (flower.y - from.y > 1.3f) return false;   /* too far above */
    if (flower.y - from.y <= 0.3f) return true;   /* same level: fine */
    int cx = (int)floorf(from.x), cy = (int)floorf(from.y), cz = (int)floorf(from.z);
    int fx = (int)floorf(flower.x), fz = (int)floorf(flower.z);
    int sx = (fx > cx) - (fx < cx), sz = (fz > cz) - (fz < cz);
    int steps = (abs(fx - cx) > abs(fz - cz)) ? abs(fx - cx) : abs(fz - cz);
    if (steps < 1) steps = 1;
    if (steps > 6) steps = 6;
    for (int t = 1; t <= steps; t++) {
        float k = (float)t / steps;
        Vector3 a = { cx + 0.5f + (fx - cx) * k, cy + 0.5f, cz + 0.5f + (fz - cz) * k };
        Vector3 b = { a.x, cy + 1.5f, a.z };
        if (Grazer_SolidAt(a) || Grazer_SolidAt(b)) return false;
    }
    return true;
}

/* a fresh flower grown from seeds: any nearby cell with turf below */
static void Grazer_PlantSeed(Vector3 at) {
    for (int attempt = 0; attempt < 8; attempt++) {
        float ang = GetRandomValue(0, 3599) * 0.001745f;
        float dist = 1.5f + GetRandomValue(0, 250) / 100.0f;
        int bx = (int)floorf(at.x + cosf(ang) * dist);
        int bz = (int)floorf(at.z + sinf(ang) * dist);
        for (int y = (int)floorf(at.y) + 2; y >= (int)floorf(at.y) - 4; y--) {
            Vector3 below = { bx, y - 1, bz };
            Vector3 cell = { bx, y, bz };
            int ground = World_GetBlock(below);
            if ((ground == 3 || ground == 57 || ground == 58) &&
                World_GetBlock(cell) == 0) {
                int species;
                if (ground == 57) {          /* ember isle: warm flora */
                    switch (GetRandomValue(0, 2)) {
                        case 0: species = 46; break;   /* embercup */
                        case 1: species = 31; break;   /* twin tulip */
                        default: species = 33; break;  /* lanternberry */
                    }
                } else if (ground == 58) {   /* frost isle: pale flora */
                    switch (GetRandomValue(0, 2)) {
                        case 0: species = 48; break;   /* frostfern */
                        case 1: species = 45; break;   /* glassbell */
                        default: species = 52; break;  /* void puff */
                    }
                } else {                     /* classic crystal meadow */
                    switch (GetRandomValue(0, 4)) {
                        case 0: species = 29; break;
                        case 1: species = 28; break;
                        case 2: species = 45; break;
                        case 3: species = 52; break;
                        default: species = 30; break;
                    }
                }
                World_SetBlock(cell, species, true);
                return;
            }
            if (World_GetBlock(below) != 0) break;
        }
    }
}

static void Grazer_SpawnTry(void) {
    if (Mobs_GrazerCount() >= 2) return;
    Vector3 center = Mob_PlayerCenter();
    Vector3 spot;
    for (int attempt = 0; attempt < 6; attempt++) {
        if (!Mob_FindSurfaceSpot(center, 9.0f, 20.0f, &spot)) return;
        bool crowded = false;
        for (int i = 0; i < GRAZER_MAX; i++)
            if (grazers[i].active && Vector3Distance(grazers[i].pos, spot) < 3.0f)
                crowded = true;
        if (!crowded) break;
        if (attempt == 5) return;
    }
    if (Mobs_InStarterSanctuary(spot)) return;
    for (int i = 0; i < GRAZER_MAX; i++) {
        Grazer *g = &grazers[i];
        if (g->active) continue;
        g->active = true;
        g->pos = spot;
        g->velY = 0.0f;
        g->phase = GetRandomValue(0, 628) / 100.0f;
        g->faceAng = GetRandomValue(0, 3599) * 0.001745f;
        g->state = 0;
        g->stateTimer = 1.0f + GetRandomValue(0, 200) / 100.0f;
        g->targetCell = (Vector3){ 0 };
        g->eats = 0;
        g->age = 0.0f;
        g->grounded = true;
        g->tint = (unsigned char)(200 + GetRandomValue(0, 40));
        if (!grazerAnnounced) {
            grazerAnnounced = true;
            Chat_AddLine(Tr("Something small is nibbling the meadow flowers."));
        }
        return;
    }
}

static void Grazer_Update(float deltaTime, double now) {
    Vector3 pc = Mob_PlayerCenter();
    for (int i = 0; i < GRAZER_MAX; i++) {
        Grazer *g = &grazers[i];
        if (!g->active) continue;
        g->age += deltaTime;
        float pd = Vector3Distance(g->pos, pc);
        if (g->age > 240.0f || pd > 48.0f) { g->active = false; continue; }

        /* flee check overrides everything but eating's last bite;
         * sleep is shallower - a grazer wakes from further away */
        if (pd < (g->state == 4 ? 5.5f : 3.4f) && g->state != 3) {
            g->state = 3;
            g->stateTimer = 1.7f;
        }

        Vector3 move = { 0 };
        if (g->state == 0) {                                   /* wander */
            g->stateTimer -= deltaTime;
            move = Vector3Subtract(g->walkTarget, g->pos);
            move.y = 0;
            if (g->stateTimer <= 0.0f || Vector3Length(move) < 0.4f) {
                float ang = GetRandomValue(0, 3599) * 0.001745f;
                float dist = 1.5f + GetRandomValue(0, 350) / 100.0f;
                g->walkTarget = Vector3Add(g->pos, V3_(cosf(ang) * dist, 0, sinf(ang) * dist));
                g->stateTimer = 2.0f + GetRandomValue(0, 250) / 100.0f;
            }
            /* sleepy? a full grazer dozes off right there */
            if (g->eats > 0 && GetRandomValue(0, 999) < 8) {
                g->state = 4;
                g->stateTimer = 7.0f + GetRandomValue(0, 800) / 100.0f;
                continue;
            }
            /* hungry? look for flowers */
            Vector3 flower;
            if (GetRandomValue(0, 100) < 3 && Grazer_FindFlower(g->pos, &flower)) {
                g->targetCell = flower;
                g->state = 1;
            }
        } else if (g->state == 1) {                            /* seek flower */
            move = Vector3Subtract(g->targetCell, g->pos);
            move.y = 0;
            float d = Vector3Length(move);
            if (d < 1.15f) {
                if (!Grazer_Reachable(g->pos, g->targetCell)) {
                    g->state = 0;          /* ledge between us: give up */
                    g->stateTimer = 1.2f;
                } else {
                    g->state = 2;
                    g->stateTimer = 2.6f;
                }
            } else if (d > 9.0f) {
                g->state = 0;   /* flower got eaten by someone else */
            }
        } else if (g->state == 2) {                            /* eat */
            g->stateTimer -= deltaTime;
            if (g->stateTimer <= 0.0f) {
                Vector3 cell = g->targetCell;
                int ate = World_GetBlock(cell);
                if (Grazer_IsDelicacy(ate)) {
                    World_SetBlock(cell, 0, true);
                    Particle_SpawnImpact(Vector3Add(cell, V3_(0.5f, 0.4f, 0.5f)));
                    g->eats++;
                    if (g->eats % 2 == 0) Grazer_PlantSeed(g->pos);
                }
                g->state = 0;
                g->stateTimer = 1.0f + GetRandomValue(0, 150) / 100.0f;
                /* v63: after a meal, a nap in the sun */
                if (GetRandomValue(0, 99) < 30) {
                    g->state = 4;
                    g->stateTimer = 7.0f + GetRandomValue(0, 800) / 100.0f;
                }
            }
        } else if (g->state == 4) {                            /* sleep */
            g->stateTimer -= deltaTime;
            if (g->stateTimer <= 0.0f) {
                g->state = 0;
                g->stateTimer = 0.5f;
                float ang = GetRandomValue(0, 3599) * 0.001745f;
                g->walkTarget = Vector3Add(g->pos, V3_(cosf(ang) * 2.0f, 0, sinf(ang) * 2.0f));
            }
            continue;   /* no movement while asleep */
        } else {                                               /* flee */
            g->stateTimer -= deltaTime;
            Vector3 away = Vector3Subtract(g->pos, pc);
            away.y = 0;
            float ad = Vector3Length(away);
            if (ad > 0.05f) move = Vector3Scale(away, 1.0f / ad);
            if (g->stateTimer <= 0.0f) g->state = 0;
        }

        /* walk + tiny hops over single blocks */
        float speed = g->state == 3 ? 3.1f : (g->state == 1 ? 1.5f : 0.8f);
        float walked = 0.0f;
        if (Vector3Length(move) > 0.05f) {
            move = Vector3Scale(Vector3Normalize(move), speed);
            g->faceAng = atan2f(move.z, move.x);
            Vector3 step = Vector3Add(g->pos, Vector3Scale(move, deltaTime));
            float groundY = Grazer_GroundY(step);
            if (groundY - g->pos.y <= 1.05f) {
                Vector3 flat = { step.x - g->pos.x, 0, step.z - g->pos.z };
                walked = Vector3Length(flat);
                step.y = groundY + 0.36f;
                g->pos = step;
            }   /* else: blocked, keep walking in place this frame */
        }
        /* v63: gait phase follows real footsteps - legs freeze when idle */
        g->gaitPhase += walked * 4.4f;
        g->speedSm += (walked / (deltaTime > 0.0001f ? deltaTime : 0.016f) - g->speedSm) *
                      (1.0f - powf(0.001f, deltaTime));
        /* v63: grazers never overlap each other */
        for (int j = 0; j < GRAZER_MAX; j++) {
            Grazer *o = &grazers[j];
            if (o == g || !o->active) continue;
            Vector3 away = Vector3Subtract(g->pos, o->pos);
            away.y = 0;
            float ad = Vector3Length(away);
            if (ad < 1.3f && ad > 0.001f) {
                Vector3 push = Vector3Scale(Vector3Normalize(away), (1.3f - ad) * 0.5f);
                g->pos = Vector3Add(g->pos, push);
            }
        }
        /* settle onto ground */
        float groundHere = Grazer_GroundY(g->pos);
        g->pos.y += (groundHere + 0.36f - g->pos.y) * (1.0f - powf(0.0001f, deltaTime));
        /* never sink under the world */
        if (isnan(g->pos.x) || isnan(g->pos.y) || isnan(g->pos.z)) g->active = false;
    }
}

static void Mob_TexturedBlob(Vector3 c2, float rx, float ry, float rz, float faceAng,
                             int tile, unsigned char r, unsigned char g, unsigned char b);

/* textured grazer body - everything is shaded atlas blobs */
static void Grazer_Draw(double now) {
    for (int i = 0; i < GRAZER_MAX; i++) {
        Grazer *g = &grazers[i];
        if (!g->active) continue;
        int sleeping = g->state == 4;
        float gaitAmp = sleeping ? 0.0f : (g->speedSm < 0.05f ? 0.0f : (g->speedSm < 1.5f ? g->speedSm / 1.5f : 1.0f));
        float bob = sleeping ? -0.10f + sinf(now * 1.6f + g->phase) * 0.012f
                             : sinf(g->gaitPhase * 2.0f) * 0.020f * gaitAmp;
        float chew = (g->state == 2) ? sinf(now * 14.0f) * 0.05f : 0.0f;
        Vector3 bodyC = { g->pos.x, g->pos.y + bob, g->pos.z };
        float ca = cosf(g->faceAng), sa = sinf(g->faceAng);
        unsigned char t = g->tint;
        /* body + head */
        Mob_TexturedBlob(bodyC, 0.40f, 0.30f, 0.36f, g->faceAng, 55, t, (unsigned char)(t * 92 / 100), (unsigned char)(t * 80 / 100));
        Vector3 head = { bodyC.x + ca * 0.44f, bodyC.y + 0.20f + chew, bodyC.z + sa * 0.44f };
        Mob_TexturedBlob(head, 0.20f, 0.18f, 0.19f, g->faceAng, 55,
                         (unsigned char)(t * 106 / 100), (unsigned char)(t * 100 / 100), (unsigned char)(t * 88 / 100));
        /* ears (droop when sleeping) */
        float earPh = sinf(now * 2.3f + g->phase) * 0.03f;
        float earY = sleeping ? 0.10f : 0.26f;
        Vector3 e1 = { head.x + ca * 0.06f - sa * 0.15f, head.y + earY + earPh, head.z + sa * 0.06f + ca * 0.15f };
        Vector3 e2 = { head.x + ca * 0.06f + sa * 0.15f, head.y + earY - earPh, head.z + sa * 0.06f - ca * 0.15f };
        Mob_TexturedBlob(e1, 0.05f, sleeping ? 0.11f : 0.14f, 0.05f, g->faceAng, 61, (unsigned char)(t * 84 / 100), (unsigned char)(t * 76 / 100), (unsigned char)(t * 70 / 100));
        Mob_TexturedBlob(e2, 0.05f, sleeping ? 0.11f : 0.14f, 0.05f, g->faceAng, 61, (unsigned char)(t * 84 / 100), (unsigned char)(t * 76 / 100), (unsigned char)(t * 70 / 100));
        /* eyes: open beady dots, or closed dark threads while asleep */
        for (int eye = 0; eye < 2; eye++) {
            float s = eye == 0 ? 1.0f : -1.0f;
            Vector3 ep = { head.x + ca * 0.13f - sa * s * 0.12f,
                           head.y + 0.05f,
                           head.z + sa * 0.13f + ca * s * 0.12f };
            if (sleeping)
                Mob_TexturedBlob(ep, 0.050f, 0.011f, 0.014f, g->faceAng, 36, 16, 14, 20);
            else
                Mob_TexturedBlob(ep, 0.035f, 0.035f, 0.035f, g->faceAng, 36, 16, 14, 20);
        }
        /* tail puff (curls around the flank while sleeping) */
        Vector3 tail = sleeping
            ? (Vector3){ bodyC.x + ca * 0.30f - sa * 0.20f, bodyC.y - 0.12f, bodyC.z + sa * 0.30f + ca * 0.20f }
            : (Vector3){ bodyC.x - ca * 0.42f, bodyC.y + 0.06f, bodyC.z - sa * 0.42f };
        Mob_TexturedBlob(tail, 0.10f, 0.10f, 0.10f, g->faceAng, 61,
                         (unsigned char)(t * 112 / 100), (unsigned char)(t * 108 / 100), (unsigned char)(t * 100 / 100));
        /* four stubby legs rooted UNDER the body (no gap) - gait only
         * while actually walking; tucked away entirely in sleep */
        if (!sleeping) {
            for (int leg = 0; leg < 4; leg++) {
                float s = (leg % 2 == 0) ? 1.0f : -1.0f;
                float ph = sinf(g->gaitPhase + leg * 1.57f) * 0.050f * gaitAmp;
                float fore = (leg < 2) ? 0.22f : -0.22f;
                Vector3 lp = { bodyC.x + ca * fore - sa * s * 0.20f,
                               bodyC.y - 0.26f + ph,
                               bodyC.z + sa * fore + ca * s * 0.20f };
                Mob_TexturedBlob(lp, 0.055f, 0.125f, 0.055f, g->faceAng, 61,
                                 (unsigned char)(t * 74 / 100), (unsigned char)(t * 66 / 100), (unsigned char)(t * 60 / 100));
            }
        }
    }
}

/* v59.6: one shared sprite batch for every flora/moth rendering. The
 * stock rlgl shader has no alpha cutout, so a sprite's TRANSPARENT
 * texels wrote depth and punched invisible holes in whatever was drawn
 * after them (flowers vanishing when you looked through another
 * flower's quad). This batch runs a shader that discards alpha < 0.5
 * and keeps the depth mask off - sprites can no longer clip anything. */
static Shader spriteCutShader;
static bool spriteCutReady = false;
static const char *spriteCutVs =
    "#version 330\n"
    "in vec3 vertexPosition;"
    "in vec2 vertexTexCoord;"
    "in vec4 vertexColor;"
    "uniform mat4 mvp;"
    "out vec2 fragTexCoord;"
    "out vec4 fragColor;"
    "void main() {"
    "    fragTexCoord = vertexTexCoord;"
    "    fragColor = vertexColor;"
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);"
    "}";
static const char *spriteCutFs =
    "#version 330\n"
    "in vec2 fragTexCoord;"
    "in vec4 fragColor;"
    "uniform sampler2D texture0;"
    "uniform vec4 colDiffuse;"
    "out vec4 finalColor;"
    "void main() {"
    "    vec4 texelColor = texture(texture0, fragTexCoord);"
    "    if (texelColor.a < 0.5) discard;"
    "    finalColor = texelColor * colDiffuse * fragColor;"
    "}";

void Mobs_SpriteBatchBegin(Texture2D atlas) {
    if (!spriteCutReady) {
        spriteCutReady = true;
        spriteCutShader = LoadShaderFromMemory(spriteCutVs, spriteCutFs);
    }
    BeginShaderMode(spriteCutShader);   /* flushes whatever batch was open */
    rlDisableDepthMask();
    /* v61: billboards are double-sided by nature - with the session's
     * backface culling left ON after World_Draw, a quad whose winding
     * faces away from the camera simply vanished (this is what silently
     * ate the single-winded pollen quads). Culling stays off for the
     * whole sprite batch; Mobs_SpriteBatchEnd restores the session norm. */
    rlDisableBackfaceCulling();
    rlBegin(RL_QUADS);                  /* begin first: rlBegin stamps the
                                         * fresh record with the DEFAULT
                                         * texture when it switches modes */
    rlSetTexture(atlas.id);
}

void Mobs_SpriteBatchEnd(void) {
    rlEnd();
    rlDrawRenderBatchActive();
    rlSetTexture(0);
    rlEnableBackfaceCulling();   /* v61: session norm is culling ON */
    rlEnableDepthMask();
    EndShaderMode();
}

/* violet shell event: a shimmering dome parks over a nearby island and
 * rains void spores; mushrooms sprout on plain dirt while it rains */
static bool shellActive;
static double shellUntil;
static Vector3 shellCenter;
static double shellEventAt = 0.0;
static double mushGrowTimer;
static bool shellAnnounced;

int Mobs_GetMushrooms(void) { return mushroomsStored; }

/* v53: restore the stored count from cosmic_progress.ini */
void Mobs_SetMushrooms(int n) {
    mushroomsStored = (n < 0) ? 0 : n;
}

int Mobs_SpiderCount(void) {
    int n = 0;
    for (int i = 0; i < SPIDER_MAX; i++) if (spiders[i].active) n++;
    return n;
}

bool Mobs_EatMushroom(void) {
    if (mushroomsStored <= 0 || player.hp >= 10) return false;
    mushroomsStored--;
    Player_Heal(3);
    SoundFx_PlayWebAttach();
    Chat_AddLine(TextFormat("The mushroom hums warmly. HP %d/10. Left: %d.", player.hp, mushroomsStored));
    return true;
}

bool Mobs_TryCollectMushroom(void) {
    Vector3 pc = Mob_PlayerCenter();
    for (int i = 0; i < MUSH_MAX; i++) {
        Mushroom *m = &mushrooms[i];
        if (!m->active) continue;
        if (Vector3Distance(m->pos, pc) < 1.7f) {
            m->active = false;
            mushroomsStored++;
            Player_HotbarAutoAdd(27);   /* v58: quick slot picks it up */
            SoundFx_PlayPlace();
            if (!mushHintShown) {
                mushHintShown = true;
                Chat_AddLine(Tr("Void mushroom stored. Press G to eat it (+3 HP)."));
            } else {
                Chat_AddLine(TextFormat("Void mushroom stored (%d).", mushroomsStored));
            }
            return true;
        }
    }
    return false;
}

/* v54: never two mushrooms on the same block */
static bool Mushroom_CellTaken(int bx, int by, int bz) {
    for (int i = 0; i < MUSH_MAX; i++) {
        Mushroom *m = &mushrooms[i];
        if (!m->active) continue;
        if (fabsf(m->pos.x - (bx + 0.5f)) < 0.5f &&
            fabsf(m->pos.z - (bz + 0.5f)) < 0.5f &&
            fabsf(m->pos.y - (by + 1.0f)) < 0.5f) return true;
    }
    return false;
}

static void Mushroom_SpawnTry(Vector3 shellC) {
    for (int attempt = 0; attempt < 4; attempt++) {
        float ang = (float)GetRandomValue(0, 3599) * 0.001745f;
        float rad = sqrtf((float)GetRandomValue(0, 1000) / 1000.0f) * 8.5f;
        int bx = (int)floorf(shellC.x + cosf(ang) * rad);
        int bz = (int)floorf(shellC.z + sinf(ang) * rad);
        for (int y = 150; y >= 2; y--) {
            Vector3 p = { bx, y, bz };
            int id = World_GetBlock(p);
            if (id == 0) continue;
            /* v52 fix: island tops are GRASS (3) over dirt (2) - the old
             * dirt-only check broke every scan instantly: zero mushrooms */
            if (id != 2 && id != 3) break;
            Vector3 above = { bx, y + 1, bz };
            if (World_GetBlock(above) != 0) break;
            if (Mushroom_CellTaken(bx, y, bz)) break;   /* v54: cell busy */
            for (int i = 0; i < MUSH_MAX; i++) {
                Mushroom *m = &mushrooms[i];
                if (m->active) continue;
                m->active = true;
                m->pos = (Vector3){ bx + 0.5f, y + 1.0f, bz + 0.5f };
                m->expireAt = (double)GetTime() + 600.0;
                m->scale = 0.8f + (float)GetRandomValue(0, 50) / 100.0f;
                return;
            }
            return;
        }
    }
}

static void Shell_Update(float deltaTime, double now) {
    if (!shellActive) {
        if (shellEventAt <= 0.0) shellEventAt = now + 75.0;
        if (now >= shellEventAt) {
            Vector3 spot;
            if (Mob_FindSurfaceSpot(Mob_PlayerCenter(), 14.0f, 26.0f, &spot)) {
                shellCenter = (Vector3){ spot.x, spot.y + 11.0f, spot.z };
                shellActive = true;
                shellUntil = now + 80.0;
                mushGrowTimer = 1.0f;
                /* v56: a few mushrooms at once so the rain is visible
                 * immediately, not a minute later */
                for (int q = 0; q < 3; q++) Mushroom_SpawnTry(shellCenter);
                if (!shellAnnounced) {
                    shellAnnounced = true;
                    Chat_AddLine(Tr("A violet shell shimmers over the islands... it is raining light."));
                } else {
                    Chat_AddLine(Tr("The violet shell returns."));
                }
                SoundFx_PlayTeleport();
            } else {
                shellEventAt = now + 30.0;
            }
        }
        return;
    }

    if (now >= shellUntil) {
        shellActive = false;
        shellEventAt = now + 210.0 + (double)GetRandomValue(0, 180);
        Chat_AddLine(Tr("The shell folds away into the nebula."));
        return;
    }

    mushGrowTimer -= deltaTime;
    if (mushGrowTimer <= 0.0f) {
        mushGrowTimer = 0.7f;
        Mushroom_SpawnTry(shellCenter);
    }
}

static void Mushrooms_Update(double now) {
    for (int i = 0; i < MUSH_MAX; i++) {
        Mushroom *m = &mushrooms[i];
        if (m->active && now > m->expireAt) m->active = false;
    }
}

void Mobs_ExplosionDamage(Vector3 center, float radius, int damage) {
    for (int i = 0; i < CRAWLER_MAX; i++) {
        Crawler *c = &crawlers[i];
        if (!c->active) continue;
        if (Vector3Distance(c->pos, center) > radius) continue;
        c->hp -= (float)damage;
        c->vel.y += 1.2f;
        Particle_SpawnImpact(c->pos);
        if (c->hp <= 0.0f) {
            c->active = false;
            Hunter_WireBurst(c->pos);
            Particle_SpawnImpact(c->pos);
            Hunter_DropShards(c->pos, 1);
            SoundFx_PlayHunterDie();
        }
    }
    for (int i = 0; i < WISP_MAX; i++) {
        Wisp *w = &wisps[i];
        if (!w->active) continue;
        if (Vector3Distance(w->pos, center) > radius) continue;
        w->active = false;
        Hunter_WireBurst(w->pos);
        Particle_SpawnImpact(w->pos);
        Hunter_DropShards(w->pos, 2);
        SoundFx_PlayHunterDie();
    }
    for (int i = 0; i < SPIDER_MAX; i++) {
        Spider *s = &spiders[i];
        if (!s->active) continue;
        if (Vector3Distance(s->pos, center) > radius) continue;
        s->hp -= (float)damage;
        s->vel.y += 1.2f;
        Particle_SpawnImpact(s->pos);
        if (s->hp <= 0.0f) {
            s->active = false;
            Hunter_WireBurst(s->pos);
            Particle_SpawnImpact(s->pos);
            Hunter_DropShards(s->pos, 2);
            SoundFx_PlayHunterDie();
        }
    }
}

void Mobs_Init(void) {
    for (int i = 0; i < CRAWLER_MAX; i++) crawlers[i].active = false;
    for (int i = 0; i < WISP_MAX; i++) wisps[i].active = false;
    for (int i = 0; i < SPIDER_MAX; i++) spiders[i].active = false;
    for (int i = 0; i < MUSH_MAX; i++) mushrooms[i].active = false;
    /* v60: the glowmoth/pollen state used to survive a world reset -
     * moths kept flying with timers anchored to the previous session's
     * clock (their pollen stream went silent), stale dust hung in the
     * void and event announcements never replayed. Wipe it all. */
    for (int i = 0; i < MOTH_MAX; i++) moths[i].active = false;
    for (int i = 0; i < GRAZER_MAX; i++) grazers[i].active = false;
    grazerAnnounced = false;
    for (int i = 0; i < POLLEN_MAX; i++) pollen[i].life = 0.0f;
    pollenNext = 0;
    crawlerAnnounced = false;
    spiderAnnounced = false;
    mothAnnounced = false;
    shellAnnounced = false;
    mushHintShown = false;
    shellActive = false;
    shellUntil = 0.0;
    shellEventAt = 0.0;
    mushroomsStored = 0;
}

void Mobs_Shutdown(void) {
    for (int i = 0; i < CRAWLER_MAX; i++) crawlers[i].active = false;
    for (int i = 0; i < WISP_MAX; i++) wisps[i].active = false;
    for (int i = 0; i < SPIDER_MAX; i++) spiders[i].active = false;
    for (int i = 0; i < MUSH_MAX; i++) mushrooms[i].active = false;
}

void Mobs_Update(float deltaTime) {
    double now = (double)GetTime();
    static double crawlerTimer = 5.0, wispTimer = 9.0;
    bool surge = Hunter_GetSurgeLevel() > 0.5f;

    crawlerTimer -= deltaTime;
    if (crawlerTimer <= 0.0f) {
        crawlerTimer = 4.0f;
        Crawler_SpawnTry(surge);
    }
    wispTimer -= deltaTime;
    if (wispTimer <= 0.0f) {
        wispTimer = 7.0f;
        Wisp_SpawnTry();
    }

    Crawler_Update(deltaTime, surge, now);
    Wisp_Update(deltaTime, now);
    Spider_Update(deltaTime, now);
    Cocoon_Scan(deltaTime);
    Shell_Update(deltaTime, now);
    Mushrooms_Update(now);
    Moth_Update(deltaTime, now);   /* v59: glowmoths */
    Grazer_Update(deltaTime, now);  /* v62: meadow grazers */
    static float grazerTimer = 8.0f;
    grazerTimer -= deltaTime;
    if (grazerTimer <= 0.0f) {
        grazerTimer = 6.0f;
        Grazer_SpawnTry();
    }

    /* v51: mobs set off volatile barrels under (or inside) them */
    for (int i = 0; i < CRAWLER_MAX; i++)
        if (crawlers[i].active) Mob_BarrelCheck(crawlers[i].pos, false);
    for (int i = 0; i < SPIDER_MAX; i++)
        if (spiders[i].active) Mob_BarrelCheck(spiders[i].pos, false);
    for (int i = 0; i < WISP_MAX; i++)
        if (wisps[i].active) Mob_BarrelCheck(wisps[i].pos, true);
}

/* --------------------------------------------------------------- combat */
typedef struct MobHit {
    int kind;         /* 0 none, 1 crawler, 2 wisp */
    int index;
    float t;
} MobHit;

static MobHit Mobs_Raypick(Vector3 origin, Vector3 dir, float maxDist) {
    MobHit hit = { 0, -1, maxDist };
    float rayLen = Vector3Length(dir);
    if (rayLen < 0.0001f) return hit;
    Vector3 rd = Vector3Scale(dir, 1.0f / rayLen);

    for (int i = 0; i < CRAWLER_MAX; i++) {
        if (!crawlers[i].active) continue;
        Vector3 oc = Vector3Subtract(crawlers[i].pos, origin);
        float t = Vector3DotProduct(oc, rd);
        if (t < 0.0f || t > hit.t) continue;
        if (Vector3LengthSqr(Vector3Subtract(oc, Vector3Scale(rd, t))) < 0.65f * 0.65f) {
            hit.kind = 1; hit.index = i; hit.t = t;
        }
    }
    for (int i = 0; i < WISP_MAX; i++) {
        if (!wisps[i].active) continue;
        Vector3 oc = Vector3Subtract(wisps[i].pos, origin);
        float t = Vector3DotProduct(oc, rd);
        if (t < 0.0f || t > hit.t) continue;
        if (Vector3LengthSqr(Vector3Subtract(oc, Vector3Scale(rd, t))) < 0.5f * 0.5f) {
            hit.kind = 2; hit.index = i; hit.t = t;
        }
    }
    for (int i = 0; i < SPIDER_MAX; i++) {
        if (!spiders[i].active) continue;
        Vector3 oc = Vector3Subtract(spiders[i].pos, origin);
        float t = Vector3DotProduct(oc, rd);
        if (t < 0.0f || t > hit.t) continue;
        if (Vector3LengthSqr(Vector3Subtract(oc, Vector3Scale(rd, t))) < 0.7f * 0.7f) {
            hit.kind = 3; hit.index = i; hit.t = t;
        }
    }
    return hit;
}

static void Mob_CrawlerDamage(Crawler *c, Vector3 rd) {
    c->hp -= 1.0f;
    c->vel = Vector3Add(Vector3Scale(rd, 2.4f), (Vector3){ 0, 0.4f, 0 });
    Particle_SpawnImpact(c->pos);
    /* v59: only the crawler you actually hit gets angry - the old blanket
     * aggro telegraphed the player to every crawler on the island */
    if (c->hp <= 0.0f) {
        c->active = false;
        Hunter_WireBurst(c->pos);          /* v52: its own wireframe burst */
        Particle_SpawnImpact(c->pos);
        Hunter_DropShards(c->pos, Hunter_GetSurgeLevel() > 0.5f ? 2 : 1);
        Player_Heal(1);
        SoundFx_PlayHunterDie();
    } else {
        SoundFx_PlayHunterHit();
    }
}

bool Mobs_MeleeHit(Vector3 origin, Vector3 dir, float maxDist) {
    MobHit hit = Mobs_Raypick(origin, dir, maxDist);
    if (hit.kind == 1) {
        Crawler *c = &crawlers[hit.index];
        Vector3 rd = Vector3Scale(dir, 1.0f);
        Mob_CrawlerDamage(c, rd);
        c->aggroTimer = 1.2f;   /* v59: a struck crawler remembers you briefly */
        return true;
    }
    if (hit.kind == 2) {
        Wisp *w = &wisps[hit.index];
        w->active = false;
        Hunter_WireBurst(w->pos);
        Particle_SpawnImpact(w->pos);
        Hunter_DropShards(w->pos, 2);
        Player_Heal(1);
        SoundFx_PlayHunterDie();
        Chat_AddLine(Tr("The wisp releases its shards."));
        return true;
    }
    if (hit.kind == 3) {
        Spider *s = &spiders[hit.index];
        Spider_Damage(s, Vector3Normalize(dir));
        return true;
    }
    return false;
}

bool Mobs_LaserHit(Vector3 origin, Vector3 dir, float maxDist, Vector3 *hitPoint) {
    MobHit hit = Mobs_Raypick(origin, dir, maxDist);
    if (hit.kind == 0) return false;
    Vector3 point = Vector3Add(origin, Vector3Scale(Vector3Normalize(dir), hit.t));
    if (hitPoint) *hitPoint = point;
    if (hit.kind == 1) {
        Crawler *c = &crawlers[hit.index];
        Vector3 rd = Vector3Normalize(dir);
        Mob_CrawlerDamage(c, rd);
        c->aggroTimer = 1.2f;   /* v59: local aggro only */
    } else if (hit.kind == 3) {
        Spider_Damage(&spiders[hit.index], Vector3Normalize(dir));
    } else {
        Wisp *w = &wisps[hit.index];
        w->active = false;
        Hunter_WireBurst(w->pos);
        Particle_SpawnImpact(w->pos);
        Hunter_DropShards(w->pos, 2);
        Player_Heal(1);
        SoundFx_PlayHunterDie();
        Chat_AddLine("The wisp releases its shards.");
    }
    return true;
}

/* ---------------------------------------------------------------- draw */

/* v52: three-line edge band (like the hunters got in v51) so every enemy
 * reads on bright terrain: a bright center line plus two 3/4-bright
 * offsets perpendicular to the view */
static void Mob_Band(Vector3 a, Vector3 b, Color c) {
    rlColor4ub(c.r, c.g, c.b, c.a);
    rlVertex3f(a.x, a.y, a.z);
    rlVertex3f(b.x, b.y, b.z);
    Vector3 mid = Vector3Scale(Vector3Add(a, b), 0.5f);
    Vector3 e = Vector3Subtract(b, a);
    Vector3 v = Vector3Subtract(player.camera.position, mid);
    Vector3 side = Vector3CrossProduct(e, v);
    float len = Vector3Length(side);
    if (len < 0.0001f) return;
    side = Vector3Scale(side, 0.022f / len);
    Vector3 a1 = Vector3Add(a, side), b1 = Vector3Add(b, side);
    Vector3 a2 = Vector3Subtract(a, side), b2 = Vector3Subtract(b, side);
    rlColor4ub((unsigned char)(c.r * 3 / 4), (unsigned char)(c.g * 3 / 4),
               (unsigned char)(c.b * 3 / 4), c.a);
    rlVertex3f(a1.x, a1.y, a1.z); rlVertex3f(b1.x, b1.y, b1.z);
    rlVertex3f(a2.x, a2.y, a2.z); rlVertex3f(b2.x, b2.y, b2.z);
}

/* v55: single camera-facing sprite - one clean mushroom instead of the
 * splayed crossed quads; square quad, world-up, right edge from the view
 * matrix so it always looks at the viewer */
/* v57: shared billboard emitter - halfW is half the quad width, h the full
 * height above `base` (a block's bottom center). Used by the void mushrooms
 * and, since v57, by every island flora block from the chunk flora lists. */
void Mobs_DrawBillboard(Vector3 base, float halfW, float h, int tile, unsigned char bright, float lean) {
    Mobs_DrawBillboardUV(base, halfW, h, tile, bright, lean, 1.0f / 16.0f, 1.0f / 16.0f);
}

void Mobs_DrawBillboardUV(Vector3 base, float halfW, float h, int tile,
                          unsigned char bright, float lean, float uSpan, float vSpan) {
    float u0 = (tile % 16) / 16.0f, v0 = (tile / 16) / 16.0f;
    float u1 = u0 + uSpan, v1 = v0 + vSpan;
    float w = halfW;
    Matrix view = rlGetMatrixModelview();
    Vector3 right = Vector3Normalize((Vector3){ view.m0, view.m4, view.m8 });
    Vector3 bl = Vector3Subtract(base, Vector3Scale(right, w));
    Vector3 br = Vector3Add(base, Vector3Scale(right, w));
    Vector3 up = { 0, h, 0 };
    /* v58: wind lean shifts only the top edge (bottom stays rooted) */
    Vector3 tl = Vector3Add(bl, up);
    Vector3 tr = Vector3Add(br, up);
    if (lean != 0.0f) {
        tl.x += lean; tl.z += lean * 0.6f;
        tr.x += lean; tr.z += lean * 0.6f;
    }
    /* v56: emitted in both windings - the icon renderer leaves backface
     * culling enabled session-wide, so a single-sided quad vanishes from
     * one side (that is why v55 mushrooms "disappeared") */
    rlColor4ub(bright, bright, bright, 255);
    rlTexCoord2f(u0, v1); rlVertex3f(bl.x, bl.y, bl.z);
    rlTexCoord2f(u0, v0); rlVertex3f(tl.x, tl.y, tl.z);
    rlTexCoord2f(u1, v0); rlVertex3f(tr.x, tr.y, tr.z);
    rlTexCoord2f(u1, v1); rlVertex3f(br.x, br.y, br.z);
    rlTexCoord2f(u1, v1); rlVertex3f(br.x, br.y, br.z);
    rlTexCoord2f(u1, v0); rlVertex3f(tr.x, tr.y, tr.z);
    rlTexCoord2f(u0, v0); rlVertex3f(tl.x, tl.y, tl.z);
    rlTexCoord2f(u0, v1); rlVertex3f(bl.x, bl.y, bl.z);
}

/* v55 void mushrooms keep their square, scale-driven billboard.
 * v61.3: the mushroom paints first and its grass ring paints AFTER it -
 * the stem base hides in the lawn like every other plant's. */
static void Mob_FloraBillboard(Vector3 base, float scale, int tile, unsigned char bright) {
    Mobs_DrawBillboard(base, 0.30f * scale, 0.60f * scale, tile, bright, 0.0f);
    double nowG = GetTime();
    const float offs[4][2] = { { 0.15f, 0.12f }, { -0.14f, 0.13f },
                               { 0.13f, -0.15f }, { -0.12f, -0.14f } };
    for (int gI = 0; gI < 4; gI++) {
        Vector3 at = { base.x + offs[gI][0], base.y, base.z + offs[gI][1] };
        float lean2 = 0.07f * sinf((float)nowG * 2.1f + base.x * 3.1f + gI * 1.7f);
        Mobs_DrawBillboard(at, 0.20f, 0.19f + 0.05f * ((gI * 29) % 4) / 4.0f,
                           gI % 2 == 0 ? 39 : 41, bright, lean2);
    }
}

/* v55: textured lat-long blob - the spider's carapace (atlas tile based);
 * faceAng rotates the long axis so the abdomen points where it walks */
static void Mob_TexturedBlob(Vector3 c, float rx, float ry, float rz, float faceAng,
                             int tile, unsigned char r, unsigned char g, unsigned char b) {
    const int SEG = 4, BAND = 3;
    float u0 = (tile % 16) / 16.0f, v0 = (tile / 16) / 16.0f;
    float du = 1.0f / 16.0f, dv = 1.0f / 16.0f;
    float ca = cosf(faceAng), sa = sinf(faceAng);
    Vector3 pt[SEG + 1][BAND + 1];
    for (int k = 0; k <= SEG; k++) {
        float phi = 6.2832f * k / SEG;
        for (int q = 0; q <= BAND; q++) {
            float th = 3.1416f * q / BAND;
            float lx = sinf(th) * cosf(phi) * rx;
            float ly = cosf(th) * ry;
            float lz = sinf(th) * sinf(phi) * rz;
            pt[k][q] = (Vector3){ c.x + lx * ca - lz * sa, c.y + ly, c.z + lx * sa + lz * ca };
        }
    }
    for (int k = 0; k < SEG; k++)
        for (int q = 0; q < BAND; q++) {
            float ua = u0 + du * k / SEG, ub = u0 + du * (k + 1) / SEG;
            float va = v0 + dv * q / BAND, vb = v0 + dv * (q + 1) / BAND;
            rlColor4ub(r, g, b, 255);
            rlTexCoord2f(ua, va); rlVertex3f(pt[k][q].x, pt[k][q].y, pt[k][q].z);
            rlTexCoord2f(ub, va); rlVertex3f(pt[k + 1][q].x, pt[k + 1][q].y, pt[k + 1][q].z);
            rlTexCoord2f(ub, vb); rlVertex3f(pt[k + 1][q + 1].x, pt[k + 1][q + 1].y, pt[k + 1][q + 1].z);
            rlTexCoord2f(ua, vb); rlVertex3f(pt[k][q + 1].x, pt[k][q + 1].y, pt[k][q + 1].z);
        }
}

void Mobs_Draw(void) {
    double now = (double)GetTime();

    /* v59.4: the umbrella wears RUNTIME textures (256x128): a bright
     * puffy cumulus on top and an alien starfield beneath - the window
     * to another world. rlBegin is called BEFORE rlSetTexture: rlBegin
     * stamps the fresh draw record with the DEFAULT texture whenever it
     * has to switch modes, which used to eat the intended binding.
     * Backface culling stays off for the shell (the icon renderer turns
     * it on session-wide and the cap's outside vanished from view). */
    if (shellActive) {
        Shell_EnsureTextures();
        float R = 9.0f, cy2 = shellCenter.y;
        float H = R * 0.55f;                    /* cap height */
        const float PH_RIM = 1.6619f;           /* rim sits just under the equator */
        if (shellTopTex.id != 0 && shellUndTex.id != 0) {
            rlBegin(RL_QUADS);
            rlSetTexture(shellTopTex.id);
            rlDisableBackfaceCulling();
            const int SEG = 16;
            const float BANDS[7] = { PH_RIM, 1.45f, 1.20f, 0.90f, 0.60f, 0.30f, 0.0f };
            float pulse = 0.92f + 0.08f * sinf((float)now * 0.8f);
            unsigned char topB = (unsigned char)(238.0f * pulse);
            unsigned char undB = (unsigned char)(215.0f * pulse);
            /* pass 1: the outside wears the puffy cumulus */
            for (int b = 0; b < 6; b++) {
                float ph0 = BANDS[b], ph1 = BANDS[b + 1];
                float r0 = R * sinf(ph0), y0 = cy2 + H * cosf(ph0);
                float r1 = R * sinf(ph1), y1 = cy2 + H * cosf(ph1);
                float v0 = ph0 / PH_RIM, v1 = ph1 / PH_RIM;   /* rim 1 .. pole 0 */
                for (int s = 0; s < SEG; s++) {
                    float th0 = 6.2832f * s / SEG, th1 = 6.2832f * (s + 1) / SEG;
                    float u0 = (float)s / SEG, u1 = (float)(s + 1) / SEG;
                    rlColor4ub(topB, topB, topB, 255);
                    rlTexCoord2f(u0, v0);
                    rlVertex3f(shellCenter.x + cosf(th0) * r0, y0, shellCenter.z + sinf(th0) * r0);
                    rlTexCoord2f(u1, v0);
                    rlVertex3f(shellCenter.x + cosf(th1) * r0, y0, shellCenter.z + sinf(th1) * r0);
                    rlTexCoord2f(u1, v1);
                    rlVertex3f(shellCenter.x + cosf(th1) * r1, y1, shellCenter.z + sinf(th1) * r1);
                    rlTexCoord2f(u0, v1);
                    rlVertex3f(shellCenter.x + cosf(th0) * r1, y1, shellCenter.z + sinf(th0) * r1);
                }
            }
            /* pass 2: the inside, slightly inset - the other world's sky */
            rlSetTexture(shellUndTex.id);
            rlColor4ub(undB, (unsigned char)(undB * 105 / 100), undB, 255);
            for (int b = 0; b < 6; b++) {
                float ph0 = BANDS[b], ph1 = BANDS[b + 1];
                float r0 = R * sinf(ph0) * 0.995f, y0 = cy2 + H * cosf(ph0) - 0.02f;
                float r1 = R * sinf(ph1) * 0.995f, y1 = cy2 + H * cosf(ph1) - 0.02f;
                float v0 = ph0 / PH_RIM, v1 = ph1 / PH_RIM;
                for (int s = 0; s < SEG; s++) {
                    float th0 = 6.2832f * s / SEG, th1 = 6.2832f * (s + 1) / SEG;
                    float u0 = (float)s / SEG, u1 = (float)(s + 1) / SEG;
                    rlTexCoord2f(u0, v0);
                    rlVertex3f(shellCenter.x + cosf(th0) * r0, y0, shellCenter.z + sinf(th0) * r0);
                    rlTexCoord2f(u1, v0);
                    rlVertex3f(shellCenter.x + cosf(th1) * r0, y0, shellCenter.z + sinf(th1) * r0);
                    rlTexCoord2f(u1, v1);
                    rlVertex3f(shellCenter.x + cosf(th1) * r1, y1, shellCenter.z + sinf(th1) * r1);
                    rlTexCoord2f(u0, v1);
                    rlVertex3f(shellCenter.x + cosf(th0) * r1, y1, shellCenter.z + sinf(th0) * r1);
                }
            }
            rlEnd();
            rlDrawRenderBatchActive();
            rlEnableBackfaceCulling();
            rlSetTexture(0);
        }
    }

    Texture2D atlasS = World_GetTerrainTexture();
    if (atlasS.id != 0) {
        rlSetTexture(atlasS.id);
        rlBegin(RL_QUADS);
        Grazer_Draw(now);   /* v62: textured grazers */
        for (int i = 0; i < SPIDER_MAX; i++) {
            Spider *s = &spiders[i];
            if (!s->active) continue;
            bool aggro = s->aggroTimer > 0.0f;
            Vector3 c0 = s->pos;
            unsigned char tr = aggro ? 255 : 215, tg = aggro ? 130 : 215, tb = aggro ? 130 : 255;
            float bob = sinf((float)now * 6.0f + s->phase) * 0.02f;
            /* abdomen along the facing axis, head sphere up front */
            Mob_TexturedBlob((Vector3){ c0.x, c0.y + 0.02f + bob, c0.z },
                             0.42f, 0.26f, 0.22f, s->faceAng, 36, tr, tg, tb);
            Mob_TexturedBlob((Vector3){ c0.x + cosf(s->faceAng) * 0.46f,
                                        c0.y + 0.06f + bob,
                                        c0.z + sinf(s->faceAng) * 0.46f },
                             0.15f, 0.14f, 0.14f, s->faceAng, 36,
                             (unsigned char)(tr * 9 / 10), (unsigned char)(tg * 8 / 10),
                             (unsigned char)(tb * 9 / 10));
            /* v56: the tail is textured now - three shrinking hide blobs
             * whipping behind the abdomen */
            float caS = cosf(s->faceAng), saS = sinf(s->faceAng);
            float whip2 = sinf((float)now * 3.4f + s->phase);
            Vector3 tp = { c0.x - caS * 0.40f, c0.y + 0.02f + bob, c0.z - saS * 0.40f };
            for (int k = 0; k < 3; k++) {
                float rr2 = 0.14f - 0.03f * k;
                Vector3 np = { tp.x - caS * 0.22f + sinf(whip2 + k) * 0.06f,
                               tp.y + 0.20f - k * 0.02f,
                               tp.z - saS * 0.22f + cosf(whip2 + k) * 0.06f };
                Mob_TexturedBlob((Vector3){ (tp.x + np.x) / 2, (tp.y + np.y) / 2, (tp.z + np.z) / 2 },
                                 rr2, rr2 * 1.1f, rr2, s->faceAng, 36,
                                 (unsigned char)(tr * (90 - k * 8) / 100),
                                 (unsigned char)(tg * (85 - k * 8) / 100),
                                 (unsigned char)(tb * (95 - k * 5) / 100));
                tp = np;
            }
        }
        rlEnd();
        rlSetTexture(0);
        rlDrawRenderBatchActive();
    }

    rlDrawRenderBatchActive();
    rlBegin(RL_LINES);

    /* crawlers: flattened octahedron body on skittering legs */
    for (int i = 0; i < CRAWLER_MAX; i++) {
        Crawler *c = &crawlers[i];
        if (!c->active) continue;
        bool aggro = (Hunter_GetSurgeLevel() > 0.5f) || c->aggroTimer > 0.0f;
        Vector3 c0 = c->pos;
        unsigned char bright = aggro ? 255 : 210;
        float bodyR = 0.34f, bodyH = 0.20f;
        float ang[4] = { 0.7854f, 2.3562f, 3.9270f, 5.4978f };
        Vector3 ring[4];
        for (int k = 0; k < 4; k++) {
            ring[k] = (Vector3){ c0.x + cosf(ang[k] + (float)now * 0.6f + c->phase) * bodyR,
                                 c0.y, c0.z + sinf(ang[k] + (float)now * 0.6f + c->phase) * bodyR };
        }
        Vector3 top = { c0.x, c0.y + bodyH, c0.z };
        Vector3 bot = { c0.x, c0.y - bodyH, c0.z };
        Color bodyC = { aggro ? 255 : bright, aggro ? 80 : bright, aggro ? 90 : bright, 255 };
        for (int k = 0; k < 4; k++) {
            Mob_Band(ring[k], ring[(k + 1) % 4], bodyC);
            Mob_Band(top, ring[k], bodyC);
            Mob_Band(bot, ring[k], bodyC);
        }
        /* legs: skitter to the ground */
        Color legC = { aggro ? 255 : 195, aggro ? 90 : 195, aggro ? 100 : 205, 255 };
        for (int k = 0; k < 4; k++) {
            float wig = sinf((float)now * 11.0f + c->phase + k * 1.57f) * 0.09f;
            Vector3 foot = { ring[k].x + cosf(ang[k]) * 0.18f + wig,
                             c0.y - MOB_BODY_HALF_H - 0.02f,
                             ring[k].z + sinf(ang[k]) * 0.18f };
            Mob_Band(ring[k], foot, legC);
        }
        /* eye tick */
        rlColor4ub(aggro ? 255 : 240, aggro ? 60 : 240, aggro ? 70 : 240, 255);
        rlVertex3f(c0.x - 0.08f, c0.y + 0.05f, c0.z);
        rlVertex3f(c0.x + 0.08f, c0.y + 0.05f, c0.z);
    }

    /* wisps: teal hovering diamonds with an orbiting ring */
    for (int i = 0; i < WISP_MAX; i++) {
        Wisp *w = &wisps[i];
        if (!w->active) continue;
        float bob = sinf((float)now * 2.2f + w->phase) * 0.10f;
        Vector3 c0 = { w->pos.x, w->pos.y + bob, w->pos.z };
        float r = 0.20f + 0.03f * sinf((float)now * 3.4f + w->phase);
        Vector3 v[6] = {
            { c0.x, c0.y + r * 1.5f, c0.z }, { c0.x, c0.y - r * 1.5f, c0.z },
            { c0.x + r, c0.y, c0.z }, { c0.x - r, c0.y, c0.z },
            { c0.x, c0.y, c0.z + r }, { c0.x, c0.y, c0.z - r }
        };
        Color wispC = { 150, 255, 230, 255 };
        for (int k = 0; k < 6; k++) {
            Mob_Band(v[0], v[2 + (k % 4)], wispC);
        }
        float oa = (float)now * 1.8f + w->phase;
        Vector3 o0 = { c0.x + cosf(oa) * 0.38f, c0.y, c0.z + sinf(oa) * 0.38f };
        Vector3 o1 = { c0.x + cosf(oa + 3.1416f) * 0.38f, c0.y, c0.z + sinf(oa + 3.1416f) * 0.38f };
        Mob_Band(o0, o1, (Color){ 110, 230, 210, 220 });
    }

    /* v51 spiders: elongated body, two striding legs, a whipping tail */
    for (int i = 0; i < SPIDER_MAX; i++) {
        Spider *s = &spiders[i];
        if (!s->active) continue;
        bool aggro = s->aggroTimer > 0.0f;
        Vector3 c0 = s->pos;
        float ca = cosf(s->faceAng), sa = sinf(s->faceAng);
        unsigned char rC = aggro ? 255 : 205, gC = aggro ? 55 : 165, bC = aggro ? 170 : 235;
        float pulse = 0.82f + 0.18f * sinf((float)now * (aggro ? 9.0f : 2.4f) + s->phase);
        unsigned char rr = (unsigned char)(rC * pulse), gg = (unsigned char)(gC * pulse), bb = (unsigned char)(bC * pulse);

        /* body: textured (drawn above); wire legs/tail only */
        Color bodyC = { rr, gg, bb, 255 };
        /* v55: the body itself is textured now - only legs/tail/eyes stay wire */
        Vector3 head = { c0.x + ca * 0.46f, c0.y + 0.06f, c0.z + sa * 0.46f };
        rlColor4ub(aggro ? 255 : 240, aggro ? 40 : 200, aggro ? 90 : 255, 255);
        rlVertex3f(head.x - 0.07f, head.y, head.z);
        rlVertex3f(head.x + 0.07f, head.y, head.z);

        /* two legs: knee up, foot down, striding out of phase */
        for (int k = 0; k < 2; k++) {
            float side = (k == 0) ? 1.0f : -1.0f;
            float gait = sinf((float)now * 10.0f + s->phase + k * 3.1416f);
            float stride = gait * 0.22f;
            float lx = -sa * side, lz = ca * side;
            Vector3 hip  = { c0.x + lx * 0.18f, c0.y + 0.05f, c0.z + lz * 0.18f };
            Vector3 knee = { c0.x + lx * 0.42f + ca * stride, c0.y + 0.34f, c0.z + lz * 0.42f + sa * stride };
            Vector3 foot = { c0.x + lx * 0.46f + ca * stride * 1.6f,
                             c0.y - MOB_BODY_HALF_H - 0.02f,
                             c0.z + lz * 0.46f + sa * stride * 1.6f };
            Mob_Band(hip, knee, bodyC);
            Mob_Band(knee, foot, bodyC);
        }

        /* tail: three segments rising from the rear, whipping */
        Vector3 tailPt = { c0.x - ca * 0.40f, c0.y, c0.z - sa * 0.40f };
        Mob_Band((Vector3){ c0.x - ca * 0.30f, c0.y, c0.z - sa * 0.30f }, tailPt, bodyC);
        float whip = sinf((float)now * 3.4f + s->phase);
        for (int k = 1; k <= 3; k++) {
            float len = 0.22f;
            float up = 0.30f + 0.10f * k;
            Vector3 next = { tailPt.x - ca * len + sinf(whip + k) * 0.05f,
                             tailPt.y + up,
                             tailPt.z - sa * len + cosf(whip + k) * 0.05f };
            Mob_Band(tailPt, next, (Color){ (unsigned char)(rr - k * 15),
                                            (unsigned char)(gg - k * 8), bb, 255 });
            tailPt = next;
        }
    }

    /* v51 violet shell event: iridescent dome + falling light rain */
    if (shellActive) {
        float t0 = (float)now;
        float R = 9.0f, cy2 = shellCenter.y;
        /* v59.2: the light rain - streaks sliding down under the
         * umbrella; it is the event's signature */
        for (int k = 0; k < 48; k++) {
            float seed = k * 7.13f;
            float ang = fmodf(seed * 1.7f, 6.2832f);
            float rad = (0.25f + 0.75f * fmodf(seed * 0.317f, 1.0f)) * R;
            float drop = fmodf(t0 * (2.0f + fmodf(seed, 2.0f)) + seed, 14.0f);
            float x = shellCenter.x + cosf(ang) * rad;
            float z = shellCenter.z + sinf(ang) * rad;
            float yTop = cy2 + R * 0.35f - drop;
            if (yTop < shellCenter.y - 12.0f) continue;
            rlColor4ub(190, 235, 255, 255);
            rlVertex3f(x, yTop, z);
            rlVertex3f(x, yTop - 0.45f, z);
        }
        /* v61.1: the crest pentagram is gone - up close it read as a
         * crooked circle scribbled over the cloud */
    }

    rlEnd();
    rlDrawRenderBatchActive();

    /* v52: flora sprites - void mushrooms as textured crossed quads.
     * v59.6: sprite batch with alpha cutout (moth wings and pollen no
     * longer erase geometry behind their transparent texels). */
    Texture2D atlas = World_GetTerrainTexture();
    if (atlas.id != 0) {
        Mobs_SpriteBatchBegin(atlas);
        for (int i = 0; i < MUSH_MAX; i++) {
            Mushroom *m = &mushrooms[i];
            if (!m->active) continue;
            double left = m->expireAt - now;
            if (left < 30.0 && ((int)(now * 2.0)) % 2 == 0) continue;  /* expiry blink */
            unsigned char br = (unsigned char)(205.0f + 50.0f * sinf((float)now * 2.0f + i));
            /* v61.3: Mob_FloraBillboard paints the mushroom, THEN its
             * grass ring - the stem base hides in the lawn */
            Mob_FloraBillboard(m->pos, m->scale, 27, br);
        }
        Moth_Draw(now);        /* cone bodies + flapping wings (atlas batch) */
        Mobs_SpriteBatchEnd();

        /* v61: pollen on the PLAIN rlgl path - default white texture,
         * default shader, culling off, depth writes off. Nothing upstream
         * (atlas swaps, texture packs, the cutout discard, face culling)
         * can make it vanish again. See Moth_PollenDraw for the history. */
        rlDrawRenderBatchActive();
        rlDisableDepthMask();
        rlDisableBackfaceCulling();
        rlSetTexture(rlGetTextureIdDefault());
        rlBegin(RL_QUADS);
        Moth_PollenDraw(now);
        rlEnd();
        rlDrawRenderBatchActive();
        rlSetTexture(0);
        rlEnableBackfaceCulling();
        rlEnableDepthMask();
    }
}
