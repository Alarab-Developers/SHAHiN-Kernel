#include "شاشة_التطوير.h"

#include "القلب/الوحدات/محرك_الفيديو/بوابة_الفيديو.h"
#include "القلب/الوحدات/محرك_الفيديو/محرك_الفيديو.h"


/* ========================================================= */
/* SETTINGS                                                    */
/* ========================================================= */

#define DEVELOPMENT_SCREEN_CHAR_WIDTH   8
#define DEVELOPMENT_SCREEN_CHAR_HEIGHT  8

#define DEVELOPMENT_SCREEN_MARGIN_X     8
#define DEVELOPMENT_SCREEN_MARGIN_Y     8


/* ========================================================= */
/* STATE                                                       */
/* ========================================================= */

static int cursor_x =
    DEVELOPMENT_SCREEN_MARGIN_X;

static int cursor_y =
    DEVELOPMENT_SCREEN_MARGIN_Y;


/* ========================================================= */
/* INTERNAL                                                    */
/* ========================================================= */

static void new_line(void)
{
    cursor_x =
        DEVELOPMENT_SCREEN_MARGIN_X;

    cursor_y +=
        DEVELOPMENT_SCREEN_CHAR_HEIGHT;

    /*
     * Temporary behavior:
     * when reaching the bottom of the screen,
     * start again from the top.
     */
    if (
        cursor_y +
        DEVELOPMENT_SCREEN_CHAR_HEIGHT >=
        (int)video_api.height()
    )
    {
        cursor_y =
            DEVELOPMENT_SCREEN_MARGIN_Y;
    }
}


static void check_line_end(void)
{
    if (
        cursor_x +
        DEVELOPMENT_SCREEN_CHAR_WIDTH >
        (int)video_api.width()
    )
    {
        new_line();
    }
}


/* ========================================================= */
/* PUBLIC API                                                  */
/* ========================================================= */

void development_screen_init(void)
{
    cursor_x =
        DEVELOPMENT_SCREEN_MARGIN_X;

    cursor_y =
        DEVELOPMENT_SCREEN_MARGIN_Y;

    video_api.clear(0x00000000);

    video_api.swap();
}


void development_screen_print(
    const char *text
)
{
    if (!text)
        return;

    while (*text)
    {
        if (*text == '\n')
        {
            new_line();

            text++;

            continue;
        }

        check_line_end();

        draw_char(
            cursor_x,
            cursor_y,
            *text,
            0x00FFFFFF
        );

        cursor_x +=
            DEVELOPMENT_SCREEN_CHAR_WIDTH;

        text++;
    }
}


void development_screen_println(
    const char *text
)
{
    development_screen_print(text);

    new_line();
}


void development_screen_update(void)
{
    video_api.swap();
}
