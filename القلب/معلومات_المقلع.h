#ifndef BOOT_INFO_H
#define BOOT_INFO_H

typedef struct
{
    void* FrameBuffer;
    unsigned Width;
    unsigned Height;
    unsigned PixelsPerScanLine;
    void* system_archive;
    uint64_t system_archive_size;
    uint64_t total_ram_bytes;
} boot_info_t;

#endif
