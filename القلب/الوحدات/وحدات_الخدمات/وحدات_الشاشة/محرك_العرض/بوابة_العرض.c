#include "بوابة_العرض.h"
#include "القلب/الوحدات/وحدات_الخدمات/وحدات_الشاشة/محرك_العرض/الأدارة/محرك_العرض.h"

/* ========================================================= */
/* WRAPPERS                                                   */
/* ========================================================= */

static void api_init(void)
{
    display_init();
}

static void api_render(void)
{
    display_render();
}

static void api_set_background(uint32_t color)
{
    display_set_background(color);
}

/* ========================================================= */
/* API TABLE                                                  */
/* ========================================================= */

display_api_t display_api =
{
    .init           = api_init,
    .render         = api_render,
    .set_background = api_set_background
};
