#include "ps2.h"

#define LOG_PREFIX "PS/2"
#include "arch/irq.h"
#include "arch/x86_64/ioport.h"
#include "log.h"
#include "uapi/kb.h"

#define PS2_DATA_PORT 0x60
#define KB_IRQ 1

static irq_t *kb_irq;

bool ps2kb_irq_handler([[maybe_unused]] irq_t *irq, [[maybe_unused]] void *data)
{
    uint8_t scancode = x86_64_ioport_inb(PS2_DATA_PORT);

    kb_event_t e;
    if (scancode < 0x80)
    {
        e.value = KEY_PRESS;
    }
    else
    {
        e.value = KEY_RELEASE;
        scancode -= 0x80;
    }
    e.key = scancode;
    e.timestamp = 0;

    log(LOG_DEBUG, "KEY: %u VALUE: %u", e.key, e.value);

    return true;
}

void ps2kb_setup()
{
    // Wait until input buffer is clear.
    while (x86_64_ioport_inb(0x64) & 0x02)
        ;

    // Send command to read command byte.
    x86_64_ioport_outb(0x64, 0x20);
    while (!(x86_64_ioport_inb(0x64) & 0x01))
        ;
    uint8_t cmd = x86_64_ioport_inb(0x60);

    // Modify: enable IRQ1 (bit 0), enable keyboard clock (bit 1).
    cmd |= 0x01; // IRQ1 enable.
    cmd |= 0x10; // Clock enable.

    // Write modified command byte.
    while (x86_64_ioport_inb(0x64) & 0x02)
        ;
    x86_64_ioport_outb(0x64, 0x60);
    while (x86_64_ioport_inb(0x64) & 0x02)
        ;
    x86_64_ioport_outb(0x60, cmd);

    // Enable first PS/2 port.
    while (x86_64_ioport_inb(0x64) & 0x02)
        ;
    x86_64_ioport_outb(0x64, 0xAE);

    kb_irq = irq_claim_legacy(1, IRQ_TRIGGER_EDGE_FALLING, ps2kb_irq_handler, 0);
    if (!kb_irq)
        log(LOG_ERROR, "Could not claim legacy IRQ 1 for PS/2 Keyboard!");
    irq_set_affinity(kb_irq, 0);

    log(LOG_WARN, "AA");
}
