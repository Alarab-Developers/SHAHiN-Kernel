#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <stdint.h>

#define MAX_WINDOWS 32
#define WINDOW_CORNER_RADIUS 12

typedef struct
{
    int x;
    int y;

    int width;
    int height;

    uint32_t color;

    int visible;

    int dragging;

    int drag_offset_x;
    int drag_offset_y;


    int maximized;

    int restore_x;
    int restore_y;

    int restore_width;
    int restore_height;

    const char *title;

} window_t;

// تعريف دوال الرسم كـ Function Pointers
typedef struct {
    void (*fill_rect)(int x, int y, int width, int height, uint32_t color);
    void (*fill_circle)(int cx, int cy, int radius, uint32_t color);

    int (*get_screen_width)(void);
    int (*get_screen_height)(void);

    void (*draw_text)(
        int,
        int,
        const char *,
        uint32_t
    );

} window_drawing_t;

void window_manager_init(void);

window_t* window_create(
    int x,
    int y,
    int width,
    int height,
    uint32_t color,
    const char *title
);

void window_manager_update(
    int mouse_x,
    int mouse_y,
    int mouse_left
);

const window_t* window_get(int index);

int window_get_count(void);

// دوال الرسم الجديدة
void window_set_drawing_functions(window_drawing_t *drawing);
void window_draw_all(void);
void window_draw_rounded_rect(int x, int y, int width, int height, uint32_t color, int radius);

#endif
