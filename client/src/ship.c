/**
 * v62: the Void Runner - a passing starship, pure decoration.
 *
 * Every minute or two a ship warps in high above the islands, glides a
 * long straight lane through the sky and warps out again. It is made of
 * real smooth-lit geometry (an octagonal fuselage, a glass canopy, swept
 * wings with navigation lights, twin engine nacelles with glowing
 * exhausts) - not blocks, not wireframe. It flies far beyond reach and
 * nothing in the game can hit or interact with it; it is sky theater.
 */
#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "ship.h"
#include "player.h"

#define SHIP_HULL_QUADS 220
#define SHIP_TRAIL 96

typedef struct ShipQuad {
    Vector3 v[4];        /* local space, CCW seen from outside */
    float   shade;       /* extra emissive weight 0..1 */
    float   r, g, b;     /* base albedo */
    int     unlit;       /* 1 = self-lit part, 0 = hull shading */
} ShipQuad;

typedef struct ShipTrailPt {
    Vector3 pos;
    float life;
} ShipTrailPt;

static ShipQuad hull[SHIP_HULL_QUADS];
static int hullCount = 0;
static bool meshReady = false;

static bool active;
static double nextWarpAt;      /* when the next pass may start */
static double passStart, passEnd;
static Vector3 from, to;       /* the lane */
static double passDuration;
static Vector3 shipPos;
static Vector3 shipFwd;        /* heading (unit) */
static float bank;             /* visual roll, radians */
static ShipTrailPt trail[SHIP_TRAIL];
static int trailNext;

static Vector3 V3(float x, float y, float z) { return (Vector3){ x, y, z }; }

/* push one quad into the mesh */
static void Q(Vector3 a, Vector3 b, Vector3 c, Vector3 d,
              float r, float g, float bc, float shade, int unlit) {
    if (hullCount >= SHIP_HULL_QUADS) return;
    ShipQuad *q = &hull[hullCount++];
    q->v[0] = a; q->v[1] = b; q->v[2] = c; q->v[3] = d;
    q->r = r; q->g = g; q->b = bc;
    q->shade = shade; q->unlit = unlit;
}

#define SHIP_RINGS 7

/* build the ship once, nose toward +z, up +y */
static void BuildMesh(void) {
    if (meshReady) return;
    meshReady = true;
    hullCount = 0;

    const float HULL_R = 0.62f, HULL_G = 0.66f, HULL_B = 0.74f;
    const float DARK = 0.72f;

    /* ---- fuselage: octagonal rings, tapered ---- */
    float zs[SHIP_RINGS]   = { -3.6f, -2.8f, -1.4f,  0.3f,  1.9f,  3.0f,  3.7f };
    float rr[SHIP_RINGS]   = {  0.42f,  0.55f,  0.62f,  0.58f,  0.44f,  0.24f,  0.03f };
    float yo[SHIP_RINGS]   = {  0.06f,  0.10f,  0.12f,  0.10f,  0.06f,  0.02f,  0.0f };
    Vector3 ring[SHIP_RINGS][8];
    for (int i = 0; i < SHIP_RINGS; i++) {
        for (int k = 0; k < 8; k++) {
            float an = 6.28318f * k / 8.0f + 0.3927f;   /* flat-ish top/bottom */
            float rad = rr[i];
            if (k == 0 || k == 4) rad *= 0.82f;
            ring[i][k] = V3(cosf(an) * rad, yo[i] + sinf(an) * rad * 0.86f, zs[i]);
        }
    }
    for (int i = 0; i < SHIP_RINGS - 1; i++)
        for (int k = 0; k < 8; k++) {
            int k2 = (k + 1) % 8;
            float dim = (k == 6 || k == 7) ? DARK : 1.0f;   /* belly */
            Q(ring[i][k], ring[i][k2], ring[i + 1][k2], ring[i + 1][k],
              HULL_R * dim, HULL_G * dim, HULL_B * dim, 0.0f, 0);
        }
    /* nose cap (four fan quads) */
    for (int k = 0; k < 8; k++) {
        int k2 = (k + 1) % 8;
        Vector3 nose = V3(0, yo[SHIP_RINGS - 1], 4.15f);
        float dim = (k == 6 || k == 7) ? DARK : 1.0f;
        Q(ring[SHIP_RINGS - 1][k], ring[SHIP_RINGS - 1][k2], nose, nose,
          HULL_R * dim, HULL_G * dim, HULL_B * dim, 0.0f, 0);
    }
    /* tail cap */
    Q(ring[0][1], ring[0][3], ring[0][5], ring[0][7], 0.10f, 0.10f, 0.13f, 0.0f, 0);

    /* panel seam rings for shape reading */
    for (int i = 1; i < SHIP_RINGS - 1; i += 2)
        for (int k = 0; k < 8; k++) {
            int k2 = (k + 1) % 8;
            Q(ring[i][k], ring[i][k2], ring[i][k2], ring[i][k],
              0.38f, 0.42f, 0.50f, 0.0f, 0);
        }

    /* ---- canopy: tinted glass hump, self-lit teal ---- */
    Vector3 c0 = V3(-0.26f, 0.48f, 0.30f), c1 = V3(0.26f, 0.48f, 0.30f);
    Vector3 c2 = V3( 0.20f, 0.56f, 1.15f), c3 = V3(-0.20f, 0.56f, 1.15f);
    Vector3 c4 = V3( 0.10f, 0.50f, 1.85f), c5 = V3(-0.10f, 0.50f, 1.85f);
    Q(c0, c1, c2, c3, 0.45f, 0.88f, 0.95f, 0.5f, 1);
    Q(c3, c2, c4, c5, 0.45f, 0.88f, 0.95f, 0.5f, 1);
    Q(V3(-0.26f, 0.30f, 0.30f), c0, c3, V3(-0.12f, 0.38f, 1.70f), 0.30f, 0.62f, 0.70f, 0.35f, 1);
    Q(c1, V3(0.26f, 0.30f, 0.30f), V3(0.12f, 0.38f, 1.70f), c2, 0.30f, 0.62f, 0.70f, 0.35f, 1);

    /* ---- swept wings with dihedral ---- */
    for (int side = 0; side < 2; side++) {
        float s = side == 0 ? 1.0f : -1.0f;
        Vector3 w0 = V3(s * 0.55f, 0.10f, -0.60f);
        Vector3 w1 = V3(s * 0.55f, 0.10f, -1.90f);
        Vector3 w2 = V3(s * 2.60f, 0.46f, -2.55f);
        Vector3 w3 = V3(s * 2.35f, 0.42f, -0.95f);
        float wr = 0.50f, wg = 0.54f, wb = 0.62f;
        Q(w0, w3, w2, w1, wr, wg, wb, 0.0f, 0);                        /* top */
        Q(w1, w2, w3, w0, wr * DARK, wg * DARK, wb * DARK, 0.0f, 0);   /* bottom */
        /* wingtip nav light: red port / green starboard, self-lit */
        float lr = side == 0 ? 1.0f : 0.15f, lg = side == 0 ? 0.15f : 1.0f;
        Vector3 tip = V3(s * 2.52f, 0.45f, -1.75f);
        Q(V3(tip.x - s * 0.09f, tip.y - 0.03f, tip.z - 0.12f),
          V3(tip.x + s * 0.09f, tip.y - 0.03f, tip.z - 0.12f),
          V3(tip.x + s * 0.09f, tip.y + 0.03f, tip.z + 0.02f),
          V3(tip.x - s * 0.09f, tip.y + 0.03f, tip.z + 0.02f),
          lr, lg, 0.20f, 0.9f, 1);
        /* gold accent stripe */
        Q(V3(s * 1.05f, 0.14f, -1.05f), V3(s * 1.50f, 0.19f, -1.32f),
          V3(s * 1.58f, 0.19f, -1.92f), V3(s * 1.10f, 0.14f, -1.68f),
          0.85f, 0.70f, 0.30f, 0.25f, 1);
    }

    /* ---- twin engine nacelles + glowing exhausts ---- */
    for (int side = 0; side < 2; side++) {
        float s = side == 0 ? 1.0f : -1.0f;
        float ex = s * 0.52f, ez0 = -3.05f, ez1 = -4.25f;
        Vector3 front[8], back[8];
        for (int k = 0; k < 8; k++) {
            float an = 6.28318f * k / 8.0f;
            front[k] = V3(ex + cosf(an) * 0.24f, 0.02f + sinf(an) * 0.24f, ez0);
            back[k]  = V3(ex + cosf(an) * 0.20f, 0.02f + sinf(an) * 0.20f, ez1);
        }
        for (int k = 0; k < 8; k++) {
            int k2 = (k + 1) % 8;
            Q(front[k], front[k2], back[k2], back[k], 0.42f, 0.45f, 0.52f, 0.0f, 0);
        }
        /* glowing exhaust cone-disc */
        for (int k = 0; k < 8; k++) {
            int k2 = (k + 1) % 8;
            Vector3 g0 = V3(ex + cosf(6.28318f * k / 8.0f) * 0.17f,
                            0.02f + sinf(6.28318f * k / 8.0f) * 0.17f, ez1 + 0.02f);
            Vector3 g1 = V3(ex + cosf(6.28318f * (k + 1) / 8.0f) * 0.17f,
                            0.02f + sinf(6.28318f * (k + 1) / 8.0f) * 0.17f, ez1 + 0.02f);
            Vector3 g2 = V3(ex, 0.02f, ez1 + 0.30f);
            Q(back[k2], back[k], g2, g1, 0.55f, 0.95f, 1.0f, 1.0f, 1);
            Q(g1, g2, g2, g0, 0.55f, 0.95f, 1.0f, 1.0f, 1);
            (void)k2;
        }
        /* pylon to hull */
        Q(V3(s * 0.28f, 0.0f, -2.95f), V3(s * 0.44f, 0.0f, -3.85f),
          V3(s * 0.44f, 0.10f, -3.85f), V3(s * 0.28f, 0.10f, -2.95f),
          0.40f, 0.43f, 0.50f, 0.0f, 0);
    }
}

#undef SHIP_RINGS

void Ship_Init(void) {
    active = false;
    nextWarpAt = 14.0;      /* first pass ~14 s after world entry */
    trailNext = 0;
    for (int i = 0; i < SHIP_TRAIL; i++) trail[i].life = 0.0f;
}

void Ship_Shutdown(void) {
    active = false;
    meshReady = false;      /* rebuilt per session */
    hullCount = 0;
}

/* start a new pass across the sky near the player */
static void Ship_BeginPass(double now) {
    Vector3 pc = player.camera.position;
    float ang = (float)GetRandomValue(0, 3599) * 0.001745f;
    float exitAng = ang + 3.1416f + ((float)GetRandomValue(-500, 500) / 1000.0f);
    float h = 34.0f + (float)GetRandomValue(0, 220) / 10.0f;
    float sideOff = 46.0f + (float)GetRandomValue(0, 320) / 10.0f;
    Vector3 mid = V3(pc.x, pc.y + h, pc.z);
    Vector3 dirA = V3(cosf(ang), 0, sinf(ang));
    Vector3 side = V3(-dirA.z, 0, dirA.x);
    from = V3(mid.x + dirA.x * 150.0f + side.x * sideOff,
              mid.y + 6.0f,
              mid.z + dirA.z * 150.0f + side.z * sideOff);
    to = V3(mid.x - dirA.x * 150.0f + side.x * sideOff * 0.55f,
            mid.y - 3.0f,
            mid.z - dirA.z * 150.0f + side.z * sideOff * 0.55f);
    passDuration = 15.0 + (double)GetRandomValue(0, 800) / 100.0;
    passStart = now;
    passEnd = now + passDuration;
    active = true;
    bank = 0.0f;
    for (int i = 0; i < SHIP_TRAIL; i++) trail[i].life = 0.0f;
}

void Ship_Update(double now) {
    if (!active) {
        if (nextWarpAt <= 0.0) nextWarpAt = now + 20.0;
        if (now >= nextWarpAt) {
            Ship_BeginPass(now);
            nextWarpAt = now + 55.0 + (double)GetRandomValue(0, 4000) / 100.0;
        }
        return;
    }
    if (now >= passEnd) { active = false; return; }

    double k = (now - passStart) / passDuration;       /* 0..1 */
    Vector3 prev = shipPos;
    shipPos = V3(from.x + (to.x - from.x) * k,
                 from.y + (to.y - from.y) * k + sinf(k * 6.28318f) * 1.6f,
                 from.z + (to.z - from.z) * k);
    shipFwd = Vector3Normalize(Vector3Subtract(shipPos, prev));
    /* bank into the (imaginary) turn - gentle sway */
    bank = 0.22f * sinf(k * 6.28318f);

    /* trail */
    trail[trailNext].pos = V3(shipPos.x - shipFwd.x * 4.3f,
                              shipPos.y - shipFwd.y * 4.3f,
                              shipPos.z - shipFwd.z * 4.3f);
    trail[trailNext].life = 1.0f;
    trailNext = (trailNext + 1) % SHIP_TRAIL;
}

static void EmitWarpFlash(Vector3 p, Vector3 axis, float k) {
    /* vertical streak + expanding ring, k = 0..1 flash progress */
    float s = 1.0f - k;
    rlColor4ub((unsigned char)(200 * s), (unsigned char)(240 * s), 255, (unsigned char)(190 * s));
    rlVertex3f(p.x, p.y - 26.0f * s, p.z);
    rlVertex3f(p.x, p.y + 26.0f * s, p.z);
    rlColor4ub((unsigned char)(140 * s), (unsigned char)(200 * s), 255, (unsigned char)(140 * s));
    rlVertex3f(p.x - 9.0f * s, p.y, p.z);
    rlVertex3f(p.x + 9.0f * s, p.y, p.z);
    float rad = 22.0f * k;
    for (int i = 0; i < 10; i++) {
        float a0 = 6.28318f * i / 10.0f, a1 = 6.28318f * (i + 1) / 10.0f;
        rlColor4ub(120, 190, 255, (unsigned char)(120 * s));
        rlVertex3f(p.x + cosf(a0) * rad, p.y + sinf(a0) * rad * 0.35f, p.z);
        rlVertex3f(p.x + cosf(a1) * rad, p.y + sinf(a1) * rad * 0.35f, p.z);
    }
}

void Ship_Draw(double now) {
    if (!active) return;
    BuildMesh();

    double k = (now - passStart) / passDuration;
    Vector3 right = Vector3Normalize(V3(-shipFwd.z, 0, shipFwd.x));
    Vector3 up = V3(-shipFwd.y * right.x + right.y, 1.0f, -shipFwd.z * right.z);
    up = Vector3Normalize(up);
    float cb = cosf(bank), sb = sinf(bank);
    /* rotated up vector for bank */
    Vector3 upB = Vector3Add(Vector3Scale(up, cb), Vector3Scale(right, sb));
    Vector3 rightB = Vector3Add(Vector3Scale(right, cb), Vector3Scale(up, -sb));

    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();

    /* ---- solid hull with directional light + rim ---- */
    rlSetTexture(rlGetTextureIdDefault());
    rlBegin(RL_QUADS);
    Vector3 lightDir = Vector3Normalize(V3(0.45f, 0.80f, 0.35f));
    Vector3 toCam = Vector3Normalize(Vector3Subtract(player.camera.position, shipPos));
    for (int i = 0; i < hullCount; i++) {
        ShipQuad *q = &hull[i];
        /* world normal from the transformed first three vertices */
        Vector3 e1 = Vector3Subtract(q->v[1], q->v[0]);
        Vector3 e2 = Vector3Subtract(q->v[2], q->v[0]);
        Vector3 n = Vector3CrossProduct(
            Vector3Add(Vector3Scale(rightB, e1.x), Vector3Add(Vector3Scale(upB, e1.y), Vector3Scale(shipFwd, e1.z))),
            Vector3Add(Vector3Scale(rightB, e2.x), Vector3Add(Vector3Scale(upB, e2.y), Vector3Scale(shipFwd, e2.z))));
        n = Vector3Normalize(n);
        if (Vector3DotProduct(n, toCam) < 0.0f) n = Vector3Scale(n, -1.0f);
        float lit = 0.34f + 0.66f * (0.5f + 0.5f * Vector3DotProduct(n, lightDir));
        float rim = 0.22f * (1.0f - fabsf(Vector3DotProduct(n, toCam)));
        float glow = q->unlit ? (0.72f + 0.28f * sinf(now * 3.1f + i)) * (0.6f + 0.4f * q->shade) : 0.0f;
        float L = q->unlit ? (0.55f + 0.45f * q->shade) * glow : lit + rim;
        unsigned char r = (unsigned char)(q->r * L * 255.0f);
        unsigned char g = (unsigned char)(q->g * L * 255.0f);
        unsigned char b = (unsigned char)(q->b * L * 255.0f);
        rlColor4ub(r, g, b, 255);
        for (int vtx = 0; vtx < 4; vtx++) {
            Vector3 lv = q->v[vtx];
            Vector3 wp = Vector3Add(shipPos,
                Vector3Add(Vector3Add(Vector3Scale(rightB, lv.x),
                                      Vector3Scale(upB, lv.y)),
                           Vector3Scale(shipFwd, lv.z)));
            rlTexCoord2f(0.5f, 0.5f);
            rlVertex3f(wp.x, wp.y, wp.z);
        }
    }
    rlEnd();
    rlDrawRenderBatchActive();

    /* ---- glow pass: exhaust cones + trail + warp flashes (additive) ---- */
    rlSetBlendMode(BLEND_ADDITIVE);
    rlBegin(RL_QUADS);
    for (int i = 0; i < SHIP_TRAIL; i++) {
        ShipTrailPt *t = &trail[i];
        if (t->life <= 0.0f) continue;
        float a = t->life;
        Vector3 side = Vector3Scale(rightB, 0.16f * a);
        rlColor4ub((unsigned char)(60 * a), (unsigned char)(170 * a),
                   (unsigned char)(220 * a), (unsigned char)(120 * a));
        rlVertex3f(t->pos.x - side.x, t->pos.y - side.y, t->pos.z - side.z);
        rlVertex3f(t->pos.x + side.x, t->pos.y + side.y, t->pos.z + side.z);
        rlVertex3f(t->pos.x + side.x * 0.3f - shipFwd.x * 2.2f,
                   t->pos.y + side.y * 0.3f - shipFwd.y * 2.2f,
                   t->pos.z + side.z * 0.3f - shipFwd.z * 2.2f);
        rlVertex3f(t->pos.x - side.x * 0.3f - shipFwd.x * 2.2f,
                   t->pos.y - side.y * 0.3f - shipFwd.y * 2.2f,
                   t->pos.z - side.z * 0.3f - shipFwd.z * 2.2f);
    }
    /* twin exhaust glow blobs at the nacelles */
    for (int sideI = 0; sideI < 2; sideI++) {
        float s = sideI == 0 ? 1.0f : -1.0f;
        Vector3 e = Vector3Add(shipPos,
            Vector3Add(Vector3Add(Vector3Scale(rightB, s * 0.52f),
                                  Vector3Scale(shipFwd, -4.35f)),
                       Vector3Scale(upB, 0.02f)));
        float pulse = 0.75f + 0.25f * sinf(now * 17.0f + sideI);
        Vector3 rr_ = Vector3Scale(rightB, 0.34f * pulse);
        Vector3 uu = Vector3Scale(upB, 0.34f * pulse);
        rlColor4ub(90, 190, 255, 160);
        rlVertex3f(e.x - rr_.x - uu.x, e.y - rr_.y - uu.y, e.z - rr_.z - uu.z);
        rlVertex3f(e.x - rr_.x + uu.x, e.y - rr_.y + uu.y, e.z - rr_.z + uu.z);
        rlVertex3f(e.x + rr_.x + uu.x, e.y + rr_.y + uu.y, e.z + rr_.z + uu.z);
        rlVertex3f(e.x + rr_.x - uu.x, e.y + rr_.y - uu.y, e.z + rr_.z - uu.z);
    }
    rlEnd();
    rlDrawRenderBatchActive();

    /* warp flashes: first/last 0.9 s of the pass */
    rlBegin(RL_LINES);
    if (k < 0.06f) EmitWarpFlash(from, shipFwd, k / 0.06f);
    if (k > 0.94f) EmitWarpFlash(to, shipFwd, (k - 0.94f) / 0.06f);
    rlEnd();
    rlDrawRenderBatchActive();
    rlSetBlendMode(BLEND_ALPHA);
    rlSetTexture(0);
    rlEnableBackfaceCulling();

    /* age the trail */
    for (int i = 0; i < SHIP_TRAIL; i++)
        if (trail[i].life > 0.0f) trail[i].life -= 0.016f * 0.8f;
}
