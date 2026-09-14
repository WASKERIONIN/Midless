/*
 * Midless: Cosmic Edition - THE WARDEN OF THE MEADOW (v65.21).
 * See golem.h for the fight script. Everything here is client-side,
 * like the hunters and mobs: combat in this game always was.
 */
#include "golem.h"
#include "chat.h"
#include "i18n.h"
#include "particle.h"
#include "player.h"
#include "pocketfx.h"
#include "world.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <stdlib.h>

/* ---------------- tiny basis math ---------------- */
typedef struct { Vector3 o, x, y, z; } B3;

static B3 b3Root(Vector3 o, float yaw) {
    B3 b;
    b.o = o;
    b.x = (Vector3){ cosf(yaw), 0, -sinf(yaw) };   /* local +X (right) */
    b.y = (Vector3){ 0, 1, 0 };
    b.z = (Vector3){ sinf(yaw), 0, cosf(yaw) };    /* local +Z (facing) */
    return b;
}
static Vector3 b3P(const B3 *b, Vector3 v) {
    return (Vector3){
        b->o.x + b->x.x * v.x + b->y.x * v.y + b->z.x * v.z,
        b->o.y + b->x.y * v.x + b->y.y * v.y + b->z.y * v.z,
        b->o.z + b->x.z * v.x + b->y.z * v.y + b->z.z * v.z,
    };
}
/* pitch about local X: +p leans the limb's up-axis toward facing */
static B3 b3Pitch(const B3 *b, float p) {
    B3 r;
    float c = cosf(p), s = sinf(p);
    r.o = b->o;
    r.x = b->x;
    r.y = (Vector3){ b->y.x * c + b->z.x * s, b->y.y * c + b->y.y * 0 + b->z.y * s, b->y.z * c + b->z.z * s };
    r.z = (Vector3){ -b->y.x * s + b->z.x * c, -b->y.y * s + b->z.y * c, -b->y.z * s + b->z.z * c };
    return r;
}
static B3 b3Yaw(const B3 *b, float q) {
    B3 r;
    float c = cosf(q), s = sinf(q);
    r.o = b->o;
    r.y = b->y;
    r.x = (Vector3){ b->x.x * c - b->z.x * s, b->x.y * c - b->z.y * s, b->x.z * c - b->z.z * s };
    r.z = (Vector3){ b->x.x * s + b->z.x * c, b->x.y * s + b->z.y * c, b->x.z * s + b->z.z * c };
    return r;
}
static B3 b3At(const B3 *b, Vector3 v) {
    B3 r = *b;
    r.o = b3P(b, v);
    return r;
}

/* ---------------- quad sink ---------------- */
typedef struct { Vector3 p[4]; Vector3 n; unsigned char c[3]; } GQ;
static GQ gq[1024];
static int gqN;
static const Vector3 SUN = { 0.35f, 0.85f, 0.30f };

static void gqBox(const B3 *b, Vector3 c, Vector3 h, unsigned char cr, unsigned char cg, unsigned char cb, float glow) {
    if (gqN + 6 > 1024) return;
    Vector3 v[8] = {
        b3P(b, (Vector3){ c.x - h.x, c.y - h.y, c.z - h.z }), b3P(b, (Vector3){ c.x + h.x, c.y - h.y, c.z - h.z }),
        b3P(b, (Vector3){ c.x + h.x, c.y - h.y, c.z + h.z }), b3P(b, (Vector3){ c.x - h.x, c.y - h.y, c.z + h.z }),
        b3P(b, (Vector3){ c.x - h.x, c.y + h.y, c.z - h.z }), b3P(b, (Vector3){ c.x + h.x, c.y + h.y, c.z - h.z }),
        b3P(b, (Vector3){ c.x + h.x, c.y + h.y, c.z + h.z }), b3P(b, (Vector3){ c.x - h.x, c.y + h.y, c.z + h.z }),
    };
    Vector3 nx[6] = { b->y, (Vector3){ -b->y.x, -b->y.y, -b->y.z }, b->x, (Vector3){ -b->x.x, -b->x.y, -b->x.z }, b->z, (Vector3){ -b->z.x, -b->z.y, -b->z.z } };
    static const unsigned char qi[6][4] = {
        { 4, 5, 6, 7 }, { 0, 3, 2, 1 }, { 1, 2, 6, 5 }, { 0, 4, 7, 3 }, { 2, 3, 7, 6 }, { 0, 1, 5, 4 },
    };
    for (int f = 0; f < 6; f++) {
        Vector3 n = nx[f];
        float ln = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
        if (ln > 1e-6f) { n.x /= ln; n.y /= ln; n.z /= ln; }
        float d = fabsf(n.x * SUN.x + n.y * SUN.y + n.z * SUN.z);
        float l = (0.45f + 0.55f * d) * 0.92f;
        if (glow > 0.0f) l = l * (1.0f - glow) + 1.25f * glow;
        GQ *q = &gq[gqN++];
        for (int k = 0; k < 4; k++) q->p[k] = v[qi[f][k]];
        q->n = n;
        q->c[0] = (unsigned char)(cr * l); q->c[1] = (unsigned char)(cg * l); q->c[2] = (unsigned char)(cb * l);
    }
}

/* ---------------- pose ---------------- */
typedef struct {
    float pelvisY, spineP;
    float shP[2], shY[2], elP[2];
    float thP[2], knP[2];
} Pose;

static const Pose KNEEL = {
    2.6f, 0.10f,
    { -1.15f, -1.15f }, { 0.25f, -0.25f }, { -0.50f, -0.50f },
    { -1.50f, -1.50f }, { 1.55f, 1.55f },
};
static const Pose STAND = {
    5.2f, 0.0f,
    { 0.08f, 0.08f }, { 0.12f, -0.12f }, { -0.15f, -0.15f },
    { 0.0f, 0.0f }, { 0.05f, 0.05f },
};
static Pose poseLerp(const Pose *a, const Pose *b, float t) {
    Pose r;
    r.pelvisY = a->pelvisY + (b->pelvisY - a->pelvisY) * t;
    r.spineP = a->spineP + (b->spineP - a->spineP) * t;
    for (int i = 0; i < 2; i++) {
        r.shP[i] = a->shP[i] + (b->shP[i] - a->shP[i]) * t;
        r.shY[i] = a->shY[i] + (b->shY[i] - a->shY[i]) * t;
        r.elP[i] = a->elP[i] + (b->elP[i] - a->elP[i]) * t;
        r.thP[i] = a->thP[i] + (b->thP[i] - a->thP[i]) * t;
        r.knP[i] = a->knP[i] + (b->knP[i] - a->knP[i]) * t;
    }
    return r;
}

/* ---------------- boss state ---------------- */
typedef enum { G_DORMANT, G_RISING, G_CHASE, G_AIM, G_VOLLEY, G_COOLDOWN, G_DYING, G_GONE } GState;

static struct {
    GState state;
    Vector3 pos;          /* feet centre */
    float yaw;
    float walkPh, tState, cooldown;
    float armHP[2], torsoHP;
    bool armGone[2];
    int volleysLeft;
    bool dropPending;
    Vector3 dropPos;
    /* frame cache for combat hooks */
    Vector3 shoulder[2], elbow[2], hand[2], torsoC, headC;
    bool armsDead;
} G;

typedef struct { bool on; Vector3 pos, vel; float life; } Orb;
static Orb orbs[10];

void Golem_Init(void) {
    G.state = G_DORMANT;
    /* the arena: north-east quadrant of the meadow, inside the peristyle */
    G.pos = (Vector3){ POCKETFX_CX + 26.0f, 155.0f, POCKETFX_CZ - 26.0f };
    G.yaw = (float)PI * 0.75f;
    G.armHP[0] = G.armHP[1] = 60.0f;
    G.torsoHP = 160.0f;
    G.armGone[0] = G.armGone[1] = false;
    G.armsDead = false;
    G.dropPending = false;
    for (int i = 0; i < 10; i++) orbs[i].on = false;
}

bool Golem_Active(void) {
    return PocketFx_Factor() > 0.5f && G.state != G_GONE;
}

/* ---------------- damage ---------------- */
static void burstAt(Vector3 p, int n) {
    for (int i = 0; i < n; i++) Particle_SpawnImpact(p);
}
static void hurtArm(int i, float dmg) {
    if (G.armGone[i] || G.state == G_DYING || G.state == G_GONE) return;
    G.armHP[i] -= dmg;
    burstAt(G.hand[i], 3);
    if (G.armHP[i] <= 0.0f) {
        G.armGone[i] = true;
        burstAt(G.elbow[i], 10);
        Chat_AddLine(Tr(i == 0 ? "The Warden's right arm tears away!" : "The Warden's left arm tears away!"));
        if (G.armGone[0] && G.armGone[1] && !G.armsDead) {
            G.armsDead = true;
            Chat_AddLine(Tr("The Warden's chest core is exposed!"));
        }
    }
}
static void hurtTorso(float dmg) {
    if (!G.armsDead || G.state == G_DYING || G.state == G_GONE) return;
    G.torsoHP -= dmg;
    burstAt(G.torsoC, 3);
    if (G.torsoHP <= 0.0f) {
        G.state = G_DYING;
        G.tState = 0.0f;
        G.dropPending = true;
        G.dropPos = G.pos;
        burstAt(G.torsoC, 16);
        Chat_AddLine(Tr("The Warden falls! Its wreckage condenses - warp cores hum in the grass."));
    }
}

bool Golem_MeleeHit(Vector3 origin, Vector3 dir, float maxDist) {
    if (!Golem_Active() || G.state == G_DORMANT || G.state == G_RISING) return false;
    /* ray-march: arms first (capsules), then the torso box */
    for (int s = 1; s <= 24; s++) {
        float t = maxDist * (float)s / 24.0f;
        Vector3 p = { origin.x + dir.x * t, origin.y + dir.y * t, origin.z + dir.z * t };
        for (int i = 0; i < 2; i++) {
            if (G.armGone[i]) continue;
            const Vector3 *seg[2][2] = { { &G.shoulder[i], &G.elbow[i] }, { &G.elbow[i], &G.hand[i] } };
            for (int k = 0; k < 2; k++) {
                Vector3 a = *seg[k][0], b = *seg[k][1];
                Vector3 ab = { b.x - a.x, b.y - a.y, b.z - a.z };
                float apx = p.x - a.x, apy = p.y - a.y, apz = p.z - a.z;
                float ab2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
                float u = ab2 > 1e-6f ? (apx * ab.x + apy * ab.y + apz * ab.z) / ab2 : 0.0f;
                if (u < 0.0f) u = 0.0f;
                if (u > 1.0f) u = 1.0f;
                Vector3 c = { a.x + ab.x * u, a.y + ab.y * u, a.z + ab.z * u };
                float dx = p.x - c.x, dy = p.y - c.y, dz = p.z - c.z;
                if (dx * dx + dy * dy + dz * dz < 1.1f * 1.1f) {
                    hurtArm(i, 12.0f);
                    return true;
                }
            }
        }
        if (G.armsDead) {
            float dx = p.x - G.torsoC.x, dy = p.y - G.torsoC.y, dz = p.z - G.torsoC.z;
            if (fabsf(dx) < 1.7f && fabsf(dy) < 1.9f && fabsf(dz) < 1.7f) {
                hurtTorso(10.0f);
                return true;
            }
        }
    }
    return false;
}

void Golem_ExplosionDamage(Vector3 center, float radius, int damage) {
    if (!Golem_Active() || G.state == G_DORMANT || G.state == G_RISING) return;
    for (int i = 0; i < 2; i++) {
        if (G.armGone[i]) continue;
        Vector3 m = { (G.elbow[i].x + G.hand[i].x) * 0.5f, (G.elbow[i].y + G.hand[i].y) * 0.5f, (G.elbow[i].z + G.hand[i].z) * 0.5f };
        if (Vector3Distance(m, center) < radius + 1.0f) hurtArm(i, (float)damage * 8.0f);
    }
    if (G.armsDead && Vector3Distance(G.torsoC, center) < radius + 1.5f) hurtTorso((float)damage * 8.0f);
}

/* ---------------- update ---------------- */
static void spawnOrb(Vector3 from) {
    for (int i = 0; i < 10; i++) {
        if (orbs[i].on) continue;
        Vector3 target = { player.position.x, player.position.y + 1.0f, player.position.z };
        Vector3 d = Vector3Subtract(target, from);
        float len = Vector3Length(d);
        if (len < 1e-4f) return;
        d = Vector3Scale(d, 16.0f / len);
        orbs[i].on = true;
        orbs[i].pos = from;
        orbs[i].vel = d;
        orbs[i].life = 4.0f;
        return;
    }
}

void Golem_Update(float dt) {
    if (PocketFx_Factor() <= 0.5f) return;   /* the fight sleeps outside the pocket */
    Vector3 pc = { player.position.x, player.position.y + 1.0f, player.position.z };
    float dist = Vector3Distance(pc, G.pos);

    switch (G.state) {
    case G_DORMANT:
        if (dist < 26.0f) {
            G.state = G_RISING;
            G.tState = 0.0f;
            Chat_AddLine(Tr("The Warden of the Meadow opens its eyes."));
        }
        break;
    case G_RISING:
        G.tState += dt / 2.8f;
        if (G.tState >= 1.0f) { G.state = G_CHASE; G.tState = 0.0f; }
        break;
    case G_CHASE: {
        Vector3 to = { pc.x - G.pos.x, 0, pc.z - G.pos.z };
        float want = atan2f(to.x, to.z);
        float dy = want - G.yaw;
        while (dy > (float)PI) dy -= 2.0f * (float)PI;
        while (dy < -(float)PI) dy += 2.0f * (float)PI;
        float turn = 2.5f * dt;
        if (dy > turn) dy = turn;
        if (dy < -turn) dy = -turn;
        G.yaw += dy;
        if (dist > 11.0f) {
            float sp = 3.2f * dt;
            G.pos.x += sinf(G.yaw) * sp;
            G.pos.z += cosf(G.yaw) * sp;
            G.walkPh += dt * 5.2f;
        }
        G.tState += dt;
        if (G.tState > 1.2f && dist < 24.0f) { G.state = G_AIM; G.tState = 0.0f; }
        break;
    }
    case G_AIM:
        G.tState += dt / 0.9f;
        if (G.tState >= 1.0f) { G.state = G_VOLLEY; G.tState = 0.0f; G.volleysLeft = 3; }
        break;
    case G_VOLLEY: {
        float prev = G.tState;
        G.tState += dt;
        for (int k = 0; k < 3; k++) {
            float at = k * 0.35f;
            if (prev < at && G.tState >= at && k < G.volleysLeft + 3) {
                if (!G.armGone[k % 2]) spawnOrb(G.hand[k % 2]);
                burstAt(G.hand[k % 2], 2);
            }
        }
        if (G.tState > 1.05f) {
            G.state = G_COOLDOWN;
            G.tState = 0.0f;
            G.cooldown = 1.2f + (float)(rand() % 100) / 100.0f * 1.2f;
        }
        break;
    }
    case G_COOLDOWN:
        G.tState += dt;
        if (G.tState > G.cooldown) { G.state = G_CHASE; G.tState = 0.0f; }
        break;
    case G_DYING:
        G.tState += dt / 2.6f;
        if (G.tState >= 1.0f) G.state = G_GONE;
        break;
    case G_GONE:
        break;
    }

    /* the condensed cores materialise once you approach the wreckage */
    if (G.dropPending && Vector3Distance(pc, G.dropPos) < 6.0f) {
        int bx = (int)floorf(G.dropPos.x), bz = (int)floorf(G.dropPos.z);
        for (int dx = 0; dx < 2; dx++)
            for (int dz = 0; dz < 2; dz++)
                Player_TryPlaceBlock((Vector3){ (float)(bx + dx), 155.0f, (float)(bz + dz) }, 22);
        G.dropPending = false;
    }

    /* orbs */
    for (int i = 0; i < 10; i++) {
        if (!orbs[i].on) continue;
        orbs[i].life -= dt;
        orbs[i].pos = Vector3Add(orbs[i].pos, Vector3Scale(orbs[i].vel, dt));
        Vector3 bp = { floorf(orbs[i].pos.x), floorf(orbs[i].pos.y), floorf(orbs[i].pos.z) };
        int blk = World_GetBlock(bp);
        if (blk > 0) { Particle_SpawnImpact(orbs[i].pos); orbs[i].on = false; continue; }
        if (Vector3Distance(orbs[i].pos, pc) < 1.3f) {
            Vector3 away = Vector3Normalize(Vector3Subtract(pc, orbs[i].pos));
            Player_Damage(3, away);
            Particle_SpawnImpact(orbs[i].pos);
            orbs[i].on = false;
            continue;
        }
        if (orbs[i].life <= 0.0f) orbs[i].on = false;
    }
}

/* ---------------- draw ---------------- */
static float ease(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

void Golem_Draw(void) {
    if (PocketFx_Factor() <= 0.5f || G.state == G_GONE) return;

    /* pose blend */
    float rise = 0.0f;
    if (G.state == G_DORMANT) rise = 0.0f;
    else if (G.state == G_RISING) rise = ease(G.tState);
    else if (G.state == G_DYING) rise = 1.0f - ease(G.tState) * 0.9f;
    else rise = 1.0f;
    Pose p = poseLerp(&KNEEL, &STAND, rise);

    bool aiming = (G.state == G_AIM || G.state == G_VOLLEY);
    float aimK = aiming ? ease(G.state == G_AIM ? G.tState : 1.0f) : 0.0f;
    if (aiming) {
        Vector3 pc = { player.position.x, player.position.y + 1.0f, player.position.z };
        for (int i = 0; i < 2; i++) {
            Vector3 sh = { G.pos.x + (i ? -1.6f : 1.6f), G.pos.y + p.pelvisY + 2.6f, G.pos.z };
            Vector3 d = Vector3Subtract(pc, sh);
            float horiz = sqrtf(d.x * d.x + d.z * d.z);
            float elev = atan2f(d.y, horiz > 0.01f ? horiz : 0.01f);
            float az = atan2f(d.x, d.z) - G.yaw;
            while (az > (float)PI) az -= 2.0f * (float)PI;
            while (az < -(float)PI) az += 2.0f * (float)PI;
            if (az > 1.2f) az = 1.2f;
            if (az < -1.2f) az = -1.2f;
            p.shP[i] = p.shP[i] * (1.0f - aimK) + (-(1.35f - elev * 0.8f)) * aimK;
            p.shY[i] = p.shY[i] * (1.0f - aimK) + (az * 0.5f + (i ? -0.10f : 0.10f)) * aimK;
            p.elP[i] = p.elP[i] * (1.0f - aimK) + (-0.15f) * aimK;
        }
    }
    if (G.state == G_CHASE || G.state == G_COOLDOWN) {
        float s = sinf(G.walkPh), c = cosf(G.walkPh);
        float moving = (G.state == G_CHASE) ? 1.0f : 0.0f;
        p.thP[0] += 0.42f * s * moving;
        p.thP[1] -= 0.42f * s * moving;
        p.knP[0] += 0.30f * (1.0f - c) * 0.5f * moving;
        p.knP[1] += 0.30f * (1.0f + c) * 0.5f * moving;
        p.shP[0] -= 0.22f * s * moving * (1.0f - aimK);
        p.shP[1] += 0.22f * s * moving * (1.0f - aimK);
    }
    if (G.state == G_DYING) {
        p.spineP += ease(G.tState) * 0.5f;
    }

    gqN = 0;
    B3 root = b3Root(G.pos, G.yaw);

    unsigned char stone[3] = { 106, 110, 126 };
    unsigned char dark[3] = { 72, 74, 90 };
    unsigned char glowC[3] = { 255, 206, 118 };
    unsigned char hotC[3] = { 255, 240, 190 };

    /* pelvis + torso + head */
    B3 pel = b3At(&root, (Vector3){ 0, p.pelvisY, 0 });
    B3 spine = b3Pitch(&pel, p.spineP);
    gqBox(&pel, (Vector3){ 0, 0.2f, 0 }, (Vector3){ 1.1f, 0.7f, 0.9f }, dark[0], dark[1], dark[2], 0.0f);
    gqBox(&spine, (Vector3){ 0, 1.5f, 0 }, (Vector3){ 1.3f, 1.5f, 1.1f }, stone[0], stone[1], stone[2], 0.0f);
    /* chest core: plated while arms live, exposed and hot after */
    if (G.armsDead)
        gqBox(&spine, (Vector3){ 0, 1.6f, 1.15f }, (Vector3){ 0.55f, 0.55f, 0.25f }, hotC[0], hotC[1], hotC[2], 1.0f);
    else
        gqBox(&spine, (Vector3){ 0, 1.6f, 1.12f }, (Vector3){ 0.55f, 0.55f, 0.18f }, dark[0], dark[1], dark[2], 0.0f);
    B3 head = b3At(&spine, (Vector3){ 0, 3.0f, 0 });
    gqBox(&head, (Vector3){ 0, 0.8f, 0 }, (Vector3){ 0.8f, 0.8f, 0.8f }, stone[0] - 8, stone[1] - 8, stone[2] - 8, 0.0f);
    gqBox(&head, (Vector3){ 0, 0.85f, 0.78f }, (Vector3){ 0.5f, 0.18f, 0.1f }, glowC[0], glowC[1], glowC[2], 0.9f);
    G.torsoC = b3P(&spine, (Vector3){ 0, 1.5f, 0 });
    G.headC = b3P(&head, (Vector3){ 0, 0.8f, 0 });

    /* arms */
    for (int i = 0; i < 2; i++) {
        float sx = i ? -1.6f : 1.6f;
        B3 sh = b3At(&spine, (Vector3){ sx, 2.6f, 0 });
        sh = b3Yaw(&sh, p.shY[i]);
        sh = b3Pitch(&sh, p.shP[i]);
        G.shoulder[i] = sh.o;
        if (G.armGone[i]) {
            /* scorched stump */
            gqBox(&sh, (Vector3){ 0, -0.5f, 0 }, (Vector3){ 0.5f, 0.6f, 0.5f }, dark[0], dark[1], dark[2], 0.0f);
            G.elbow[i] = b3P(&sh, (Vector3){ 0, -1.0f, 0 });
            G.hand[i] = G.elbow[i];
            continue;
        }
        gqBox(&sh, (Vector3){ 0, -1.1f, 0 }, (Vector3){ 0.5f, 1.1f, 0.5f }, stone[0], stone[1], stone[2], 0.0f);
        B3 el = b3At(&sh, (Vector3){ 0, -2.2f, 0 });
        el = b3Pitch(&el, p.elP[i]);
        gqBox(&el, (Vector3){ 0, -0.5f, 0 }, (Vector3){ 0.42f, 0.55f, 0.42f }, dark[0], dark[1], dark[2], 0.0f);
        gqBox(&el, (Vector3){ 0, -1.0f, 0 }, (Vector3){ 0.44f, 1.0f, 0.44f }, stone[0] - 6, stone[1] - 6, stone[2] - 6, 0.0f);
        B3 hd = b3At(&el, (Vector3){ 0, -2.0f, 0 });
        float g = aiming ? 1.0f : 0.55f;
        gqBox(&hd, (Vector3){ 0, -0.5f, 0 }, (Vector3){ 0.6f, 0.6f, 0.6f },
              aiming ? hotC[0] : glowC[0], aiming ? hotC[1] : glowC[1], aiming ? hotC[2] : glowC[2], g);
        G.elbow[i] = el.o;
        G.hand[i] = b3P(&hd, (Vector3){ 0, -0.6f, 0 });
    }

    /* legs */
    for (int i = 0; i < 2; i++) {
        float hx = i ? -0.8f : 0.8f;
        B3 hip = b3At(&pel, (Vector3){ hx, -0.4f, 0 });
        hip = b3Pitch(&hip, p.thP[i]);
        gqBox(&hip, (Vector3){ 0, -1.15f, 0 }, (Vector3){ 0.6f, 1.15f, 0.6f }, stone[0], stone[1], stone[2], 0.0f);
        B3 kn = b3At(&hip, (Vector3){ 0, -2.3f, 0 });
        kn = b3Pitch(&kn, p.knP[i]);
        gqBox(&kn, (Vector3){ 0, -0.5f, 0 }, (Vector3){ 0.5f, 0.55f, 0.5f }, dark[0], dark[1], dark[2], 0.0f);
        gqBox(&kn, (Vector3){ 0, -1.1f, 0 }, (Vector3){ 0.5f, 1.1f, 0.5f }, stone[0] - 6, stone[1] - 6, stone[2] - 6, 0.0f);
        B3 ft = b3At(&kn, (Vector3){ 0, -2.2f, 0 });
        gqBox(&ft, (Vector3){ 0, -0.35f, 0.45f }, (Vector3){ 0.6f, 0.35f, 0.95f }, dark[0], dark[1], dark[2], 0.0f);
    }

    /* push: culling off (mixed winding), flush while off, restore */
    rlDisableBackfaceCulling();
    rlBegin(RL_TRIANGLES);
        for (int q = 0; q < gqN; q++) {
            GQ *qu = &gq[q];
            /* two triangles per quad: 0-1-2, 0-2-3 */
            static const unsigned char tri[6] = { 0, 1, 2, 0, 2, 3 };
            for (int k = 0; k < 6; k++) {
                Vector3 v = qu->p[tri[k]];
                rlColor4ub(qu->c[0], qu->c[1], qu->c[2], 255);
                rlVertex3f(v.x, v.y, v.z);
            }
        }
        /* orbs: little hot cubes */
        for (int i = 0; i < 10; i++) {
            if (!orbs[i].on) continue;
            float s = 0.35f;
            Vector3 o = orbs[i].pos;
            Vector3 v[8] = {
                { o.x - s, o.y - s, o.z - s }, { o.x + s, o.y - s, o.z - s }, { o.x + s, o.y - s, o.z + s }, { o.x - s, o.y - s, o.z + s },
                { o.x - s, o.y + s, o.z - s }, { o.x + s, o.y + s, o.z - s }, { o.x + s, o.y + s, o.z + s }, { o.x - s, o.y + s, o.z + s },
            };
            static const unsigned char qi[6][4] = {
                { 4, 5, 6, 7 }, { 0, 3, 2, 1 }, { 1, 2, 6, 5 }, { 0, 4, 7, 3 }, { 2, 3, 7, 6 }, { 0, 1, 5, 4 },
            };
            for (int f = 0; f < 6; f++) {
                static const unsigned char tri[6] = { 0, 1, 2, 0, 2, 3 };
                for (int k = 0; k < 6; k++) {
                    Vector3 pv = v[qi[f][tri[k]]];
                    rlColor4ub(255, 236, 170, 255);
                    rlVertex3f(pv.x, pv.y, pv.z);
                }
            }
        }
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

/* ---------------- HUD ---------------- */
void Golem_DrawHUD(void) {
    if (!Golem_Active() || G.state == G_DORMANT || G.state == G_RISING) return;
    int w = GetScreenWidth();
    int bw = 420, bh = 14;
    int x = (w - bw) / 2, y = 46;
    DrawRectangle(x - 2, y - 2, bw + 4, bh + 4, (Color){ 20, 16, 28, 190 });
    I18n_DrawText(Tr("WARDEN OF THE MEADOW"), x, y - 22, 16, (Color){ 255, 214, 140, 255 });
    if (!G.armsDead) {
        /* two arm bars while the arms still hold */
        for (int i = 0; i < 2; i++) {
            int half = bw / 2 - 4;
            int hx = i == 0 ? x : x + bw / 2 + 4;
            float f = G.armHP[i] / 60.0f;
            if (f < 0.0f) f = 0.0f;
            DrawRectangle(hx, y, half, bh, (Color){ 60, 54, 70, 255 });
            DrawRectangle(hx, y, (int)(half * f), bh, (Color){ 214, 178, 120, 255 });
        }
    } else {
        float f = G.torsoHP / 160.0f;
        if (f < 0.0f) f = 0.0f;
        DrawRectangle(x, y, bw, bh, (Color){ 60, 54, 70, 255 });
        DrawRectangle(x, y, (int)(bw * f), bh, (Color){ 255, 196, 110, 255 });
    }
}
