#include "رأس_التطبيق.h"

typedef struct
{
    app_entry_t entry;

} app_context_t;

static app_context_t app_slots[MAX_APPS];

static int app_slot_count = 0;

/* ========================================================= */
/* Launchers */
/* ========================================================= */

static void app_launcher_0()
{
    app_slots[0].entry(&kapi);

    while (1)
        asm volatile("hlt");
}

static void app_launcher_1()
{
    app_slots[1].entry(&kapi);

    while (1)
        asm volatile("hlt");
}

static void app_launcher_2()
{
    app_slots[2].entry(&kapi);

    while (1)
        asm volatile("hlt");
}

static void app_launcher_3()
{
    app_slots[3].entry(&kapi);

    while (1)
        asm volatile("hlt");
}

static void app_launcher_4()
{
    app_slots[4].entry(&kapi);

    while (1)
        asm volatile("hlt");
}

static void app_launcher_5()
{
    app_slots[5].entry(&kapi);

    while (1)
        asm volatile("hlt");
}

static void app_launcher_6()
{
    app_slots[6].entry(&kapi);

    while (1)
        asm volatile("hlt");
}

static void app_launcher_7()
{
    app_slots[7].entry(&kapi);

    while (1)
        asm volatile("hlt");
}

typedef void (*launcher_fn)(void);

static launcher_fn launchers[MAX_APPS] =
{
    app_launcher_0,
    app_launcher_1,
    app_launcher_2,
    app_launcher_3,
    app_launcher_4,
    app_launcher_5,
    app_launcher_6,
    app_launcher_7
};

int app_allocate_slot(void)
{
    if (app_slot_count >= MAX_APPS)
        return -1;

    return app_slot_count++;
}

void app_set_entry(
    int slot,
    app_entry_t entry
)
{
    if (slot < 0 || slot >= MAX_APPS)
        return;

    app_slots[slot].entry = entry;
}

launcher_fn app_get_launcher(
    int slot
)
{
    if (slot < 0 || slot >= MAX_APPS)
        return 0;

    return launchers[slot];
}
