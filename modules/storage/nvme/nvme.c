#include "nvme.h"
#include "mm/heap.h"
#include "dev/bus/pci.h"
#include <stdint.h>

// Helpers
static inline uint32_t nvme_read_reg(uintptr_t nvme_base_addr, uint32_t offset) {
	volatile uint32_t *nvme_reg = (volatile uint32_t *)(nvme_base_addr + offset);
	return *nvme_reg;
}

static inline void nvme_write_reg(uintptr_t nvme_base_addr, uint32_t offset, uint32_t value) {
	volatile uint32_t *nvme_reg = (volatile uint32_t *)(nvme_base_addr + offset);
	*nvme_reg = value;
}

void nvme_reset(nvme_t *nvme)
{
    if(nvme->registers->CC.en)
        while(nvme->registers->CSTS.rdy)
            ;
    nvme->registers->CC.en = 0;
}

void nvme_start(nvme_t *nvme)
{
    nvme->registers->CC.ams = 0;
    nvme->registers->CC.mps = 0; // 4kb page shift
    nvme->registers->CC.css = 0;
    nvme->registers->CC.en  = 1;
}

bool nvme_probe(device_t *device)
{
    pci_header_type0_t *header = (pci_header_type0_t *) device->bus_data;

    if (header->common.class != 0x01 || header->common.subclass != 0x08)
        return false;

    nvme_t *nvme = heap_alloc(sizeof(nvme_t));
    nvme->registers = (nvme_regs_t *)(uintptr_t)header->bar[0];

    nvme_reset(nvme);

    return true;
}

void nvme_init(pci_header_type0_t header)
{

}

void nvme_identify()
{

}
