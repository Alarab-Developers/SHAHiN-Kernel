#ifndef VIRTUAL_MOUSE_H
#define VIRTUAL_MOUSE_H

#include "القلب/المكتبات/المكتبات.h"

extern int mouse_x;
extern int mouse_y;
extern int mouse_left;
extern int mouse_dx;
extern int mouse_dy;

void mouse_init();
void mouse_handler();


#endif
