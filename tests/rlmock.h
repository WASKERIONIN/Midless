/**
 * rlmock.h - exact behavioral model of raylib 4.5.0's GLSL 3.3 rlgl
 * immediate-mode batching, ported from rlgl.h 4.5.0 (rlBegin, rlSetTexture,
 * rlVertex3f, rlCheckRenderBatchLimit, rlDrawRenderBatch, rlSetShader,
 * rlSetBlendMode). No GPU needed: it tracks the same draw-record / vertex
 * counter / alignment arithmetic the real renderer performs and FLAGS the
 * two failure modes that broke the moth pollen:
 *
 *  - "SHEAR": a RL_QUADS draw record whose vertexCount is not a multiple
 *    of 4. rlgl renders quads with glDrawElements(count/4*6) over a shared
 *    index buffer and never re-aligns QUADS records (vertexAlignment is 0
 *    for them), so every later record in the same batch reads shifted
 *    vertices -> the historic "rainbow lines across the whole screen".
 *  - "OVERFLOW": more vertices than the batch buffer or more draw records
 *    than RL_DEFAULT_BATCH_DRAWCALLS, to exercise rlgl's mid-primitive
 *    flush paths (the ones that used to split quads into half-quads).
 */
#ifndef RLMOCK_H
#define RLMOCK_H

#include <stdio.h>
#include <string.h>

/* GL primitive ids, same values rlgl uses */
#ifndef RL_LINES
#define RL_LINES     0x0001
#endif
#ifndef RL_TRIANGLES
#define RL_TRIANGLES 0x0004
#endif
#ifndef RL_QUADS
#define RL_QUADS     0x0007
#endif

#define RLMOCK_MAX_DRAWS       256    /* RL_DEFAULT_BATCH_DRAWCALLS */
#define RLMOCK_BUFFER_ELEMENTS 8192   /* RL_DEFAULT_BATCH_BUFFER_ELEMENTS */

typedef struct RLMockDraw {
    int mode;
    int vertexCount;
    unsigned int textureId;
    int vertexAlignment;
} RLMockDraw;

typedef struct RLMockStats {
    int shearEvents;       /* misaligned QUADS records drawn */
    int overflowFlushes;   /* mid-primitive buffer-limit flushes */
    int drawsExecuted;     /* total draw records processed */
    int batchesFlushed;
} RLMockStats;

static RLMockDraw RLM_draws[RLMOCK_MAX_DRAWS];
static int RLM_drawCounter = 1;
static int RLM_vertexCounter = 0;
static int RLM_elementCount = RLMOCK_BUFFER_ELEMENTS;  /* test may shrink */
static int RLM_maxDraws = RLMOCK_MAX_DRAWS;            /* test may shrink */
static unsigned int RLM_defaultTextureId = 111;
static unsigned int RLM_defaultShaderId = 222;
static unsigned int RLM_currentShaderId = 222;
static int RLM_currentBlendMode = 0;

static RLMockStats RLM_stats;
static char RLM_lastError[256];

static void RLM_Reset(void) {
    memset(RLM_draws, 0, sizeof(RLM_draws));
    for (int i = 0; i < RLMOCK_MAX_DRAWS; i++) RLM_draws[i].textureId = RLM_defaultTextureId;
    for (int i = 0; i < RLMOCK_MAX_DRAWS; i++) RLM_draws[i].mode = RL_QUADS;
    RLM_drawCounter = 1;
    RLM_vertexCounter = 0;
    memset(&RLM_stats, 0, sizeof(RLM_stats));
    RLM_lastError[0] = 0;
    RLM_currentShaderId = RLM_defaultShaderId;
}

/* ---- internals (ported) ---- */

static void RLM_DrawRenderBatch(void) {
    /* draw phase: replay the record walk from rlglDraw/rlDrawRenderBatch */
    if (RLM_vertexCounter > 0) {
        int vertexOffset = 0;
        for (int i = 0; i < RLM_drawCounter; i++) {
            if (RLM_draws[i].vertexCount > 0) {
                RLM_stats.drawsExecuted++;
                if (RLM_draws[i].mode == RL_LINES || RLM_draws[i].mode == RL_TRIANGLES) {
                    /* glDrawArrays - any vertex count is legal */
                } else {
                    /* glDrawElements(GL_TRIANGLES, vertexCount/4*6, offset=vertexOffset/4*6) */
                    if (RLM_draws[i].vertexCount % 4 != 0) {
                        RLM_stats.shearEvents++;
                        snprintf(RLM_lastError, sizeof(RLM_lastError),
                                 "SHEAR: QUADS record %d has vertexCount %d (not %%4); "
                                 "later records in this batch read shifted vertices",
                                 i, RLM_draws[i].vertexCount);
                    }
                }
            }
            vertexOffset += RLM_draws[i].vertexCount + RLM_draws[i].vertexAlignment;
        }
    }
    /* reset phase (exact) */
    RLM_vertexCounter = 0;
    for (int i = 0; i < RLMOCK_MAX_DRAWS; i++) {
        RLM_draws[i].mode = RL_QUADS;
        RLM_draws[i].vertexCount = 0;
        RLM_draws[i].textureId = RLM_defaultTextureId;
        RLM_draws[i].vertexAlignment = 0;
    }
    RLM_drawCounter = 1;
    RLM_stats.batchesFlushed++;
}

static int RLM_CheckRenderBatchLimit(int vCount) {
    int overflow = 0;
    if ((RLM_vertexCounter + vCount) >= (RLM_elementCount * 4)) {
        overflow = 1;
        RLM_stats.overflowFlushes++;
        int currentMode = RLM_draws[RLM_drawCounter - 1].mode;
        unsigned int currentTexture = RLM_draws[RLM_drawCounter - 1].textureId;
        RLM_DrawRenderBatch();
        RLM_draws[RLM_drawCounter - 1].mode = currentMode;
        RLM_draws[RLM_drawCounter - 1].textureId = currentTexture;
    }
    return overflow;
}

/* ---- rlgl API surface used by the game (exact ports) ---- */

static void rlBegin(int mode) {
    if (RLM_draws[RLM_drawCounter - 1].mode != mode) {
        if (RLM_draws[RLM_drawCounter - 1].vertexCount > 0) {
            if (RLM_draws[RLM_drawCounter - 1].mode == RL_LINES)
                RLM_draws[RLM_drawCounter - 1].vertexAlignment =
                    (RLM_draws[RLM_drawCounter - 1].vertexCount < 4)
                        ? RLM_draws[RLM_drawCounter - 1].vertexCount
                        : RLM_draws[RLM_drawCounter - 1].vertexCount % 4;
            else if (RLM_draws[RLM_drawCounter - 1].mode == RL_TRIANGLES)
                RLM_draws[RLM_drawCounter - 1].vertexAlignment =
                    (RLM_draws[RLM_drawCounter - 1].vertexCount < 4)
                        ? 1
                        : 4 - (RLM_draws[RLM_drawCounter - 1].vertexCount % 4);
            else
                RLM_draws[RLM_drawCounter - 1].vertexAlignment = 0;

            if (!RLM_CheckRenderBatchLimit(RLM_draws[RLM_drawCounter - 1].vertexAlignment)) {
                RLM_vertexCounter += RLM_draws[RLM_drawCounter - 1].vertexAlignment;
                RLM_drawCounter++;
            }
        }
        if (RLM_drawCounter >= RLM_maxDraws) RLM_DrawRenderBatch();
        RLM_draws[RLM_drawCounter - 1].mode = mode;
        RLM_draws[RLM_drawCounter - 1].vertexCount = 0;
        RLM_draws[RLM_drawCounter - 1].textureId = RLM_defaultTextureId;
    }
}

static void rlEnd(void) { /* GL 3.3 path: no-op, exactly like rlgl 4.5 */ }

static void rlColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    (void)r; (void)g; (void)b; (void)a; /* stored per-vertex in real rlgl */
}

static void rlTexCoord2f(float x, float y) { (void)x; (void)y; }

static void rlVertex3f(float x, float y, float z) {
    (void)x; (void)y; (void)z;
    if (RLM_vertexCounter > (RLM_elementCount * 4 - 4)) {
        if ((RLM_draws[RLM_drawCounter - 1].mode == RL_LINES) &&
            (RLM_draws[RLM_drawCounter - 1].vertexCount % 2 == 0)) {
            RLM_CheckRenderBatchLimit(2 + 1);
        } else if ((RLM_draws[RLM_drawCounter - 1].mode == RL_TRIANGLES) &&
                   (RLM_draws[RLM_drawCounter - 1].vertexCount % 3 == 0)) {
            RLM_CheckRenderBatchLimit(3 + 1);
        } else if ((RLM_draws[RLM_drawCounter - 1].mode == RL_QUADS) &&
                   (RLM_draws[RLM_drawCounter - 1].vertexCount % 4 == 0)) {
            RLM_CheckRenderBatchLimit(4 + 1);
        }
    }
    RLM_vertexCounter++;
    RLM_draws[RLM_drawCounter - 1].vertexCount++;
}

static void rlSetTexture(unsigned int id) {
    if (id == 0) {
        /* real rlgl only forces a flush when the buffer is exactly full */
        if (RLM_vertexCounter >= RLM_elementCount * 4) RLM_DrawRenderBatch();
    } else if (RLM_draws[RLM_drawCounter - 1].textureId != id) {
        if (RLM_draws[RLM_drawCounter - 1].vertexCount > 0) {
            if (RLM_draws[RLM_drawCounter - 1].mode == RL_LINES)
                RLM_draws[RLM_drawCounter - 1].vertexAlignment =
                    (RLM_draws[RLM_drawCounter - 1].vertexCount < 4)
                        ? RLM_draws[RLM_drawCounter - 1].vertexCount
                        : RLM_draws[RLM_drawCounter - 1].vertexCount % 4;
            else if (RLM_draws[RLM_drawCounter - 1].mode == RL_TRIANGLES)
                RLM_draws[RLM_drawCounter - 1].vertexAlignment =
                    (RLM_draws[RLM_drawCounter - 1].vertexCount < 4)
                        ? 1
                        : 4 - (RLM_draws[RLM_drawCounter - 1].vertexCount % 4);
            else
                RLM_draws[RLM_drawCounter - 1].vertexAlignment = 0;

            if (!RLM_CheckRenderBatchLimit(RLM_draws[RLM_drawCounter - 1].vertexAlignment)) {
                RLM_vertexCounter += RLM_draws[RLM_drawCounter - 1].vertexAlignment;
                RLM_drawCounter++;
            }
        }
        if (RLM_drawCounter >= RLM_maxDraws) RLM_DrawRenderBatch();
        RLM_draws[RLM_drawCounter - 1].textureId = id;
        RLM_draws[RLM_drawCounter - 1].vertexCount = 0;
    }
}

static void rlDrawRenderBatchActive(void) { RLM_DrawRenderBatch(); }

static void rlSetShader(unsigned int id, int *locs) {
    (void)locs;
    if (RLM_currentShaderId != id) {
        RLM_DrawRenderBatch();
        RLM_currentShaderId = id;
    }
}

static void rlSetBlendMode(int mode) {
    if (RLM_currentBlendMode != mode) {
        RLM_DrawRenderBatch();
        RLM_currentBlendMode = mode;
    }
}

/* state toggles used by the game (tracked, no-op for the model) */
static int RLM_depthMask = 1, RLM_backfaceCulling = 1;
static void rlDisableDepthMask(void) { RLM_depthMask = 0; }
static void rlEnableDepthMask(void) { RLM_depthMask = 1; }
static void rlDisableBackfaceCulling(void) { RLM_backfaceCulling = 0; }
static void rlEnableBackfaceCulling(void) { RLM_backfaceCulling = 1; }

#endif /* RLMOCK_H */
