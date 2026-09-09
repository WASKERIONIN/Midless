#include "bird.h"
#include "world.h"
#include "player.h"
#include "block.h"
#include <math.h>

#define BIRD_COUNT 18

typedef enum { BIRD_FLY, BIRD_SIT, BIRD_PECK } BirdState;

typedef struct {
    Vector3 pos;
    Vector3 vel;
    BirdState state;
    float timer;
    Color color;
} Bird;

static Bird birds[BIRD_COUNT];
static bool ready;

static int GroundY(int x, int z) {
    int start = (int)player.position.y + 24;
    for (int y = start; y >= 8; y--) {
        int id = World_GetBlock((Vector3){(float)x, (float)y, (float)z});
        const Block *b = Block_GetDefinition(id);
        if (b->colliderType == BLOCK_COLLIDER_SOLID) return y + 1;
    }
    return -1;
}

static void SpawnOne(Bird *b, unsigned int salt) {
    int x = (int)player.position.x + (int)(salt % 40) - 20;
    int z = (int)player.position.z + (int)((salt / 40) % 40) - 20;
    int y = GroundY(x, z);
    if (y < 0) y = (int)player.position.y + 8;
    b->pos = (Vector3){(float)x + 0.5f, (float)y + 0.2f, (float)z + 0.5f};
    b->vel = (Vector3){0};
    b->state = BIRD_SIT;
    b->timer = 1.0f + (salt % 5);
    unsigned int c = salt * 17u;
    b->color = (Color){(unsigned char)(80 + c % 100), (unsigned char)(40 + (c / 3) % 80),
                       (unsigned char)(140 + c % 80), 255};
}

void Bird_Init(void) {
    for (int i = 0; i < BIRD_COUNT; i++) SpawnOne(&birds[i], (unsigned int)(i * 7919 + 13));
    ready = true;
}

void Bird_Shutdown(void) { ready = false; }
void Bird_Clear(void) {
    if (ready) Bird_Init();
}

void Bird_Update(float dt) {
    if (!ready) return;
    for (int i = 0; i < BIRD_COUNT; i++) {
        Bird *b = &birds[i];
        b->timer -= dt;
        if (b->state == BIRD_SIT || b->state == BIRD_PECK) {
            if (b->timer <= 0) {
                if (b->state == BIRD_SIT && (i % 3 == 0)) {
                    b->state = BIRD_PECK;
                    b->timer = 0.6f;
                } else {
                    b->state = BIRD_FLY;
                    float ang = (float)i * 0.7f + (float)GetTime();
                    b->vel = (Vector3){cosf(ang) * 6.0f, 2.5f, sinf(ang) * 6.0f};
                    b->timer = 2.0f + (i % 4);
                }
            }
        } else {
            b->pos.x += b->vel.x * dt;
            b->pos.y += b->vel.y * dt;
            b->pos.z += b->vel.z * dt;
            b->vel.y -= 4.0f * dt;
            int gy = GroundY((int)b->pos.x, (int)b->pos.z);
            if (gy > 0 && b->pos.y <= gy + 0.2f && b->vel.y <= 0) {
                b->pos.y = gy + 0.2f;
                b->vel = (Vector3){0};
                b->state = BIRD_SIT;
                b->timer = 2.0f + (i % 5);
            }
            if (b->timer <= 0) {
                b->state = BIRD_SIT;
                b->vel = (Vector3){0};
                b->timer = 2.0f;
            }
        }
        float dx = b->pos.x - player.position.x;
        float dz = b->pos.z - player.position.z;
        if (dx * dx + dz * dz > 90.0f * 90.0f) SpawnOne(b, (unsigned int)GetTime() * 100 + (unsigned)i);
    }
}

void Bird_Draw(void) {
    if (!ready) return;
    for (int i = 0; i < BIRD_COUNT; i++) {
        Bird *b = &birds[i];
        float bob = (b->state == BIRD_PECK) ? -0.08f : 0.0f;
        DrawCube((Vector3){b->pos.x, b->pos.y + bob, b->pos.z}, 0.28f, 0.16f, 0.22f, b->color);
        DrawCube((Vector3){b->pos.x + 0.16f, b->pos.y + 0.06f + bob, b->pos.z}, 0.12f, 0.12f, 0.12f,
                 (Color){30, 20, 40, 255});
    }
}

void Bird_GetStats(int *total, int *fly, int *sit, int *peck) {
    int f = 0, s = 0, p = 0;
    for (int i = 0; i < BIRD_COUNT; i++) {
        if (birds[i].state == BIRD_FLY) f++;
        else if (birds[i].state == BIRD_PECK) p++;
        else s++;
    }
    if (total) *total = BIRD_COUNT;
    if (fly) *fly = f;
    if (sit) *sit = s;
    if (peck) *peck = p;
}
