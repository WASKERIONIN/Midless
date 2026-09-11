/**
 * v45: Void hunters — wireframe predators of the open void.
 *
 * Client-side actors: they spawn in empty space between islands, drift and
 * watch, chase the walker, sting on touch, and burst into line fragments
 * when destroyed. Pure black & white vector rendering (rlgl line pipeline).
 */
#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "hunter.h"
#include "player.h"
#include "world.h"
#include "chunk.h"
#include "block.h"
#include "soundfx.h"
#include "particle.h"
#include "chat.h"

#define HUNTER_MAX      6
#define HUNTER_TARGET   3      /* desired simultaneous population */
#define HUNTER_HP       3
#define HUNTER_SPEED    2.1f
#define CHASE_RANGE     16.0f
#define STING_RANGE     1.25f
#define DESPAWN_RANGE   70.0f
#define BURST_MAX       8

typedef struct Hunter {
    bool active;
    Vector3 pos;
    Vector3 vel;
    float hp;
    float phase;
    float hitFlash;
    float retreatUntil;   /* GetTime() timestamp: backing off after a sting */
    float age;
    /* v46.1 steering: remembered detour so the hunter commits to a route */
    Vector3 avoidDir;
    float avoidUntil;
    /* v48.1: detection - the hunter only hunts what it has actually seen */
    float aggroTimer;
} Hunter;

/* v48.1: spark particles (wireframe streaks) for hits and laser shots */
#define SPARK_MAX 64
typedef struct Spark {
    bool active;
    Vector3 pos;
    Vector3 vel;
    float age;
    Color color;
} Spark;
static Spark sparks[SPARK_MAX];

typedef struct Burst {
    bool active;
    Vector3 pos;
    float age;
} Burst;

/* v48: void shards - the warp currency, drops from fallen hunters */
#define SHARD_MAX 24
typedef struct Shard {
    bool active;
    Vector3 pos;
    Vector3 vel;
    float age;
    float bob;
} Shard;
static Shard shardDrops[SHARD_MAX];

static Hunter hunters[HUNTER_MAX];
static Burst bursts[BURST_MAX];
static float spawnTimer;
static int bounty;
static bool ready;

/* v47: void tide - periodic hunter surges */
static bool surge = false;
static float waveTimer = 150.0f;
static bool tideWarned = false;
static float surgeLevel = 0.0f;

static Vector3 PlayerCenter(void) {
    return (Vector3){ player.position.x + 0.5f, player.position.y + 0.9f, player.position.z + 0.5f };
}

static bool IsAirAt(Vector3 p) {
    return World_GetBlock(p) == 0;
}

/* 3x3x3 pocket of open space so hunters never clip out of rock */
static bool IsOpenSpace(Vector3 p) {
    for (int dy = -1; dy <= 1; dy++)
        for (int dz = -1; dz <= 1; dz++)
            for (int dx = -1; dx <= 1; dx++) {
                if (!IsAirAt((Vector3){ p.x + dx, p.y + dy, p.z + dz })) return false;
            }
    return true;
}

/* v49.3: solid test for the hunter's BODY, not a single point.
 * Body: radius 0.42, height ~1.1 (ring plus spines). */
#define HN_BODY_R 0.42f
#define HN_BODY_HALF_H 0.55f

static bool Hunter_BodyBlocked(Vector3 center) {
    int minX = (int)floorf(center.x - HN_BODY_R), maxX = (int)floorf(center.x + HN_BODY_R);
    int minY = (int)floorf(center.y - HN_BODY_HALF_H), maxY = (int)floorf(center.y + HN_BODY_HALF_H);
    int minZ = (int)floorf(center.z - HN_BODY_R), maxZ = (int)floorf(center.z + HN_BODY_R);
    for (int y = minY; y <= maxY; y++)
        for (int z = minZ; z <= maxZ; z++)
            for (int x = minX; x <= maxX; x++) {
                int id = World_GetBlock((Vector3){ x, y, z });
                if (id == 0) continue;
                const Block *b = Block_GetDefinition(id);
                if (b->colliderType != BLOCK_COLLIDER_SOLID) continue;
                /* precise AABB overlap against the block's actual bounds */
                float bx0 = x + b->minBB.x / 16.0f, bx1 = x + b->maxBB.x / 16.0f;
                float by0 = y + b->minBB.y / 16.0f, by1 = y + b->maxBB.y / 16.0f;
                float bz0 = z + b->minBB.z / 16.0f, bz1 = z + b->maxBB.z / 16.0f;
                if (center.x + HN_BODY_R > bx0 && center.x - HN_BODY_R < bx1 &&
                    center.y + HN_BODY_HALF_H > by0 && center.y - HN_BODY_HALF_H < by1 &&
                    center.z + HN_BODY_R > bz0 && center.z - HN_BODY_R < bz1) return true;
            }
    return false;
}

/* v48.1: is the line hunter -> player clear of blocks? */
static bool Hunter_HasLineOfSight(Vector3 from, Vector3 to) {
    Vector3 delta = Vector3Subtract(to, from);
    float dist = Vector3Length(delta);
    if (dist < 0.01f) return true;
    Vector3 dir = Vector3Scale(delta, 1.0f / dist);
    for (float t = 0.8f; t < dist; t += 0.6f) {
        if (!IsAirAt(Vector3Add(from, Vector3Scale(dir, t)))) return false;
    }
    return true;
}

static void Hunter_SpawnAttempt(void) {
    if (player.flying) return;
    Vector3 center = PlayerCenter();
    for (int attempt = 0; attempt < 6; attempt++) {
        float ang = (float)GetRandomValue(0, 3599) * 0.001745f;
        float dist = 15.0f + (float)GetRandomValue(0, 700) / 100.0f;
        float dy = -5.0f + (float)GetRandomValue(0, 1200) / 100.0f;
        Vector3 p = { center.x + cosf(ang) * dist, center.y + dy, center.z + sinf(ang) * dist };
        if (!IsOpenSpace(p)) continue;
        /* v51: never materialize in the player's face */
        Vector3 toSpot = Vector3Subtract(p, center);
        float sd = Vector3Length(toSpot);
        if (sd < 14.0f && Vector3DotProduct(Vector3Scale(toSpot, 1.0f / (sd > 0.01f ? sd : 1.0f)),
                                            Player_GetForwardVector()) > 0.5f) continue;
        for (int i = 0; i < HUNTER_MAX; i++) {
            Hunter *h = &hunters[i];
            if (h->active) continue;
            h->active = true;
            h->pos = p;
            h->vel = (Vector3){ 0 };
            h->hp = HUNTER_HP;
            h->phase = (float)GetRandomValue(0, 628) / 100.0f;
            h->hitFlash = 0.0f;
            h->retreatUntil = 0.0f;
            h->age = 0.0f;
            h->avoidDir = (Vector3){ 0 };
            h->avoidUntil = 0.0f;
            h->aggroTimer = 0.0f;
            return;
        }
        return;
    }
}

static void Shard_Spawn(Vector3 pos, int count) {
    for (int n = 0; n < count; n++) {
        for (int i = 0; i < SHARD_MAX; i++) {
            Shard *s = &shardDrops[i];
            if (s->active) continue;
            s->active = true;
            s->pos = pos;
            s->vel = (Vector3){
                -0.6f + (float)GetRandomValue(0, 120) / 100.0f,
                0.8f + (float)GetRandomValue(0, 80) / 100.0f,
                -0.6f + (float)GetRandomValue(0, 120) / 100.0f
            };
            s->age = 0.0f;
            s->bob = (float)GetRandomValue(0, 628) / 100.0f;
            break;
        }
    }
}

static void Spark_Spawn(Vector3 pos, Color color, int count) {
    for (int n = 0; n < count; n++) {
        for (int i = 0; i < SPARK_MAX; i++) {
            Spark *s = &sparks[i];
            if (s->active) continue;
            s->active = true;
            s->pos = pos;
            s->vel = (Vector3){
                -2.4f + (float)GetRandomValue(0, 480) / 100.0f,
                -1.0f + (float)GetRandomValue(0, 320) / 100.0f,
                -2.4f + (float)GetRandomValue(0, 480) / 100.0f
            };
            s->age = 0.0f;
            s->color = color;
            break;
        }
    }
}

static void Burst_Spawn(Vector3 pos) {
    for (int i = 0; i < BURST_MAX; i++) {
        if (bursts[i].active) continue;
        bursts[i].active = true;
        bursts[i].pos = pos;
        bursts[i].age = 0.0f;
        return;
    }
}

void Hunter_Init(void) {
    for (int i = 0; i < HUNTER_MAX; i++) hunters[i].active = false;
    for (int i = 0; i < BURST_MAX; i++) bursts[i].active = false;
    for (int i = 0; i < SHARD_MAX; i++) shardDrops[i].active = false;
    for (int i = 0; i < SPARK_MAX; i++) sparks[i].active = false;
    spawnTimer = 6.0f;   /* grace period after world start */
    bounty = 0;
    surge = false;
    tideWarned = false;
    surgeLevel = 0.0f;
    waveTimer = 90.0f;   /* first tide arrives inside the first minute and a half */
    ready = true;
}

void Hunter_Shutdown(void) {
    ready = false;
}

static bool Hunter_MoveWithCollision(Hunter *h, Vector3 delta) {
    Vector3 next = Vector3Add(h->pos, delta);
    if (!Hunter_BodyBlocked(next)) {
        h->pos = next;
        return true;
    }
    /* axis slides with the body box, not a point */
    Vector3 x = { h->pos.x + delta.x, h->pos.y, h->pos.z };
    Vector3 y = { h->pos.x, h->pos.y + delta.y, h->pos.z };
    Vector3 z = { h->pos.x, h->pos.y, h->pos.z + delta.z };
    bool moved = false;
    if (!Hunter_BodyBlocked(x)) { h->pos = x; moved = true; } else h->vel.x = -h->vel.x * 0.5f;
    if (!Hunter_BodyBlocked(y)) { h->pos = y; moved = true; } else h->vel.y = -h->vel.y * 0.5f;
    if (!Hunter_BodyBlocked(z)) { h->pos = z; moved = true; } else h->vel.z = -h->vel.z * 0.5f;
    return moved;
}

/* v51: barrels cook off when an enemy overlaps them or passes overhead */
static void Hunter_BarrelCheck(Hunter *h) {
    int hx = (int)floorf(h->pos.x), hz = (int)floorf(h->pos.z);
    int hy = (int)floorf(h->pos.y);
    for (int dy = 1; dy <= 2; dy++) {
        Vector3 cell = { hx, hy - dy, hz };
        if (World_GetBlock(cell) == 26) { World_ExplodeAt(cell); return; }
    }
    /* overlap: the body occupies the barrel's cell */
    if (World_GetBlock((Vector3){ hx, hy, hz }) == 26) {
        World_ExplodeAt((Vector3){ hx, hy, hz });
    }
}

/* v52: mob kills share the hunter loot pipeline - floating shard drops
 * and the white wireframe death burst ("their own" debris) */
void Hunter_DropShards(Vector3 pos, int count) { Shard_Spawn(pos, count); }
void Hunter_WireBurst(Vector3 pos) { Burst_Spawn(pos); }

/* v51: blast damage - knock hunters away, kill the ones too close */
void Hunter_ExplosionDamage(Vector3 center, float radius, int damage) {
    for (int i = 0; i < HUNTER_MAX; i++) {
        Hunter *h = &hunters[i];
        if (!h->active) continue;
        Vector3 d = Vector3Subtract(h->pos, center);
        float dist = Vector3Length(d);
        if (dist > radius) continue;
        h->hp -= (float)damage;
        h->hitFlash = 0.25f;
        h->aggroTimer = 2.5f;
        Vector3 away = (dist > 0.01f) ? Vector3Scale(d, 1.0f / dist) : (Vector3){ 0, 1, 0 };
        h->vel = Vector3Add(h->vel, Vector3Scale(away, 3.5f));
        h->vel.y += 1.5f;
        Particle_SpawnImpact(h->pos);
        if (h->hp <= 0.0f) {
            Burst_Spawn(h->pos);
            Shard_Spawn(h->pos, Hunter_GetSurgeLevel() > 0.5f ? 2 : 1);
            h->active = false;
            bounty++;
        }
    }
}

void Hunter_Update(float deltaTime) {
    if (!ready) return;
    double now = (double)GetTime();
    Vector3 center = PlayerCenter();

    /* v47: void tide director - calm 150 s, warn 10 s before, surge 45 s */
    waveTimer -= deltaTime;
    if (!surge && !tideWarned && waveTimer <= 10.0f) {
        tideWarned = true;
        Chat_AddLine("The void stirs... a tide of hunters rises.");
    }
    if (waveTimer <= 0.0f) {
        surge = !surge;
        if (surge) {
            waveTimer = 45.0f;
            Chat_AddLine("THE VOID TIDE RISES - survive!");
            /* opening burst: the tide crashes in, not trickles */
            for (int b = 0; b < 3; b++) Hunter_SpawnAttempt();
        } else {
            waveTimer = 150.0f;
            tideWarned = false;
            Player_Heal(2);
            Chat_AddLine("The tide recedes. The void grants +2 vitality.");
        }
    }
    surgeLevel += ((surge ? 1.0f : 0.0f) - surgeLevel) * (1.0f - powf(0.05f, deltaTime));
    int populationCap = surge ? 6 : HUNTER_TARGET;
    float spawnInterval = surge ? 0.8f : 1.6f;

    /* population control */
    spawnTimer -= deltaTime;
    int alive = 0;
    for (int i = 0; i < HUNTER_MAX; i++) if (hunters[i].active) alive++;
    if (spawnTimer <= 0.0f) {
        spawnTimer = spawnInterval;
        if (alive < populationCap) Hunter_SpawnAttempt();
    }

    for (int i = 0; i < HUNTER_MAX; i++) {
        Hunter *h = &hunters[i];
        if (!h->active) continue;
        h->age += deltaTime;
        if (h->hitFlash > 0.0f) h->hitFlash -= deltaTime;

        Vector3 toPlayer = Vector3Subtract(center, h->pos);
        float dist = Vector3Length(toPlayer);
        if (dist > DESPAWN_RANGE || h->age > 120.0f) {
            h->active = false;
            continue;
        }

        float spd = HUNTER_SPEED * (surge ? 1.15f : 1.0f);
        /* v48.1: detection - aggro needs line of sight; sight memory 2.5 s.
         * No more player magnet: unseen hunters keep drifting. */
        if (!player.flying && dist < CHASE_RANGE &&
            Hunter_HasLineOfSight(h->pos, center)) {
            h->aggroTimer = 2.5f;
        } else if (h->aggroTimer > 0.0f) {
            h->aggroTimer -= deltaTime;
        }
        bool aggro = surge || h->aggroTimer > 0.0f;
        bool chasing = aggro && !player.flying && now >= h->retreatUntil;
        Vector3 desired;
        if (chasing) {
            desired = Vector3Scale(Vector3Scale(toPlayer, 1.0f / dist), spd);
        } else if (now < h->retreatUntil) {
            /* back off after a sting */
            desired = Vector3Scale(Vector3Scale(toPlayer, -1.0f / (dist > 0.01f ? dist : 1.0f)), spd * 0.8f);
        } else {
            /* lazy drift on a lissajous wander */
            float t = (float)now * 0.35f + h->phase;
            desired = (Vector3){ sinf(t) * 0.7f, sinf(t * 1.7f + h->phase) * 0.4f, cosf(t * 0.83f) * 0.7f };
        }

        /* ---- v46.1 steering: obstacle avoidance + separation ----------------
         * Research approach (steering behaviors / whisker probes): cast a
         * short feeler along the desired direction; when it hits a block the
         * hunter commits to the best of 10 escape directions (the one closest
         * to the goal that stays in open air) for ~0.7 s instead of jittering
         * against the wall. Hunters also repel each other so packs wrap
         * around their target instead of stacking. */
        float dLen = Vector3Length(desired);
        if (dLen > 0.01f) {
            Vector3 dir = Vector3Scale(desired, 1.0f / dLen);
            bool blocked = !IsAirAt(Vector3Add(h->pos, Vector3Scale(dir, 1.4f)));

            if (now >= h->avoidUntil) {
                if (blocked) {
                    /* pick the open direction that best keeps the goal */
                    static const Vector3 escapes[10] = {
                        { 1, 0, 0}, {-1, 0, 0}, { 0, 0, 1}, { 0, 0,-1},
                        { 1, 0, 1}, {-1, 0, 1}, { 1, 0,-1}, {-1, 0,-1},
                        { 0, 1, 0.35f}, { 0,-1, 0.35f}
                    };
                    float bestScore = -1e9f;
                    Vector3 bestDir = dir;
                    for (int k = 0; k < 10; k++) {
                        Vector3 cand = Vector3Normalize(escapes[k]);
                        /* bias: keep moving toward the player, add per-hunter
                         * randomness so a pack splits around cover both ways */
                        float goal = Vector3DotProduct(cand, dir);
                        /* deterministic per-hunter bias: the pack splits around
                         * cover in different directions (some left, some right) */
                        float rnd = ((int)(h->phase * 7.0f) + k) % 2 == 0 ? 0.10f : -0.10f;
                        Vector3 probe = Vector3Add(h->pos, Vector3Scale(cand, 1.6f));
                        if (!IsAirAt(probe)) goal -= 0.9f;   /* prefer open lanes */
                        float score = goal + rnd;
                        if (score > bestScore) { bestScore = score; bestDir = cand; }
                    }
                    h->avoidDir = bestDir;
                    h->avoidUntil = (float)now + 0.7f;
                    desired = Vector3Scale(bestDir, spd);
                }
            } else {
                /* still detouring: keep the remembered lane */
                desired = Vector3Scale(h->avoidDir, spd);
            }

            /* separation: hunters repel each other within 2.2 blocks */
            for (int j = 0; j < HUNTER_MAX; j++) {
                if (j == i || !hunters[j].active) continue;
                Vector3 away = Vector3Subtract(h->pos, hunters[j].pos);
                float d2 = Vector3LengthSqr(away);
                if (d2 < 2.2f * 2.2f && d2 > 0.0001f) {
                    float d = sqrtf(d2);
                    desired = Vector3Add(desired,
                        Vector3Scale(Vector3Scale(away, 1.0f / d), spd * (1.0f - d / 2.2f) * 1.4f));
                }
            }
        }

        /* steer */
        h->vel = Vector3Lerp(h->vel, desired, 1.0f - powf(0.12f, deltaTime));
        Hunter_MoveWithCollision(h, Vector3Scale(h->vel, deltaTime));

        Hunter_BarrelCheck(h);
        if (!h->active) continue;   /* walked onto a barrel */

        /* sting - only a hunter that actually noticed you */
        if (!player.flying && aggro && dist < STING_RANGE && now >= h->retreatUntil) {
            Vector3 push = Vector3Scale(Vector3Scale(toPlayer, -1.0f / (dist > 0.01f ? dist : 1.0f)), 1.0f);
            Player_Damage(2, push);
            h->retreatUntil = (float)now + 1.4f;
        }
    }

    for (int i = 0; i < BURST_MAX; i++) {
        if (!bursts[i].active) continue;
        bursts[i].age += deltaTime;
        if (bursts[i].age > 0.5f) bursts[i].active = false;
    }

    /* v48.1: sparks fly and fade */
    for (int i = 0; i < SPARK_MAX; i++) {
        Spark *s = &sparks[i];
        if (!s->active) continue;
        s->age += deltaTime;
        if (s->age > 0.45f) { s->active = false; continue; }
        s->vel.y -= 4.5f * deltaTime;
        s->pos = Vector3Add(s->pos, Vector3Scale(s->vel, deltaTime));
    }

    /* v48: shard pickups - drift, magnet to the player, collect */
    for (int i = 0; i < SHARD_MAX; i++) {
        Shard *s = &shardDrops[i];
        if (!s->active) continue;
        s->age += deltaTime;
        if (s->age > 60.0f) { s->active = false; continue; }
        s->vel.y -= 0.02f * deltaTime * 60.0f * 0.1f;
        s->vel = Vector3Scale(s->vel, powf(0.4f, deltaTime));
        s->pos = Vector3Add(s->pos, Vector3Scale(s->vel, deltaTime));

        Vector3 toPlayer = Vector3Subtract(center, s->pos);
        float d = Vector3Length(toPlayer);
        if (d < 3.5f && d > 0.01f) {
            s->pos = Vector3Add(s->pos, Vector3Scale(Vector3Scale(toPlayer, 1.0f / d), 6.0f * deltaTime));
        }
        if (d < 0.9f) {
            s->active = false;
            Player_AddShards(1);
            SoundFx_PlayWebAttach();
        }
    }
}

bool Hunter_TryHit(Vector3 origin, Vector3 dir, float maxDist) {
    if (!ready) return false;
    float rayLen = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (rayLen < 0.0001f) return false;
    Vector3 rd = Vector3Scale(dir, 1.0f / rayLen);

    int best = -1;
    float bestT = maxDist;
    for (int i = 0; i < HUNTER_MAX; i++) {
        Hunter *h = &hunters[i];
        if (!h->active) continue;
        Vector3 oc = Vector3Subtract(h->pos, origin);
        float t = Vector3DotProduct(oc, rd);            /* projection along ray */
        if (t < 0.0f || t > bestT) continue;
        float distSq = Vector3LengthSqr(Vector3Subtract(oc, Vector3Scale(rd, t)));
        if (distSq < 0.75f * 0.75f) {
            best = i;
            bestT = t;
        }
    }
    if (best < 0) return false;

    Hunter *h = &hunters[best];
    h->hp -= 1.0f;
    h->hitFlash = 0.15f;
    h->vel = Vector3Add(Vector3Scale(rd, 3.2f), (Vector3){ 0, 0.8f, 0 });
    h->retreatUntil = (float)GetTime() + 0.35f;
    /* v51: impact = small bright sparks */
    Particle_SpawnImpact(h->pos);
    Spark_Spawn(h->pos, (Color){ 255, 255, 255 }, 7);
    if (h->hp <= 0.0f) {
        Burst_Spawn(h->pos);
        Shard_Spawn(h->pos, surge ? 2 : 1);
        h->active = false;
        bounty++;
        Player_Heal(1);
        SoundFx_PlayHunterDie();
    } else {
        SoundFx_PlayHunterHit();
    }
    return true;
}

bool Hunter_LaserHit(Vector3 origin, Vector3 dir, float maxDist, Vector3 *hitPoint) {
    if (!ready) return false;
    float rayLen = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (rayLen < 0.0001f) return false;
    Vector3 rd = Vector3Scale(dir, 1.0f / rayLen);

    int best = -1;
    float bestT = maxDist;
    for (int i = 0; i < HUNTER_MAX; i++) {
        Hunter *h = &hunters[i];
        if (!h->active) continue;
        Vector3 oc = Vector3Subtract(h->pos, origin);
        float t = Vector3DotProduct(oc, rd);
        if (t < 0.0f || t > bestT) continue;
        float distSq = Vector3LengthSqr(Vector3Subtract(oc, Vector3Scale(rd, t)));
        if (distSq < 0.75f * 0.75f) { best = i; bestT = t; }
    }
    if (best < 0) return false;

    Hunter *h = &hunters[best];
    h->hp -= 1.0f;
    h->hitFlash = 0.15f;
    h->vel = Vector3Add(Vector3Scale(rd, 2.4f), (Vector3){ 0, 0.5f, 0 });
    h->retreatUntil = (float)GetTime() + 0.25f;
    h->aggroTimer = 2.5f;   /* being shot gets its attention */
    if (hitPoint) *hitPoint = Vector3Add(origin, Vector3Scale(rd, bestT));
    /* v51: small bright sparks */
    Particle_SpawnImpact(h->pos);
    Spark_Spawn(h->pos, (Color){ 255, 240, 200 }, 9);
    if (h->hp <= 0.0f) {
        Burst_Spawn(h->pos);
        Shard_Spawn(h->pos, surge ? 2 : 1);
        h->active = false;
        bounty++;
        Player_Heal(1);
        SoundFx_PlayHunterDie();
    } else {
        SoundFx_PlayHunterHit();
    }
    return true;
}

bool Hunter_TouchingPlayer(Vector3 playerPos, Vector3 *pushDir) {
    (void)playerPos; (void)pushDir;
    return false; /* stings are applied inside Hunter_Update */
}

int Hunter_AliveCount(void) {
    int n = 0;
    for (int i = 0; i < HUNTER_MAX; i++) if (hunters[i].active) n++;
    return n;
}

int Hunter_GetBounty(void) {
    return bounty;
}

/* ---- rendering: black & white wireframe, rlgl line pipeline ---- */

static Vector3 Hn_RotateXZ(Vector3 v, float ang) {
    float c = cosf(ang), s = sinf(ang);
    return (Vector3){ v.x * c - v.z * s, v.y, v.x * s + v.z * c };
}

static void Hn_Edge(Vector3 a, Vector3 b, unsigned char bright) {
    /* v51: enemies must read on bright terrain - each edge is drawn as a
     * 3-line band (center + two perpendicular offsets). */
    rlColor4ub(bright, bright, bright, 255);
    Vector3 mid = Vector3Scale(Vector3Add(a, b), 0.5f);
    Vector3 dir = Vector3Subtract(b, a);
    float len = Vector3Length(dir);
    Vector3 side = { 0, 1, 0 };
    if (len > 0.001f) {
        dir = Vector3Scale(dir, 1.0f / len);
        Vector3 view = Vector3Subtract(player.camera.position, mid);
        if (Vector3Length(view) > 0.001f) {
            side = Vector3CrossProduct(dir, Vector3Normalize(view));
            if (Vector3Length(side) > 0.001f) side = Vector3Normalize(side);
            else side = (Vector3){ 0, 1, 0 };
        }
    }
    float w = 0.022f;
    Vector3 o1 = Vector3Scale(side, w);
    rlVertex3f(a.x, a.y, a.z);            rlVertex3f(b.x, b.y, b.z);
    rlColor4ub((unsigned char)(bright * 3 / 4), (unsigned char)(bright * 3 / 4), (unsigned char)(bright * 3 / 4), 255);
    rlVertex3f(a.x + o1.x, a.y + o1.y, a.z + o1.z);  rlVertex3f(b.x + o1.x, b.y + o1.y, b.z + o1.z);
    rlVertex3f(a.x - o1.x, a.y - o1.y, a.z - o1.z);  rlVertex3f(b.x - o1.x, b.y - o1.y, b.z - o1.z);
}

static const int SHARD_OCTA[12][2] = {
    {0,2},{0,3},{0,4},{0,5},{1,2},{1,3},{1,4},{1,5},{2,4},{4,3},{3,5},{5,2}
};

void Hunter_Draw(void) {
    if (!ready) return;
    double now = (double)GetTime();

    rlDrawRenderBatchActive();
    rlBegin(RL_LINES);

    for (int i = 0; i < HUNTER_MAX; i++) {
        Hunter *h = &hunters[i];
        if (!h->active) continue;

        float spin = (float)now * 2.2f + h->phase;
        float bob = sinf((float)now * 3.1f + h->phase) * 0.06f;
        Vector3 c = { h->pos.x, h->pos.y + bob, h->pos.z };
        unsigned char bright = (h->hitFlash > 0.0f) ? 255 : 165;

        /* hex ring */
        Vector3 ring[6];
        for (int k = 0; k < 6; k++) {
            float a = spin + k * 1.0472f;
            ring[k] = (Vector3){ c.x + cosf(a) * 0.55f, c.y, c.z + sinf(a) * 0.55f };
        }
        /* v48.1: a hunting hunter reads red from any distance - its ring turns
         * red and a wider pulsing halo ring surrounds it */
        float distNow = Vector3Length(Vector3Subtract(PlayerCenter(), h->pos));
        bool isAggro = surge || h->aggroTimer > 0.0f;
        for (int k = 0; k < 6; k++) {
            if (isAggro) rlColor4ub(255, 76, 86, 255);
            else rlColor4ub(bright, bright, bright, 255);
            rlVertex3f(ring[k].x, ring[k].y, ring[k].z);
            rlVertex3f(ring[(k + 1) % 6].x, ring[(k + 1) % 6].y, ring[(k + 1) % 6].z);
        }
        if (isAggro) {
            float rr = 0.74f + 0.07f * sinf((float)now * 6.0f);
            for (int k = 0; k < 6; k++) {
                float a = -spin * 0.8f + k * 1.0472f;
                Vector3 p0 = { c.x + cosf(a) * rr, c.y, c.z + sinf(a) * rr };
                float a2 = -spin * 0.8f + (k + 1) * 1.0472f;
                Vector3 p1 = { c.x + cosf(a2) * rr, c.y, c.z + sinf(a2) * rr };
                rlColor4ub(255, 70, 80, 255);
                rlVertex3f(p0.x, p0.y, p0.z);
                rlVertex3f(p1.x, p1.y, p1.z);
            }
        }

        /* spines: top and bottom tripods to the ring */
        Vector3 top = { c.x, c.y + 0.62f, c.z };
        Vector3 bot = { c.x, c.y - 0.62f, c.z };
        unsigned char spine = (unsigned char)(bright * 3 / 4);
        for (int k = 0; k < 6; k += 2) {
            Hn_Edge(top, ring[k], spine);
            Hn_Edge(bot, ring[(k + 3) % 6], spine);
        }
        /* axis */
        Hn_Edge((Vector3){ c.x, c.y + 0.82f, c.z }, (Vector3){ c.x, c.y - 0.82f, c.z }, spine);

        /* eye: small counter-rotating diamond.
         * v48.1: red while this hunter is actively hunting (seen you recently). */
        bool aggro = surge || (!player.flying && distNow < CHASE_RANGE) || now < h->retreatUntil;
        float eyePulse = aggro ? (0.15f + 0.06f * sinf((float)now * 11.0f)) : 0.12f;
        Vector3 eye[4];
        for (int k = 0; k < 4; k++) {
            float a = -spin * 1.6f + k * 1.5708f;
            Vector3 local = { cosf(a) * eyePulse, sinf(a) * eyePulse, 0 };
            local = Hn_RotateXZ(local, spin * 0.5f);
            eye[k] = (Vector3){ c.x + local.x, c.y + local.y, c.z + local.z };
        }
        Color eyeColor = aggro
            ? (Color){ 255, (unsigned char)(70 + 40 * sinf((float)now * 11.0f)), 70, 255 }
            : (Color){ 200, 200, 200, 255 };
        if (h->hitFlash > 0.0f) eyeColor = (Color){ 255, 255, 255, 255 };
        rlColor4ub(eyeColor.r, eyeColor.g, eyeColor.b, 255);
        for (int k = 0; k < 4; k++) Hn_Edge(eye[k], eye[(k + 1) % 4], 255);
    }

    /* death bursts: expanding, fading octahedra */
    for (int i = 0; i < BURST_MAX; i++) {
        if (!bursts[i].active) continue;
        float k = bursts[i].age / 0.5f;
        float r = 0.3f + 1.5f * k;
        unsigned char bright = (unsigned char)(230.0f * (1.0f - k));
        Vector3 c = bursts[i].pos;
        Vector3 v[6] = {
            { c.x, c.y + r, c.z }, { c.x, c.y - r, c.z },
            { c.x + r, c.y, c.z }, { c.x - r, c.y, c.z },
            { c.x, c.y, c.z + r }, { c.x, c.y, c.z - r }
        };
        static const int edges[12][2] = {
            {0,2},{0,3},{0,4},{0,5},{1,2},{1,3},{1,4},{1,5},{2,4},{4,3},{3,5},{5,2}
        };
        for (int e = 0; e < 12; e++) Hn_Edge(v[edges[e][0]], v[edges[e][1]], bright);
    }

    /* v48: shard pickups - small spinning teal octahedra */
    for (int i = 0; i < SHARD_MAX; i++) {
        Shard *s = &shardDrops[i];
        if (!s->active) continue;
        /* v48.1: shards hover in place - a slow bob and a breathing pulse,
         * no pointless spinning */
        float bobY = sinf((float)now * 2.0f + s->bob) * 0.10f;
        Vector3 c = { s->pos.x, s->pos.y + bobY, s->pos.z };
        float r = 0.15f + 0.02f * sinf((float)now * 3.0f + s->bob);
        unsigned char fade = (s->age > 50.0f)
            ? (unsigned char)(255.0f * (60.0f - s->age) / 10.0f) : 255;
        Vector3 v[6] = {
            { c.x, c.y + r, c.z }, { c.x, c.y - r, c.z },
            { c.x + r, c.y, c.z }, { c.x - r, c.y, c.z },
            { c.x, c.y, c.z + r }, { c.x, c.y, c.z - r }
        };
        rlColor4ub(96, 255, 214, fade);
        for (int e = 0; e < 12; e++) {
            rlVertex3f(v[SHARD_OCTA[e][0]].x, v[SHARD_OCTA[e][0]].y, v[SHARD_OCTA[e][0]].z);
            rlVertex3f(v[SHARD_OCTA[e][1]].x, v[SHARD_OCTA[e][1]].y, v[SHARD_OCTA[e][1]].z);
        }
    }

    rlEnd();
    rlDrawRenderBatchActive();
}

float Hunter_GetSurgeLevel(void) {
    return surgeLevel;
}

float Hunter_GetSurgeTimeLeft(void) {
    return surge ? waveTimer : 0.0f;
}

float Hunter_GetCalmTimeLeft(void) {
    return (!surge && tideWarned) ? waveTimer : 0.0f;
}
