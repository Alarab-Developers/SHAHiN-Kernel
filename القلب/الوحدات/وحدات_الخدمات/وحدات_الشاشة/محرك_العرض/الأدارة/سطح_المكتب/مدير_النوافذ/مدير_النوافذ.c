#include "مدير_النوافذ.h"
#include <string.h>

static window_t windows[MAX_WINDOWS];
static int window_count = 0;

// دوال الرسم (تُعيّن من الخارج)
static window_drawing_t drawing;

void window_set_drawing_functions(window_drawing_t *drawing_funcs)
{
    if (drawing_funcs)
    {
        drawing = *drawing_funcs;
    }
}

void window_manager_init(void)
{
    window_count = 0;
}

window_t* window_create(
    int x,
    int y,
    int width,
    int height,
    uint32_t color,
    const char *title
)
{
    if (window_count >= MAX_WINDOWS)
        return 0;

    window_t *window = &windows[window_count++];

    window->x = x;
    window->y = y;

    window->width  = width;
    window->height = height;

    window->color = color;
    window->visible = 1;
    
    window->title = title;
    
    return window;
}

const window_t* window_get(int index)
{
    if (index < 0 || index >= window_count)
        return 0;

    return &windows[index];
}

int window_get_count(void)
{
    return window_count;
}


void window_draw_bottom_rounded_rect(
    int x,
    int y,
    int width,
    int height,
    uint32_t color,
    int radius)
{
    if (!drawing.fill_rect || !drawing.fill_circle)
        return;

    if (radius <= 0 || width < radius * 2 || height < radius)
    {
        drawing.fill_rect(x, y, width, height, color);
        return;
    }

    /* الجزء العلوي بالكامل */
    drawing.fill_rect(
        x,
        y,
        width,
        height - radius,
        color
    );

    /* الشريط السفلي الأوسط */
    drawing.fill_rect(
        x + radius,
        y + height - radius,
        width - radius * 2,
        radius,
        color
    );

    /* الزاوية السفلية اليسرى */
    drawing.fill_circle(
        x + radius,
        y + height - radius,
        radius,
        color
    );

    /* الزاوية السفلية اليمنى */
    drawing.fill_circle(
        x + width - radius,
        y + height - radius,
        radius,
        color
    );
}


void window_manager_update(
    int mouse_x,
    int mouse_y,
    int mouse_left
)
{
    static window_t *active = 0;
    static int last_mouse_left = 0;

    if (!mouse_left)
    {
        active = 0;
        last_mouse_left = 0;
        return;
    }

    if (mouse_left && !last_mouse_left)
    {
        for (int i = window_count - 1; i >= 0; i--)
        {
            window_t *window = &windows[i];

            if (!window->visible)
                continue;

            int button_radius = 12;

            int button_margin_x;
            int button_margin_y;

            if (window->maximized)
            {
                button_margin_x = 8;
                button_margin_y = 2;
            }
            else
            {
                button_margin_x = 6;
                button_margin_y = 5;
            }

            /* ===================================================== */
            /* زر الإغلاق */
            /* ===================================================== */

            int close_x =
                window->x +
                window->width -
                (button_radius * 2) -
                button_margin_x;

            int close_y =
                window->y +
                button_margin_y;

            int dx = mouse_x - (close_x + button_radius);
            int dy = mouse_y - (close_y + button_radius);

            if ((dx * dx + dy * dy) <= (button_radius * button_radius))
            {
                window->visible = 0;
                last_mouse_left = 1;
                return;
            }

            /* ===================================================== */
            /* زر التكبير */
            /* ===================================================== */

            int maximize_x =
                close_x -
                (button_radius * 2 + 8);

            int maximize_y = close_y;

            dx = mouse_x - (maximize_x + button_radius);
            dy = mouse_y - (maximize_y + button_radius);

            if ((dx * dx + dy * dy) <= (button_radius * button_radius))
            {
                if (!window->maximized)
                {
                    window->restore_x = window->x;
                    window->restore_y = window->y;
                    window->restore_width = window->width;
                    window->restore_height = window->height;

                    window->x = 0;
                    window->y = 0;

                    window->width = drawing.get_screen_width();
                    window->height = drawing.get_screen_height();

                    window->maximized = 1;
                }
                else
                {
                    window->x = window->restore_x;
                    window->y = window->restore_y;

                    window->width = window->restore_width;
                    window->height = window->restore_height;

                    window->maximized = 0;
                }

                last_mouse_left = 1;
                return;
            }

            /* ===================================================== */
            /* سحب النافذة */
            /* ===================================================== */

            if (mouse_x < window->x)
                continue;

            if (mouse_y < window->y)
                continue;

            if (mouse_x >= window->x + window->width)
                continue;

            if (mouse_y >= window->y + 24)
                continue;

            active = window;

            active->drag_offset_x = mouse_x - window->x;
            active->drag_offset_y = mouse_y - window->y;

            break;
        }
    }

    if (active && !active->maximized)
    {
        active->x = mouse_x - active->drag_offset_x;
        active->y = mouse_y - active->drag_offset_y;
    }

    last_mouse_left = mouse_left;
}

// ============================================================
// دوال الرسم الخاصة بالنوافذ (تستخدم الدوال المُمررة)
// ============================================================

void window_draw_rounded_rect(int x, int y, int width, int height, uint32_t color, int radius)
{
    // التحقق من وجود دوال الرسم
    if (!drawing.fill_rect || !drawing.fill_circle)
        return;

    if (width < radius * 2 || height < radius * 2)
    {
        drawing.fill_rect(x, y, width, height, color);
        return;
    }

    // 1. رسم المستطيل الرئيسي (بدون الزوايا)
    drawing.fill_rect(x + radius, y, width - (radius * 2), height, color);
    drawing.fill_rect(x, y + radius, radius, height - (radius * 2), color);
    drawing.fill_rect(x + width - radius, y + radius, radius, height - (radius * 2), color);

    // 2. رسم الزوايا الأربع كدوائر مملوءة
    drawing.fill_circle(x + radius, y + radius, radius, color);
    drawing.fill_circle(x + width - radius, y + radius, radius, color);
    drawing.fill_circle(x + radius, y + height - radius, radius, color);
    drawing.fill_circle(x + width - radius, y + height - radius, radius, color);
}

void window_draw_all(void)
{
    for (int i = 0; i < window_count; i++)
    {
        const window_t *window = &windows[i];

        if (!window->visible)
            continue;
        /* رسم النافذة */
        if (window->maximized)
        {
            drawing.fill_rect(
                window->x,
                window->y,
                window->width,
                window->height,
                window->color
            );
        }
        else
        {
            window_draw_bottom_rounded_rect(
                window->x,
                window->y,
                window->width,
                window->height,
                window->color,
                WINDOW_CORNER_RADIUS
            );
        }

        // رسم شريط العنوان
        uint32_t title_color = 0x00353535;
        int title_height = 28;

        if (window->maximized)
        {
            drawing.fill_rect(
                window->x,
                window->y,
                window->width,
                title_height,
                title_color
            );
        }
        else
        {
            drawing.fill_rect(
                window->x + 2,
                window->y + 2,
                window->width - 4,
                title_height,
                title_color
            );
        }
    

    int text_width = strlen(window->title) * 8;

    int text_x =
        window->x +
        (window->width - text_width) / 2;

    int text_y =
        window->y +
        (title_height - 8) / 2;

    drawing.draw_text(
        text_x,
        text_y,
        window->title,
        0x00FFFFFF
    );

    
        int button_radius = 12;

        int button_margin_x;
        int button_margin_y;

        if (window->maximized)
        {
            button_margin_x = 8;
            button_margin_y = 2;
        }
        else
        {
            button_margin_x = 6;
            button_margin_y = 5;
        }

        int close_x = window->x +
                    window->width -
                    button_radius * 2 -
                    button_margin_x;

        int close_y = window->y + button_margin_y;

        drawing.fill_circle(
            close_x + button_radius,
            close_y + button_radius,
            button_radius,
            0x00CC3333
        );

        int maximize_x = close_x - (button_radius * 2 + 8);
        int maximize_y = close_y;

        drawing.fill_circle(
            maximize_x + button_radius,
            maximize_y + button_radius,
            button_radius,
            0x0000CC33
        );
    
    }
}
