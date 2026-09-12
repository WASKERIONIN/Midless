/* test stub - minimal player surface used by client/src/mobs.c */
#ifndef PLAYER_H_STUB
#define PLAYER_H_STUB
#include "raylib.h"

typedef struct Player {
    Camera camera;
    Vector3 position;
    Vector3 velocity;
    bool flying;
    int hp;
} Player;

#define COSMIC_SPAWN_X 8.0f
#define COSMIC_SPAWN_Y 77.0f
#define COSMIC_SPAWN_Z 8.0f
#define COSMIC_VOID_Y  8.0f

extern Player player;

void Player_Damage(int amount, Vector3 push);
void Player_Heal(int amount);
void Player_HotbarAutoAdd(int blockId);

#endif
