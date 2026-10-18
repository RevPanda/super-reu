/*
 * Questions about the 8726 that QuickReuTest leaves open. It prints
 * what it finds next to what VICE does.
 *
 * C64 only.
 */

#include <c64.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "reu.h"

#define IRQVEC (*(void (*volatile *)(void))0x0314)

// In zreuprobe_asm.s.
void reu_irq(void);
volatile uint8_t irq_count, irq_other, irq_status;

static uint8_t buf[256];

static void sei(void) { __asm__ volatile("sei" : : : "memory"); }
static void cli(void) { __asm__ volatile("cli" : : : "memory"); }

// SDK printf has no field widths for strings.
static const char pad[] = "                      ";

static void name(const char *s)
{
    printf("%s%s", s, pad + strlen(s));
}

static void status_line(const char *s, uint8_t got, uint8_t vice)
{
    name(s);
    printf("%02x        %02x\n", got, vice);
}

static void count_line(const char *s, uint8_t got, uint8_t vice)
{
    name(s);
    printf("%u         %u\n", got, vice);
}

// The bank as read, and whether that is where the transfer left it or
// what was written. The unused bits read as ones, and how many there are
// differs between REUs.
static void bank_line(const char *s, uint8_t got, uint8_t left,
                      uint8_t written)
{
    name(s);
    printf("%02x %s kept\n", got,
           got == left      ? "kept   "
           : got == written ? "written"
                            : "neither");
}

// 256 bytes from $00FF80, into bank 1, no autoload.
static void stash_into_bank1(void)
{
    reu_xfer(0x90, (uint16_t)buf, 0xff80, 0x100);
}

// A one-byte stash with the mask given, interrupts off.
static void stash_with_mask(uint8_t mask)
{
    REU[REU_C64LO] = (uint16_t)buf;
    REU[REU_C64HI] = (uint16_t)buf >> 8;
    REU[REU_REULO] = 0;
    REU[REU_REUHI] = 0;
    REU[REU_REUBANK] = 0;
    REU[REU_LENLO] = 1;
    REU[REU_LENHI] = 0;
    REU[REU_ADDRCTL] = 0;
    REU[REU_IRQMASK] = mask;
    REU[REU_COMMAND] = 0x90;
}

// Get us one IRQ
static void let_in(void)
{
    irq_count = irq_other = irq_status = 0;
    cli();
    for (volatile uint8_t i = 0; i < 100; i++)
        ;
    sei();
}

int main(void)
{
    uint8_t written, left, bank_lo, bank_hi;
    uint8_t count_on, st_on, count_off, st_off, count_m, st_m, other;

    printf("REU probe\n\n");

    if (!reu_present()) {
        printf("no reu at $df00\n");
        return 0;
    }
    (void)REU[REU_STATUS];

    // The bank register reads back with its unused bits set, so what it
    // says for bank 0 is what "written" looks like.
    REU[REU_REUBANK] = 0;
    written = REU[REU_REUBANK];
    stash_into_bank1();
    left = REU[REU_REUBANK];
    REU[REU_REULO] = 0x80;
    bank_lo = REU[REU_REUBANK];
    stash_into_bank1();
    REU[REU_REUHI] = 0xff;
    bank_hi = REU[REU_REUBANK];

    // Only the REU may interrupt: CIA 1's interrupts off, and the
    // handler in place.
    sei();
    void (*kernal)(void) = IRQVEC;
    IRQVEC = reu_irq;
    CIA1.icr = 0x7f;
    (void)CIA1.icr;

    // End of block with the master enable, the mask left alone.
    stash_with_mask(0xc0);
    let_in();
    count_on = irq_count;
    st_on = irq_status;
    other = irq_other;

    // The same, with the mask cleared before interrupts are let in. If
    // none is taken, the status is read directly instead.
    stash_with_mask(0xc0);
    REU[REU_IRQMASK] = 0;
    let_in();
    count_off = irq_count;
    st_off = irq_count ? irq_status : REU[REU_STATUS];
    other += irq_other;

    // And with only the master enable cleared.
    stash_with_mask(0xc0);
    REU[REU_IRQMASK] = 0x40;
    let_in();
    count_m = irq_count;
    st_m = irq_count ? irq_status : REU[REU_STATUS];
    other += irq_other;

    REU[REU_IRQMASK] = 0;
    (void)REU[REU_STATUS];
    IRQVEC = kernal;
    CIA1.icr = 0x81;
    cli();

    printf("                      got       vice\n");
    printf("stash over $00ffff\n");
    name(" bank after it");
    printf("%02x\n", left);
    bank_line(" $df04 written: bank", bank_lo, left, written);
    bank_line(" $df05 written: bank", bank_hi, left, written);

    printf("\nend of block, mask $c0\n");
    count_line(" interrupts", count_on, 1);
    status_line(" status", st_on & 0xe0, 0xc0);
    printf("mask cleared before cli\n");
    count_line(" interrupts", count_off, 1);
    status_line(" status", st_off & 0xe0, 0xc0);
    printf("master off before cli\n");
    count_line(" interrupts", count_m, 1);
    status_line(" status", st_m & 0xe0, 0xc0);
    if (other)
        printf("\n%u interrupts not from the reu\n", other);
    return 0;
}
