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
static Texture2D rockTex;
static float windOffset;
static float beltTime;
static int meshBaseX, meshBaseZ;
static bool meshBuilt;

static int Ast_Wrap(int v) {
    v %= AST_PATTERN_SIZE;
    return v < 0 ? v + AST_PATTERN_SIZE : v;
}
static bool HasCell(int x, int z) {
    return pattern[Ast_Wrap(z) * AST_PATTERN_SIZE + Ast_Wrap(x)] != 0;
}

static void AddVertex(float *v, float *n, unsigned char *c, float *uv, int *cnt, Vector3 p, Vector3 nm,
                      unsigned char r, unsigned char g, unsigned char b, float u, float vt) {
    int vi = *cnt * 3, ci = *cnt * 4, ui = *cnt * 2;
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
    uv[ui] = u;
    uv[ui + 1] = vt;
    (*cnt)++;
}

static void AddTri(float *v, float *n, unsigned char *c, float *uv, int *cnt,
                   Vector3 a, Vector3 b, Vector3 d, Vector2 ta, Vector2 tb, Vector2 td,
                   unsigned char r, unsigned char g, unsigned char bb) {
    Vector3 nm = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(d, a)));
    AddVertex(v, n, c, uv, cnt, a, nm, r, g, bb, ta.x, ta.y);
    AddVertex(v, n, c, uv, cnt, b, nm, r, g, bb, tb.x, tb.y);
    AddVertex(v, n, c, uv, cnt, d, nm, r, g, bb, td.x, td.y);
}

static float Jitter(int x, int z, int k) {
    int h = x * 374761 + z * 668265 + k * 127412;
    h = (h ^ (h >> 13)) * 127412787;
    return ((h & 255) / 255.0f) * 2.0f - 1.0f;
}

/* ---- v43.5: rocky asteroid bodies ------------------------------------- *
 * Each asteroid is a noise-displaced UV sphere carrying a slice of a shared
 * procedural rock texture (craters, mineral veins, gold flecks), so nothing
 * reads as a bare triangle primitive any more. */

static float RockHash3(int x, int y, int z, int seed) {
    int h = x * 374761 + y * 668265 + z * 2147483647u + seed * 1442695040u;
    h = (h ^ (h >> 13)) * 1274126177;
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
}

static float RockNoise3(float x, float y, float z, int seed) {
    int xi = (int)floorf(x), yi = (int)floorf(y), zi = (int)floorf(z);
    float xf = x - xi, yf = y - yi, zf = z - zi;
    xf = xf * xf * (3.0f - 2.0f * xf);
    yf = yf * yf * (3.0f - 2.0f * yf);
    zf = zf * zf * (3.0f - 2.0f * zf);
    float c000 = RockHash3(xi, yi, zi, seed),     c100 = RockHash3(xi + 1, yi, zi, seed);
    float c010 = RockHash3(xi, yi + 1, zi, seed), c110 = RockHash3(xi + 1, yi + 1, zi, seed);
    float c001 = RockHash3(xi, yi, zi + 1, seed), c101 = RockHash3(xi + 1, yi, zi + 1, seed);
    float c011 = RockHash3(xi, yi + 1, zi + 1, seed), c111 = RockHash3(xi + 1, yi + 1, zi + 1, seed);
    float x00 = c000 + (c100 - c000) * xf, x10 = c010 + (c110 - c010) * xf;
    float x01 = c001 + (c101 - c001) * xf, x11 = c011 + (c111 - c011) * xf;
    float y0 = x00 + (x10 - x00) * yf, y1 = x01 + (x11 - x01) * yf;
    return y0 + (y1 - y0) * zf;
}

static float RockFbm3(float x, float y, float z, int seed, int octaves) {
    float sum = 0.0f, amp = 0.5f, freq = 1.0f, norm = 0.0f;
    for (int o = 0; o < octaves; o++) {
        sum += RockNoise3(x * freq, y * freq, z * freq, seed + o * 37) * amp;
        norm += amp;
        amp *= 0.55f;
        freq *= 2.05f;
    }
    return sum / norm;
}

static Texture2D MakeRockTexture(void) {
    const int size = 128;
    const int seed = 0xA57E12;
    Image image = GenImageColor(size, size, BLANK);
    Color *pixels = (Color *)image.data;

    /* base: horizontally-tileable fbm rock (cylindrical sampling) */
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float ang = 6.2831f * x / size;
            float px = cosf(ang) * 5.0f, pz = sinf(ang) * 5.0f, py = y * 9.0f / size;
            float n = RockFbm3(px, py, pz, seed, 4);
            float grain = RockNoise3(px * 4.0f, py * 4.0f, pz * 4.0f, seed + 11) * 0.25f;
            float v = 0.55f + 0.75f * n + grain;
            int r = (int)(52 * v + 14);
            int g = (int)(44 * v + 11);
            int b = (int)(96 * v + 26);
            /* teal mineral veins along ridged noise */
            float vein = 1.0f - fabsf(2.0f * RockFbm3(px * 1.6f, py * 1.6f, pz * 1.6f, seed + 23, 3) - 1.0f);
            if (vein > 0.78f) {
                float k = (vein - 0.78f) / 0.22f;
                r = (int)(r * (1.0f - k) + 40 * k);
                g = (int)(g * (1.0f - k) + 190 * k);
                b = (int)(b * (1.0f - k) + 180 * k);
            }
            r = (r < 0) ? 0 : (r > 255 ? 255 : r);
            g = (g < 0) ? 0 : (g > 255 ? 255 : g);
            b = (b < 0) ? 0 : (b > 255 ? 255 : b);
            pixels[y * size + x] = (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
        }
    }

    /* craters: dark bowl, bright rim, wrapped horizontally */
    for (int c = 0; c < 11; c++) {
        float ccx = RockHash3(c, 1, 7, seed) * size;
        float ccy = 8.0f + RockHash3(c, 2, 7, seed) * (size - 16.0f);
        float rad = 3.0f + RockHash3(c, 3, 7, seed) * 7.0f;
        for (int dy = -(int)rad - 1; dy <= (int)rad + 1; dy++) {
            for (int dx = -(int)rad - 1; dx <= (int)rad + 1; dx++) {
                float d = sqrtf(dx * dx + dy * dy);
                if (d > rad) continue;
                int px = (int)(ccx + dx) % size;
                if (px < 0) px += size;
                int py = (int)ccy + dy;
                if (py < 0 || py >= size) continue;
                Color p = pixels[py * size + px];
                float t = d / rad;
                float bowl = 1.0f - 0.45f * (1.0f - t) * (1.0f - t);
                float rim = (t > 0.72f) ? 1.0f + 0.38f * (t - 0.72f) / 0.28f : 1.0f;
                float m = bowl * rim;
                p.r = (unsigned char)(p.r * m > 255 ? 255 : p.r * m);
                p.g = (unsigned char)(p.g * m > 255 ? 255 : p.g * m);
                p.b = (unsigned char)(p.b * m > 255 ? 255 : p.b * m);
                pixels[py * size + px] = p;
            }
        }
    }

    /* sparse gold flecks */
    for (int c = 0; c < 26; c++) {
        int px = (int)(RockHash3(c, 5, 9, seed) * size);
        int py = (int)(RockHash3(c, 6, 9, seed) * size);
        pixels[py * size + px] = (Color){ 255, 196, 96, 255 };
    }

    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
    return texture;
}

static void AddAsteroid(float *v, float *n, unsigned char *c, float *uv, int *cnt, int localX, int localZ,
                        int patX, int patZ) {
    int sc = scale[Ast_Wrap(patZ) * AST_PATTERN_SIZE + Ast_Wrap(patX)];
    int hc = heightClass[Ast_Wrap(patZ) * AST_PATTERN_SIZE + Ast_Wrap(patX)];
    float size = 2.2f + sc * 1.6f;
    float cx = localX * AST_CELL_SIZE + AST_CELL_SIZE * 0.5f;
    float cz = localZ * AST_CELL_SIZE + AST_CELL_SIZE * 0.5f;
    float cy = (hc - 1) * 38.0f + Jitter(patX, patZ, 9) * 10.0f;

    /* per-asteroid identity: seed, lumpy axis scale, tint */
    int seed = (patX * 73856093) ^ (patZ * 19349663) ^ 0x5EED;
    float sx = size * (1.0f + Jitter(patX, patZ, 3) * 0.30f);
    float sy = size * (0.72f + Jitter(patX, patZ, 2) * 0.18f);
    float sz = size * (1.0f + Jitter(patX, patZ, 5) * 0.30f);
    unsigned char tr = (unsigned char)(170 + (patX * 13 + patZ * 7) % 24);
    unsigned char tg = (unsigned char)(162 + (patX * 5 + patZ * 17) % 20);
    unsigned char tb = (unsigned char)(188 + (patX * 11 + patZ * 3) % 26);

    const int SLICES = 8, STACKS = 4;   /* rings at bands 1..STACKS-1, caps at poles */
    Vector3 band[16], prev[16];
    Vector2 bandUV[16], prevUV[16];
    Vector3 top = (Vector3){ cx, cy + sy * (0.9f + Jitter(patX, patZ, 1) * 0.22f), cz };
    Vector3 bot = (Vector3){ cx, cy - sy * (0.85f + Jitter(patX, patZ, 6) * 0.22f), cz };

    for (int s = 1; s <= STACKS; s++) {
        float ny = 1.0f - 2.0f * s / (float)STACKS;
        float ringR = sqrtf(1.0f - ny * ny);
        for (int j = 0; j < SLICES; j++) {
            float nx = cosf(6.2831f * j / SLICES) * ringR;
            float nz = sinf(6.2831f * j / SLICES) * ringR;
            float d = RockFbm3(nx * 1.7f + seed * 0.001f, ny * 1.7f, nz * 1.7f - seed * 0.0007f,
                               seed & 0xFFFF, 3);
            float radius = 0.82f + 0.5f * d;
            band[j] = (Vector3){ cx + nx * radius * sx, cy + ny * radius * sy, cz + nz * radius * sz };
            bandUV[j] = (Vector2){ j / (float)SLICES + 0.5f / SLICES, s / (float)STACKS };
        }

        /* shading: top of the body catches the nebula light */
        float bandShade = 196.0f - 76.0f * s / (float)STACKS;
        unsigned char sr = (unsigned char)(tr * bandShade / 190);
        unsigned char sg = (unsigned char)(tg * bandShade / 190);
        unsigned char sb = (unsigned char)(tb * bandShade / 190);

        if (s == 1) {
            /* top cap fan around the north pole */
            unsigned char cr = (unsigned char)(tr * 216 / 190);
            unsigned char cg = (unsigned char)(tg * 216 / 190);
            unsigned char cb = (unsigned char)(tb * 216 / 190);
            for (int j = 0; j < SLICES; j++) {
                int j2 = (j + 1) % SLICES;
                Vector2 t0 = { bandUV[j].x - 0.5f / SLICES, 0.04f };
                Vector2 t1 = { bandUV[j2].x - 0.5f / SLICES, 0.04f };
                AddTri(v, n, c, uv, cnt, top, band[j2], band[j],
                       (Vector2){ 0.5f, 0.0f }, t1, t0, cr, cg, cb);
            }
        }
        if (s > 1) {
            /* body quads between this band and the previous one */
            for (int j = 0; j < SLICES; j++) {
                int j2 = (j + 1) % SLICES;
                AddTri(v, n, c, uv, cnt, band[j], prev[j], band[j2],
                       bandUV[j], prevUV[j], bandUV[j2], sr, sg, sb);
                AddTri(v, n, c, uv, cnt, band[j2], prev[j], prev[j2],
                       bandUV[j2], prevUV[j], prevUV[j2], sr, sg, sb);
            }
        }
        for (int j = 0; j < SLICES; j++) { prev[j] = band[j]; prevUV[j] = bandUV[j]; }
    }

    /* bottom cap fan around the south pole */
    unsigned char br2 = (unsigned char)(tr * 96 / 190);
    unsigned char bg2 = (unsigned char)(tg * 96 / 190);
    unsigned char bb2 = (unsigned char)(tb * 96 / 190);
    for (int j = 0; j < SLICES; j++) {
        int j2 = (j + 1) % SLICES;
        Vector2 b0 = { prevUV[j].x - 0.5f / SLICES, 0.96f };
        Vector2 b1 = { prevUV[j2].x - 0.5f / SLICES, 0.96f };
        AddTri(v, n, c, uv, cnt, bot, prev[j], prev[j2],
               (Vector2){ 0.5f, 1.0f }, b0, b1, br2, bg2, bb2);
    }
}

static void RebuildMesh(int baseX, int baseZ) {
    if (meshBuilt) UnloadMesh(mesh);
    mesh = (Mesh){0};
    float *vertices = MemAlloc(AST_MAX_VERTICES * 3 * sizeof(float));
    float *normals = MemAlloc(AST_MAX_VERTICES * 3 * sizeof(float));
    float *texcoords = MemAlloc(AST_MAX_VERTICES * 2 * sizeof(float));
    unsigned char *colors = MemAlloc(AST_MAX_VERTICES * 4);
    int vertexCount = 0;
    for (int z = -AST_RADIUS; z <= AST_RADIUS; z++) {
        for (int x = -AST_RADIUS; x <= AST_RADIUS; x++) {
            if (HasCell(baseX + x, baseZ + z)) {
                AddAsteroid(vertices, normals, colors, texcoords, &vertexCount, x, z, baseX + x, baseZ + z);
            }
        }
    }
    mesh.vertexCount = vertexCount;
    mesh.triangleCount = vertexCount / 3;
    mesh.vertices = vertices;
    mesh.normals = normals;
    mesh.colors = colors;
    mesh.texcoords = texcoords;
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
    rockTex = MakeRockTexture();
    material.maps[MATERIAL_MAP_DIFFUSE].texture = rockTex;
    RebuildMesh(0, 0);
}

void Asteroid_Shutdown(void) {
    if (meshBuilt) UnloadMesh(mesh);
    UnloadTexture(rockTex);
    UnloadMaterial(material);
    UnloadShader(shader);
    meshBuilt = false;
}

void Asteroid_Update(float dt) {
    windOffset += AST_SPEED * dt;
    beltTime += dt;
}

void Asteroid_Draw(Vector3 cameraPosition, float sunlightStrength) {
    int baseX = (int)floorf((cameraPosition.x - windOffset) / AST_CELL_SIZE);
    int baseZ = (int)floorf(cameraPosition.z / AST_CELL_SIZE);
    if (baseX != meshBaseX || baseZ != meshBaseZ) RebuildMesh(baseX, baseZ);

    unsigned char br = (unsigned char)(105.0f + 115.0f * sunlightStrength);
    material.maps[MATERIAL_MAP_DIFFUSE].color =
        (Color){br, (unsigned char)(br * 0.88f), (unsigned char)fminf(255.0f, br * 1.22f), 255};

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

    Matrix t = MatrixTranslate(baseX * AST_CELL_SIZE + windOffset,
                               148.0f + sinf(beltTime * 0.16f) * 2.2f,
                               baseZ * AST_CELL_SIZE);
    rlDisableBackfaceCulling();
    DrawMesh(mesh, material, t);
    rlEnableBackfaceCulling();
}
