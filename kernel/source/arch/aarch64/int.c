#include "arch/aarch64/devices/gic.h"
#include "arch/lcpu.h"
#include "log.h"
#include "panic.h"
#include "sync/spinlock.h"

// Interrupt handling

typedef struct
{
    uint64_t x[31];
}
__attribute__((packed))
cpu_state_t;

static void (*arch_timer_handler)();

void arch_timer_set_handler_per_cpu(void (*handler)())
{
    arch_timer_handler = handler;
}

void aarch64_int_handler(
    const uint64_t source,
    cpu_state_t const *cpu_state,
    const uint64_t esr, const uint64_t elr,
    const uint64_t spsr, const uint64_t far
)
{
    (void)cpu_state;

    switch (source)
    {
        // Synchronous
        case 0:
        case 4:
        {
            log(
                LOG_FATAL,
                "SYNC exception ESR=%lx ELR=%lx FAR=%lx SPSR=%lx",
                esr, elr, far, spsr
            );
            panic("sync");
        }

        // IRQ
        case 1: // current EL
        case 5: // lower EL
        {
            uint32_t iar = aarch64_gic->ack_int();
            uint32_t intid = iar & 0x3ff;

            if (intid < 16)// SGIs
            {
                panic("Unhandled SGI %d", intid);
            }
            else if (intid < 32) // PPIs
            {
                switch (intid)
                {
                    case 27: // Timer
                        arch_timer_handler();
                        break;
                    default:
                        panic("Unhandled PPI %d", intid);
                        break;
                }
            }
            else if (intid < 1020) // SPIs
            {
                panic("Unhandled SPI %d", intid);
            }

            aarch64_gic->end_of_int(iar);
            return;
        }

        // FIQ
        case 2:
        case 6:
            panic("FIQ");

        // SError
        case 3:
        case 7:
            panic("SError");

        default:
            panic("EVT ");
    }
}

// Initialization

extern void __arch64_evt_load();

void aarch64_int_init_cpu()
{
    arch_lcpu_int_mask();
    __arch64_evt_load();
    arch_lcpu_int_unmask();

    log(LOG_DEBUG, "Initialized interrupts.");
}
