#ifndef MIDLESS_CLIENT_MOBS_H
#define MIDLESS_CLIENT_MOBS_H

#include "raylib.h"
#include <stdbool.h>

/* v50: island fauna - crawlers (hostile ground bots) and wisps (shy shard
 * carriers). Wireframe style, same visual language as the hunters. */

void Mobs_Init(void);
void Mobs_Shutdown(void);
void Mobs_Update(float deltaTime);
void Mobs_Draw(void);

/* weapon dispatchers: return true if something was hit */
bool Mobs_MeleeHit(Vector3 origin, Vector3 dir, float maxDist);
bool Mobs_LaserHit(Vector3 origin, Vector3 dir, float maxDist, Vector3 *hitPoint);

int Mobs_CrawlerCount(void);
int Mobs_WispCount(void);
void Mobs_ExplosionDamage(Vector3 center, float radius, int damage);
bool Mobs_SpawnSpider(Vector3 pos);          /* cocoon hatches; false = hatch busy */
void Mobs_DrawBillboard(Vector3 base, float halfW, float h, int tile, unsigned char bright, float lean); /* v57 flora, v58 wind lean */
void Mobs_SpriteBatchBegin(Texture2D atlas);   /* v59.6: alpha-cutout sprite batch */
void Mobs_SpriteBatchEnd(void);
int Mobs_MothCount(void);                /* v59: glowmoths on the wing */
int Mobs_GetMushrooms(void);
void Mobs_SetMushrooms(int n);               /* progress restore */
int Mobs_SpiderCount(void);                  /* satchel fauna line */
bool Mobs_EatMushroom(void);                 /* +3 HP if any */
bool Mobs_TryCollectMushroom(void);          /* E near one */
bool Mobs_CocoonLaser(Vector3 origin, Vector3 dir, float maxDist, Vector3 *hitPoint);
bool Mobs_InStarterSanctuary(Vector3 p);     /* v61.1: spawn-free starter isle */

#endif
