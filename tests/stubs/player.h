/* test stub - minimal player surface used by client/src/mobs.c */
#ifndef PLAYER_H_STUB
#define PLAYER_H_STUB
#include "raylib.h"

typedef struct Player {
    Camera camera;
    Vector3 position;
    bool flying;
    int hp;
} Player;

extern Player player;

void Player_Damage(int amount, Vector3 push);
void Player_Heal(int amount);
void Player_HotbarAutoAdd(int blockId);

#endif
