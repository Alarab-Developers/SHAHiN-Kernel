#ifndef SATA_H
#define SATA_H

#include "القلب/المكتبات/المكتبات.h"

typedef struct
{
    uint64_t sector_count;
    uint32_t sector_size;
    uint8_t initialized;
} sata_device_t;


/* تهيئة متحكم SATA */
int sata_init(void);


/* قراءة قطاع واحد */
int sata_read_sector(
    uint64_t lba,
    uint8_t* buffer
);


/* كتابة قطاع واحد */
int sata_write_sector(
    uint64_t lba,
    uint8_t* buffer
);


/* معلومات القرص */
sata_device_t* sata_get_device(void);


#endif
