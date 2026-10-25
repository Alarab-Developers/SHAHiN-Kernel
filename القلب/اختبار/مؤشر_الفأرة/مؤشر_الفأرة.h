#ifndef VIRTUAL_MOUSE_H
#define VIRTUAL_MOUSE_H

#include "القلب/المكتبات/المكتبات.h"

extern int mouse_x;
extern int mouse_y;
extern int mouse_left;
extern int mouse_left_clicked;   /* حدث النقر المتراكم */
extern int mouse_dx;
extern int mouse_dy;

void mouse_init();
void mouse_handler();

/*
 * استهلاك حدث النقر:
 * ترجع 1 إذا حدثت نقرة منذ آخر استهلاك، ثم تصفّر العلم.
 * ترجع 0 إن لم توجد نقرة معلّقة.
 */
int mouse_consume_click(void);

#endif
