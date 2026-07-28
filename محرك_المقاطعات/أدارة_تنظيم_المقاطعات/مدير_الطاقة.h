#ifndef POWER_H
#define POWER_H

#include <stdint.h>

void power_init(void *rsdp_physical_address);

void system_shutdown(void);
void system_reboot(void);

void power_debug_checkpoint(uint8_t code);
void power_ensure_mapped(uint64_t phys_addr, uint64_t length);

#endif
