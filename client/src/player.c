/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <limits.h>
#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "player.h"
#include "world.h"
#include "raycast.h"
#include "screens.h"
#include "chat.h"
#include "block.h"
#include "networkhandler.h"
#include "packet.h"
#include "particle.h"
#include "hunter.h"
#include "entity.h"
#include "mapview.h"
#include "soundfx.h"

#define MOUSE_SENSITIVITY 0.003f
#define THIRD_PERSON_DISTANCE 4.0f
#define WATER_MOVE_SCALE 0.5f
#define WATER_GRAVITY 0.003f
#define WATER_MAX_FALL_SPEED 0.2f
#define WATER_SWIM_ACCELERATION 0.025f
#define WATER_DRAG 0.8f

Vector2 playerOldMousePosition = {0.0f, 0.0f};
Vector2 playerCameraAngle = {0.0f, 0.0f};
double playerLastPositionPacketTime;
Player player;
static int lastSentHeldBlock;

/* v43 game feel: coyote time, jump buffering, and a void-rescue fade */
#define PLAYER_COYOTE_SECONDS 0.12
#define PLAYER_JUMP_BUFFER_SECONDS 0.16
static double lastGroundedTime = -100.0;
static double jumpPressedTime = -100.0;
static float respawnFade = 0.0f;

float Player_GetRespawnFade(void) { return respawnFade; }

void Player_Init(void) {

    Camera camera = { 0 };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    player.camera = camera;
    
    player.velocity = (Vector3) {0, 0, 0};
    player.position = (Vector3) { COSMIC_SPAWN_X, COSMIC_SPAWN_Y, COSMIC_SPAWN_Z };
    player.speed = 0.125f / 6;
    
    player.collisionBox.min = (Vector3) { 0.2f, 0, 0.2f };
    player.collisionBox.max = (Vector3) { 0.8f, 1.5f, 0.8f };

    player.blockSelected = 1;
    player.flying = false;
    lastSentHeldBlock = -1;
    player.entityType = 0;
    player.modelId = 0;
    player.hasEntityModel = false;
    player.cameraMode = PLAYER_CAMERA_FIRST_PERSON;
    player.crouching = false;
    player.crouchT = 0.0f;
    player.dashChargesUsed = 0;
    player.webActive = false;
    player.webAnchor = (Vector3){ 0 };
    player.webBlock = (Vector3){ 0 };
    player.hp = 10;
    player.invulnUntil = 0.0;
    player.lastHurtTime = -100.0;
    player.airJumpsUsed = 0;
    player.dashReadyTime = 0.0;
    player.dashActiveUntil = 0.0;
    player.dashDir = (Vector3){ 0 };
    player.entityModel = (EntityModel){0};
    EntityAnimation_Init(&player.animation, player.position);

    playerLastPositionPacketTime = 0;

    UpdateCamera(&player.camera, CAMERA_CUSTOM);
    DisableCursor();
}

void Player_SetEntityModel(int type, int modelId) {
    if (modelId < 0 || modelId >= 256) return;
    Player_ClearEntityModel();
    player.entityType = (unsigned char)type;
    player.modelId = (unsigned char)modelId;
    EntityModel_Create(&player.entityModel, *EntityModel_GetDefinition(modelId));
    player.hasEntityModel = true;
}

void Player_ClearEntityModel(void) {
    if (!player.hasEntityModel) return;
    EntityModel_Unload(&player.entityModel);
    EntityModel_Destroy(&player.entityModel);
    player.hasEntityModel = false;
    player.entityType = 0;
}

void Player_Damage(int amount, Vector3 fromDir) {
    if (player.flying) return;
    double now = GetTime();
    if (now < player.invulnUntil) return;
    player.hp -= amount;
    player.invulnUntil = now + 0.9;
    player.lastHurtTime = now;
    player.velocity.x += fromDir.x * 0.28f;
    player.velocity.y += 0.14f;
    player.velocity.z += fromDir.z * 0.28f;
    SoundFx_PlayPlayerHurt();
    if (player.hp <= 0) {
        player.hp = 10;
        Player_Teleport((Vector3){ COSMIC_SPAWN_X, COSMIC_SPAWN_Y, COSMIC_SPAWN_Z });
        respawnFade = 1.0f;
        SoundFx_PlayTeleport();
        Chat_AddLine("The hunters got you. Wake up on the starter island.");
    }
}

void Player_Heal(int amount) {
    player.hp += amount;
    if (player.hp > 10) player.hp = 10;
}

int Player_GetHp(void) {
    return player.hp;
}

void Player_Teleport(Vector3 position) {
    player.position = position;
    player.animation.lastPosition = position;
    player.velocity = (Vector3){0};
    player.canJump = false;

    player.camera.position = position;
    player.camera.position.x += 0.5f;
    player.camera.position.y += 1.5f;
    player.camera.position.z += 0.5f;
}

void Player_Draw(void) {
    float pitch = playerCameraAngle.y - PI / 2.0f;

    if (player.hasEntityModel) {
        for (int i = 0; i < player.entityModel.partCount; i++) {
            EntityModelPart *part = &player.entityModel.parts[i];
            if (part->type == PART_TYPE_HEAD) part->rotation.x = pitch;
        }
    }

    Entity localEntity = {0};
    localEntity.type = (char)player.entityType;
    localEntity.modelId = player.modelId;
    localEntity.position = (Vector3){player.position.x + 0.5f, player.position.y, player.position.z + 0.5f};
    localEntity.rotation = (Vector3){0, -playerCameraAngle.x + PI / 2.0f, 0};
    localEntity.model = player.entityModel;
    localEntity.animation = player.animation;
    localEntity.heldBlock = Block_IsSelectable(player.blockSelected) ? player.blockSelected : 0;
    if (player.cameraMode == PLAYER_CAMERA_FIRST_PERSON) {
        float swingProgress = EntityAnimation_GetSwingProgress(
            &player.animation, ENTITY_ANIMATION_SWING_RIGHT_ARM);
        Entity_DrawFirstPerson(&localEntity, player.camera, swingProgress);
        return;
    }
    if (!player.hasEntityModel) return;
    Entity_Draw(&localEntity);
}

/* ---- v46: web grapple ------------------------------------------------- */
#define WEB_RANGE      40.0f
#define WEB_REEL       0.050f    /* acceleration toward the anchor */
#define WEB_MAX_SPEED  0.62f     /* total velocity cap while reeling */
#define WEB_DETACH_DIST 1.5f

static void Player_WebDetach(void) {
    if (!player.webActive) return;
    player.webActive = false;
}

/* long-range ray for the web; returns the pull point and the anchored cell */
static bool Player_WebRay(Vector3 origin, Vector3 dir, Vector3 *pullPoint, Vector3 *blockCell) {
    float step = 0.06f;
    Vector3 pos = origin;
    for (float traveled = 0.0f; traveled < WEB_RANGE; traveled += step) {
        pos = Vector3Add(pos, Vector3Scale(dir, step));
        int id = World_GetBlock(pos);
        if (id == 0) continue;
        const Block *block = Block_GetDefinition(id);
        if (block->modelType == BLOCK_MODEL_GAS) continue;
        if (block->colliderType == BLOCK_COLLIDER_LIQUID) continue;
        *blockCell = (Vector3){ floorf(pos.x - dir.x * 0.02f),
                                floorf(pos.y - dir.y * 0.02f),
                                floorf(pos.z - dir.z * 0.02f) };
        *pullPoint = Vector3Subtract(pos, Vector3Scale(dir, 0.12f));
        return true;
    }
    return false;
}

void Player_CheckInputs() {
    if (!chatOpen) {
        if (IsKeyPressed(KEY_SPACE)) jumpPressedTime = GetTime();
        if (IsKeyPressed(KEY_F3)) screenShowDebug = !screenShowDebug;
        if (IsKeyPressed(KEY_F5))
            player.cameraMode = (PlayerCameraMode)((player.cameraMode + 1) % 3);
        if (IsKeyPressed(KEY_M) && currentScreen == SCREEN_GAME) MapView_Toggle();
        if (IsKeyPressed(KEY_TAB) && currentScreen == SCREEN_GAME) {
            player.flying = !player.flying;
            player.velocity.y = 0;
        }
    }
    
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (screenCursorEnabled) {
            DisableCursor();
            chatOpen = false;
            Screen_Switch(SCREEN_GAME);
        } else {
            EnableCursor();
            Screen_Switch(SCREEN_PAUSE);
        }
        screenCursorEnabled = !screenCursorEnabled;
    } else if (IsKeyPressed(KEY_T)) {
        if (screenCursorEnabled && !chatOpen) {
            DisableCursor();
            screenCursorEnabled = false;
            Screen_Switch(SCREEN_GAME);
        } else {
            chatOpen = true;
            EnableCursor();
            screenCursorEnabled = true;
        }
    }
    
    
    Vector2 mousePositionDelta = { 0.0f, 0.0f };
    Vector2 mousePos = GetMousePosition();
    
    mousePositionDelta.x = mousePos.x - playerOldMousePosition.x;
    mousePositionDelta.y = mousePos.y - playerOldMousePosition.y;
    
    playerOldMousePosition = GetMousePosition();
    
    if (!screenCursorEnabled) {
        playerCameraAngle.x -= (mousePositionDelta.x * -MOUSE_SENSITIVITY);
        playerCameraAngle.y -= (mousePositionDelta.y * -MOUSE_SENSITIVITY);
        
        //Limit head rotation
        float maxCamAngleY = PI - 0.01f;
        float minCamAngleY = 0.01f;
        
        if (playerCameraAngle.y >= maxCamAngleY) 
            playerCameraAngle.y = maxCamAngleY;
        else if (playerCameraAngle.y <= minCamAngleY) 
            playerCameraAngle.y = minCamAngleY;
    }
    
    
    //Calculate direction vectors of the camera angle
    float cx = cosf(playerCameraAngle.x);
    float sx = sinf(playerCameraAngle.x);
    
    float cx90 = cosf(playerCameraAngle.x + PI / 2);
    float sx90 = sinf(playerCameraAngle.x + PI / 2);
    
    float sy = sinf(playerCameraAngle.y);
    float cy = cosf(playerCameraAngle.y);
    
    float forwardX = cx * sy;
    float forwardY = cy;
    float forwardZ = sx * sy;
    Vector3 forward = {forwardX, forwardY, forwardZ};
    Vector3 eyePosition = {player.position.x + 0.5f, player.position.y + 1.5f, player.position.z + 0.5f};
    
    if (!screenCursorEnabled) {
        //Handle keys & mouse
        if (player.flying) {
            player.velocity.y = 0;
            if (IsKeyDown(KEY_SPACE)) player.velocity.y = 0.22f;
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) player.velocity.y = -0.22f;
        } else if (IsKeyDown(KEY_SPACE) || GetTime() - jumpPressedTime < PLAYER_JUMP_BUFFER_SECONDS) {
            if (player.liquidSubmersion > 0.0f) {
                if (IsKeyDown(KEY_SPACE)) {
                    player.velocity.y += WATER_SWIM_ACCELERATION * (GetFrameTime() * 60.0f);
                    if (player.velocity.y > 0.2f) player.velocity.y = 0.2f;
                }
            } else if (player.canJump || GetTime() - lastGroundedTime < PLAYER_COYOTE_SECONDS) {
                player.velocity.y += 0.26f;
                player.canJump = false;
                player.airJumpsUsed = 0;
                lastGroundedTime = -100.0;
                jumpPressedTime = -100.0;
                SoundFx_PlayJump();
            } else if (player.airJumpsUsed < 1 && IsKeyPressed(KEY_SPACE)) {
                /* v44: double jump - one extra mid-air jump on a fresh press */
                player.velocity.y = 0.22f;
                player.airJumpsUsed++;
                lastGroundedTime = -100.0;
                jumpPressedTime = -100.0;
                SoundFx_PlayJump();
            }
        }
        Vector3 moveDir = { 0 };
        
        if (IsKeyDown(KEY_W)) {
            moveDir.z += sx;
            moveDir.x += cx;
        }
        
        if (IsKeyDown(KEY_S)) {
            moveDir.z -= sx;
            moveDir.x -= cx;
        }
        
        if (IsKeyDown(KEY_A)) {
            moveDir.z -= sx90;
            moveDir.x -= cx90;
        }
        
        if (IsKeyDown(KEY_D)) {
            moveDir.z += sx90;
            moveDir.x += cx90;
        }

        moveDir = Vector3ClampValue(moveDir, 0.0f, 1.0f); // normalize

        /* v44/v46.1: dash - Shift burst, two charges refilled on landing */
        double nowDash = GetTime();
        if (!player.flying && !player.webActive &&
            (IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT)) &&
            player.dashChargesUsed < 2 &&
            nowDash >= player.dashReadyTime) {
            player.dashChargesUsed++;
            Vector3 dir = moveDir;
            if (Vector3Length(dir) < 0.01f) dir = (Vector3){ cx, 0, sx };
            dir.y = 0;
            if (Vector3Length(dir) > 0.01f) {
                dir = Vector3Normalize(dir);
                player.dashDir = dir;
                player.dashActiveUntil = nowDash + 0.16;
                player.dashReadyTime = nowDash + 0.45;   /* short gap between the two dashes */
                player.velocity.y += 0.06f;
                SoundFx_PlayTeleport();
            }
        }
        if (nowDash < player.dashActiveUntil) {
            player.velocity.x += player.dashDir.x * player.speed * 2.6f;
            player.velocity.z += player.dashDir.z * player.speed * 2.6f;
        }

        /* v46: web grapple - F fires, Shift reels, Space releases with momentum */
        if (IsKeyPressed(KEY_F)) {
            if (player.webActive) {
                Player_WebDetach();
                SoundFx_PlayClick();
            } else {
                Vector3 pullPoint, blockCell;
                if (Player_WebRay(eyePosition, forward, &pullPoint, &blockCell)) {
                    player.webActive = true;
                    player.webAnchor = pullPoint;
                    player.webBlock = blockCell;
                    SoundFx_PlayWebAttach();
                } else {
                    SoundFx_PlayWebShoot();
                }
            }
        }
        if (player.webActive) {
            if (World_GetBlock(player.webBlock) == 0) {
                Player_WebDetach();   /* anchored block was broken */
            } else if (IsKeyPressed(KEY_SPACE)) {
                Player_WebDetach();   /* keep momentum for the jump chain */
            } else {
                Vector3 bodyCenter = { player.position.x + 0.5f, player.position.y + 1.0f, player.position.z + 0.5f };
                Vector3 toAnchor = Vector3Subtract(player.webAnchor, bodyCenter);
                float dist = Vector3Length(toAnchor);
                if (dist < WEB_DETACH_DIST) {
                    Player_WebDetach();   /* arrived - velocity keeps, you fling past */
                } else if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                    Vector3 dir = Vector3Scale(toAnchor, 1.0f / dist);
                    float frameScale = GetFrameTime() * 60.0f;
                    player.velocity = Vector3Add(player.velocity,
                        Vector3Scale(dir, WEB_REEL * frameScale));
                    float speed = Vector3Length(player.velocity);
                    if (speed > WEB_MAX_SPEED)
                        player.velocity = Vector3Scale(player.velocity, WEB_MAX_SPEED / speed);
                }
            }
        }

        Vector3 moveVel = Vector3Scale(moveDir, player.speed);
        /* v46.1: crouch on C - slow and low */
        player.crouching = !player.flying && IsKeyDown(KEY_C);
        if (player.crouching) moveVel = Vector3Scale(moveVel, 0.55f);
        if (player.liquidSubmersion > 0.0f) {
            moveVel = Vector3Scale(moveVel, WATER_MOVE_SCALE);
        }
        player.velocity = Vector3Add(player.velocity, moveVel);
        
        float wheel = GetMouseWheelMove();
        if (wheel > 0.35f) player.blockSelected = Block_NextSelectable(player.blockSelected, 1);
        if (wheel < -0.35f) player.blockSelected = Block_NextSelectable(player.blockSelected, -1);
        
        player.crouchT += ((player.crouching ? 1.0f : 0.0f) - player.crouchT) *
                          (1.0f - powf(0.0001f, GetFrameTime()));
        eyePosition.y -= 0.4f * player.crouchT;   /* v46.1: crouch lowers the eye */
        player.rayResult = Raycast_Cast(eyePosition, forward, true);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { //Strike / Break Block
            EntityAnimation_Start(&player.animation, ENTITY_ANIMATION_SWING_RIGHT_ARM);
            Network_Send(Packet_CreatePlayerClick(0));
            /* v45: a swing at a hunter takes priority over mining */
            if (Hunter_TryHit(eyePosition, forward, 4.5f)) {
                SoundFx_PlayClick();
            } else if (player.rayResult.hitblockId != -1) {
                Particle_SpawnBlockBreak(player.rayResult.hitPos, player.rayResult.hitblockId);
                World_SetBlock(player.rayResult.hitPos, 0, true);
                Network_Send(Packet_CreateSetBlock(0, player.rayResult.hitPos));
                SoundFx_PlayDig();
            }
        } else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) { //Place Block
            EntityAnimation_Start(&player.animation, ENTITY_ANIMATION_SWING_RIGHT_ARM);
            Network_Send(Packet_CreatePlayerClick(1));
            Vector3 placePos = Vector3Add(player.rayResult.hitPos, player.rayResult.normal);
            
            if (player.rayResult.hitblockId != -1) {
                int bottomblockId = World_GetBlock(Vector3Add(placePos, (Vector3){0, -1, 0}));
                if (Block_IsOverridden(player.blockSelected)) {
                    Player_TryPlaceBlock(placePos, player.blockSelected);
                } else switch (player.blockSelected)
                {
                    case -1: // null
                    case 0: // air
                        break;

                    case 12:
                    case 13:
                        if (bottomblockId == 2 || bottomblockId == 3 || bottomblockId == 6) { // dirt, grass, sand
                            Player_TryPlaceBlock(placePos, player.blockSelected);
                        }
                        break;
                        
                    case 17: // stone_slab
                        if (player.rayResult.normal.y == 1 && player.rayResult.hitblockId == 17) {
                            Player_TryPlaceBlock(player.rayResult.hitPos, 1);
                            break;
                        }

                    case 18: // wood_slab
                        if (player.rayResult.normal.y == 1 && player.rayResult.hitblockId == 18) {
                            Player_TryPlaceBlock(player.rayResult.hitPos, 4);
                            break;
                        }

                    default:
                        Player_TryPlaceBlock(placePos, player.blockSelected);
                        break;
                }
            }
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) { //Pick Block
            int pickedId = World_GetBlock(player.rayResult.hitPos);
            if (Block_IsSelectable(pickedId)) player.blockSelected = pickedId;
        }
    }
    player.camera.position = eyePosition;
    if (player.cameraMode != PLAYER_CAMERA_FIRST_PERSON) {
        Vector3 cameraDirection = player.cameraMode == PLAYER_CAMERA_THIRD_PERSON_BEHIND
            ? Vector3Negate(forward)
            : forward;
        Vector3 desiredPosition = Vector3Add(eyePosition, Vector3Scale(cameraDirection, THIRD_PERSON_DISTANCE));
        RaycastResult cameraHit = Raycast_Cast(eyePosition, cameraDirection, true);
        float hitDistance = Vector3Distance(eyePosition, cameraHit.prevPos);
        if (cameraHit.hitblockId != -1 && hitDistance < THIRD_PERSON_DISTANCE) {
            player.camera.position = Vector3Subtract(cameraHit.prevPos, Vector3Scale(cameraDirection, 0.1f));
        } else {
            player.camera.position = desiredPosition;
        }
    }
    player.camera.target = Vector3Add(eyePosition, forward);
}

bool Player_TryPlaceBlock(Vector3 pos, int blockId)
{
    if (!Block_IsSelectable(blockId)) return false;
    int oldBlock = World_GetBlock(pos);
    World_SetBlock(pos, blockId, true);
    if (Player_TestCollision((Vector3){ 0 }))
    {
        World_SetBlock(pos, oldBlock, true);
        return false;
    }

    Network_Send(Packet_CreateSetBlock(blockId, pos));
    SoundFx_PlayPlace();
    return true;
}



void Player_Update(void) {
    
    if(GetTime() - playerLastPositionPacketTime > 0.05) {
        Network_Send(Packet_CreatePlayerPosition((Vector3) { player.position.x + 0.5f, player.position.y, player.position.z + 0.5f }, (Vector3) {playerCameraAngle.y - PI / 2, -playerCameraAngle.x + PI / 2, 0}));
        playerLastPositionPacketTime = GetTime();
    }

    float frameScale = GetFrameTime() * 60.0f;
    player.liquidSubmersion = Player_GetLiquidSubmersion();

    if (player.flying) {
        /* Tab fly: no gravity */
    } else if (player.liquidSubmersion > 0.0f) {
        player.velocity.y -= WATER_GRAVITY * frameScale;
        if (player.velocity.y < -WATER_MAX_FALL_SPEED) {
            player.velocity.y = -WATER_MAX_FALL_SPEED;
        }
    } else {
        player.velocity.y -= 0.0085f * frameScale;
        if (player.velocity.y <= -0.85f) player.velocity.y = -0.85f;
        /* v44: glide - hold Space while falling to float down gently */
        if (!player.canJump && player.liquidSubmersion <= 0.0f &&
            IsKeyDown(KEY_SPACE) && player.velocity.y < -0.18f) {
            player.velocity.y = -0.18f;
        }
    }

    if (player.position.y < COSMIC_VOID_Y) {
        Player_Teleport((Vector3){ COSMIC_SPAWN_X, COSMIC_SPAWN_Y, COSMIC_SPAWN_Z });
        respawnFade = 1.0f;
        SoundFx_PlayTeleport();
        Chat_AddLine("The void lets you go. Returned to the starter island.");
    }
    if (respawnFade > 0.0f) respawnFade = fmaxf(0.0f, respawnFade - GetFrameTime() * 1.3f);
    
    //Calculate velocity with delta time
    Vector3 velXdt = Vector3Scale(player.velocity, frameScale);
    
    int steps = 8;
    
    //Move Y & Test Collisions
    for (int i = 0; i < steps; i++) {
        player.position.y += velXdt.y / steps;
        if (Player_TestCollision((Vector3){ 0 })) {
            player.position.y -= velXdt.y / steps;
            if (player.velocity.y <= 0) {
                player.canJump = true;
                player.airJumpsUsed = 0;
                player.dashChargesUsed = 0;
                lastGroundedTime = GetTime();
            }
            player.velocity.y = 0;
            break;
        } else {
            player.canJump = false;
        }
    }

    //Move X & Test Collisions
    for (int i = 0; i < steps; i++) {
        player.position.x += velXdt.x / steps;
        if (Player_TestCollision((Vector3){ 0 })) {
            if (player.velocity.y != 0 || Player_TestCollision((Vector3){0,0.51f,0})) {
                player.position.x -= velXdt.x / steps;
            } else {
                player.position.y += 0.1f / steps;
            }
        }
    }

    //Move Z & Test Collisions
    for (int i = 0; i < steps; i++) {
        player.position.z += velXdt.z / steps;
        if (Player_TestCollision((Vector3){ 0 })) {
            if (player.velocity.y != 0 || Player_TestCollision((Vector3){0,0.51f,0})) {
                player.position.z -= velXdt.z / steps;
            } else {
                player.position.y += 0.1f / steps;
                break;
            }
        }
    }

    World_LoadChunks();

    if (player.liquidSubmersion > 0.0f) {
        float drag = powf(WATER_DRAG, frameScale);
        player.velocity = Vector3Scale(player.velocity, drag);
    } else {
        player.velocity.x -= player.velocity.x / 6.0f;
        player.velocity.z -= player.velocity.z / 6.0f;
    }
    
    Player_CheckInputs();
    int heldBlock = Block_IsSelectable(player.blockSelected) ? player.blockSelected : 0;
    if (heldBlock != lastSentHeldBlock && player.hasEntityModel) {
        Network_Send(Packet_CreateHeldBlock(heldBlock));
        lastSentHeldBlock = heldBlock;
    }
    EntityAnimation_Update(&player.animation, player.position, GetFrameTime());
}

bool Player_TestCollision(Vector3 offset) {
    
    BoundingBox pB = player.collisionBox;
    pB.min = Vector3Add(Vector3Add(pB.min, player.position), offset);
    pB.max = Vector3Add(Vector3Add(pB.max, player.position), offset);
    
    for (int x = (int)(pB.min.x - 1); x < (int)(pB.max.x + 1); x++) {
        for (int z = (int)(pB.min.z - 1); z < (int)(pB.max.z + 1); z++) {
            for (int y = (int)(pB.min.y - 1); y < (int)(pB.max.y + 1); y++) {
                Vector3 blockPos = (Vector3) {x, y, z};

                Vector3 chunkPos = (Vector3) { floor(blockPos.x / CHUNK_SIZE_X), floor(blockPos.y / CHUNK_SIZE_Y), floor(blockPos.z / CHUNK_SIZE_Z) };
                Chunk* chunk = World_GetChunkAt(chunkPos);
                if (chunk == NULL || chunk->isBlockDataReady == false) return true;

                int blockId = World_GetBlock(blockPos);
                const Block *blockDef = Block_GetDefinition(blockId);
                if (blockDef->colliderType != BLOCK_COLLIDER_SOLID) continue;
                
                BoundingBox blockB;
                blockB.min = (Vector3) {x + (blockDef->minBB.x / 16), y + (blockDef->minBB.y / 16), z + (blockDef->minBB.z / 16)};
                blockB.max = (Vector3) {x + (blockDef->maxBB.x / 16), y + (blockDef->maxBB.y / 16), z + (blockDef->maxBB.z / 16)};
                
                if (CheckCollisionBoxes(pB, blockB)) return true;
            }
        }
    }
    
    return false;
}

float Player_GetLiquidSubmersion(void) {
    BoundingBox playerBox = player.collisionBox;
    playerBox.min = Vector3Add(playerBox.min, player.position);
    playerBox.max = Vector3Add(playerBox.max, player.position);

    float highestLiquidSurface = -INFINITY;
    for (int x = (int)floorf(playerBox.min.x); x <= (int)floorf(playerBox.max.x - 0.001f); x++) {
        for (int z = (int)floorf(playerBox.min.z); z <= (int)floorf(playerBox.max.z - 0.001f); z++) {
            for (int y = (int)floorf(playerBox.min.y); y <= (int)floorf(playerBox.max.y - 0.001f); y++) {
                Vector3 blockPosition = {(float)x, (float)y, (float)z};
                Vector3 chunkPosition = {
                    floorf(blockPosition.x / CHUNK_SIZE_X),
                    floorf(blockPosition.y / CHUNK_SIZE_Y),
                    floorf(blockPosition.z / CHUNK_SIZE_Z)
                };
                Chunk *chunk = World_GetChunkAt(chunkPosition);
                if (chunk == NULL || !chunk->isBlockDataReady) continue;

                const Block *block = Block_GetDefinition(World_GetBlock(blockPosition));
                if (block->colliderType != BLOCK_COLLIDER_LIQUID) continue;

                BoundingBox liquidBox = {
                    .min = {x + block->minBB.x / 16.0f, y + block->minBB.y / 16.0f,
                            z + block->minBB.z / 16.0f},
                    .max = {x + block->maxBB.x / 16.0f, y + block->maxBB.y / 16.0f,
                            z + block->maxBB.z / 16.0f}
                };
                if (CheckCollisionBoxes(playerBox, liquidBox) && liquidBox.max.y > highestLiquidSurface) {
                    highestLiquidSurface = liquidBox.max.y;
                }
            }
        }
    }

    if (highestLiquidSurface == -INFINITY) return 0.0f;
    float playerHeight = playerBox.max.y - playerBox.min.y;
    return Clamp((highestLiquidSurface - playerBox.min.y) / playerHeight, 0.0f, 1.0f);
}

bool Player_GetCameraLiquidTint(Color *tint) {
    Vector3 blockPosition = {
        floorf(player.camera.position.x),
        floorf(player.camera.position.y),
        floorf(player.camera.position.z)
    };
    Vector3 chunkPosition = {
        floorf(blockPosition.x / CHUNK_SIZE_X),
        floorf(blockPosition.y / CHUNK_SIZE_Y),
        floorf(blockPosition.z / CHUNK_SIZE_Z)
    };
    Chunk *chunk = World_GetChunkAt(chunkPosition);
    if (chunk == NULL || !chunk->isBlockDataReady) return false;

    int blockId = World_GetBlock(blockPosition);
    const Block *block = Block_GetDefinition(blockId);
    if (block->colliderType != BLOCK_COLLIDER_LIQUID) return false;

    float liquidSurface = blockPosition.y + block->maxBB.y / 16.0f;
    if (player.camera.position.y >= liquidSurface) return false;
    if (tint != NULL) *tint = block->liquidTint;
    return true;
}


Vector3 Player_GetForwardVector(void) {
    float cx = cosf(playerCameraAngle.x);
    float sx = sinf(playerCameraAngle.x);
    
    float sy = sinf(playerCameraAngle.y);
    float cy = cosf(playerCameraAngle.y);
    
    return (Vector3) {cx * sy, cy, sx * sy};
}

Vector3 Player_GetChunkPosition(void) {
    return (Vector3) {(int)floor(player.position.x / CHUNK_SIZE_X), (int)floor(player.position.y / CHUNK_SIZE_Y), (int)floor(player.position.z / CHUNK_SIZE_Z)};
}

/* ---- v46: web rendering - white wireframe line + anchor diamond ---- */
void Player_DrawWeb(void) {
    if (!player.webActive) return;

    Vector3 hand = { player.position.x + 0.5f, player.position.y + 1.4f, player.position.z + 0.5f };
    Vector3 toAnchor = Vector3Subtract(player.webAnchor, hand);
    float len = Vector3Length(toAnchor);
    if (len < 0.1f) return;
    Vector3 dir = Vector3Scale(toAnchor, 1.0f / len);
    /* perpendicular for the shimmer strand */
    Vector3 side = Vector3Normalize(Vector3CrossProduct(dir, (Vector3){ 0, 1, 0.01f }));
    float t = (float)GetTime();
    float sag = sinf(t * 7.0f) * 0.05f * (1.0f - len / 60.0f);

    rlDrawRenderBatchActive();
    rlBegin(RL_LINES);
    /* main strand */
    rlColor4ub(235, 235, 245, 255);
    rlVertex3f(hand.x, hand.y, hand.z);
    rlVertex3f(player.webAnchor.x, player.webAnchor.y, player.webAnchor.z);
    /* shimmer strand */
    rlColor4ub(150, 150, 170, 200);
    rlVertex3f(hand.x + side.x * 0.06f, hand.y + 0.06f, hand.z + side.z * 0.06f);
    rlVertex3f(player.webAnchor.x + side.x * (0.1f + sag), player.webAnchor.y - 0.05f + sag,
               player.webAnchor.z + side.z * (0.1f + sag));
    /* anchor diamond */
    {
        float r = 0.14f + 0.03f * sinf(t * 6.0f);
        float a = t * 2.0f;
        Vector3 d[4];
        for (int k = 0; k < 4; k++) {
            float ang = a + k * 1.5708f;
            d[k] = (Vector3){ player.webAnchor.x + cosf(ang) * r,
                              player.webAnchor.y + sinf(ang) * r * 0.7f,
                              player.webAnchor.z + sinf(ang) * r };
        }
        rlColor4ub(255, 255, 255, 255);
        for (int k = 0; k < 4; k++) {
            rlVertex3f(d[k].x, d[k].y, d[k].z);
            rlVertex3f(d[(k + 1) % 4].x, d[(k + 1) % 4].y, d[(k + 1) % 4].z);
        }
    }
    rlEnd();
    rlDrawRenderBatchActive();
}

/* v46.1: aiming feedback - is there a valid web anchor under the crosshair? */
bool Player_GetWebTarget(Vector3 *blockCell) {
    if (player.webActive || player.flying) return false;
    Vector3 eye = { player.position.x + 0.5f, player.position.y + 1.5f, player.position.z + 0.5f };
    Vector3 pullPoint, cell;
    if (!Player_WebRay(eye, Player_GetForwardVector(), &pullPoint, &cell)) return false;
    if (blockCell) *blockCell = cell;
    return true;
}
