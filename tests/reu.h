/*
 * For a 1700, 1750 or 1764 REU, or anything else that answers at
 * $DF00 the same way.
 */

#ifndef REU_H
#define REU_H

#include <stdint.h>

#define REU ((volatile uint8_t *)0xdf00)
enum { REU_STATUS, REU_COMMAND, REU_C64LO, REU_C64HI, REU_REULO, REU_REUHI,
       REU_REUBANK, REU_LENLO, REU_LENHI, REU_IRQMASK, REU_ADDRCTL };

// Execute now, $FF00 not involved.
enum { REU_STASH = 0x90, REU_FETCH = 0x91, REU_SWAP = 0x92,
       REU_VERIFY = 0x93 };

// Status bits.
#define REU_EOB 0x40
#define REU_FLT 0x20

/* A REU's registers read back what was written; open bus does not. */
static inline int reu_present(void)
{
    REU[REU_C64LO] = 0xa5;
    REU[REU_C64HI] = 0x5a;
    return REU[REU_C64LO] == 0xa5 && REU[REU_C64HI] == 0x5a;
}

/* Start a transfer. Note: Reading clears STATUS. */
static inline uint8_t reu_run(uint8_t command)
{
    REU[REU_COMMAND] = command;
    return REU[REU_STATUS];
}

/* A transfer with both addresses counting, REU bank 0, no interrupts. */
static inline uint8_t reu_xfer(uint8_t command, uint16_t c64, uint16_t reu,
                               uint16_t len)
{
    REU[REU_C64LO] = c64;
    REU[REU_C64HI] = c64 >> 8;
    REU[REU_REULO] = reu;
    REU[REU_REUHI] = reu >> 8;
    REU[REU_REUBANK] = 0;
    REU[REU_LENLO] = len;
    REU[REU_LENHI] = len >> 8;
    REU[REU_IRQMASK] = 0;
    REU[REU_ADDRCTL] = 0;
    return reu_run(command);
}

#endif
