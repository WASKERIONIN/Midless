/* test stub - replaces client rlgl.h so mobs.c drives the batching model */
#ifndef RLGL_H_STUB
#define RLGL_H_STUB
#include "../rlmock.h"
/* matrices the game reads out of rlgl during draw */
static Matrix rlGetMatrixModelview(void) {
    /* a plain camera-space identity view: billboards built from this are
     * axis-aligned, which is all the model needs */
    Matrix m = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
    return m;
}
static Matrix rlGetMatrixProjection(void) {
    Matrix m = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
    return m;
}
#endif
