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
static Mesh mesh;
static Material material;
static Shader shader;
static float windOffset;
static int meshBaseX, meshBaseZ;
static bool meshBuilt;

static int Ast_Wrap(int v) {
    v %= AST_PATTERN_SIZE;
    return v < 0 ? v + AST_PATTERN_SIZE : v;
}
static bool HasCell(int x, int z) {
    return pattern[Ast_Wrap(z) * AST_PATTERN_SIZE + Ast_Wrap(x)] != 0;
}

static void AddVertex(float *v, float *n, unsigned char *c, int *cnt, Vector3 p, Vector3 nm,
                      unsigned char r, unsigned char g, unsigned char b) {
    int vi = *cnt * 3, ci = *cnt * 4;
    v[vi] = p.x;
    v[vi + 1] = p.y;
    v[vi + 2] = p.z;
    n[vi] = nm.x;
    n[vi + 1] = nm.y;
    n[vi + 2] = nm.z;
    c[ci] = r;
    c[ci + 1] = g;
    c[ci + 2] = b;
    c[ci + 3] = 255;
    (*cnt)++;
}

static void AddTri(float *v, float *n, unsigned char *c, int *cnt, Vector3 a, Vector3 b, Vector3 d,
                   unsigned char r, unsigned char g, unsigned char bb) {
    Vector3 nm = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(d, a)));
    AddVertex(v, n, c, cnt, a, nm, r, g, bb);
    AddVertex(v, n, c, cnt, b, nm, r, g, bb);
    AddVertex(v, n, c, cnt, d, nm, r, g, bb);
}

static float Jitter(int x, int z, int k) {
    int h = x * 374761 + z * 668265 + k * 127412;
    h = (h ^ (h >> 13)) * 127412787;
    return ((h & 255) / 255.0f) * 2.0f - 1.0f;
}

static void AddAsteroid(float *v, float *n, unsigned char *c, int *cnt, int localX, int localZ,
                        int patX, int patZ) {
    int sc = scale[Ast_Wrap(patZ) * AST_PATTERN_SIZE + Ast_Wrap(patX)];
    int hc = heightClass[Ast_Wrap(patZ) * AST_PATTERN_SIZE + Ast_Wrap(patX)];
    float size = 2.2f + sc * 1.6f;
    float cx = localX * AST_CELL_SIZE + AST_CELL_SIZE * 0.5f;
    float cz = localZ * AST_CELL_SIZE + AST_CELL_SIZE * 0.5f;
    float cy = (hc - 1) * 38.0f + Jitter(patX, patZ, 9) * 10.0f;

    Vector3 p[6];
    p[0] = (Vector3){cx, cy + size * (0.9f + Jitter(patX, patZ, 1) * 0.25f), cz};
    p[1] = (Vector3){cx, cy - size * (0.9f + Jitter(patX, patZ, 2) * 0.25f), cz};
    p[2] = (Vector3){cx + size * (1.0f + Jitter(patX, patZ, 3) * 0.35f), cy, cz};
    p[3] = (Vector3){cx - size * (1.0f + Jitter(patX, patZ, 4) * 0.35f), cy, cz};
    p[4] = (Vector3){cx, cy, cz + size * (1.0f + Jitter(patX, patZ, 5) * 0.35f)};
    p[5] = (Vector3){cx, cy, cz - size * (1.0f + Jitter(patX, patZ, 6) * 0.35f)};

    unsigned char r = (unsigned char)(62 + (patX * 13 + patZ * 7) % 40);
    unsigned char g = (unsigned char)(48 + (patX * 5 + patZ * 17) % 28);
    unsigned char b = (unsigned char)(88 + (patX * 11 + patZ * 3) % 50);

    AddTri(v, n, c, cnt, p[0], p[2], p[4], (unsigned char)(r + 25), (unsigned char)(g + 18), (unsigned char)(b + 20));
    AddTri(v, n, c, cnt, p[0], p[4], p[3], (unsigned char)(r + 18), (unsigned char)(g + 10), b);
    AddTri(v, n, c, cnt, p[0], p[3], p[5], (unsigned char)(r + 12), g, b);
    AddTri(v, n, c, cnt, p[0], p[5], p[2], (unsigned char)(r + 20), (unsigned char)(g + 8), (unsigned char)(b + 10));
    AddTri(v, n, c, cnt, p[1], p[4], p[2], (unsigned char)(r - 10), (unsigned char)(g - 8), (unsigned char)(b - 6));
    AddTri(v, n, c, cnt, p[1], p[3], p[4], (unsigned char)(r - 14), (unsigned char)(g - 10), (unsigned char)(b - 8));
    AddTri(v, n, c, cnt, p[1], p[5], p[3], (unsigned char)(r - 8), (unsigned char)(g - 6), b);
    AddTri(v, n, c, cnt, p[1], p[2], p[5], (unsigned char)(r - 6), g, (unsigned char)(b - 4));
}

static void RebuildMesh(int baseX, int baseZ) {
    if (meshBuilt) UnloadMesh(mesh);
    mesh = (Mesh){0};
    float *vertices = MemAlloc(AST_MAX_VERTICES * 3 * sizeof(float));
    float *normals = MemAlloc(AST_MAX_VERTICES * 3 * sizeof(float));
    unsigned char *colors = MemAlloc(AST_MAX_VERTICES * 4);
    int vertexCount = 0;
    for (int z = -AST_RADIUS; z <= AST_RADIUS; z++) {
        for (int x = -AST_RADIUS; x <= AST_RADIUS; x++) {
            if (HasCell(baseX + x, baseZ + z)) {
                AddAsteroid(vertices, normals, colors, &vertexCount, x, z, baseX + x, baseZ + z);
            }
        }
    }
    mesh.vertexCount = vertexCount;
    mesh.triangleCount = vertexCount / 3;
    mesh.vertices = vertices;
    mesh.normals = normals;
    mesh.colors = colors;
    mesh.texcoords = MemAlloc((size_t)vertexCount * 2 * sizeof(float));
    memset(mesh.texcoords, 0, (size_t)vertexCount * 2 * sizeof(float));
    UploadMesh(&mesh, false);
    meshBuilt = true;
    meshBaseX = baseX;
    meshBaseZ = baseZ;
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
#if defined(PLATFORM_WEB)
    const char *vs =
#include "chunk/shaders/cloud_shader_gl100.vs"
        ;
    const char *fs =
#include "chunk/shaders/cloud_shader_gl100.fs"
        ;
#else
    const char *vs =
#include "chunk/shaders/cloud_shader.vs"
        ;
    const char *fs =
#include "chunk/shaders/cloud_shader.fs"
        ;
#endif
    shader = LoadShaderFromMemory(vs, fs);
    material = LoadMaterialDefault();
    material.shader = shader;
    RebuildMesh(0, 0);
}

void Asteroid_Shutdown(void) {
    if (meshBuilt) UnloadMesh(mesh);
    UnloadMaterial(material);
    UnloadShader(shader);
    meshBuilt = false;
}

void Asteroid_Update(float dt) { windOffset += AST_SPEED * dt; }

void Asteroid_Draw(Vector3 cameraPosition, float sunlightStrength) {
    int baseX = (int)floorf((cameraPosition.x - windOffset) / AST_CELL_SIZE);
    int baseZ = (int)floorf(cameraPosition.z / AST_CELL_SIZE);
    if (baseX != meshBaseX || baseZ != meshBaseZ) RebuildMesh(baseX, baseZ);

    unsigned char br = (unsigned char)(100.0f + 110.0f * sunlightStrength);
    material.maps[MATERIAL_MAP_DIFFUSE].color =
        (Color){br, (unsigned char)(br * 0.82f), (unsigned char)fminf(255.0f, br * 1.2f), 255};

    float fogEnd = AST_RADIUS * AST_CELL_SIZE;
    float fogStart = fogEnd * 0.55f;
    float fogColor[3] = {0.07f + 0.04f * sunlightStrength, 0.03f + 0.02f * sunlightStrength,
                         0.14f + 0.06f * sunlightStrength};
    Color liquidTint;
    if (Player_GetCameraLiquidTint(&liquidTint)) {
        fogStart = 10.0f;
        fogEnd = 32.0f;
        fogColor[0] = liquidTint.r / 255.0f;
        fogColor[1] = liquidTint.g / 255.0f;
        fogColor[2] = liquidTint.b / 255.0f;
    }
    SetShaderValue(shader, GetShaderLocation(shader, "cameraPosition"), &cameraPosition,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, GetShaderLocation(shader, "fogColor"), fogColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, GetShaderLocation(shader, "fogStart"), &fogStart, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, GetShaderLocation(shader, "fogEnd"), &fogEnd, SHADER_UNIFORM_FLOAT);

    Matrix t = MatrixTranslate(baseX * AST_CELL_SIZE + windOffset, 148.0f,
                               baseZ * AST_CELL_SIZE);
    rlDisableBackfaceCulling();
    DrawMesh(mesh, material, t);
    rlEnableBackfaceCulling();
}
