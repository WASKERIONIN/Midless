/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#if !defined(PLATFORM_WEB)
    #define __clang__ true
#endif
#define STB_DS_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include "raylib.h"
#include "stb_ds.h"
#include "rlgl.h"
#include "raymath.h"
#include "world.h"
#include "rotation.h"
#include "player.h"
#include "chunkmeshgeneration.h"
#include "chunklightning.h"
#include "screens.h"
#include "networkhandler.h"
#include "packet.h"
#include "entity.h"
#include "entitymodel.h"
#include "localserver.h"
#include "particle.h"
#include "asteroid.h"
#include "mobs.h"
#include "hunter.h"
#include "soundfx.h"
#include "dropshadow.h"
#include "starfield.h"
#include "settings.h"
#include "bird.h"
#include "mapview.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

World world;

void World_Init(void) {
    world.material = LoadMaterialDefault();
    world.loadChunks = false;
    /* v47.1: fixed draw distance - the user wants the whole sky visible */
    world.drawDistance = 20;
    world.time = 0;

    world.entities = MemAlloc(WORLD_MAX_ENTITIES * sizeof(Entity));
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) world.entities[i].type = 0; //type 0 = none

    ChunkMeshGeneration_Init();
    Particle_Clear();
    Asteroid_Init();
}

void World_LoadMultiplayer(void) {
    player.position = (Vector3) { COSMIC_SPAWN_X, COSMIC_SPAWN_Y, COSMIC_SPAWN_Z };
    Screen_Switch(SCREEN_GAME);
    world.loadChunks = true;
}

bool World_LoadSingleplayer(void) {
    return LocalServer_Start();
}

void World_UpdateChunksWithBudget(double budgetMs) {
    double endTime = GetTime() + budgetMs / 1000.0;

    while (arrlen(world.generateChunksQueue) > 0) {
        World_ReadChunksQueues();

        if (GetTime() >= endTime)
            break;
    }
}

void World_Update(void) { 
    float deltaTime = GetFrameTime();
    world.time += deltaTime;
    while (world.time >= WORLD_DAY_LENGTH_SECONDS) {
        world.time -= WORLD_DAY_LENGTH_SECONDS;
    }

    World_UpdateChunksWithBudget(4.0);
    Particle_Update(deltaTime);
    Asteroid_Update(deltaTime);
    float interpolationAmount = 1.0f - expf(-20.0f * deltaTime);
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *entity = &world.entities[i];
        if (entity->type == 0) continue;

        entity->position = Vector3Lerp(entity->position, entity->targetPosition, interpolationAmount);
        entity->rotation.y = Rotation_Interpolate(entity->rotation.y, entity->targetRotation.y, interpolationAmount);
        entity->rotation.z = Rotation_Interpolate(entity->rotation.z, entity->targetRotation.z, interpolationAmount);
        if (entity->type != 1)
            entity->rotation.x = Rotation_Interpolate(entity->rotation.x, entity->targetRotation.x, interpolationAmount);
        for (int partIndex = 0; partIndex < entity->model.partCount; partIndex++) {
            EntityModelPart *part = &entity->model.parts[partIndex];
            if (entity->type == 1 && part->type == PART_TYPE_HEAD) {
                part->rotation.x = Rotation_Interpolate(part->rotation.x, entity->targetHeadPitch, interpolationAmount);
            }
        }

        EntityAnimation_Update(&entity->animation, entity->position, deltaTime);
    }
    
}

void World_ReadChunksQueues(void) {

        if (world.loadChunks == true) {

            int index = World_GetClosestChunkIndex(world.generateChunksQueue, Player_GetChunkPosition());

            if (index != -1) {
                Chunk *chunk = world.generateChunksQueue[index];

                if(!chunk->isBuilt) {
                    for (int i = 0; i < 6; i++) {
                        if (chunk->neighbours[i] == NULL) continue;
                        World_QueueChunk(chunk->neighbours[i], false);
                    }
                }

                Chunk_Generate(chunk);
                ChunkMeshGeneration_Build(chunk);

                arrdel(world.generateChunksQueue, index);

                chunk->isGenerating = false;

                for (int i = 0; i < hmlen(world.chunks); i++) {
                    Chunk *lightDirtyChunk = world.chunks[i].value;
                    if (lightDirtyChunk->isBuilt && lightDirtyChunk->isLightDirty)
                        World_QueueChunk(lightDirtyChunk, false);
                }
            }
            
        }  
}

void World_QueueChunk(Chunk *chunk, bool immediate) {

    if (chunk->isGenerating == false) {
        if(!immediate) {
            arrput(world.generateChunksQueue, chunk);
        } else {
            arrins(world.generateChunksQueue, 0, chunk);
        }
    }
    chunk->isGenerating = true;
    
}


Chunk* World_GetChunkAt(Vector3 position) {
    long int p = Chunk_GetPackedPos(position);
    int index = hmgeti(world.chunks, p);
    if (index >= 0) {
        return world.chunks[index].value;
    }
    
    return NULL;
}

int World_GetClosestChunkIndex(Chunk* *array, Vector3 pos) {
    int arrLength = arrlen(array);
    if (arrLength > 0) {
        int index = 0;
        float closestDistanceSquared = Vector3DistanceSqr(array[0]->position, pos);
        for (int i = 1; i < arrLength; i++) {
            float distanceSquared = Vector3DistanceSqr(array[i]->position, pos);
            if (distanceSquared < closestDistanceSquared) {
                closestDistanceSquared = distanceSquared;
                index = i;
            }
        }
        return index;
    }

    return -1;
}

void World_AddChunk(Vector3 position) {

    long int p = Chunk_GetPackedPos(position);
    int index = hmgeti(world.chunks, p);
    if (index == -1) {
        Chunk *newChunk = Chunk_Create(position);
        if (newChunk == NULL) return;

        hmput(world.chunks, p, newChunk);
        World_QueueChunk(newChunk, false);
        
    }
}

void World_RemoveChunk(Chunk *currentChunk) {

    if(currentChunk->isGenerating == true) {
        for(int i = 0; i < arrlen(world.generateChunksQueue); i++) {
            if(world.generateChunksQueue[i] == currentChunk) {
                arrdel(world.generateChunksQueue, i);
            }
        }
    }

    long int p = Chunk_GetPackedPos(currentChunk->position);
    hmdel(world.chunks, p);

    Chunk_UpdateNeighbours(currentChunk, true);
    if (currentChunk->modified) Chunk_SaveFile(currentChunk);
    Chunk_Unload(currentChunk);
    Chunk_Destroy(currentChunk);
}

void World_LoadChunks(void) {

    if (!world.loadChunks || networkConnectedToServer) return;

    Vector3 pos = Player_GetChunkPosition();

    //Create chunks or prepare array of chunks to be sorted
    int loadingHeight = fmin(world.drawDistance, 4);
    for (int y = loadingHeight; y >= -loadingHeight; y--) {
        for (int x = -world.drawDistance ; x <= world.drawDistance; x++) {
            for (int z = -world.drawDistance ; z <= world.drawDistance; z++) {
                Vector3 chunkPos = (Vector3) {pos.x + x, pos.y + y, pos.z + z};

                if (Vector3Distance(chunkPos, pos) < world.drawDistance) {
                    World_AddChunk(chunkPos);
                }
            }
        }
    }
    
    //destroy far chunks
    for (int i = hmlen(world.chunks) - 1; i >= 0 ; i--) {
        Chunk *chunk = world.chunks[i].value;

        if (Vector3Distance(chunk->position, pos) >= world.drawDistance) {
            World_RemoveChunk(chunk);
        }
    }
    
}

void World_Reload(void) {
    if (!networkConnectedToServer) World_Clear();
    world.loadChunks = true;
}

void World_Clear(void) {
    world.loadChunks = false;
    Particle_Clear();
    Bird_Clear();
    MapView_Reset();
    Player_ClearEntityModel();

    arrfree(world.generateChunksQueue);
    world.generateChunksQueue = NULL;

    for (int i = hmlen(world.chunks) - 1; i >= 0; i--) {
        World_RemoveChunk(world.chunks[i].value);
    }

    for(int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        World_RemoveEntity(i);
    }

    hmfree(world.chunks);
    world.chunks = NULL;

}

void World_Shutdown(void) {
    Asteroid_Shutdown();
    World_Clear();
    world.material.maps[MATERIAL_MAP_DIFFUSE].texture.id = 0; // Texture owner releases it.
    UnloadMaterial(world.material);
    MemFree(world.entities);
    world.entities = NULL;
    ChunkMeshGeneration_Shutdown();
}

void World_ApplyTexture(Texture2D texture) {
    SetMaterialTexture(&world.material, MATERIAL_MAP_DIFFUSE, texture);
}

/* v52: flora sprites (mushrooms) sample the same atlas the chunks use */
Texture2D World_GetTerrainTexture(void) {
    return world.material.maps[MATERIAL_MAP_DIFFUSE].texture;
}

void World_ApplyShader(Shader shader) {
    world.material.shader = shader;
}

static Vector4 World_TransformVector4(Vector4 vector, Matrix matrix) {
    return (Vector4) {
        matrix.m0 * vector.x + matrix.m4 * vector.y + matrix.m8  * vector.z + matrix.m12 * vector.w,
        matrix.m1 * vector.x + matrix.m5 * vector.y + matrix.m9  * vector.z + matrix.m13 * vector.w,
        matrix.m2 * vector.x + matrix.m6 * vector.y + matrix.m10 * vector.z + matrix.m14 * vector.w,
        matrix.m3 * vector.x + matrix.m7 * vector.y + matrix.m11 * vector.z + matrix.m15 * vector.w
    };
}

static bool World_IsChunkInFrustum(const Chunk *chunk, Matrix view, Matrix projection) {
    Vector3 min = chunk->blockPosition;
    Vector3 max = Vector3Add(min, CHUNK_SIZE_VEC3);
    unsigned char outsideAllCorners = 0x3F;

    for (int i = 0; i < 8; i++) {
        Vector4 corner = {
            (i & 1) ? max.x : min.x,
            (i & 2) ? max.y : min.y,
            (i & 4) ? max.z : min.z,
            1.0f
        };
        Vector4 clip = World_TransformVector4(World_TransformVector4(corner, view), projection);
        unsigned char outside = 0;

        if (clip.x < -clip.w) outside |= 1u << 0;
        if (clip.x >  clip.w) outside |= 1u << 1;
        if (clip.y < -clip.w) outside |= 1u << 2;
        if (clip.y >  clip.w) outside |= 1u << 3;
        if (clip.z < -clip.w) outside |= 1u << 4;
        if (clip.z >  clip.w) outside |= 1u << 5;

        outsideAllCorners &= outside;
    }

    return outsideAllCorners == 0;
}

void World_Draw(Vector3 camPosition) {

    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    ChunkMesh_PrepareDrawing(world.material);

    int amountChunks = hmlen(world.chunks);
    Matrix view = rlGetMatrixModelview();
    Matrix projection = rlGetMatrixProjection();
    
    Vector3 chunkLocalCenter = (Vector3){CHUNK_SIZE_X / 2, CHUNK_SIZE_Y / 2, CHUNK_SIZE_Z / 2};

    //Create the sorted chunk list
    struct { Chunk *chunk; float dist; } sortedChunks[amountChunks > 0 ? amountChunks : 1];

    int sortedLength = 0;
    for (int i=0; i < hmlen(world.chunks); i++) {
        Chunk *chunk = world.chunks[i].value;

        if (chunk->onlyAir) continue;
        if (!World_IsChunkInFrustum(chunk, view, projection)) continue;

        if (chunk->hasTransparency) {
            Vector3 centerChunk = Vector3Add(chunk->blockPosition, chunkLocalCenter);
            float distFromCam = Vector3Distance(centerChunk, camPosition);

            sortedChunks[sortedLength].dist = distFromCam;
            sortedChunks[sortedLength].chunk = chunk;
            sortedLength++;
        }
        {
            Matrix matrix = (Matrix) { 1, 0, 0, chunk->blockPosition.x,
                0, 1, 0, chunk->blockPosition.y,
                0, 0, 1, chunk->blockPosition.z,
                0, 0, 0, 1 };
        
            ChunkMesh_Draw(&chunk->mesh, world.material, matrix);
        }
    }
    
    ChunkMesh_FinishDrawing();

    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        if (world.entities[i].type == 0) continue;
        Entity_Draw(&world.entities[i]);
    }
    if (player.cameraMode != PLAYER_CAMERA_FIRST_PERSON) Player_Draw();
    DropShadow_DrawAll();
    Asteroid_Draw(camPosition, World_GetSunlightStrength());
    Particle_Draw(player.camera, world.material.maps[MATERIAL_MAP_DIFFUSE].texture);
    rlDrawRenderBatchActive();

    //Sort chunks back to front
    for (int i = 1; i < sortedLength; i++) {
        int j = i;
        while (j > 0 && sortedChunks[j-1].dist <= sortedChunks[j].dist) {
            struct { Chunk *chunk; float dist; } tempC;
            tempC.chunk = sortedChunks[j].chunk;
            tempC.dist = sortedChunks[j].dist;

            sortedChunks[j] = sortedChunks[j - 1];
            sortedChunks[j - 1].chunk = tempC.chunk;
            sortedChunks[j - 1].dist = tempC.dist;
            j = j - 1;
        }
    }
    
    ChunkMesh_PrepareDrawing(world.material);

    rlDisableDepthMask();
    /* v43.5: cull back faces - water interior surfaces no longer bleed
     * through the surface (the "seeing inside sides" artifact). */
    rlEnableBackfaceCulling();

    for (int i = 0; i < sortedLength; i++) {
        Chunk *chunk = sortedChunks[i].chunk;

        Matrix matrix = (Matrix) { 1, 0, 0, chunk->blockPosition.x,
                                   0, 1, 0, chunk->blockPosition.y,
                                   0, 0, 1, chunk->blockPosition.z,
                                   0, 0, 0, 1 };

        ChunkMesh_Draw(&chunk->meshTransparent, world.material, matrix);
    }

    ChunkMesh_FinishDrawing();
    rlEnableDepthMask();
}

int World_GetBlock(Vector3 blockPos) {
    
    //Get Chunk
    Vector3 chunkPos = (Vector3) { floor(blockPos.x / CHUNK_SIZE_X), floor(blockPos.y / CHUNK_SIZE_Y), floor(blockPos.z / CHUNK_SIZE_Z) };
    Chunk* chunk = World_GetChunkAt(chunkPos);
    
    if (chunk == NULL) return 0;
    
    //Get Block
    Vector3 blockPosInChunk = (Vector3) { 
                                floor(blockPos.x) - chunk->blockPosition.x,
                                floor(blockPos.y) - chunk->blockPosition.y, 
                                floor(blockPos.z) - chunk->blockPosition.z 
                               };

    return Chunk_GetBlock(chunk, blockPosInChunk);
}

void World_SetBlock(Vector3 blockPos, int blockId, bool immediate) {
    
    //Get Chunk
    Vector3 chunkPos = (Vector3) { floor(blockPos.x / CHUNK_SIZE_X), floor(blockPos.y / CHUNK_SIZE_Y), floor(blockPos.z / CHUNK_SIZE_Z) };
    Chunk* chunk = World_GetChunkAt(chunkPos);
    
    if (chunk == NULL) return;

    //Set Block
    Vector3 blockPosInChunk = (Vector3) { 
                                floor(blockPos.x) - chunkPos.x * CHUNK_SIZE_X, 
                                floor(blockPos.y) - chunkPos.y * CHUNK_SIZE_Y, 
                                floor(blockPos.z) - chunkPos.z * CHUNK_SIZE_Z 
                               };

    if (!chunk->isLightGenerated) {
        if (Chunk_IsValidPos(blockPosInChunk)) {
            chunk->data[Chunk_PosToIndex(blockPosInChunk)] = blockId;
        }
        return;
    }
    
    Chunk_SetBlock(chunk, blockPosInChunk, blockId);

    if (blockId == 0) {
        World_QueueChunk(chunk, immediate);
        for (int i = 0; i < 26; i++) {
            if (chunk->neighbours[i] == NULL) continue;
            World_QueueChunk(chunk->neighbours[i], immediate);
        }
    } else {
        for (int i = 0; i < 26; i++) {
            if (chunk->neighbours[i] == NULL) continue;
            World_QueueChunk(chunk->neighbours[i], immediate);
        }
        World_QueueChunk(chunk, immediate); 
    }

}

/* v48: visited warp cores tint teal - the network you have opened */
static Vector3 visitedCores[64];
static int visitedCoreCount = 0;

void World_MarkCoreVisited(Vector3 pos) {
    for (int i = 0; i < visitedCoreCount; i++) {
        if (Vector3Distance(visitedCores[i], pos) < 2.0f) return;
    }
    if (visitedCoreCount < 64) visitedCores[visitedCoreCount++] = pos;
}

bool World_IsCoreVisited(Vector3 pos) {
    for (int i = 0; i < visitedCoreCount; i++) {
        if (Vector3Distance(visitedCores[i], pos) < 2.0f) return true;
    }
    return false;
}

/* ---- v51: volatile barrels --------------------------------------------
 * Detonate a barrel: destroys its own cell, two above and two below, chunks
 * out debris, and hurts anything nearby. Chained barrels cook off too. */
void World_ExplodeAt(Vector3 blockPos) {
    static int explodeDepth = 0;
    if (explodeDepth > 4) return;   /* v51: cook-off chain guard */
    explodeDepth++;
    int bx = (int)floorf(blockPos.x), by = (int)floorf(blockPos.y), bz = (int)floorf(blockPos.z);
    SoundFx_PlayExplosion();
    Vector3 center = { bx + 0.5f, by + 0.5f, bz + 0.5f };

    for (int dy = 2; dy >= -2; dy--) {
        Vector3 p = { bx, by + dy, bz };
        int id = World_GetBlock(p);
        if (id == 0) continue;
        if (id == 26 && dy != 0) {
            /* v51: another barrel caught in the column cooks off; its own
             * blast clears its cell first, so the chain always terminates */
            World_ExplodeAt(p);
            continue;
        }
        Particle_SpawnBlockBreak(p, id == 0 ? 1 : id);
        World_SetBlock(p, 0, true);
    }
    Particle_SpawnBlockBreak(center, 26);
    Particle_SpawnImpact(center);

    /* hurt everything nearby */
    Hunter_ExplosionDamage(center, 3.0f, 2);
    Mobs_ExplosionDamage(center, 3.0f, 2);
    Vector3 playerC = { player.position.x + 0.5f, player.position.y + 0.9f, player.position.z + 0.5f };
    float pd = Vector3Distance(playerC, center);
    if (pd < 3.2f) {
        Vector3 away = Vector3Scale(Vector3Subtract(playerC, center), 1.0f / (pd > 0.01f ? pd : 1.0f));
        Player_Damage(3, away);
    }
    explodeDepth--;
}

/* ---- v44: black & white wireframe auras --------------------------------
 * Special world objects get animated vector frames: warp cores carry a
 * slowly spinning octahedron, launch pads emit an expanding ring. Drawn
 * through the standard rlgl line pipeline after the chunk pass. */
static void World_DrawWireAurasAt(Vector3 center, int kind) {
    double t = (double)GetTime();
    float phase = (sinf(center.x * 12.9f) + sinf(center.z * 7.3f)) * 0.5f;

    if (kind == 0) {
        /* warp core: tumbling octahedron, flattened vertically so it never
         * stabs through the neighbouring blocks */
        float ang = (float)t * 0.9f + phase * 3.0f;
        float r = 0.55f + 0.06f * sinf((float)t * 1.7f + phase * 5.0f);
        float ry = r * 0.5f;
        float cx = cosf(ang), sx = sinf(ang);
        Vector3 v[6];
        v[0] = (Vector3){ center.x, center.y + ry, center.z };
        v[1] = (Vector3){ center.x, center.y - ry, center.z };
        v[2] = (Vector3){ center.x + r * cx, center.y, center.z + r * sx };
        v[3] = (Vector3){ center.x - r * cx, center.y, center.z - r * sx };
        v[4] = (Vector3){ center.x - r * sx, center.y, center.z + r * cx };
        v[5] = (Vector3){ center.x + r * sx, center.y, center.z - r * cx };
        int edges[12][2] = {{0,2},{0,3},{0,4},{0,5},{1,2},{1,3},{1,4},{1,5},{2,4},{4,3},{3,5},{5,2}};
        unsigned char bright = (unsigned char)(190.0f + 50.0f * sinf((float)t * 2.3f + phase * 4.0f));
        Color c;
        if (World_IsCoreVisited(center)) {
            c = (Color){ 60, bright, (unsigned char)((bright + 40) / 2), 255 };  /* teal: opened */
        } else {
            c = (Color){ bright, bright, (unsigned char)(bright + 8 > 255 ? 255 : bright + 8), 255 };
        }
        for (int e = 0; e < 12; e++) DrawLine3D(v[edges[e][0]], v[edges[e][1]], c);
        /* axis ticks top/bottom */
        DrawLine3D((Vector3){center.x, center.y + ry + 0.12f, center.z},
                   (Vector3){center.x, center.y + ry, center.z}, c);
        DrawLine3D((Vector3){center.x, center.y - ry, center.z},
                   (Vector3){center.x, center.y - ry - 0.12f, center.z}, c);
    } else if (kind == 1) {
        /* launch pad: square ring expanding from the pad surface */
        float period = 1.8f;
        float k = (float)fmod(t, period) / period;      /* 0..1 */
        float half = 0.22f + 0.26f * k;
        float fade = 1.0f - k;
        unsigned char bright = (unsigned char)(90.0f + 150.0f * fade);
        Color c = { bright, bright, bright, 255 };
        float y = center.y + 0.5101f;
        Vector3 a = (Vector3){ center.x - half, y, center.z - half };
        Vector3 b = (Vector3){ center.x + half, y, center.z - half };
        Vector3 d = (Vector3){ center.x + half, y, center.z + half };
        Vector3 e = (Vector3){ center.x - half, y, center.z + half };
        DrawLine3D(a, b, c); DrawLine3D(b, d, c);
        DrawLine3D(d, e, c); DrawLine3D(e, a, c);
    } else {
        /* v54: void cocoon - a breathing alien egg rendered as a textured
 * lat-long shell (atlas tile 34) instead of the old wireframe */
        float breathe2 = 1.0f + 0.05f * sinf((float)t * 2.1f + phase * 2.0f);
        float rx = 0.26f * breathe2, ry = 0.44f * breathe2;
        Texture2D atlas = World_GetTerrainTexture();
        if (atlas.id != 0) {
            float u0 = (34 % 16) / 16.0f, v0 = (34 / 16) / 16.0f;
            float du = 1.0f / 16.0f, dv = 1.0f / 16.0f;
            unsigned char pr = (unsigned char)(205.0f + 40.0f * sinf((float)t * 3.0f + phase * 2.0f));
            rlSetTexture(atlas.id);
            rlBegin(RL_QUADS);
            const int SEG = 4, BAND = 3;
            for (int k = 0; k < SEG; k++) {
                float phi0 = 6.2832f * k / SEG, phi1 = 6.2832f * (k + 1) / SEG;
                for (int b = 0; b < BAND; b++) {
                    float th0 = 3.1416f * b / BAND, th1 = 3.1416f * (b + 1) / BAND;
                    Vector3 c00 = { center.x + sinf(th0) * cosf(phi0) * rx,
                                    center.y + cosf(th0) * ry * 1.05f,
                                    center.z + sinf(th0) * sinf(phi0) * rx };
                    Vector3 c10 = { center.x + sinf(th0) * cosf(phi1) * rx,
                                    center.y + cosf(th0) * ry * 1.05f,
                                    center.z + sinf(th0) * sinf(phi1) * rx };
                    Vector3 c01 = { center.x + sinf(th1) * cosf(phi0) * rx,
                                    center.y + cosf(th1) * ry * 1.05f,
                                    center.z + sinf(th1) * sinf(phi0) * rx };
                    Vector3 c11 = { center.x + sinf(th1) * cosf(phi1) * rx,
                                    center.y + cosf(th1) * ry * 1.05f,
                                    center.z + sinf(th1) * sinf(phi1) * rx };
                    float ua = u0 + du * k / SEG,      ub = u0 + du * (k + 1) / SEG;
                    float va = v0 + dv * b / BAND,     vb = v0 + dv * (b + 1) / BAND;
                    /* v56: both windings - session-wide backface culling
                     * (enabled by the icon renderer) hid one side */
                    rlColor4ub(pr, 90, 235, 255);
                    rlTexCoord2f(ua, va); rlVertex3f(c00.x, c00.y, c00.z);
                    rlTexCoord2f(ub, va); rlVertex3f(c10.x, c10.y, c10.z);
                    rlTexCoord2f(ub, vb); rlVertex3f(c11.x, c11.y, c11.z);
                    rlTexCoord2f(ua, vb); rlVertex3f(c01.x, c01.y, c01.z);
                    rlTexCoord2f(ua, vb); rlVertex3f(c01.x, c01.y, c01.z);
                    rlTexCoord2f(ub, vb); rlVertex3f(c11.x, c11.y, c11.z);
                    rlTexCoord2f(ub, va); rlVertex3f(c10.x, c10.y, c10.z);
                    rlTexCoord2f(ua, va); rlVertex3f(c00.x, c00.y, c00.z);
                }
            }
            rlEnd();
            rlSetTexture(0);
        }
    }
}

void World_DrawWireAuras(void) {
    Matrix view = rlGetMatrixModelview();
    Matrix projection = rlGetMatrixProjection();
    for (int i = 0; i < hmlen(world.chunks); i++) {
        Chunk *chunk = world.chunks[i].value;
        if (chunk->specialCount[0] == 0 && chunk->specialCount[1] == 0 &&
            chunk->specialCount[2] == 0) continue;
        if (!World_IsChunkInFrustum(chunk, view, projection)) continue;
        for (int s = 0; s < chunk->specialCount[0]; s++) World_DrawWireAurasAt(chunk->specialPos[0][s], 0);
        for (int s = 0; s < chunk->specialCount[1]; s++) World_DrawWireAurasAt(chunk->specialPos[1][s], 1);
        for (int s = 0; s < chunk->specialCount[2]; s++) World_DrawWireAurasAt(chunk->specialPos[2][s], 2);
    }
}

float World_GetSunlightStrength(void) {
    /* No sun — nebula ambient with a slow pulse. Never drop to night-black.
     * v43.4: dialed back down after the overbright report. */
    return 0.68f + 0.04f * sinf(world.time * 0.12f);
}

float World_GetBrightness(Vector3 position) {
    Vector3 chunkPosition = {
        floorf(position.x / CHUNK_SIZE_X),
        floorf(position.y / CHUNK_SIZE_Y),
        floorf(position.z / CHUNK_SIZE_Z)
    };
    Chunk *chunk = World_GetChunkAt(chunkPosition);
    if (!chunk || !chunk->isLightGenerated) return 1.0f;

    Vector3 localPosition = {
        floorf(position.x) - chunk->blockPosition.x,
        floorf(position.y) - chunk->blockPosition.y,
        floorf(position.z) - chunk->blockPosition.z
    };
    float blockLight = Chunk_GetLight(chunk, localPosition, false) / 15.0f;
    float sunlight = Chunk_GetLight(chunk, localPosition, true) / 15.0f;
    sunlight *= World_GetSunlightStrength();
    return Clamp(fmaxf(blockLight, sunlight), 0.1f, 1.0f);
}

/*-------------------------------------------------------------------------------------------------------*
*-------------------------------------------World Entities-----------------------------------------------*
*--------------------------------------------------------------------------------------------------------*/

void World_TeleportEntity(int id, Vector3 position, Vector3 rotation) {
    if (id < 0 || id >= WORLD_MAX_ENTITIES) return;
    Entity *entity = &world.entities[id];
    if (entity->type == 0) return;
    if (Vector3DistanceSqr(entity->position, position) > 64.0f) {
        entity->position = position;
        entity->rotation = (Vector3) {entity->type == 1 ? 0 : rotation.x, rotation.y, rotation.z};
        entity->targetPosition = position;
        entity->targetRotation = entity->rotation;
        entity->targetHeadPitch = rotation.x;
        entity->animation.lastPosition = position;
        for (int i = 0; i < entity->model.partCount; i++) {
            if (entity->type == 1 && entity->model.parts[i].type == PART_TYPE_HEAD) {
                entity->model.parts[i].rotation.x = rotation.x;
            }
        }
        return;
    }

    entity->targetPosition = position;
    entity->targetRotation = (Vector3) {entity->type == 1 ? 0 : rotation.x, rotation.y, rotation.z};
    entity->targetHeadPitch = rotation.x;
}

void World_AddEntity(int id, int type, int modelId, Vector3 position, Vector3 rotation) {
    if (id < 0 || id >= WORLD_MAX_ENTITIES) return;
    if (modelId < 0 || modelId >= 256) return;

    if (world.entities[id].type != 0) Entity_Destroy(&world.entities[id]);
    world.entities[id].type = type;
    world.entities[id].modelId = (unsigned char)modelId;
    world.entities[id].position = position;
    world.entities[id].rotation = rotation;
    world.entities[id].targetPosition = position;
    world.entities[id].targetRotation = rotation;
    world.entities[id].targetHeadPitch = rotation.x;
    EntityAnimation_Init(&world.entities[id].animation, position);
    
    EntityModel_Create(&world.entities[id].model, *EntityModel_GetDefinition(modelId));
}

void World_RemoveEntity(int id) {
    if (!world.entities || id < 0 || id >= WORLD_MAX_ENTITIES) return;
    Entity_Destroy(&world.entities[id]);
}

void World_PlayEntityAnimation(int id, EntityAnimationType animation) {
    if (id < 0 || id >= WORLD_MAX_ENTITIES) return;
    if (world.entities[id].type == 0) return;
    EntityAnimation_Start(&world.entities[id].animation, animation);
}

void World_InvalidateBlockDefinitions(bool relight) {
    for (int i = 0; i < hmlen(world.chunks); i++) {
        Chunk *chunk = world.chunks[i].value;
        if (relight) {
            memset(chunk->lightData, 0, sizeof(chunk->lightData));
            memset(chunk->sunlightData, 0, sizeof(chunk->sunlightData));
            chunk->isLightGenerated = false;
            chunk->incompleteLightFaces = 0;
            chunk->incompleteSunlightFaces = 0;
            chunk->isLightDirty = true;
        }
        World_QueueChunk(chunk, false);
    }
}
