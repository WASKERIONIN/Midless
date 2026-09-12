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
 * around; attacks by leaping at the player, then bounces back. */
#define SPIDER_MAX 2
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

/* v55: returns false when both hatches are busy - the caller must then
 * leave the cocoon intact so somebody can actually hatch later */
bool Mobs_SpawnSpider(Vector3 pos) {
    for (int i = 0; i < SPIDER_MAX; i++) {
        Spider *s = &spiders[i];
        if (s->active) continue;
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
        SoundFx_PlayExplosion();
        Particle_SpawnImpact(pos);
        if (!spiderAnnounced) {
            spiderAnnounced = true;
            Chat_AddLine("The cocoon splits open. Something many-legged rises.");
        }
        return true;
    }
    return false;
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
        Chat_AddLine("The hatchling collapses into shards.");
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

        if (hdist < SPIDER_SIGHT && Mob_HasLOS(s->pos, center)) s->aggroTimer = 3.0f;
        else if (s->aggroTimer > 0.0f) s->aggroTimer -= deltaTime;
        bool aggro = s->aggroTimer > 0.0f;

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

        /* leap attack: from up to three blocks away it jumps at the player */
        if (aggro && s->grounded && s->jumpCd <= 0.0f &&
            dist3 > 1.3f && dist3 < SPIDER_LEAP_RANGE && now >= s->bounceBackUntil) {
            Vector3 dir = (hdist > 0.05f) ? Vector3Scale(toPlayer, 1.0f / hdist) : (Vector3){ 0, 0, 1 };
            s->vel = (Vector3){ dir.x * 3.3f, 1.55f, dir.z * 3.3f };
            s->airborne = true;
            s->grounded = false;
            s->jumpCd = 1.7f;
            SoundFx_PlayJump();
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
    /* v55 fix: only consume the cocoon when a spider actually spawns -
     * with both hatch slots busy the old code destroyed the egg for nobody */
    if (!Mobs_SpawnSpider(spawn)) return;
    World_SetBlock(cell, 0, true);
    SoundFx_PlayCocoonOpen();   /* v59.2: the bloom instead of a harsh hit */
    Hunter_WireBurst((Vector3){ cell.x + 0.5f, cell.y + 0.5f, cell.z + 0.5f });
    Particle_SpawnImpact((Vector3){ cell.x + 0.5f, cell.y + 0.5f, cell.z + 0.5f });
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
            Chat_AddLine("The cocoon bursts under the beam. Silence... for now.");
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

/* ---------------------------- v59: glowmoths ---------------------------- */
#define MOTH_MAX 10
#define POLLEN_MAX 200
typedef struct Moth {
    bool active;
    Vector3 pos;
    Vector3 vel;
    Vector3 target;
    double targetAt;
    float phase;
    double pollenAt;
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

static void Moth_PickTarget(Moth *m) {
    for (int attempt = 0; attempt < 6; attempt++) {
        float ang = GetRandomValue(0, 3599) * 0.001745f;
        float dist = 6.0f + GetRandomValue(0, 900) / 100.0f;
        Vector3 want = { m->pos.x + cosf(ang) * dist,
                         m->pos.y + (GetRandomValue(-400, 500) / 100.0f),
                         m->pos.z + sinf(ang) * dist };
        if (World_GetBlock(want) != 0) continue;   /* drift through open space only */
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
        m->pos = Vector3Add(m->pos, Vector3Scale(m->vel, deltaTime));

        /* glowing pollen trails behind the flight */
        if (now >= m->pollenAt) {
            m->pollenAt = now + 0.10;
            Pollen *p = &pollen[pollenNext];
            pollenNext = (pollenNext + 1) % POLLEN_MAX;
            p->pos = (Vector3){ m->pos.x + GetRandomValue(-8, 8) / 100.0f,
                                m->pos.y + GetRandomValue(-6, 6) / 100.0f,
                                m->pos.z + GetRandomValue(-8, 8) / 100.0f };
            p->vel = (Vector3){ GetRandomValue(-15, 15) / 100.0f, -0.22f, GetRandomValue(-15, 15) / 100.0f };
            p->life = 1.7f + GetRandomValue(0, 60) / 100.0f;
            p->size = 0.045f + GetRandomValue(0, 40) / 1000.0f;
            p->shift = GetRandomValue(0, 628) / 100.0f;
        }
    }
    for (int i = 0; i < POLLEN_MAX; i++) {
        Pollen *p = &pollen[i];
        if (p->life <= 0.0f) continue;
        p->life -= deltaTime;
        p->vel.y -= 0.05f * deltaTime;
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
            rlColor4ub(br, bg, bb, 255);
            rlTexCoord2f(u24, v24 + 1.0f / 16.0f); rlVertex3f(a.x, a.y, a.z);
            rlTexCoord2f(u24 + 1.0f / 16.0f, v24 + 1.0f / 16.0f); rlVertex3f(b.x, b.y, b.z);
            rlTexCoord2f(u24 + 0.5f / 16.0f, v24); rlVertex3f(apex.x, apex.y, apex.z);
            /* both windings: the batch is drawn with culling possible */
            rlColor4ub((unsigned char)(br * 3 / 4), (unsigned char)(bg * 3 / 4),
                       (unsigned char)(bb * 3 / 4), 255);
            rlTexCoord2f(u24 + 0.5f / 16.0f, v24); rlVertex3f(apex.x, apex.y, apex.z);
            rlTexCoord2f(u24 + 1.0f / 16.0f, v24 + 1.0f / 16.0f); rlVertex3f(b.x, b.y, b.z);
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

/* v59.2: pollen - small camera-aligned quads in the SAME atlas batch
 * (tile 24 white x vertex color). Exactly 6 vertices per ghost quad;
 * the old version emitted 7, shearing every later triangle into those
 * full-screen color bands. */
static void Moth_PollenDraw(double now) {
    Matrix view = rlGetMatrixModelview();
    Vector3 right = Vector3Normalize((Vector3){ view.m0, view.m4, view.m8 });
    Vector3 up = Vector3Normalize((Vector3){ view.m1, view.m5, view.m9 });
    float u24 = (24 % 16) / 16.0f, v24 = (24 / 16) / 16.0f;
    for (int i = 0; i < POLLEN_MAX; i++) {
        Pollen *p = &pollen[i];
        if (p->life <= 0.0f) continue;
        float k = Clamp(p->life, 0.0f, 1.0f);
        float s = p->size * (0.6f + 0.4f * k);
        unsigned char alpha = (unsigned char)(120.0f * k);
        for (int ch = 0; ch < 3; ch++) {
            float ph = p->shift + ch * 2.094f;
            unsigned char r = (unsigned char)(127.0f + 127.0f * sinf(now * 2.6f + ph));
            unsigned char g = (unsigned char)(127.0f + 127.0f * sinf(now * 2.6f + ph + 2.094f));
            unsigned char bc = (unsigned char)(127.0f + 127.0f * sinf(now * 2.6f + ph + 4.188f));
            Vector3 off = Vector3Scale(right, (ch - 1) * p->size * 0.9f);
            Vector3 c = Vector3Add(p->pos, off);
            Vector3 rx = Vector3Scale(right, s), uy = Vector3Scale(up, s);
            Vector3 a = Vector3Subtract(Vector3Subtract(c, rx), uy);
            Vector3 b = Vector3Add(Vector3Subtract(c, rx), uy);
            Vector3 d = Vector3Add(Vector3Add(c, rx), uy);
            Vector3 e = Vector3Subtract(Vector3Add(c, rx), uy);
            rlColor4ub(r, g, bc, alpha);
            rlTexCoord2f(u24, v24 + 1.0f / 16.0f); rlVertex3f(a.x, a.y, a.z);
            rlTexCoord2f(u24, v24); rlVertex3f(b.x, b.y, b.z);
            rlTexCoord2f(u24 + 1.0f / 16.0f, v24); rlVertex3f(d.x, d.y, d.z);
            rlTexCoord2f(u24 + 1.0f / 16.0f, v24 + 1.0f / 16.0f); rlVertex3f(e.x, e.y, e.z);
            rlTexCoord2f(u24, v24 + 1.0f / 16.0f); rlVertex3f(a.x, a.y, a.z);
            rlTexCoord2f(u24 + 1.0f / 16.0f, v24); rlVertex3f(d.x, d.y, d.z);
        }
    }
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
                Chat_AddLine("Void mushroom stored. Press G to eat it (+3 HP).");
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
                    Chat_AddLine("A violet shell shimmers over the islands... it is raining light.");
                } else {
                    Chat_AddLine("The violet shell returns.");
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
        Chat_AddLine("The shell folds away into the nebula.");
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
    crawlerAnnounced = false;
    spiderAnnounced = false;
    shellActive = false;
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
        Chat_AddLine("The wisp releases its shards.");
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
    float u0 = (tile % 16) / 16.0f, v0 = (tile / 16) / 16.0f;
    float u1 = u0 + 1.0f / 16.0f, v1 = v0 + 1.0f / 16.0f;
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

/* v55 void mushrooms keep their square, scale-driven billboard */
static void Mob_FloraBillboard(Vector3 base, float scale, int tile, unsigned char bright) {
    Mobs_DrawBillboard(base, 0.30f * scale, 0.60f * scale, tile, bright, 0.0f);
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

    /* v59.2: the violet shell is an UMBRELLA again (shallow cap, R wide)
     * - the top wears the puffy cloud texture (tile 43), and looking up
     * under it shows the window to another world: an alien starfield
     * (tile 44) on the inner surface. Both sides, bottom included. */
    if (shellActive) {
        float R = 9.0f, cy2 = shellCenter.y;
        Texture2D shellTex = World_GetTerrainTexture();
        if (shellTex.id != 0) {
            rlSetTexture(shellTex.id);
            rlBegin(RL_QUADS);
            const int SEG = 16;
            const float BANDS[7] = { 0.25f, 0.52f, 0.79f, 1.06f, 1.30f, 1.47f, 1.5708f };
            float pulse = 0.88f + 0.12f * sinf((float)now * 0.8f);
            unsigned char topB = (unsigned char)(255.0f * pulse);
            unsigned char undB = (unsigned char)(205.0f * pulse);
            for (int b = 0; b < 6; b++) {
                float ph0 = BANDS[b], ph1 = BANDS[b + 1];
                float r0 = R * sinf(ph0), y0 = cy2 + R * cosf(ph0) * 0.55f;
                float r1 = R * sinf(ph1), y1 = cy2 + R * cosf(ph1) * 0.55f;
                float vv0 = (43 % 16) / 16.0f + (1.0f / 16.0f) * (b / 6.0f);
                float vv1 = (43 % 16) / 16.0f + (1.0f / 16.0f) * ((b + 1) / 6.0f);
                float wu = (43 % 16) / 16.0f, wv = (43 / 16) / 16.0f;
                vv0 = wv + (1.0f / 16.0f) * (b / 6.0f);
                vv1 = wv + (1.0f / 16.0f) * ((b + 1) / 6.0f);
                float su = (44 % 16) / 16.0f, sv = (44 / 16) / 16.0f;
                float wv2 = (44 / 16) / 16.0f;
                float ivv0 = wv2 + (1.0f / 16.0f) * (b / 6.0f);
                float ivv1 = wv2 + (1.0f / 16.0f) * ((b + 1) / 6.0f);
                for (int s = 0; s < SEG; s++) {
                    float th0 = 6.2832f * s / SEG, th1 = 6.2832f * (s + 1) / SEG;
                    float uu0 = wu + (1.0f / 16.0f) * (float)s / SEG;
                    float uu1 = wu + (1.0f / 16.0f) * (float)(s + 1) / SEG;
                    float iu0 = su + (1.0f / 16.0f) * (float)s / SEG;
                    float iu1 = su + (1.0f / 16.0f) * (float)(s + 1) / SEG;
                    Vector3 a = { shellCenter.x + cosf(th0) * r0, y0, shellCenter.z + sinf(th0) * r0 };
                    Vector3 b = { shellCenter.x + cosf(th1) * r0, y0, shellCenter.z + sinf(th1) * r0 };
                    Vector3 c = { shellCenter.x + cosf(th1) * r1, y1, shellCenter.z + sinf(th1) * r1 };
                    Vector3 d = { shellCenter.x + cosf(th0) * r1, y1, shellCenter.z + sinf(th0) * r1 };
                    /* outside: cloud texture */
                    rlColor4ub(topB, topB, topB, 255);
                    rlTexCoord2f(uu0, vv1); rlVertex3f(a.x, a.y, a.z);
                    rlTexCoord2f(uu1, vv1); rlVertex3f(b.x, b.y, b.z);
                    rlTexCoord2f(uu1, vv0); rlVertex3f(c.x, c.y, c.z);
                    rlTexCoord2f(uu0, vv0); rlVertex3f(d.x, d.y, d.z);
                    /* inside: the other world's sky, slightly inset */
                    float k0 = 0.995f, k1 = 0.995f;
                    Vector3 a2 = { shellCenter.x + cosf(th0) * r0 * k0, y0 - 0.01f, shellCenter.z + sinf(th0) * r0 * k0 };
                    Vector3 b2 = { shellCenter.x + cosf(th1) * r0 * k0, y0 - 0.01f, shellCenter.z + sinf(th1) * r0 * k0 };
                    Vector3 c2 = { shellCenter.x + cosf(th1) * r1 * k1, y1 - 0.01f, shellCenter.z + sinf(th1) * r1 * k1 };
                    Vector3 d2 = { shellCenter.x + cosf(th0) * r1 * k1, y1 - 0.01f, shellCenter.z + sinf(th0) * r1 * k1 };
                    rlColor4ub(undB, (unsigned char)(undB * 105 / 100), undB, 255);
                    rlTexCoord2f(iu0, ivv1); rlVertex3f(a2.x, a2.y, a2.z);
                    rlTexCoord2f(iu1, ivv1); rlVertex3f(b2.x, b2.y, b2.z);
                    rlTexCoord2f(iu1, ivv0); rlVertex3f(c2.x, c2.y, c2.z);
                    rlTexCoord2f(iu0, ivv0); rlVertex3f(d2.x, d2.y, d2.z);
                    rlColor4ub(undB, (unsigned char)(undB * 105 / 100), undB, 255);
                    rlTexCoord2f(iu0, ivv0); rlVertex3f(d2.x, d2.y, d2.z);
                    rlTexCoord2f(iu1, ivv0); rlVertex3f(c2.x, c2.y, c2.z);
                    rlTexCoord2f(iu1, ivv1); rlVertex3f(b2.x, b2.y, b2.z);
                    rlTexCoord2f(iu0, ivv1); rlVertex3f(a2.x, a2.y, a2.z);
                }
            }
            rlEnd();
            rlDrawRenderBatchActive();
            rlSetTexture(0);
        }
    }

    /* v55: spider bodies are textured chitin (own textured batch first) */
    Texture2D atlasS = World_GetTerrainTexture();
    if (atlasS.id != 0) {
        rlSetTexture(atlasS.id);
        rlBegin(RL_QUADS);
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
        float blend = 0.5f + 0.5f * sinf(t0 * 0.6f);
        unsigned char cr = (unsigned char)(150.0f + 90.0f * sinf(t0 * 0.9f));
        unsigned char cg = (unsigned char)(60.0f + 70.0f * blend);
        unsigned char cb = (unsigned char)(200.0f + 55.0f * sinf(t0 * 0.7f + 2.0f));
        Color shellC = { cr, cg, cb, 255 };
        /* latitude rings */
        for (int k = 0; k < 4; k++) {
            float ph = 0.35f + k * 0.38f;
            float rr2 = R * sinf(ph * 1.5708f / 1.6f) * 1.012f;
            float yy = cy2 + R * cosf(ph * 1.5708f / 1.6f) * 0.55f + 0.03f;
            float px = shellCenter.x + rr2, py = yy, pz = shellCenter.z;
            for (int sIdx = 1; sIdx <= 18; sIdx++) {
                float th = 6.2832f * sIdx / 18.0f;
                float nx = shellCenter.x + cosf(th) * rr2;
                float nz = shellCenter.z + sinf(th) * rr2;
                rlColor4ub(shellC.r, shellC.g, shellC.b, shellC.a);
                rlVertex3f(px, py, pz);
                rlVertex3f(nx, py, nz);
                px = nx; pz = nz;
            }
        }
        /* meridian arcs */
        for (int k = 0; k < 8; k++) {
            float th = 6.2832f * k / 8.0f + t0 * 0.05f;
            float px = shellCenter.x, py = cy2 + R * 0.55f, pz = shellCenter.z;
            for (int sIdx = 1; sIdx <= 8; sIdx++) {
                float ph = 1.5708f * sIdx / 8.0f;
                float nx = shellCenter.x + cosf(th) * R * sinf(ph) * 1.012f;
                float ny = cy2 + R * cosf(ph) * 0.55f + 0.03f;
                float nz = shellCenter.z + sinf(th) * R * sinf(ph) * 1.012f;
                rlColor4ub(shellC.r, shellC.g, shellC.b, shellC.a);
                rlVertex3f(px, py, pz);
                rlVertex3f(nx, ny, nz);
                px = nx; py = ny; pz = nz;
            }
        }
        /* v59.2: the light rain is back - streaks sliding down under the
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
        /* v59: ONE plain pentagram on the dome TOP (straight chords, no
         * ornaments) - it used to hang under the cloud as a twin spiral */
        {
            float py = cy2 + R * 0.58f;   /* the flattened dome's crest */
            float pr = R * 0.38f;
            float spin = t0 * 0.5f;
            unsigned char pb = (unsigned char)(200.0f + 55.0f * sinf(t0 * 2.6f));
            Color pc = { 255, 70, pb, 255 };
            Vector3 pts[5];
            for (int k = 0; k < 5; k++) {
                float an = spin + 6.2832f * k / 5.0f;
                pts[k] = (Vector3){ shellCenter.x + cosf(an) * pr, py, shellCenter.z + sinf(an) * pr };
            }
            for (int k = 0; k < 5; k++) {   /* star: straight chords, k -> k+2 */
                Vector3 a = pts[k], b = pts[(k + 2) % 5];
                rlColor4ub(pc.r, pc.g, pc.b, 255);
                rlVertex3f(a.x, a.y, a.z);
                rlVertex3f(b.x, b.y, b.z);
            }
        }
    }

    rlEnd();
    rlDrawRenderBatchActive();

    /* v52: flora sprites - void mushrooms as textured crossed quads */
    Texture2D atlas = World_GetTerrainTexture();
    if (atlas.id != 0) {
        rlSetTexture(atlas.id);
        rlBegin(RL_QUADS);
        for (int i = 0; i < MUSH_MAX; i++) {
            Mushroom *m = &mushrooms[i];
            if (!m->active) continue;
            double left = m->expireAt - now;
            if (left < 30.0 && ((int)(now * 2.0)) % 2 == 0) continue;  /* expiry blink */
            unsigned char br = (unsigned char)(205.0f + 50.0f * sinf((float)now * 2.0f + i));
            /* v59: a grass ring so the mushroom grows out of a lawn */
            {
                float tt = (float)now;
                float gu = 0.6f + 0.4f * sinf(tt * 0.35f + m->pos.z * 0.08f);
                const float offs[4][2] = { { 0.15f, 0.12f }, { -0.14f, 0.13f }, { 0.13f, -0.15f }, { -0.12f, -0.14f } };
                for (int gI = 0; gI < 4; gI++) {
                    Vector3 at = { m->pos.x + offs[gI][0], m->pos.y, m->pos.z + offs[gI][1] };
                    float lean = 0.07f * gu * sinf(tt * 2.1f + m->pos.x * 3.1f + gI * 1.7f);
                    float hh = 0.19f + 0.05f * ((gI * 29) % 4) / 4.0f;
                    Mobs_DrawBillboard(at, 0.20f, hh, gI % 2 == 0 ? 39 : 41, br, lean);
                }
            }
            Mob_FloraBillboard(m->pos, m->scale, 27, br);
        }
        Moth_Draw(now);        /* cone bodies + flapping wings */
        Moth_PollenDraw(now);  /* RGB-shift pollen, same atlas batch */
        rlEnd();
        rlDrawRenderBatchActive();
    }
}
