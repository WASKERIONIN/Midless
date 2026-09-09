/*
 * Starfield — a procedural sky of drifting stars and faint nebula glow.
 *
 * Stars are deterministic: each one is hashed from its cell index and
 * the world seed, so the sky never flickers when the camera moves.
 * The field is rendered as a single GL_POINTS draw call inside a large
 * sphere that always surrounds the camera.
 */

#include <math.h>
#include <string.h>
#include <stdint.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "starfield.h"

#define STAR_COUNT      2400
#define STAR_RADIUS     450.0f   /* far enough to always envelop chunks */
#define NEBULA_BANDS    3

static Mesh     starMesh;
static bool     starMeshBuilt;
static float    starDrift;

/* Cheap integer hash — same spirit as the worldgen seed mixer. */
static uint32_t StarHash(uint32_t i, uint32_t salt)
{
    uint32_t h = i ^ salt;
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    return h ^ (h >> 16);
}

static float ToUnit(uint32_t h) { return (h >> 8) * (1.0f / 16777216.0f); }

static void BuildStars(void)
{
    if (starMeshBuilt) UnloadMesh(starMesh);
    starMesh = (Mesh){0};

    float *positions = MemAlloc(STAR_COUNT * 3 * sizeof(float));
    unsigned char *colors = MemAlloc(STAR_COUNT * 4);

    /* Palette: warm yellow-white stars, cool blue stars, rare magenta. */
    for (int i = 0; i < STAR_COUNT; i++) {
        /* Uniform-on-sphere via Marsaglia. */
        float u = ToUnit(StarHash((uint32_t)i, 1u)) * 2.0f - 1.0f;
        float v = ToUnit(StarHash((uint32_t)i, 2u)) * 2.0f - 1.0f;
        float w = ToUnit(StarHash((uint32_t)i, 3u)) * 2.0f - 1.0f;
        float len = sqrtf(u*u + v*v + w*w);
        if (len < 0.0001f) { u = 1; v = 0; w = 0; len = 1; }
        u /= len; v /= len; w /= len;

        positions[i*3+0] = u * STAR_RADIUS;
        positions[i*3+1] = v * STAR_RADIUS;
        positions[i*3+2] = w * STAR_RADIUS;

        float colorRoll = ToUnit(StarHash((uint32_t)i, 4u));
        float bright    = 0.55f + ToUnit(StarHash((uint32_t)i, 5u)) * 0.45f;
        unsigned char r, g, b;
        if (colorRoll < 0.55f) {
            /* white/yellow */
            r = (unsigned char)(255.0f * bright);
            g = (unsigned char)(240.0f * bright);
            b = (unsigned char)(200.0f * bright);
        } else if (colorRoll < 0.85f) {
            /* cool blue */
            r = (unsigned char)(160.0f * bright);
            g = (unsigned char)(200.0f * bright);
            b = (unsigned char)(255.0f * bright);
        } else {
            /* magenta/pink — rare and magical */
            r = (unsigned char)(255.0f * bright);
            g = (unsigned char)(140.0f * bright);
            b = (unsigned char)(230.0f * bright);
        }
        colors[i*4+0] = r; colors[i*4+1] = g; colors[i*4+2] = b; colors[i*4+3] = 255;
    }

    starMesh.vertexCount  = STAR_COUNT;
    starMesh.triangleCount = 0;
    starMesh.vertices = positions;
    starMesh.colors   = colors;
    starMesh.normals  = MemAlloc(STAR_COUNT * 3 * sizeof(float));
    starMesh.texcoords = MemAlloc(STAR_COUNT * 2 * sizeof(float));
    memset(starMesh.normals, 0, STAR_COUNT * 3 * sizeof(float));
    memset(starMesh.texcoords, 0, STAR_COUNT * 2 * sizeof(float));
    UploadMesh(&starMesh, false);
    starMeshBuilt = true;
}

void Starfield_Init(void)   { starDrift = 0; BuildStars(); }
void Starfield_Shutdown(void) { if (starMeshBuilt) UnloadMesh(starMesh); starMeshBuilt = false; }
void Starfield_Update(float dt) { starDrift += dt * 0.02f; }

void Starfield_Draw(Vector3 cameraPosition, float sunlightStrength)
{
    if (!starMeshBuilt) return;

    /* Stars dim during "day" so the cosmic night is most vivid. */
    float alpha = 1.0f - fmaxf(0.0f, sunlightStrength - 0.4f) * 1.2f;
    if (alpha < 0.15f) alpha = 0.15f;

    rlEnableShader(starMesh.vaoId > 0 ? rlGetShaderIdDefault() : rlGetShaderIdDefault());

    /* Re-centre the star sphere on the camera so stars never clip. */
    rlPushMatrix();
    rlTranslatef(cameraPosition.x, cameraPosition.y, cameraPosition.z);
    rlRotatef(starDrift * 10.0f, 0.0f, 1.0f, 0.0f);

    /* Use rlDrawVertexArray for GL_POINTS — bypasses Raylib's mesh
     * pipeline which expects triangles.  Fall back to a simple loop. */
    rlSetLineWidth(2.0f);
    rlBegin(RL_POINTS);
    for (int i = 0; i < STAR_COUNT; i++) {
        float px = starMesh.vertices[i*3+0];
        float py = starMesh.vertices[i*3+1];
        float pz = starMesh.vertices[i*3+2];
        unsigned char r = (unsigned char)(starMesh.colors[i*4+0] * alpha);
        unsigned char g = (unsigned char)(starMesh.colors[i*4+1] * alpha);
        unsigned char b = (unsigned char)(starMesh.colors[i*4+2] * alpha);
        rlColor4ub(r, g, b, 255);
        rlVertex3f(px, py, pz);
    }
    rlEnd();

    /* A couple of wide soft "nebula" quads behind the stars — cheap but
     * evocative bands of teal/violet/magenta glow. */
    static const float nebula[NEBULA_BANDS][3] = {
        { 60.0f/255.0f,  20.0f/255.0f, 120.0f/255.0f }, /* violet */
        { 20.0f/255.0f,  80.0f/255.0f, 120.0f/255.0f }, /* teal   */
        {160.0f/255.0f,  40.0f/255.0f, 120.0f/255.0f }, /* magenta*/
    };
    float nebAlpha = 0.18f * alpha;
    rlBegin(RL_QUADS);
    for (int b = 0; b < NEBULA_BANDS; b++) {
        float bandY = (b - 1) * STAR_RADIUS * 0.35f;
        float bandSize = STAR_RADIUS * 0.9f;
        unsigned char nr = (unsigned char)(nebula[b][0] * 255.0f);
        unsigned char ng = (unsigned char)(nebula[b][1] * 255.0f);
        unsigned char nb = (unsigned char)(nebula[b][2] * 255.0f);
        unsigned char a  = (unsigned char)(nebAlpha * 255.0f);
        rlColor4ub(nr, ng, nb, a);
        rlVertex3f(-bandSize, bandY, -bandSize);
        rlVertex3f( bandSize, bandY, -bandSize);
        rlVertex3f( bandSize, bandY,  bandSize);
        rlVertex3f(-bandSize, bandY,  bandSize);
    }
    rlEnd();

    rlPopMatrix();
}
