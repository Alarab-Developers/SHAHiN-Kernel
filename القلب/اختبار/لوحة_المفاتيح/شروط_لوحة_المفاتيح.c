#include "شروط_لوحة_المفاتيح.h"
#include "لوحة_المفاتيح.h"
#include "القلب/الوحدات/اللغة/ادوات/ادوات.h"

typedef struct {
    const char *name;
    unsigned char key_code;
} key_map_entry_t;

static const key_map_entry_t key_map[] = {
    /* الأسهم */
    {"↑", KEY_UP},
    {"↓", KEY_DOWN},
    {"→", KEY_RIGHT},
    {"←", KEY_LEFT},

    /* المفاتيح العادية (حروف، أرقام، رموز) */
    {"Esc", KEY_ESC},
    {"1", KEY_1}, {"2", KEY_2}, {"3", KEY_3}, {"4", KEY_4},
    {"5", KEY_5}, {"6", KEY_6}, {"7", KEY_7}, {"8", KEY_8},
    {"9", KEY_9}, {"0", KEY_0},
    {"-", KEY_MINUS}, {"=", KEY_EQUALS},
    {"Backspace", KEY_BACKSPACE},
    {"Tab", KEY_TAB},
    {"Q", KEY_Q}, {"W", KEY_W}, {"E", KEY_E}, {"R", KEY_R},
    {"T", KEY_T}, {"Y", KEY_Y}, {"U", KEY_U}, {"I", KEY_I},
    {"O", KEY_O}, {"P", KEY_P},
    {"[", KEY_LBRACKET}, {"]", KEY_RBRACKET},
    {"Enter", KEY_ENTER},
    {"Ctrl", KEY_CTRL},
    {"A", KEY_A}, {"S", KEY_S}, {"D", KEY_D}, {"F", KEY_F},
    {"G", KEY_G}, {"H", KEY_H}, {"J", KEY_J}, {"K", KEY_K},
    {"L", KEY_L},
    {";", KEY_SEMICOLON}, {"'", KEY_APOSTROPHE}, {"`", KEY_GRAVE},
    {"Shift", KEY_LSHIFT},
    {"\\", KEY_BACKSLASH},
    {"Z", KEY_Z}, {"X", KEY_X}, {"C", KEY_C}, {"V", KEY_V},
    {"B", KEY_B}, {"N", KEY_N}, {"M", KEY_M},
    {",", KEY_COMMA}, {".", KEY_PERIOD}, {"/", KEY_SLASH},
    {"*", KEY_KP_MULTIPLY},
    {"Space", KEY_SPACE},
    {"CapsLock", KEY_CAPSLOCK},
    {"F1", KEY_F1}, {"F2", KEY_F2}, {"F3", KEY_F3}, {"F4", KEY_F4},
    {"F5", KEY_F5}, {"F6", KEY_F6}, {"F7", KEY_F7}, {"F8", KEY_F8},
    {"F9", KEY_F9}, {"F10", KEY_F10}, {"F11", KEY_F11}, {"F12", KEY_F12},

    /* أسماء عربية بديلة */
    {"مسافة", KEY_SPACE},
    {"دخول", KEY_ENTER},
    {"رجوع", KEY_BACKSPACE},
    {"تبويب", KEY_TAB},
    {"هروب", KEY_ESC},
    {"تحكم", KEY_CTRL},
    {"بديل", KEY_LALT},
    {"كابس", KEY_CAPSLOCK},
    {"ف1", KEY_F1}, {"ف2", KEY_F2}, {"ف3", KEY_F3}, {"ف4", KEY_F4},
    {"ف5", KEY_F5}, {"ف6", KEY_F6}, {"ف7", KEY_F7}, {"ف8", KEY_F8},
    {"ف9", KEY_F9}, {"ف10", KEY_F10}, {"ف11", KEY_F11}, {"ف12", KEY_F12},

    {NULL, 0}
};

int keyboard_condition(const char *p) {
    if (!p) return -1;

    p = skip_spaces(p);
    if (!starts_with(p, "لوحة_المفاتيح(")) return -1;
    p = skip_prefix(p, "لوحة_المفاتيح(");
    p = skip_spaces(p);

    char key_name[32];
    int i = 0;
    while (*p && *p != ')' && i < 31) {
        key_name[i++] = *p++;
    }
    key_name[i] = '\0';
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;

    for (int j = 0; key_map[j].name != NULL; j++) {
        if (strcmp(key_name, key_map[j].name) == 0) {
            return keyboard_is_pressed(key_map[j].key_code);
        }
    }
    return -1;
}
