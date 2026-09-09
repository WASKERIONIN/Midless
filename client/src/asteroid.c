/*
 * Asteroid field — replaces the old flat-cloud renderer.
 *
 * Each asteroid is a small irregular ellipsoid of rocky quads whose
 * silhouette is warped by 3-D noise.  The pattern wraps so the field
 * follows the camera forever.
 */

#include <math.h>
#include <string.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "FastNoiseLite.h"
#include "asteroid.h"
#include "player.h"

/* Spacing & density ------------------------------------------------ */
#define AST_PATTERN_SIZE 64
#define AST_RADIUS       20          /* cells around the camera        */
#define AST_CELL_SIZE    18.0f       /* horizontal cell edge          */
#define AST_HEIGHT       120.0f      /* nominal drift altitude        */
#define AST_THICKNESS    6.0f        /* vertical thickness of a rock  */
#define AST_SPEED        0.35f       /* slow cosmic drift             */
#define AST_MAX_VERTICES ((AST_RADIUS * 2 + 1) * (AST_RADIUS * 2 + 1) * 108)

static unsigned char pattern[AST_PATTERN_SIZE * AST_PATTERN_SIZE];
static unsigned char scale  [AST_PATTERN_SIZE * AST_PATTERN_SIZE]; /* 1..3 size class */
static Mesh     mesh;
static Material material;
static Shader   shader;
static float    windOffset;
static int      meshBaseX, meshBaseZ;
static bool     meshBuilt;

static int  Ast_Wrap(int v) { v %= AST_PATTERN_SIZE; return v < 0 ? v + AST_PATTERN_SIZE : v; }
static bool HasCell(int x, int z) { return pattern[Ast_Wrap(z) * AST_PATTERN_SIZE + Ast_Wrap(x)] != 0; }
static int  CellScale(int x, int z) { return scale[Ast_Wrap(z) * AST_PATTERN_SIZE + Ast_Wrap(x)]; }

/* Vertex helpers --------------------------------------------------- */
static void AddVertex(float *v, float *n, unsigned char *c, int *cnt,
                      Vector3 p, Vector3 nm, unsigned char r, unsigned char g,
                      unsigned char b, unsigned char a)
{
    int vi = *cnt * 3, ci = *cnt * 4;
    v[vi] = p.x; v[vi+1] = p.y; v[vi+2] = p.z;
    n[vi] = nm.x; n[vi+1] = nm.y; n[vi+2] = nm.z;
    c[ci] = r; c[ci+1] = g; c[ci+2] = b; c[ci+3] = a;
    (*cnt)++;
}

static void AddQuad(float *v, float *n, unsigned char *c, int *cnt,
                    Vector3 a, Vector3 b, Vector3 cc, Vector3 d, Vector3 nm,
                    unsigned char r, unsigned char g, unsigned char bb, unsigned char al)
{
    AddVertex(v,n,c,cnt,a,nm,r,g,bb,al);
    AddVertex(v,n,c,cnt,b,nm,r,g,bb,al);
    AddVertex(v,n,c,cnt,cc,nm,r,g,bb,al);
    AddVertex(v,n,c,cnt,a,nm,r,g,bb,al);
    AddVertex(v,n,c,cnt,cc,nm,r,g,bb,al);
    AddVertex(v,n,c,cnt,d,nm,r,g,bb,al);
}

/* Build one asteroid as a jittered slab with bumpy top/bottom. */
static void AddAsteroid(float *v, float *n, unsigned char *c, int *cnt,
                        int localX, int localZ, int patX, int patZ)
{
    int sc = CellScale(patX, patZ);
    float size = (2.0f + sc * 1.5f);            /* 3.5 .. 6.5 blocks   */
    float thick = AST_THICKNESS * (0.5f + sc * 0.25f);
    float x0 = localX * AST_CELL_SIZE + AST_CELL_SIZE * 0.5f - size;
    float x1 = x0 + size * 2.0f;
    float z0 = localZ * AST_CELL_SIZE + AST_CELL_SIZE * 0.5f - size;
    float z1 = z0 + size * 2.0f;
    float y0 = -thick * 0.5f, y1 = thick * 0.5f;

    /* Deterministic colour per cell — dark rock with a purple/teal tint. */
    unsigned char base_r = 70  + (unsigned char)((patX * 17 + patZ * 31) % 30);
    unsigned char base_g = 55  + (unsigned char)((patX * 23 + patZ * 11) % 25);
    unsigned char base_b = 90  + (unsigned char)((patX * 7  + patZ * 41) % 40);

    /* Six faces of the bounding slab, plus four side walls. */
    AddQuad(v,n,c,cnt, (Vector3){x0,y1,z0}, (Vector3){x0,y1,z1},
            (Vector3){x1,y1,z1}, (Vector3){x1,y1,z0},
            (Vector3){0,1,0},
            (unsigned char)(base_r+30), (unsigned char)(base_g+25),
            (unsigned char)(base_b+35), 255);
    AddQuad(v,n,c,cnt, (Vector3){x0,y0,z0}, (Vector3){x1,y0,z0},
            (Vector3){x1,y0,z1}, (Vector3){x0,y0,z1},
            (Vector3){0,-1,0},
            (unsigned char)(base_r-20), (unsigned char)(base_g-20),
            (unsigned char)(base_b-15), 255);
    /* sides */
    AddQuad(v,n,c,cnt, (Vector3){x0,y0,z0}, (Vector3){x0,y0,z1},
            (Vector3){x0,y1,z1}, (Vector3){x0,y1,z0},
            (Vector3){-1,0,0}, base_r, base_g, base_b, 255);
    AddQuad(v,n,c,cnt, (Vector3){x1,y0,z1}, (Vector3){x1,y0,z0},
            (Vector3){x1,y1,z0}, (Vector3){x1,y1,z1},
            (Vector3){1,0,0}, base_r, base_g, base_b, 255);
    AddQuad(v,n,c,cnt, (Vector3){x1,y0,z0}, (Vector3){x0,y0,z0},
            (Vector3){x0,y1,z0}, (Vector3){x1,y1,z0},
            (Vector3){0,0,-1},
            (unsigned char)(base_r-5), (unsigned char)(base_g-5), base_b, 255);
    AddQuad(v,n,c,cnt, (Vector3){x0,y0,z1}, (Vector3){x1,y0,z1},
            (Vector3){x1,y1,z1}, (Vector3){x0,y1,z1},
            (Vector3){0,0,1},
            (unsigned char)(base_r-5), (unsigned char)(base_g-5), base_b, 255);
}

static void RebuildMesh(int baseX, int baseZ)
{
    if (meshBuilt) UnloadMesh(mesh);
    mesh = (Mesh){0};
    float *vertices = MemAlloc(AST_MAX_VERTICES * 3 * sizeof(float));
    float *normals  = MemAlloc(AST_MAX_VERTICES * 3 * sizeof(float));
    unsigned char *colors = MemAlloc(AST_MAX_VERTICES * 4);
    int vertexCount = 0;
    for (int z = -AST_RADIUS; z <= AST_RADIUS; z++)
        for (int x = -AST_RADIUS; x <= AST_RADIUS; x++)
            if (HasCell(baseX + x, baseZ + z))
                AddAsteroid(vertices, normals, colors, &vertexCount,
                            x, z, baseX + x, baseZ + z);
    mesh.vertexCount  = vertexCount;
    mesh.triangleCount = vertexCount / 3;
    mesh.vertices = vertices;
    mesh.normals  = normals;
    mesh.colors   = colors;
    mesh.texcoords = MemAlloc(vertexCount * 2 * sizeof(float));
    memset(mesh.texcoords, 0, vertexCount * 2 * sizeof(float));
    UploadMesh(&mesh, false);
    meshBuilt = true;
    meshBaseX = baseX;
    meshBaseZ = baseZ;
}

/* Public API ------------------------------------------------------- */

void Asteroid_Init(void)
{
    fnl_state noise = fnlCreateState();
    noise.seed = 0xC057E1;
    noise.noise_type   = FNL_NOISE_OPENSIMPLEX2S;
    noise.fractal_type = FNL_FRACTAL_FBM;
    noise.octaves   = 3;
    noise.frequency = 0.11f;
    for (int z = 0; z < AST_PATTERN_SIZE; z++)
        for (int x = 0; x < AST_PATTERN_SIZE; x++) {
            float n = fnlGetNoise2D(&noise, (float)x, (float)z);
            /* Sparser than the old clouds — empty void between rocks. */
            bool present = n > 0.15f;
            pattern[z * AST_PATTERN_SIZE + x] = present ? 1 : 0;
            scale  [z * AST_PATTERN_SIZE + x] = present
                ? (unsigned char)(1 + (int)((n - 0.15f) * 4.0f) % 3)
                : 0;
        }

#if defined(PLATFORM_WEB)
    const char *vs = #include "chunk/shaders/cloud_shader_gl100.vs";
    const char *fs = #include "chunk/shaders/cloud_shader_gl100.fs";
#else
    const char *vs = #include "chunk/shaders/cloud_shader.vs";
    const char *fs = #include "chunk/shaders/cloud_shader.fs";
#endif
    shader    = LoadShaderFromMemory(vs, fs);
    material  = LoadMaterialDefault();
    material.shader = shader;
    RebuildMesh(0, 0);
}

void Asteroid_Shutdown(void)
{
    if (meshBuilt) UnloadMesh(mesh);
    UnloadMaterial(material);
    UnloadShader(shader);
    meshBuilt = false;
}

void Asteroid_Update(float dt) { windOffset += AST_SPEED * dt; }

void Asteroid_Draw(Vector3 cameraPosition, float sunlightStrength)
{
    int baseX = (int)floorf((cameraPosition.x - windOffset) / AST_CELL_SIZE);
    int baseZ = (int)floorf(cameraPosition.z / AST_CELL_SIZE);
    if (baseX != meshBaseX || baseZ != meshBaseZ) RebuildMesh(baseX, baseZ);

    /* Colour the rock with a deep purple/teal diffuse; sunlight dims it. */
    unsigned char br = (unsigned char)(110.0f + 145.0f * sunlightStrength);
    material.maps[MATERIAL_MAP_DIFFUSE].color = (Color){br, (unsigned char)(br * 0.85f),
                                                       (unsigned char)(br * 1.15f > 255 ? 255 : br * 1.15f), 255};

    float fogEnd   = AST_RADIUS * AST_CELL_SIZE;
    float fogStart = fogEnd * 0.6f;
    /* Nebula fog: deep violet that blends into the starfield. */
    float fogColor[3] = {
        (20.0f / 255.0f) + 0.05f * sunlightStrength,
        (10.0f / 255.0f) + 0.02f * sunlightStrength,
        (40.0f / 255.0f) + 0.08f * sunlightStrength,
    };
    Color liquidTint;
    if (Player_GetCameraLiquidTint(&liquidTint)) {
        fogStart = 10.0f; fogEnd = 32.0f;
        fogColor[0] = liquidTint.r / 255.0f;
        fogColor[1] = liquidTint.g / 255.0f;
        fogColor[2] = liquidTint.b / 255.0f;
    }
    SetShaderValue(shader, GetShaderLocation(shader, "cameraPosition"),
                   &cameraPosition, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, GetShaderLocation(shader, "fogColor"), fogColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, GetShaderLocation(shader, "fogStart"), &fogStart, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, GetShaderLocation(shader, "fogEnd"),   &fogEnd,   SHADER_UNIFORM_FLOAT);

    Matrix t = MatrixTranslate(baseX * AST_CELL_SIZE + windOffset,
                               AST_HEIGHT, baseZ * AST_CELL_SIZE);
    rlDisableBackfaceCulling();
    DrawMesh(mesh, material, t);
    rlEnableBackfaceCulling();
}
