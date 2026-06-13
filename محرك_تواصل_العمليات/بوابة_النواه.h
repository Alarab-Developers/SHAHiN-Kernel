#ifndef KERNEL_GATEWAY_H
#define KERNEL_GATEWAY_H

#include <stdint.h>
#include "محرك_تواصل_العمليات/محرك_تواصل_العمليات.h"

typedef struct {

    /* IPC */
    int (*ipc_send)(int pid, Message* msg);
    int (*ipc_receive)(Message* msg);

    /* PORT IO */
    void    (*outb)(uint16_t port, uint8_t value);
    uint8_t (*inb)(uint16_t port);

    /* TIMER */
    uint64_t (*timer_ticks)();
    int (*mouse_get_x)();
    int (*mouse_get_y)();

    /* VIDEO */
    uint32_t* (*video_map)();
    uint32_t  (*screen_width)();
    uint32_t  (*screen_height)();
    uint32_t  (*screen_pitch)();
    void      (*video_present)();
    void      (*swap)();

    /* تشغيل تطبيق مباشرة */
    int (*launch_app)(const char* name);

} kernel_api_t;

extern kernel_api_t kapi;

#endif
