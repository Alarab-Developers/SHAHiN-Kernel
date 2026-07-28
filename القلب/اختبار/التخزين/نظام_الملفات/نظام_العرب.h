#ifndef DISK_FS_H
#define DISK_FS_H

#include <stdint.h>

extern int sata_read_sector(uint64_t lba, uint8_t* buffer);
extern int sata_write_sector(uint64_t lba, uint8_t* buffer);


void diskfs_read(const char* name, char* buffer);
int arabfs_init();


#endif
