/*
 * Interface to Raspberry Pi mailbox system, used by libpi to interact
 * with GPU. Note that this interface has been deprecated and so future
 * code should use the property interface instead.
 *
 * Author: Pat Hanrahan <hanrahan@cs.stanford.edu>
 * Author: Philip Levis <pal@cs.stanford.edu>
 * Date: 6/20/17
 */

#include "mailbox.h"

#define MAILBOX_BASE     0x2000B880

// This bit is set in status register if no space to write to mailbox
#define MAILBOX_FULL   (1<<31)

// This bit is set in status register if nothing to read in mailbox
#define MAILBOX_EMPTY  (1<<30)

// This prevents the GPU and CPU from caching mailbox messages
#define GPU_NOCACHE 0x40000000

typedef struct {
    unsigned int read;
    unsigned int padding[3]; // note padding to skip 3 words
    unsigned int peek;
    unsigned int sender;
    unsigned int status;
    unsigned int configuration;
    unsigned int write;
} mailbox_t;


// If the mailbox memory is not qualified as volatile, the compiler may
// take liberties in optimizing the code that can be problematic.
// Below we use the preprocessor to compile two versions of mailbox:
// one where mailbox is properly qualified as volatile and a second
// (buggy) one that does not. Use `make` to generate both versions
// and compare the assembly instructions in the two .list files. Can
// you spot the problems introduced by the compiler's optimizations
// of the non-volatile version?
#ifdef NVOLATILE
static mailbox_t *mailbox = (mailbox_t *)MAILBOX_BASE;
#else
static volatile mailbox_t *mailbox = (mailbox_t *)MAILBOX_BASE;
#endif

bool mailbox_request(unsigned int channel, unsigned int addr) {
    if (!mailbox_write(channel, addr))
        return false;
    return (mailbox_read(channel) == 0); // returned data is 0 on success
}

bool mailbox_write(unsigned int channel, unsigned int addr) {
    // mailbox has a maximum of 16 channels
    if (channel >= MAILBOX_MAXCHANNEL) {
        return false;
    }

    // addr must be a multiple of 16
    if (addr & 0xF) {
        return false;
    }

    // wait until mailbox is not full ...
    while (mailbox->status & MAILBOX_FULL) ;

    // set GPU_NOCACHE bit so that the GPU does not cache the memory
    addr |= GPU_NOCACHE;

    // addr is a multiple of 16, so the low 4 bits are zeros
    // 4-bit channel number is stuffed into those low bits
    mailbox->write = addr | channel;
    return true;
}

unsigned int mailbox_read(unsigned int channel) {
    if (channel >= MAILBOX_MAXCHANNEL) {
        return 1;
    }

    while (1) {
        // wait until mailbox is not empty ...
        while (mailbox->status & MAILBOX_EMPTY) ;
        // read the data, low 4 bits are channel, upper 28 are result
        unsigned int data = mailbox->read;
        if ((data & 0xF) == channel) { // if this message is for our channel
            return (data >> 4);
        }
    }
    return 1;
}
