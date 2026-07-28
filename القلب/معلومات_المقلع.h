#ifndef BOOT_INFO_H
#define BOOT_INFO_H

typedef struct
{
    void* FrameBuffer;
    unsigned Width;
    unsigned Height;
    unsigned PixelsPerScanLine;
} boot_info_t;

#endif
