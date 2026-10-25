#include "الهياكل.h"
#include "../ادوات/ادوات.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"

typedef struct
{
    char        *name;           /* ديناميكي */
    arp_value_t  default_value;
} struct_field_def_t;

typedef struct
{
    char                *name;   /* ديناميكي */
    struct_field_def_t  *fields; /* ديناميكي */
    int                  field_count;
} struct_def_t;

static struct_def_t *g_structs        = NULL;
static int           g_struct_count   = 0;
static int           g_struct_capacity = 0;

/* ========================================================= */
/* تحرير عنصر واحد                                           */
/* ========================================================= */
static void free_struct_def(struct_def_t *s)
{
    if (!s) return;

    if (s->name) { memory_api.free(s->name); s->name = NULL; }

    if (s->fields)
    {
        for (int i = 0; i < s->field_count; i++)
        {
            if (s->fields[i].name) memory_api.free(s->fields[i].name);
            arp_value_free(&s->fields[i].default_value);
        }
        memory_api.free(s->fields);
        s->fields = NULL;
    }
    s->field_count = 0;
}

void structs_init(void)
{
    for (int i = 0; i < g_struct_count; i++)
        free_struct_def(&g_structs[i]);

    if (g_structs) memory_api.free(g_structs);
    g_structs         = NULL;
    g_struct_count    = 0;
    g_struct_capacity = 0;
}

static struct_def_t *find_struct(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; i < g_struct_count; i++)
        if (g_structs[i].name && strcmp(g_structs[i].name, name) == 0)
            return &g_structs[i];
    return NULL;
}

int struct_exists(const char *name)
{
    return find_struct(name) ? 1 : 0;
}

int struct_register(
    const char        *name,
    const char       **field_names,
    const arp_value_t *defaults,
    int                field_count
)
{
    if (!name || !*name) return -1;
    if (field_count < 0) return -1;
    if (field_count > 0 && (!field_names || !defaults)) return -1;

    struct_def_t *existing = find_struct(name);

    if (!existing)
    {
        if (g_struct_count >= g_struct_capacity)
        {
            int nc = g_struct_capacity ? g_struct_capacity * 2 : 4;
            struct_def_t *na = (struct_def_t *)
                memory_api.alloc(nc * sizeof(struct_def_t));
            if (!na) return -1;
            if (g_structs)
            {
                memcpy(na, g_structs,
                       g_struct_count * sizeof(struct_def_t));
                memory_api.free(g_structs);
            }
            g_structs         = na;
            g_struct_capacity = nc;
        }

        existing = &g_structs[g_struct_count];
        memset(existing, 0, sizeof(struct_def_t));
        existing->name = arp_strdup(name);
        if (!existing->name) return -1;
        g_struct_count++;
    }
    else
    {
        /* استبدال: حرّر الحقول القديمة فقط */
        if (existing->fields)
        {
            for (int i = 0; i < existing->field_count; i++)
            {
                memory_api.free(existing->fields[i].name);
                arp_value_free(&existing->fields[i].default_value);
            }
            memory_api.free(existing->fields);
            existing->fields = NULL;
        }
        existing->field_count = 0;
    }

    if (field_count > 0)
    {
        existing->fields = (struct_field_def_t *)
            memory_api.alloc(field_count * sizeof(struct_field_def_t));
        if (!existing->fields) return -1;

        for (int i = 0; i < field_count; i++)
        {
            existing->fields[i].name = arp_strdup(field_names[i]);
            if (!existing->fields[i].name)
            {
                for (int j = 0; j < i; j++)
                {
                    memory_api.free(existing->fields[j].name);
                    arp_value_free(&existing->fields[j].default_value);
                }
                memory_api.free(existing->fields);
                existing->fields = NULL;
                return -1;
            }

            arp_value_init(&existing->fields[i].default_value, ARP_TYPE_UNKNOWN);
            if (arp_value_copy(&existing->fields[i].default_value,
                               &defaults[i]) < 0)
            {
                memory_api.free(existing->fields[i].name);
                for (int j = 0; j < i; j++)
                {
                    memory_api.free(existing->fields[j].name);
                    arp_value_free(&existing->fields[j].default_value);
                }
                memory_api.free(existing->fields);
                existing->fields = NULL;
                return -1;
            }
        }
        existing->field_count = field_count;
    }

    return 0;
}

int struct_field_count(const char *name)
{
    struct_def_t *s = find_struct(name);
    return s ? s->field_count : -1;
}

const char *struct_field_name_at(const char *name, int index)
{
    struct_def_t *s = find_struct(name);
    if (!s || index < 0 || index >= s->field_count) return NULL;
    return s->fields[index].name;
}

const arp_value_t *struct_field_default_at(const char *name, int index)
{
    struct_def_t *s = find_struct(name);
    if (!s || index < 0 || index >= s->field_count) return NULL;
    return &s->fields[index].default_value;
}
