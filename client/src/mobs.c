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

void Mobs_Init(void) {
    for (int i = 0; i < CRAWLER_MAX; i++) crawlers[i].active = false;
    for (int i = 0; i < WISP_MAX; i++) wisps[i].active = false;
    crawlerAnnounced = false;
}

void Mobs_Shutdown(void) {
    for (int i = 0; i < CRAWLER_MAX; i++) crawlers[i].active = false;
    for (int i = 0; i < WISP_MAX; i++) wisps[i].active = false;
}

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
            if (Mob_BodyBlocked(spot)) return false;
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
        if (!crawlerAnnounced) {
            crawlerAnnounced = true;
            Chat_AddLine("Something skitters across the island...");
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
        if (!Mob_BodyBlocked(p)) {
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

        /* sight-based aggro like the hunters */
        if (!player.flying && hdist < CRAWLER_SIGHT &&
            Mob_HasLOS(c->pos, center)) {
            c->aggroTimer = CRAWLER_SIGHT_MEM;
        } else if (c->aggroTimer > 0.0f) {
            c->aggroTimer -= deltaTime;
        }
        bool aggro = (surge || c->aggroTimer > 0.0f) && now >= c->retreatUntil;

        Vector3 desired;
        if (aggro && hdist > 0.05f) {
            desired = Vector3Scale(Vector3Scale(toPlayer, 1.0f / hdist), CRAWLER_SPEED * (surge ? 1.2f : 1.0f));
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

        c->vel.x = c->vel.x + (desired.x - c->vel.x) * (1.0f - powf(0.05f, deltaTime));
        c->vel.z = c->vel.z + (desired.z - c->vel.z) * (1.0f - powf(0.05f, deltaTime));
        c->vel.y -= 0.02f * deltaTime * 60.0f;
        if (c->vel.y < -0.5f) c->vel.y = -0.5f;

        /* horizontal move with step-up hop */
        Vector3 want = Vector3Add(c->pos, Vector3Scale((Vector3){ c->vel.x, 0, c->vel.z }, deltaTime));
        if (!Mob_BodyBlocked(want)) {
            c->pos = want;
        } else if (c->grounded) {
            Vector3 stepUp = c->pos;
            stepUp.y += 1.05f;
            Vector3 stepOver = Vector3Add(stepUp, Vector3Scale((Vector3){ c->vel.x, 0, c->vel.z }, deltaTime * 2.0f));
            if (!Mob_BodyBlocked(stepOver)) {
                c->pos = stepOver;
                c->vel.y = 0.1f;
            } else {
                c->vel.x = -c->vel.x * 0.4f;
                c->vel.z = -c->vel.z * 0.4f;
                c->wanderTimer = 0.0f;
            }
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
    return hit;
}

static void Mob_CrawlerDamage(Crawler *c, Vector3 rd) {
    c->hp -= 1.0f;
    c->vel = Vector3Add(Vector3Scale(rd, 2.4f), (Vector3){ 0, 0.4f, 0 });
    Particle_SpawnBlockBreak(c->pos, 20);
    c->aggroTimer = CRAWLER_SIGHT_MEM;
    if (c->hp <= 0.0f) {
        c->active = false;
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
        return true;
    }
    if (hit.kind == 2) {
        Wisp *w = &wisps[hit.index];
        w->active = false;
        Particle_SpawnBlockBreak(w->pos, 22);
        Player_AddShards(2);
        Player_Heal(1);
        SoundFx_PlayHunterDie();
        Chat_AddLine("The wisp releases its shards.");
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
    } else {
        Wisp *w = &wisps[hit.index];
        w->active = false;
        Particle_SpawnBlockBreak(w->pos, 22);
        Player_AddShards(2);
        Player_Heal(1);
        SoundFx_PlayHunterDie();
        Chat_AddLine("The wisp releases its shards.");
    }
    return true;
}

/* ---------------------------------------------------------------- draw */
static void Mob_Edge(Vector3 a, Vector3 b, unsigned char bright) {
    rlColor4ub(bright, bright, bright, 255);
    rlVertex3f(a.x, a.y, a.z);
    rlVertex3f(b.x, b.y, b.z);
}

void Mobs_Draw(void) {
    double now = (double)GetTime();

    rlDrawRenderBatchActive();
    rlBegin(RL_LINES);

    /* crawlers: flattened octahedron body on skittering legs */
    for (int i = 0; i < CRAWLER_MAX; i++) {
        Crawler *c = &crawlers[i];
        if (!c->active) continue;
        bool aggro = (Hunter_GetSurgeLevel() > 0.5f) || c->aggroTimer > 0.0f;
        Vector3 c0 = c->pos;
        unsigned char bright = aggro ? 255 : 190;
        float bodyR = 0.34f, bodyH = 0.20f;
        float ang[4] = { 0.7854f, 2.3562f, 3.9270f, 5.4978f };
        Vector3 ring[4];
        for (int k = 0; k < 4; k++) {
            ring[k] = (Vector3){ c0.x + cosf(ang[k] + (float)now * 0.6f + c->phase) * bodyR,
                                 c0.y, c0.z + sinf(ang[k] + (float)now * 0.6f + c->phase) * bodyR };
        }
        Vector3 top = { c0.x, c0.y + bodyH, c0.z };
        Vector3 bot = { c0.x, c0.y - bodyH, c0.z };
        for (int k = 0; k < 4; k++) {
            rlColor4ub(aggro ? 255 : bright, aggro ? 80 : bright, aggro ? 90 : bright, 255);
            rlVertex3f(ring[k].x, ring[k].y, ring[k].z);
            rlVertex3f(ring[(k + 1) % 4].x, ring[k].y, ring[(k + 1) % 4].z);
            rlVertex3f(top.x, top.y, top.z);
            rlVertex3f(ring[k].x, ring[k].y, ring[k].z);
            rlVertex3f(bot.x, bot.y, bot.z);
            rlVertex3f(ring[k].x, ring[k].y, ring[k].z);
        }
        /* legs: skitter to the ground */
        for (int k = 0; k < 4; k++) {
            float wig = sinf((float)now * 11.0f + c->phase + k * 1.57f) * 0.09f;
            Vector3 foot = { ring[k].x + cosf(ang[k]) * 0.18f + wig,
                             c0.y - MOB_BODY_HALF_H - 0.02f,
                             ring[k].z + sinf(ang[k]) * 0.18f };
            rlColor4ub(aggro ? 255 : 150, aggro ? 90 : 150, aggro ? 100 : 150, 255);
            rlVertex3f(ring[k].x, ring[k].y, ring[k].z);
            rlVertex3f(foot.x, foot.y, foot.z);
        }
        /* eye tick */
        rlColor4ub(aggro ? 255 : 210, aggro ? 60 : 210, aggro ? 70 : 210, 255);
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
        rlColor4ub(110, 255, 220, 255);
        for (int k = 0; k < 6; k++) {
            rlVertex3f(v[0].x, v[0].y, v[0].z);
            rlVertex3f(v[2 + (k % 4)].x, v[2 + (k % 4)].y, v[2 + (k % 4)].z);
        }
        float oa = (float)now * 1.8f + w->phase;
        Vector3 o0 = { c0.x + cosf(oa) * 0.38f, c0.y, c0.z + sinf(oa) * 0.38f };
        Vector3 o1 = { c0.x + cosf(oa + 3.1416f) * 0.38f, c0.y, c0.z + sinf(oa + 3.1416f) * 0.38f };
        rlColor4ub(70, 200, 180, 220);
        rlVertex3f(o0.x, o0.y, o0.z);
        rlVertex3f(o1.x, o1.y, o1.z);
    }

    rlEnd();
    rlDrawRenderBatchActive();
}
