#include "sata.h"
#include "ahci.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "القلب/اختبار/طرفية/طرفية.h"
#include "القلب/مكتبة_المحركات/مكتبة_معالجة_النصوص.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define FIS_TYPE_REG_H2D 0x27
#define ATA_CMD_READ_DMA_EXT  0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35

#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ  0x08

#define AHCI_GHC_AE (1u << 31)


static sata_device_t device;
static hba_port_t *sata_port = 0;

/* منافذ MMIO الخاصة بـ AHCI */
static volatile hba_memory_t* hba;


/*
    قراءة PCI
*/
static uint32_t pci_read(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t offset
)
{
    uint32_t address =
        (1 << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)func << 8) |
        (offset & 0xFC);


    asm volatile(
        "outl %0,%1"
        :
        :"a"(address),"Nd"(PCI_CONFIG_ADDRESS)
    );


    uint32_t result;

    asm volatile(
        "inl %1,%0"
        :"=a"(result)
        :"Nd"(PCI_CONFIG_DATA)
    );


    return result;
}


static void pci_write(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t offset,
    uint32_t value
)
{
    uint32_t address =
        (1 << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)func << 8) |
        (offset & 0xFC);

    asm volatile(
        "outl %0,%1"
        :
        :"a"(address),"Nd"(PCI_CONFIG_ADDRESS)
    );

    asm volatile(
        "outl %0,%1"
        :
        :"a"(value),"Nd"(PCI_CONFIG_DATA)
    );
}



/*
    البحث عن متحكم AHCI
*/
static int find_ahci()
{
    for(uint16_t bus=0; bus<256; bus++)
    {
        for(uint8_t slot=0; slot<32; slot++)
        {
            uint32_t id =
                pci_read(bus,slot,0,0);


            if(id == 0xFFFFFFFF)
                continue;


            uint32_t class =
                pci_read(bus,slot,0,8);


            uint8_t base =
                (class >> 24) & 0xff;


            uint8_t sub =
                (class >> 16) & 0xff;


            /*
              SATA AHCI:
              class 01
              subclass 06
              prog IF 01
            */

            if(base == 0x01 &&
               sub  == 0x06)
            {
                /* تفعيل Memory Space (bit1) وBus Master (bit2)
                   في PCI Command Register قبل استخدام ABAR،
                   بعض الأجهزة الحقيقية (وبعض إعدادات QEMU) لا
                   تفعّلهما تلقائياً */
                uint32_t pci_cmd =
                    pci_read(bus,slot,0,0x04);

                pci_cmd |= (1 << 1) | (1 << 2);

                pci_write(bus,slot,0,0x04,pci_cmd);


                uint64_t bar =
                    (uint64_t)pci_read(bus,slot,0,0x24);


                    hba =
                        (volatile hba_memory_t*)(bar & 0xfffffff0ULL);


                return 1;
            }
        }
    }


    return 0;
}


static int ahci_port_init(hba_port_t *port)
{
    //terminal_print("Stopping Command Engine");

    /* إيقاف المحرك: يجب إيقاف ST أولاً والانتظار حتى ينطفئ CR،
       ثم إيقاف FRE والانتظار حتى ينطفئ FR. إيقافهما معاً وانتظار
       الاثنين دفعة واحدة قد يسبب تعليق (hang) على أجهزة حقيقية
       إذا كان المحرك يعمل فعلاً وقت النداء */
    port->cmd &= ~(1 << 0);   /* ST  */

    while (port->cmd & (1 << 15)); /* انتظار CR */

    port->cmd &= ~(1 << 4);   /* FRE */

    while (port->cmd & (1 << 14)); /* انتظار FR */

    //terminal_print("Engine Stopped");


    /* Command List */

    uint64_t clb = memory_api.alloc_page();

    if (!clb)
    {
        //terminal_print("CLB Allocation Failed");
        return -1;
    }

    port->clb  = (uint32_t)clb;
    port->clbu = (uint32_t)(clb >> 32);

    //terminal_print("CLB Allocated");


    /* FIS Receive */

    uint64_t fb = memory_api.alloc_page();

    if (!fb)
    {
        terminal_print("FB Allocation Failed");
        return -1;
    }

    port->fb  = (uint32_t)fb;
    port->fbu = (uint32_t)(fb >> 32);

    //terminal_print("FB Allocated");


    memset((void*)clb,0,4096);
    memset((void*)fb ,0,4096);

    //terminal_print("Memory Cleared");


    /* إعداد Command Headers */

    hba_cmd_header_t *cmdheader =
        (hba_cmd_header_t *)(uintptr_t)port->clb;

    for (int i = 0; i < 32; i++)
    {
        cmdheader[i].prdtl = 1;

        uint64_t ctba = memory_api.alloc_page();

        cmdheader[i].ctba  = (uint32_t)ctba;
        cmdheader[i].ctbau = (uint32_t)(ctba >> 32);

        memset((void*)ctba, 0, 4096);
    }


    /* تشغيل المحرك */

    port->cmd |= (1 << 4); /* FRE */
    port->cmd |= (1 << 0); /* ST  */

    //terminal_print("Engine Started");

    return 0;
}



int sata_init(void)
{
    if (!find_ahci())
    {
        terminal_print("AHCI Controller Not Found");

        device.initialized = 0;
        return -1;
    }

    terminal_print("AHCI Controller Found");

    /* لازم تفعيل GHC.AE قبل التعامل مع أي منفذ، وإلا فإن بعض
       الأجهزة (وبعض المحاكيات) لا تعرض سجلات المنافذ بشكل سليم */
    hba->ghc |= AHCI_GHC_AE;

    char text[9];

    //terminal_print("VERSION");
    hex_to_string(hba->vs, text);
    terminal_print(text);

    //terminal_print("CAP");
    hex_to_string(hba->cap, text);
    terminal_print(text);

    //terminal_print("PI");
    hex_to_string(hba->pi, text);
    terminal_print(text);

    for (int i = 0; i < 32; i++)
    {
        if (!(hba->pi & (1 << i)))
            continue;

        terminal_print("----------------");

        terminal_print("PORT");
        hex_to_string(i, text);
        terminal_print(text);

        uint32_t ssts = hba->ports[i].ssts;
        //terminal_print("SSTS");
        hex_to_string(ssts, text);
        terminal_print(text);

        uint8_t det = ssts & 0x0F;
        uint8_t ipm = (ssts >> 8) & 0x0F;
        uint8_t spd = (ssts >> 4) & 0x0F;

        //terminal_print("DET");
        hex_to_string(det, text);
        terminal_print(text);

        //terminal_print("IPM");
        hex_to_string(ipm, text);
        terminal_print(text);

        //terminal_print("SPD");
        hex_to_string(spd, text);
        terminal_print(text);

        uint32_t sig = hba->ports[i].sig;

        //terminal_print("SIG");
        hex_to_string(sig, text);
        terminal_print(text);

        if (det != 3)
        {
            terminal_print("No Device");
            continue;
        }

        if (ipm != 1)
        {
            terminal_print("Link Not Active");
            continue;
        }

        switch (sig)
        {
            case 0x00000101:
                terminal_print("SATA Drive");

                if (ahci_port_init(&hba->ports[i]) == 0)
                {
                    sata_port = &hba->ports[i];
                    terminal_print("Port Init OK");
                }
                else
                {
                    terminal_print("Port Init Failed");
                }
        
                break;

            case 0xEB140101:
                //terminal_print("ATAPI Drive");
                break;

            case 0xC33C0101:
                terminal_print("Enclosure");
                break;

            case 0x96690101:
                terminal_print("Port Multiplier");
                break;

            default:
                terminal_print("Unknown Device");
                break;
        }
    }

    device.sector_size = 512;
    device.sector_count = 0;
    device.initialized = 1;

    terminal_print("AHCI Init OK");

    return 0;
}




/*
    البحث عن Command Slot فارغ (غير مستخدم في sact أو ci)
*/
static int find_cmdslot(hba_port_t *port)
{
    uint32_t slots = port->sact | port->ci;

    for (int i = 0; i < 32; i++)
    {
        if ((slots & (1u << i)) == 0)
            return i;
    }

    return -1;
}


/*
    تجهيز وإصدار أمر READ/WRITE DMA EXT لقطاع واحد (512 بايت)،
    والانتظار حتى ينتهي المتحكم من تنفيذه.

    هذه الدالة هي ما كان ناقصاً في النسخة السابقة: كان يتم تجهيز
    FIS وجدول الأوامر فقط دون قرع الجرس (port->ci) ودون انتظار
    اكتمال الأمر، لذلك كانت sata_read_sector ترجع -1 دائماً.
*/
static int ahci_send_rw_command(
    uint64_t lba,
    uint8_t *buffer,
    uint8_t ata_command,
    int is_write
)
{
    if (!device.initialized)
        return -1;

    if (!sata_port)
    {
        terminal_print("No SATA Port");
        return -1;
    }

    int slot = find_cmdslot(sata_port);

    if (slot == -1)
    {
        terminal_print("No Free Slot");
        return -1;
    }

    hba_cmd_header_t *cmdheader =
        (hba_cmd_header_t *)(uintptr_t)sata_port->clb;

    hba_cmd_header_t *cmd =
        &cmdheader[slot];

    hba_cmd_table_t *table =
        (hba_cmd_table_t *)(uintptr_t)
        (((uint64_t)cmd->ctbau << 32) | cmd->ctba);

    memset((void *)table, 0, 4096);

    table->prdt[0].dba =
        (uint32_t)(uintptr_t)buffer;

    table->prdt[0].dbau =
        (uint32_t)(((uint64_t)(uintptr_t)buffer) >> 32);

    table->prdt[0].dbc = 511; /* عدد البايتات ناقص 1 => 512 بايت */
    table->prdt[0].i = 1;

    cmd->cfl   = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);
    cmd->w     = is_write ? 1 : 0;
    cmd->prdtl = 1;
    cmd->prdbc = 0;

    fis_reg_h2d_t *fis =
        (fis_reg_h2d_t *)table->cfis;

    memset((void *)fis, 0, sizeof(fis_reg_h2d_t));

    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c        = 1;
    fis->command  = ata_command;
    fis->device   = 1 << 6;

    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);

    fis->countl = 1;
    fis->counth = 0;

    /* الانتظار حتى يصبح المنفذ جاهزاً (لا BSY ولا DRQ) قبل قرع الجرس */
    int spin = 0;

    while ((sata_port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) &&
           spin < 1000000)
    {
        spin++;
    }

    if (spin == 1000000)
    {
        terminal_print("Port Hung");
        return -1;
    }

    /* مسح حالة المقاطعات القديمة (كل البتات RWC) */
    sata_port->is = (uint32_t)-1;

    /* قرع الجرس: هذا هو ما يجعل المتحكم ينفذ الأمر فعلياً */
    sata_port->ci = (1u << slot);

    /* الانتظار حتى ينتهي تنفيذ الأمر أو يحدث خطأ */
    while (1)
    {
        if (!(sata_port->ci & (1u << slot)))
            break;

        if (sata_port->is & (1 << 30)) /* TFES: Task File Error Status */
        {
            terminal_print("Command Error (TFES)");
            return -1;
        }
    }

    if (sata_port->tfd & 0x01) /* ERR bit في Task File Data */
    {
        terminal_print("Command Error (TFD)");
        return -1;
    }

    return 0;
}


int sata_read_sector(
    uint64_t lba,
    uint8_t *buffer
)
{
    return ahci_send_rw_command(lba, buffer, ATA_CMD_READ_DMA_EXT, 0);
}


int sata_write_sector(
    uint64_t lba,
    uint8_t* buffer
)
{
    return ahci_send_rw_command(lba, buffer, ATA_CMD_WRITE_DMA_EXT, 1);
}




sata_device_t* sata_get_device()
{
    return &device;
}
