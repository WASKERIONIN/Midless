#include <math.h>
#include <string.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "FastNoiseLite.h"
#include "asteroid.h"
#include "player.h"

#define AST_PATTERN_SIZE 64
#define AST_RADIUS 18
#define AST_CELL_SIZE 22.0f
#define AST_SPEED 0.28f
#define AST_MAX_VERTICES ((AST_RADIUS * 2 + 1) * (AST_RADIUS * 2 + 1) * 216)

static unsigned char pattern[AST_PATTERN_SIZE * AST_PATTERN_SIZE];
static unsigned char scale[AST_PATTERN_SIZE * AST_PATTERN_SIZE];
static unsigned char heightClass[AST_PATTERN_SIZE * AST_PATTERN_SIZE];
static float windOffset;
static float beltTime;

static int Ast_Wrap(int v) {
    v %= AST_PATTERN_SIZE;
    return v < 0 ? v + AST_PATTERN_SIZE : v;
}
static bool HasCell(int x, int z) {
    return pattern[Ast_Wrap(z) * AST_PATTERN_SIZE + Ast_Wrap(x)] != 0;
}

static float Jitter(int x, int z, int k) {
    int h = x * 374761 + z * 668265 + k * 127412;
    h = (h ^ (h >> 13)) * 127412787;
    return ((h & 255) / 255.0f) * 2.0f - 1.0f;
}

static float smoothstep_f(float a, float b, float x) {
    float t = (x - a) / (b - a);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

/* ---- v44: black & white wireframe sky ---------------------------------
 * The asteroid belt is drawn as tumbling wireframe polyhedra (octahedra,
 * cubes, icosahedra) - pure lines, no surfaces, in the Y2K vector style.
 * Slow rotation and a breathing pulse keep the sky alive. */

typedef struct WireShape {
    int vertexCount;
    int edgeCount;
    const Vector3 *vertices;
    const unsigned char *edges;
} WireShape;

/* octahedron */
static const Vector3 OCTA_V[6] = {
    { 0,  1,  0}, { 0, -1,  0}, { 1, 0, 0}, {-1, 0, 0}, {0, 0,  1}, {0, 0, -1}
};
static const unsigned char OCTA_E[12][2] = {
    {0,2},{0,3},{0,4},{0,5},{1,2},{1,3},{1,4},{1,5},{2,4},{4,3},{3,5},{5,2}
};

/* cube */
static const Vector3 CUBE_V[8] = {
    {-1,-1,-1},{ 1,-1,-1},{ 1, 1,-1},{-1, 1,-1},
    {-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}
};
static const unsigned char CUBE_E[12][2] = {
    {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
};

/* icosahedron (golden-ratio construction) */
static const Vector3 ICO_V[12] = {
    {-1,  0.618f, 0}, { 1,  0.618f, 0}, {-1, -0.618f, 0}, { 1, -0.618f, 0},
    { 0, -1,  0.618f}, { 0,  1,  0.618f}, { 0, -1, -0.618f}, { 0,  1, -0.618f},
    { 0.618f, 0, -1}, { 0.618f, 0,  1}, {-0.618f, 0, -1}, {-0.618f, 0,  1}
};
static const unsigned char ICO_E[30][2] = {
    {0,1},{0,5},{0,7},{0,10},{0,11},{1,5},{1,7},{1,8},{1,9},{2,3},
    {2,4},{2,6},{2,10},{2,11},{3,4},{3,6},{3,8},{3,9},{4,5},{4,9},
    {4,11},{5,9},{5,11},{6,7},{6,8},{6,10},{7,10},{8,9},{8,10},{9,11}
};

static const WireShape kShapes[3] = {
    { 6, 12, OCTA_V, &OCTA_E[0][0] },
    { 8, 12, CUBE_V, &CUBE_E[0][0] },
    {12, 30, ICO_V,  &ICO_E[0][0] },
};

static Vector3 WireRotate(Vector3 v, float ax, float ay) {
    /* rotate around X then Y */
    float cx = cosf(ax), sx = sinf(ax), cy = cosf(ay), sy = sinf(ay);
    float y = v.y * cx - v.z * sx;
    float z = v.y * sx + v.z * cx;
    float x = v.x * cy + z * sy;
    z = -v.x * sy + z * cy;
    return (Vector3){ x, y, z };
}

static void DrawWireShape(Vector3 center, const WireShape *shape, float scale,
                          Vector3 axisScale, float ax, float ay, unsigned char bright) {
    Vector3 rotated[16];
    for (int i = 0; i < shape->vertexCount; i++) {
        Vector3 v = WireRotate(shape->vertices[i], ax, ay);
        rotated[i] = (Vector3){
            center.x + v.x * scale * axisScale.x,
            center.y + v.y * scale * axisScale.y,
            center.z + v.z * scale * axisScale.z
        };
    }
    rlColor4ub(bright, bright, (unsigned char)(bright + 10 > 255 ? 255 : bright + 10), 255);
    for (int e = 0; e < shape->edgeCount; e++) {
        Vector3 a = rotated[shape->edges[e * 2]];
        Vector3 b = rotated[shape->edges[e * 2 + 1]];
        rlVertex3f(a.x, a.y, a.z);
        rlVertex3f(b.x, b.y, b.z);
    }
}

void Asteroid_Init(void) {
    fnl_state noise = fnlCreateState();
    noise.seed = 0xC057E1;
    noise.noise_type = FNL_NOISE_OPENSIMPLEX2S;
    noise.fractal_type = FNL_FRACTAL_FBM;
    noise.octaves = 3;
    noise.frequency = 0.10f;
    for (int z = 0; z < AST_PATTERN_SIZE; z++) {
        for (int x = 0; x < AST_PATTERN_SIZE; x++) {
            float n = fnlGetNoise2D(&noise, (float)x, (float)z);
            bool present = n > 0.42f;
            pattern[z * AST_PATTERN_SIZE + x] = present ? 1 : 0;
            scale[z * AST_PATTERN_SIZE + x] =
                present ? (unsigned char)(1 + ((int)((n - 0.22f) * 5.0f) % 3)) : 0;
            heightClass[z * AST_PATTERN_SIZE + x] =
                present ? (unsigned char)(((x * 17 + z * 29) % 3)) : 0;
        }
    }
}

void Asteroid_Shutdown(void) {
}

void Asteroid_Update(float dt) {
    windOffset += AST_SPEED * dt;
    beltTime += dt;
}

void Asteroid_Draw(Vector3 cameraPosition, float sunlightStrength) {
    int baseX = (int)floorf((cameraPosition.x - windOffset) / AST_CELL_SIZE);
    int baseZ = (int)floorf(cameraPosition.z / AST_CELL_SIZE);

    /* distance fade: belt dissolves into the void long before the grid ends */
    float fadeEnd = AST_RADIUS * AST_CELL_SIZE * 0.85f;
    float fadeStart = fadeEnd * 0.55f;

    rlDrawRenderBatchActive();
    rlSetBlendMode(BLEND_ALPHA);
    rlBegin(RL_LINES);
    for (int z = -AST_RADIUS; z <= AST_RADIUS; z++) {
        for (int x = -AST_RADIUS; x <= AST_RADIUS; x++) {
            int patX = baseX + x, patZ = baseZ + z;
            if (!HasCell(patX, patZ)) continue;
            int sc = scale[Ast_Wrap(patZ) * AST_PATTERN_SIZE + Ast_Wrap(patX)];
            int hc = heightClass[Ast_Wrap(patZ) * AST_PATTERN_SIZE + Ast_Wrap(patX)];
            float size = 2.2f + sc * 1.6f;
            float cxx = (baseX + x) * AST_CELL_SIZE + AST_CELL_SIZE * 0.5f + windOffset;
            float czz = (baseZ + z) * AST_CELL_SIZE + AST_CELL_SIZE * 0.5f;
            float cyy = 148.0f + (hc - 1) * 38.0f + Jitter(patX, patZ, 9) * 10.0f
                      + sinf(beltTime * 0.16f) * 2.2f;

            float dx = cxx - cameraPosition.x, dz = czz - cameraPosition.z;
            float dist = sqrtf(dx * dx + dz * dz);
            if (dist > fadeEnd) continue;
            float fade = 1.0f - smoothstep_f(fadeStart, fadeEnd, dist);
            unsigned char bright = (unsigned char)((90.0f + 70.0f * sunlightStrength) * fade + 40.0f);

            int variant = (int)(((patX & 3) * 7 + (patZ & 3) * 5) % 3);
            float phase = Jitter(patX, patZ, 1) * 3.1f;
            float ax = beltTime * (0.10f + 0.05f * phase) + phase;
            float ay = beltTime * (0.14f - 0.04f * phase) - phase * 2.0f;
            float pulse = 1.0f + 0.04f * sinf(beltTime * 0.9f + phase * 5.0f);
            Vector3 axisScale = {
                1.0f + Jitter(patX, patZ, 3) * 0.35f,
                0.62f + Jitter(patX, patZ, 2) * 0.2f,
                1.0f + Jitter(patX, patZ, 5) * 0.35f
            };
            DrawWireShape((Vector3){ cxx, cyy, czz }, &kShapes[variant],
                          size * 0.62f * pulse, axisScale, ax, ay, bright);
        }
    }
    rlEnd();
    rlDrawRenderBatchActive();
}


