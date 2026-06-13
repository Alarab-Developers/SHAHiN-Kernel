#ifndef FRAMEBUFFER_INFO_H
#define FRAMEBUFFER_INFO_H

#include <stdint.h>

typedef struct {

    uint64_t address;

    uint32_t width;
    uint32_t height;

    uint32_t pitch;

    uint8_t bpp;

} framebuffer_info_t;

#endif
