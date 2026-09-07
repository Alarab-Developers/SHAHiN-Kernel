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

/*
 * Upper bounds for the line buffer.
 * Actual visible rows/cols are computed at init time
 * from the real screen resolution, these are just
 * safe ceilings for the static arrays below.
 */
#define DEVELOPMENT_SCREEN_MAX_ROWS     128
#define DEVELOPMENT_SCREEN_MAX_COLS     256


/* ========================================================= */
/* STATE                                                       */
/* ========================================================= */

static int cursor_x =
    DEVELOPMENT_SCREEN_MARGIN_X;

static int cursor_y =
    DEVELOPMENT_SCREEN_MARGIN_Y;

/*
 * Text buffer of everything currently on screen,
 * kept so the screen can be redrawn after a scroll.
 */
static char line_buffer
    [DEVELOPMENT_SCREEN_MAX_ROWS]
    [DEVELOPMENT_SCREEN_MAX_COLS];

static int line_length
    [DEVELOPMENT_SCREEN_MAX_ROWS];

static int visible_rows = 0;
static int current_row  = 0;


/* ========================================================= */
/* INTERNAL                                                    */
/* ========================================================= */

static void redraw_screen(void)
{
    video_api.clear(0x00000000);

    for (int row = 0; row < visible_rows; row++)
    {
        int x = DEVELOPMENT_SCREEN_MARGIN_X;
        int y =
            DEVELOPMENT_SCREEN_MARGIN_Y +
            row * DEVELOPMENT_SCREEN_CHAR_HEIGHT;

        for (int col = 0; col < line_length[row]; col++)
        {
            draw_char(
                x,
                y,
                line_buffer[row][col],
                0x00FFFFFF
            );

            x += DEVELOPMENT_SCREEN_CHAR_WIDTH;
        }
    }
}


static void scroll_up(void)
{
    for (int row = 0; row < visible_rows - 1; row++)
    {
        line_length[row] = line_length[row + 1];

        for (int col = 0; col < line_length[row]; col++)
            line_buffer[row][col] = line_buffer[row + 1][col];
    }

    line_length[visible_rows - 1] = 0;

    redraw_screen();
}


static void new_line(void)
{
    cursor_x =
        DEVELOPMENT_SCREEN_MARGIN_X;

    /*
     * If there is still an empty row below, move to it.
     * Otherwise, scroll everything up by one line
     * (like a normal terminal / dev console), instead
     * of wrapping back to the top of the screen.
     */
    if (current_row + 1 < visible_rows)
    {
        current_row++;
    }
    else
    {
        scroll_up();
    }

    cursor_y =
        DEVELOPMENT_SCREEN_MARGIN_Y +
        current_row * DEVELOPMENT_SCREEN_CHAR_HEIGHT;

    line_length[current_row] = 0;
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

    visible_rows =
        ((int)video_api.height() - 2 * DEVELOPMENT_SCREEN_MARGIN_Y) /
        DEVELOPMENT_SCREEN_CHAR_HEIGHT;

    if (visible_rows > DEVELOPMENT_SCREEN_MAX_ROWS)
        visible_rows = DEVELOPMENT_SCREEN_MAX_ROWS;

    if (visible_rows < 1)
        visible_rows = 1;

    for (int row = 0; row < DEVELOPMENT_SCREEN_MAX_ROWS; row++)
        line_length[row] = 0;

    current_row = 0;

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

        if (line_length[current_row] < DEVELOPMENT_SCREEN_MAX_COLS)
        {
            line_buffer[current_row][line_length[current_row]] = *text;

            line_length[current_row]++;
        }

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


void development_screen_print_hex64(
    uint64_t value
)
{
    static const char hex_digits[] = "0123456789ABCDEF";

    /* "0x" + 16 hex digits + '\0' */
    char buffer[19];

    buffer[0] = '0';
    buffer[1] = 'x';

    for (int i = 0; i < 16; i++)
    {
        int shift = (15 - i) * 4;

        buffer[2 + i] =
            hex_digits[(value >> shift) & 0xF];
    }

    buffer[18] = '\0';

    development_screen_print(buffer);
}


void development_screen_update(void)
{
    video_api.swap();
}
