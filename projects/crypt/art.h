/* art.h - the pictures of CRYPT, each a PNG under art/ turned into C by
 * tools/mkart.py at build time (CMakeLists.txt).  The ones here are the
 * stand-ins of art/placeholders.py until real ones come.  The walls
 * are not pictures: view.c draws them from art/geometry.py's tables. */

#ifndef CRYPT_ART_H
#define CRYPT_ART_H

#include <ms0515.h>
#include <tex.h>

extern const struct tex skeleton;	/* 4 columns by 32, on nothing */
extern const struct ms_image face1, face2, face3, face4;	/* 4 cells by 32 */

#endif
