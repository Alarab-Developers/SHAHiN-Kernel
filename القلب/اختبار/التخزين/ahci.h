#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>

typedef volatile struct
{
    uint32_t clb;
    uint32_t clbu;

    uint32_t fb;
    uint32_t fbu;

    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t reserved0;

    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;

    uint32_t reserved1[11];
    uint32_t vendor[4];

} hba_port_t;


typedef volatile struct
{
    uint32_t dba;
    uint32_t dbau;

    uint32_t reserved;

    uint32_t dbc : 22;
    uint32_t reserved1 : 9;
    uint32_t i : 1;

} hba_prdt_entry_t;


typedef volatile struct
{
    uint8_t cfis[64];

    uint8_t acmd[16];

    uint8_t reserved[48];

    hba_prdt_entry_t prdt[1];

} hba_cmd_table_t;




typedef volatile struct
{
    uint8_t cfl:5;
    uint8_t a:1;
    uint8_t w:1;
    uint8_t p:1;

    uint8_t r:1;
    uint8_t b:1;
    uint8_t c:1;
    uint8_t reserved0:1;
    uint8_t pmp:4;

    uint16_t prdtl;

    volatile uint32_t prdbc;

    uint32_t ctba;
    uint32_t ctbau;

    uint32_t reserved1[4];

} hba_cmd_header_t;

typedef volatile struct
{
    uint8_t fis_type;

    uint8_t pmport:4;
    uint8_t reserved0:3;
    uint8_t c:1;

    uint8_t command;
    uint8_t featurel;

    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t featureh;

    uint8_t countl;
    uint8_t counth;

    uint8_t icc;
    uint8_t control;

    uint8_t reserved1[4];

} fis_reg_h2d_t;


typedef volatile struct
{
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;

    uint8_t reserved[0xA0 - 0x2C];
    uint8_t vendor[0x100 - 0xA0];

    hba_port_t ports[32];

} hba_memory_t;

#endif
