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
#include "soundfx.h"
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
} Hunter;

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

static void Hunter_SpawnAttempt(void) {
    if (player.flying) return;
    Vector3 center = PlayerCenter();
    for (int attempt = 0; attempt < 6; attempt++) {
        float ang = (float)GetRandomValue(0, 3599) * 0.001745f;
        float dist = 15.0f + (float)GetRandomValue(0, 700) / 100.0f;
        float dy = -5.0f + (float)GetRandomValue(0, 1200) / 100.0f;
        Vector3 p = { center.x + cosf(ang) * dist, center.y + dy, center.z + sinf(ang) * dist };
        if (!IsOpenSpace(p)) continue;
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
    if (IsAirAt(next)) {
        h->pos = next;
        return true;
    }
    /* axis slides */
    Vector3 x = { h->pos.x + delta.x, h->pos.y, h->pos.z };
    Vector3 y = { h->pos.x, h->pos.y + delta.y, h->pos.z };
    Vector3 z = { h->pos.x, h->pos.y, h->pos.z + delta.z };
    bool moved = false;
    if (IsAirAt(x)) { h->pos = x; moved = true; } else h->vel.x = -h->vel.x * 0.5f;
    if (IsAirAt(y)) { h->pos = y; moved = true; } else h->vel.y = -h->vel.y * 0.5f;
    if (IsAirAt(z)) { h->pos = z; moved = true; } else h->vel.z = -h->vel.z * 0.5f;
    return moved;
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
        bool chasing = !player.flying && dist < CHASE_RANGE && now >= h->retreatUntil;
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

        /* sting */
        if (!player.flying && dist < STING_RANGE && now >= h->retreatUntil) {
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
    rlColor4ub(bright, bright, bright, 255);
    rlVertex3f(a.x, a.y, a.z);
    rlVertex3f(b.x, b.y, b.z);
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
        for (int k = 0; k < 6; k++) Hn_Edge(ring[k], ring[(k + 1) % 6], bright);

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
         * v46: glows red when the hunter is on the hunt or just stung. */
        float dist = Vector3Length(Vector3Subtract(PlayerCenter(), h->pos));
        bool aggro = surge || (!player.flying && dist < CHASE_RANGE) || now < h->retreatUntil;
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
        float spin = (float)now * 2.6f + s->bob;
        float bobY = sinf((float)now * 2.4f + s->bob) * 0.08f;
        Vector3 c = { s->pos.x, s->pos.y + bobY, s->pos.z };
        float r = 0.16f;
        unsigned char fade = (s->age > 50.0f)
            ? (unsigned char)(255.0f * (60.0f - s->age) / 10.0f) : 255;
        Vector3 v[6] = {
            { c.x, c.y + r, c.z }, { c.x, c.y - r, c.z },
            { c.x + r, c.y, c.z }, { c.x - r, c.y, c.z },
            { c.x, c.y, c.z + r }, { c.x, c.y, c.z - r }
        };
        for (int k = 0; k < 6; k++) v[k] = Hn_RotateXZ(v[k], spin);
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
