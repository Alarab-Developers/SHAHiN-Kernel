#ifndef BOOT_INFO_H
#define BOOT_INFO_H

typedef struct
{
    void* FrameBuffer;
    unsigned Width;
    unsigned Height;
    unsigned PixelsPerScanLine;
    void* system_archive;
    uint32_t system_archive_size;
} boot_info_t;

#endif
