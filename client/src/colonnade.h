/*
 * Midless: Cosmic Edition - the pocket peristyle (v65.16).
 *
 * The dreamcore cloud billboards lost: sprites poke the camera and
 * flash through at grazing angles. The pocket perimeter is real
 * geometry now - a ring of round marble columns carrying round
 * arches (a curved arcade, not square lintels), one merged mesh,
 * one draw call, own tiny two-sided marble shader.
 */
#ifndef COLONNADE_H
#define COLONNADE_H

void Colonnade_Init(void);
void Colonnade_Draw(float pocketFactor);

#endif
