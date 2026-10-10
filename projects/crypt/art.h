/* art.h - the pictures of CRYPT, each a PNG under art/ turned into C by
 * tools/mkart.py at build time (CMakeLists.txt).  The ones here are the
 * stand-ins of art/placeholders.py until real ones come.  The walls
 * along the party's axis are not here: art/planes.py draws them into
 * CRYPT.DAT (data.h). */

#ifndef CRYPT_ART_H
#define CRYPT_ART_H

#include <ms0515.h>
#include <tex.h>

extern const struct tex wall;		/* red bricks, 16 columns by 32: a wall ahead */
extern const struct tex wall2, wall4;	/* the same reduced twice and four times: farther walls */
extern const struct tex skeleton;	/* 4 columns by 32, on nothing */
extern const struct ms_image face1, face2, face3, face4;	/* 4 cells by 32 */

#endif
