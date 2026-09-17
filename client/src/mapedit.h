#ifndef MIDLESS_MAPEDIT_H
#define MIDLESS_MAPEDIT_H
/* v65.32: the parkour map editor - a standalone screen with its own
 * professional raygui workbench UI (docks, palette, properties, map
 * library), a fly camera and chain/region building tools. Maps save to
 * maps/*.pmap and load into the second pocket zone at host start. */
void MapEdit_Enter(void);
void MapEdit_Frame(void);   /* update + 3D viewport + 2D workbench UI */
#endif
