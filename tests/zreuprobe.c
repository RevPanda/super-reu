/*
 * Questions about the 8726 that QuickReuTest leaves open. It prints
 * what it finds next to what VICE does.
 *
 * C64 only.
 */

#include <c64.h>
#include <stdbool.h>
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

// A one-byte stash with the mask given, does not write the command.
static void set_up_stash(uint8_t mask)
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
}

// The same, run it, interrupts off.
static void stash_with_mask(uint8_t mask)
{
    set_up_stash(mask);
    REU[REU_COMMAND] = 0x90;
}

// Interrupts on for long enough for one to be taken, then off again.
static void let_in(void)
{
    irq_count = irq_other = irq_status = 0;
    cli();
    for (volatile uint8_t i = 0; i < 100; i++)
        ;
    sei();
}

/*
 * Vuild code at $C000 that writes the command for the stash set up,
 * and n cycles after that write (4 to 35), writes mask to $DF09, or
 * reads $DF00 if read is set. It returns A, which is what it read.
 */
#define CODE ((volatile uint8_t *)0xc000)
#define PTR (*(volatile uint16_t *)0xfb)

// The 6502 opcodes gen() uses, with their cycle counts.
enum {
    BIT_ZP = 0x24,   // 3
    NOP = 0xea,      // 2
    RTS = 0x60,
    LDA_IMM = 0xa9,  // 2
    LDX_IMM = 0xa2,  // 2
    LDY_IMM = 0xa0,  // 2
    LDA_ABS = 0xad,  // 4, reads on the 4th
    LDA_INDY = 0xb1, // 5 within a page, reads on the 5th
    STA_ABS = 0x8d,  // 4, writes on the 4th
    STA_ABSY = 0x99, // 5, writes on the 5th
    STX_ABS = 0x8e,  // 4, writes on the 4th
};

static void gen(uint8_t n, uint8_t mask, bool read)
{
    volatile uint8_t *p = CODE;
    uint8_t c = n >= 6 ? n - 4 : 0;

    *p++ = LDX_IMM; *p++ = 0x90;
    *p++ = LDA_IMM; *p++ = mask;
    *p++ = LDY_IMM; *p++ = 0x00;
    *p++ = STX_ABS; *p++ = 0x01; *p++ = 0xdf;
    if (c & 1) {
        *p++ = BIT_ZP; *p++ = 0xfb;
        c -= 3;
    }
    for (; c; c -= 2)
        *p++ = NOP;
    if (read && n == 5) {
        *p++ = LDA_INDY; *p++ = 0xfb;       // $DF00 through PTR
    } else if (read) {
        *p++ = LDA_ABS; *p++ = 0x00; *p++ = 0xdf;
    } else {
        *p++ = n == 5 ? STA_ABSY : STA_ABS; *p++ = 0x09; *p++ = 0xdf;
    }
    *p = RTS;
}

static uint8_t run(void)
{
    return ((uint8_t (*)(void))CODE)();
}

static uint8_t other;

// One character for one mask sweep run: see header.
static char mask_at(uint8_t n, uint8_t mask)
{
    uint8_t st;

    gen(n, mask, false);
    set_up_stash(0xc0);
    run();
    let_in();
    st = (irq_count ? irq_status : REU[REU_STATUS]) & 0xe0;
    other += irq_other;
    REU[REU_IRQMASK] = 0;
    return irq_count == 1 && st == 0xc0   ? '1'
           : irq_count == 0 && st == 0x40 ? '0'
                                          : '?';
}

static char status_at(uint8_t n)
{
    uint8_t st;

    gen(n, 0, true);
    set_up_stash(0);
    st = run();
    (void)REU[REU_STATUS];
    return "0123456789abcdef"[(st & 0xe0) >> 4];
}

#define FIRST 4
#define SWEEP 32

static char sweep[3][SWEEP + 1];

// Three runs for each n, a star where they disagree.
static void do_sweep(void)
{
    for (uint8_t i = 0; i < SWEEP; i++) {
        uint8_t n = FIRST + i;
        for (uint8_t k = 0; k < 3; k++) {
            char m0 = mask_at(n, 0), m4 = mask_at(n, 0x40);
            char s = status_at(n);
            if (k == 0) {
                sweep[0][i] = m0;
                sweep[1][i] = m4;
                sweep[2][i] = s;
            } else {
                if (sweep[0][i] != m0)
                    sweep[0][i] = '*';
                if (sweep[1][i] != m4)
                    sweep[1][i] = '*';
                if (sweep[2][i] != s)
                    sweep[2][i] = '*';
            }
        }
    }
}

int main(void)
{
    uint8_t written, left, bank_lo, bank_hi;
    uint8_t count_on, st_on;

    printf("REU probe\n\n");

    if (!reu_present()) {
        printf("no reu at $df00\n");
        return 0;
    }
    (void)REU[REU_STATUS];

    // The bank register reads back with its unused bits set, so what it
    // says for bank 0 is whats "written".
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

    // Blanked from the top of a frame, so that there are no badlines.
    VIC.ctrl1 &= ~0x10;
    while (!(VIC.ctrl1 & 0x80))
        ;
    while (VIC.ctrl1 & 0x80)
        ;
    PTR = 0xdf00;
    do_sweep();
    VIC.ctrl1 |= 0x10;

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
    printf("\n$df09 written n cycles after $df01\n");
    printf("n      00000011111111112222222222333333\n");
    printf("       45678901234567890123456789012345\n");
    printf("$00    %s\n", sweep[0]);
    printf("$40    %s\n", sweep[1]);
    printf("status %s\n", sweep[2]);
    if (other)
        printf("\n%u interrupts not from the reu\n", other);
    return 0;
}
