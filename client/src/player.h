/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_PLAYER_H
#define MIDLESS_CLIENT_PLAYER_H

#include "raylib.h"
#include "raycast.h"
#include "entitymodel.h"
#include "entity.h"

#define COSMIC_SPAWN_X 8.0f
#define COSMIC_SPAWN_Y 77.0f
#define COSMIC_SPAWN_Z 8.0f
#define COSMIC_VOID_Y  8.0f

typedef enum PlayerCameraMode {
    PLAYER_CAMERA_FIRST_PERSON,
    PLAYER_CAMERA_THIRD_PERSON_BEHIND,
    PLAYER_CAMERA_THIRD_PERSON_FRONT
} PlayerCameraMode;

typedef struct Player{
    Camera camera;
    float speed;
    Vector3 position;
    Vector3 direction;
    Vector3 velocity;
    BoundingBox collisionBox;
    RaycastResult rayResult;
    int blockSelected;
    bool canJump;
    float liquidSubmersion;
    unsigned char entityType;
    unsigned char modelId;
    bool hasEntityModel;
    bool flying;
    /* v48.2: weapons - 0 = blade (melee), 1 = laser (ranged) */
    int weaponMode;
    /* v46.1: crouch + double dash */
    bool crouching;
    float crouchT;         /* smooth 0..1 eye/speed blend */
    int dashChargesUsed;   /* 2 charges, refill on landing */
    /* v46: web grapple */
    bool webActive;
    Vector3 webAnchor;    /* pull point, slightly off the hit face */
    Vector3 webBlock;     /* cell of the anchored block (validity check) */
    /* v45: vitals */
    int hp;
    double invulnUntil;
    double lastHurtTime;
    /* v44 traversal: double jump, dash, glide */
    int airJumpsUsed;
    double dashReadyTime;      /* time the dash becomes available again */
    double dashActiveUntil;    /* burst window end */
    Vector3 dashDir;           /* normalized horizontal dash direction */
    PlayerCameraMode cameraMode;
    EntityModel entityModel;
    EntityAnimation animation;
} Player;
extern Player player;

//Initialize a player.
void Player_Init(void);

//Check/Do Inputs
void Player_CheckInputs(void);

//Update a player.
void Player_Update(void);
void Player_Draw(void);
void Player_DrawWeb(void);
void Player_DrawLaser(void);
bool Player_GetWebTarget(Vector3 *blockCell);
bool Player_NearWarpCore(void);
void Player_SetEntityModel(int type, int modelId);
void Player_ClearEntityModel(void);
void Player_Teleport(Vector3 position);
float Player_GetRespawnFade(void);

bool Player_TryPlaceBlock(Vector3 pos, int blockId);

bool Player_TestCollision(Vector3 offset);
void Player_Damage(int amount, Vector3 fromDir);
void Player_Heal(int amount);
int Player_GetHp(void);
int Player_GetShards(void);
int Player_GetLaserRangeLvl(void);
int Player_GetBurstLvl(void);     /* v51 volley fire 0..3 */
int Player_GetCoolLvl(void);      /* v53 cooling upgrade 0..3 */
int Player_GetLaserRateLvl(void);
int Player_GetLaserRange(void);
float Player_GetLaserCooldown(void);
bool Player_BuyLaserUpgrade(int kind); /* 0 = range, 1 = rate */
void Player_AddShards(int n);
bool Player_BuyArmorUpgrade(void);      /* v58 */
int Player_GetArmorLvl(void);           /* v58 */
void Player_AddScroll(int n);           /* v58 */
int Player_GetScrollCount(void);        /* v58 */
int Player_TakeScroll(void);            /* v58 */
double Player_GetGazeTimeLeft(void);    /* v58: seconds of clear black-hole sight */
int Player_HotbarItem(int slot);               /* v58 */
const char *Player_HotbarItemName(int slot);   /* v58 */
bool Player_HotbarIsEmpty(void);        /* v58 */
int Player_HotbarAutoAdd(int item);     /* v58 */
bool Player_HotbarAssign(int slot, int item);  /* v58 */
void Player_HotbarUseSlot(int slot);    /* v58 */
void Player_SaveProgress(void);
void Player_LoadProgress(void);
float Player_GetLiquidSubmersion(void);
bool Player_GetCameraLiquidTint(Color *tint);
Vector3 Player_GetForwardVector(void);

//Get player position in chunk units.
Vector3 Player_GetChunkPosition(void);

#endif
