#include "التهيئة.h"
#include "العقدة.h"
#include "نظام_العرب.h"
#include "التعريف_العام.h"
#include "../ساتا.h"


#define SECTOR_SIZE 512
#define FILE_TABLE_SECTOR 2
#define FILE_TABLE_SECTORS 8
#define DATA_START_SECTOR 10

#define SIGNATURE_SECTOR 1
#define SIGNATURE_TEXT "ARABFS"

extern file_entry_t table[MAX_FILES];

static unsigned int next_free_sector = DATA_START_SECTOR;

static void arabfs_memset(void* ptr, int value, int size)
{
    unsigned char* p = ptr;

    for (int i = 0; i < size; i++)
        p[i] = value;
}


int diskfs_is_formatted()
{
    unsigned char sig[SECTOR_SIZE];

    sata_read_sector(SIGNATURE_SECTOR, sig);

    if (!strcmp((char*)sig, SIGNATURE_TEXT))
        return 1;

    return 0;
}

void diskfs_format()
{

    arabfs_memset(table, 0, sizeof(file_entry_t) * MAX_FILES);



    table[0].used = 1;
    table[0].type = 1;
    table[0].id = 0;
    table[0].parent_id = 0;

    strcpy(table[0].name, "/");


    for (int s = 0; s < FILE_TABLE_SECTORS; s++)
    {

        sata_write_sector(
            FILE_TABLE_SECTOR + s,
            ((unsigned char*)table) + (s * SECTOR_SIZE)
        );

    }

    unsigned char sig[SECTOR_SIZE];

    arabfs_memset(sig, 0, SECTOR_SIZE);

    strcpy((char*)sig, SIGNATURE_TEXT);


    sata_write_sector(SIGNATURE_SECTOR, sig);

    next_free_sector = DATA_START_SECTOR;
}


int arabfs_init()
{
    if (diskfs_is_formatted())
    {
        return 0;
    }


    diskfs_format();

    return 0;
}
