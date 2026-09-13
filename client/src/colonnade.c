/*
 * Midless: Cosmic Edition - the pocket peristyle (v65.16).
 * Geometry is built once at init in pocket-local coordinates
 * (origin at the pocket centre, lawn top at y=154) and drawn with a
 * single translation to POCKETFX_CX/CZ while the pocket factor holds.
 */
#include "colonnade.h"
#include "pocketfx.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <stdlib.h>

#define COL_N      64                 /* columns around the ring */
#define COL_R      62.0f              /* ring radius (lawn half is 64) */
#define COL_BASE   153.6f
#define SPRING_Y   158.40f            /* arch springing line */

/* ---------------- mesh builder ---------------- */
typedef struct {
    float *v, *n;
    unsigned short *idx;
    int vn, in, capV, capI;
} MB;

static void mbInit(MB *m, int maxV, int maxI) {
    m->v = (float *)malloc(sizeof(float) * 3 * maxV);
    m->n = (float *)malloc(sizeof(float) * 3 * maxV);
    m->idx = (unsigned short *)malloc(sizeof(unsigned short) * maxI);
    m->vn = m->in = 0;
    m->capV = maxV;
    m->capI = maxI;
}
static void mbTri(MB *m, const float a[3], const float b[3], const float c[3]) {
    if (m->vn + 3 > m->capV || m->in + 3 > m->capI) return;
    float u[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
    float w[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
    float nrm[3] = {
        u[1] * w[2] - u[2] * w[1],
        u[2] * w[0] - u[0] * w[2],
        u[0] * w[1] - u[1] * w[0],
    };
    float len = sqrtf(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (len < 1e-8f) { nrm[0] = 0; nrm[1] = 1; nrm[2] = 0; len = 1; }
    nrm[0] /= len; nrm[1] /= len; nrm[2] /= len;
    const float *p[3] = { a, b, c };
    unsigned short base = (unsigned short)m->vn;
    for (int i = 0; i < 3; i++) {
        m->v[3 * m->vn + 0] = p[i][0];
        m->v[3 * m->vn + 1] = p[i][1];
        m->v[3 * m->vn + 2] = p[i][2];
        m->n[3 * m->vn + 0] = nrm[0];
        m->n[3 * m->vn + 1] = nrm[1];
        m->n[3 * m->vn + 2] = nrm[2];
        m->vn++;
    }
    m->idx[m->in++] = base;
    m->idx[m->in++] = (unsigned short)(base + 1);
    m->idx[m->in++] = (unsigned short)(base + 2);
}
static void mbQuad(MB *m, const float a[3], const float b[3], const float c[3], const float d[3]) {
    mbTri(m, a, b, c);
    mbTri(m, a, c, d);
}
/* vertical cylinder, open bottom, capped top */
static void mbCyl(MB *m, float cx, float cz, float y0, float y1, float r, int seg) {
    for (int i = 0; i < seg; i++) {
        float a0 = (float)i / seg * 2.0f * (float)PI;
        float a1 = (float)(i + 1) / seg * 2.0f * (float)PI;
        float p0[3] = { cx + cosf(a0) * r, y0, cz + sinf(a0) * r };
        float p1[3] = { cx + cosf(a1) * r, y0, cz + sinf(a1) * r };
        float p2[3] = { cx + cosf(a1) * r, y1, cz + sinf(a1) * r };
        float p3[3] = { cx + cosf(a0) * r, y1, cz + sinf(a0) * r };
        mbQuad(m, p0, p1, p2, p3);
        float c0[3] = { cx, y1, cz };
        mbTri(m, c0, p3, p2);
    }
}
static void mbBox(MB *m, float cx, float cy, float cz, float hx, float hy, float hz) {
    float x0 = cx - hx, x1 = cx + hx, y0 = cy - hy, y1 = cy + hy, z0 = cz - hz, z1 = cz + hz;
    float a[3], b[3], c[3], d[3];
    /* +Y */ a[0]=x0;a[1]=y1;a[2]=z0; b[0]=x1;b[1]=y1;b[2]=z0; c[0]=x1;c[1]=y1;c[2]=z1; d[0]=x0;d[1]=y1;d[2]=z1; mbQuad(m,a,b,c,d);
    /* -Y */ a[0]=x0;a[1]=y0;a[2]=z0; b[0]=x0;b[1]=y0;b[2]=z1; c[0]=x1;c[1]=y0;c[2]=z1; d[0]=x1;d[1]=y0;d[2]=z0; mbQuad(m,a,b,c,d);
    /* +X */ a[0]=x1;a[1]=y0;a[2]=z0; b[0]=x1;b[1]=y0;b[2]=z1; c[0]=x1;c[1]=y1;c[2]=z1; d[0]=x1;d[1]=y1;d[2]=z0; mbQuad(m,a,b,c,d);
    /* -X */ a[0]=x0;a[1]=y0;a[2]=z0; b[0]=x0;b[1]=y1;b[2]=z0; c[0]=x0;c[1]=y1;c[2]=z1; d[0]=x0;d[1]=y0;d[2]=z1; mbQuad(m,a,b,c,d);
    /* +Z */ a[0]=x0;a[1]=y0;a[2]=z1; b[0]=x1;b[1]=y0;b[2]=z1; c[0]=x1;c[1]=y1;c[2]=z1; d[0]=x0;d[1]=y1;d[2]=z1; mbQuad(m,a,b,c,d);
    /* -Z */ a[0]=x0;a[1]=y0;a[2]=z0; b[0]=x0;b[1]=y1;b[2]=z0; c[0]=x1;c[1]=y1;c[2]=z0; d[0]=x1;d[1]=y0;d[2]=z0; mbQuad(m,a,b,c,d);
}
/* half-annulus band: round arch between two column tops.
 * C = springing centre, T = chord direction, Rd = ring-out direction */
static void mbArch(MB *m, const float C[3], const float T[3], const float Rd[3],
                   float rIn, float rOut, float w, int seg) {
    for (int i = 0; i < seg; i++) {
        float t0 = (float)i / seg * (float)PI;
        float t1 = (float)(i + 1) / seg * (float)PI;
        float P[2][2][2][3];   /* [radius][side][theta] */
        for (int r = 0; r < 2; r++) {
            float rad = r ? rOut : rIn;
            for (int s = 0; s < 2; s++) {
                float sw = (s ? 0.5f : -0.5f) * w;
                for (int k = 0; k < 2; k++) {
                    float th = k ? t1 : t0;
                    float dx = T[0] * cosf(th) * rad + Rd[0] * sw;
                    float dy = sinf(th) * rad;
                    float dz = T[2] * cosf(th) * rad + Rd[2] * sw;
                    P[r][s][k][0] = C[0] + dx;
                    P[r][s][k][1] = C[1] + dy;
                    P[r][s][k][2] = C[2] + dz;
                }
            }
        }
        /* side faces (flat half-annulus cheeks) */
        mbQuad(m, P[0][0][0], P[1][0][0], P[1][0][1], P[0][0][1]);
        mbQuad(m, P[0][1][0], P[1][1][0], P[1][1][1], P[0][1][1]);
        /* outer and inner soffits */
        mbQuad(m, P[1][0][0], P[1][1][0], P[1][1][1], P[1][0][1]);
        mbQuad(m, P[0][0][0], P[0][1][0], P[0][1][1], P[0][0][1]);
    }
}

/* ---------------- baked marble shading ---------------- */
static const float SUN_DIR[3] = { 0.35f, 0.85f, 0.30f };

/* per-vertex colour bytes: two-sided lambert baked at build time.
 * v65.18: NO UploadMesh, NO VAO, NO custom shader, NO DrawMesh - the
 * v65.16/17 crossing crashes lived on that path. The terrain has
 * drawn through rlgl's immediate batch since v1, so the peristyle
 * joins it: CPU positions + baked colours, rlColor3ub/rlVertex3f. */
static unsigned char *vcol;
static float *vpos;
static unsigned short *vidx;
static int vCount, iCount;
static bool ready;

void Colonnade_Init(void) {
    MB m;
    mbInit(&m, 60000, 60000 * 3);

    /* stylobate: the low ring the columns stand on */
    {
        int seg = 96;
        float r0 = COL_R - 0.85f, r1 = COL_R + 0.85f;
        float y0 = 153.75f, y1 = 154.15f;
        for (int i = 0; i < seg; i++) {
            float a0 = (float)i / seg * 2.0f * (float)PI;
            float a1 = (float)(i + 1) / seg * 2.0f * (float)PI;
            float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
            float o0[3] = { c0 * r1, y0, s0 * r1 }, o1[3] = { c1 * r1, y0, s1 * r1 };
            float o2[3] = { c1 * r1, y1, s1 * r1 }, o3[3] = { c0 * r1, y1, s0 * r1 };
            float i0[3] = { c0 * r0, y0, s0 * r0 }, i1[3] = { c1 * r0, y0, s1 * r0 };
            float i2[3] = { c1 * r0, y1, s1 * r0 }, i3[3] = { c0 * r0, y1, s0 * r0 };
            mbQuad(&m, o0, o1, o2, o3);
            mbQuad(&m, i0, i1, i2, i3);
            mbQuad(&m, o3, o2, i2, i3);
        }
    }

    float step = 2.0f * (float)PI / COL_N;
    float chord = 2.0f * COL_R * sinf(step * 0.5f);
    for (int i = 0; i < COL_N; i++) {
        float a = i * step;
        float cx = cosf(a) * COL_R, cz = sinf(a) * COL_R;
        /* plinth, fluted-look shaft, echinus, abacus */
        mbCyl(&m, cx, cz, COL_BASE, 154.05f, 0.62f, 12);
        mbCyl(&m, cx, cz, 154.05f, 157.90f, 0.42f, 12);
        mbCyl(&m, cx, cz, 157.90f, SPRING_Y - 0.20f, 0.60f, 12);
        mbBox(&m, cx, (SPRING_Y - 0.20f + SPRING_Y) * 0.5f, cz, 0.66f, 0.10f, 0.66f);
    }
    for (int i = 0; i < COL_N; i++) {
        float am = (i + 0.5f) * step;
        float C[3] = { cosf(am) * COL_R, SPRING_Y - 0.05f, sinf(am) * COL_R };
        float T[3] = { -sinf(am), 0.0f, cosf(am) };
        float Rd[3] = { cosf(am), 0.0f, sinf(am) };
        float rOut = chord * 0.5f;
        mbArch(&m, C, T, Rd, rOut - 0.55f, rOut, 0.70f, 12);
    }

    vCount = m.vn;
    iCount = m.in;
    vpos = m.v;
    vidx = m.idx;
    vcol = (unsigned char *)malloc(sizeof(unsigned char) * 3 * vCount);
    for (int i = 0; i < vCount; i++) {
        float nx = m.n[3 * i], ny = m.n[3 * i + 1], nz = m.n[3 * i + 2];
        float d = fabsf(nx * SUN_DIR[0] + ny * SUN_DIR[1] + nz * SUN_DIR[2]);
        float l = 0.85f * (0.46f + 0.54f * d);   /* pocket sun, calm */
        vcol[3 * i + 0] = (unsigned char)(235.0f * l);
        vcol[3 * i + 1] = (unsigned char)(231.0f * l);
        vcol[3 * i + 2] = (unsigned char)(222.0f * l);
    }
    free(m.n);
    ready = vpos != NULL && vcol != NULL && vidx != NULL;
    TraceLog(LOG_INFO, "COLONNADE init: verts=%d tris=%d ready=%d (immediate batch path)",
        vCount, iCount / 3, (int)ready);
}

void Colonnade_Draw(float pocketFactor) {
    if (!ready || pocketFactor <= 0.5f) return;
    static bool loggedFirst = false;
    if (!loggedFirst) {
        TraceLog(LOG_INFO, "COLONNADE: first draw at factor %.2f", pocketFactor);
        loggedFirst = true;
    }
    rlBegin(RL_TRIANGLES);
        for (int i = 0; i < iCount; i++) {
            unsigned short vi = vidx[i];
            rlColor4ub(vcol[3 * vi], vcol[3 * vi + 1], vcol[3 * vi + 2], 255);
            rlVertex3f(vpos[3 * vi] + POCKETFX_CX, vpos[3 * vi + 1], vpos[3 * vi + 2] + POCKETFX_CZ);
        }
    rlEnd();
}
