/*
 * Try to figure out if what ran before changes a 1-byte REU verify's
 * end of block.
 *
 * A 1700 sets end of block on a verify difference on the last byte,
 * or on the next-to-last with the last equal, but not when both
 * differ. Theory: the comparator runs a pair behind the bus, and end
 * of block is set while the length is 1 and the comparator says
 * equal, which during the last round is first the previous pair's
 * result and then the last pair's own.
 *
 * A 1-byte verify that differs then gets end of block only from what the
 * comparator held before it started. Each line runs something, then a
 * 1-byte verify of $C010 ($44) against an REU byte that differs ($BB). If
 * the comparator keeps its result between transfers, the lines after a
 * differing pair lose end of block. VICE and super-reu start every
 * transfer afresh: end of block and fault on every line but the control.
 *
 * C64 only: the C64 side of the transfers is fixed RAM at $C000-$C102,
 * well clear of the soft stack that grows down from $D000.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "reu.h"

#define VBUF ((volatile uint8_t *)0xc000) // $11 $22 $33
#define VONE ((volatile uint8_t *)0xc010) // $44
#define FBUF 0xc100                       // fetch target

static const uint8_t pat_eq[] =   { 0x11, 0x22, 0x33 };
static const uint8_t pat_mid[] =  { 0xee, 0x22, 0x33 };
static const uint8_t pat_both[] = { 0x11, 0xdd, 0xcc };
static const uint8_t pat_swap[] = { 0xee, 0xdd, 0xcc };
static const uint8_t one[] =      { 0xbb, 0x44 }; // to REU $000200

static void put(const uint8_t *pattern)
{
    reu_xfer(REU_STASH, (uint16_t)pattern, 0x0100, 3);
}

static uint8_t verify3(const uint8_t *pattern)
{
    put(pattern);
    return reu_xfer(REU_VERIFY, 0xc000, 0x0100, 3);
}

// Runs first on each line.
static uint8_t pre_eq(void) { return verify3(pat_eq); }
static uint8_t pre_mid(void) { return verify3(pat_mid); }
static uint8_t pre_both(void) { return verify3(pat_both); }

static uint8_t pre_one(void)
{
    put(pat_mid); // $EE vs $11
    return reu_xfer(REU_VERIFY, 0xc000, 0x0100, 1);
}

static uint8_t pre_stash(void)
{
    return reu_xfer(REU_STASH, 0xc000, 0x0300, 3);
}

static uint8_t pre_fetch(void)
{
    put(pat_both);
    return reu_xfer(REU_FETCH, FBUF, 0x0100, 3);
}

static uint8_t pre_sweq(void)
{
    put(pat_eq);
    return reu_xfer(REU_SWAP, 0xc000, 0x0100, 3);
}

// Swapped with different bytes and back, so $C000 holds $11 $22 $33.
static uint8_t pre_swdiff(void)
{
    put(pat_swap);
    reu_xfer(REU_SWAP, 0xc000, 0x0100, 3);
    return reu_xfer(REU_SWAP, 0xc000, 0x0100, 3);
}

// Which 1-byte verify follows: the last one re-run from where it stopped,
// or $C010 against REU $000200 ($BB) or $000201 ($44).
enum { RESUME, DIFFERING, MATCHING };

static const struct {
    const char *name;
    uint8_t (*pre)(void);
    uint8_t then;
} cases[] = {
    { "all equal",          pre_eq, DIFFERING },
    { "mid-block",         pre_mid, DIFFERING },
    { "diff one byte",     pre_one, DIFFERING },
    { "diff both last",   pre_both, DIFFERING },
    { "resume both",      pre_both, RESUME },   // $33 against $CC at $C002
    { "both, eq byte",    pre_both, MATCHING }, // the control
    { "stash",           pre_stash, DIFFERING },
    { "fetch",           pre_fetch, DIFFERING },
    { "swap equal",       pre_sweq, DIFFERING },
    { "swap differing", pre_swdiff, DIFFERING },
};

// SDK printf has no field widths for strings.
static const char pad[] = "               ";

int main(void)
{
    uint8_t i;

    printf("1-byte vfy fail after other ops: eob\n\n");

    if (!reu_present()) {
        printf("no reu at $df00\n");
        return 0;
    }

    VBUF[0] = 0x11;
    VBUF[1] = 0x22;
    VBUF[2] = 0x33;
    *VONE = 0x44;
    reu_xfer(REU_STASH, (uint16_t)one, 0x0200, 2);

    printf("               pre eob flt st addr len\n");
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        uint8_t pre = cases[i].pre();
        uint8_t st;

        if (cases[i].then == RESUME)
            st = reu_run(REU_VERIFY); // from where the last one stopped
        else
            st = reu_xfer(REU_VERIFY, 0xc010,
                      cases[i].then == MATCHING ? 0x0201 : 0x0200, 1);
        printf("%s%s%02x  %s%s%02x %02x%02x %02x%02x\n", cases[i].name,
               pad + strlen(cases[i].name), pre,
               st & REU_EOB ? "eob " : "    ",
               st & REU_FLT ? "flt " : "    ", st,
               REU[REU_C64HI], REU[REU_C64LO],
               REU[REU_LENHI], REU[REU_LENLO]);
    }
    return 0;
}
