#ifndef DEVELOPMENT_SCREEN_H
#define DEVELOPMENT_SCREEN_H

void development_screen_init(void);

void development_screen_print(
    const char *text
);

void development_screen_println(
    const char *text
);

void development_screen_update(void);

#endif
