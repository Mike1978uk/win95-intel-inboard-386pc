/*
 * 86Box     A hypervisor and IBM PC system emulator.
 *
 *           Micro Solutions BackPack parallel-port CD-ROM.
 *
 *           The BackPack wire protocol is already modelled in lpt_ditto.c,
 *           because the Iomega Ditto rides on the same Micro Solutions
 *           bridge. What differs is everything above it: the Ditto exposes a
 *           tape controller, while a BackPack CD-ROM exposes an ATA task file
 *           with an ATAPI drive behind it. This file carries its own copy of
 *           the wire layer so neither device can break the other; the two are
 *           worth factoring together once both are proven.
 *
 *           Authors: Mike Lycett
 *
 *           Copyright 2026 Mike Lycett.
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/timer.h>
#include <86box/device.h>
#include <86box/lpt.h>
#include <86box/plat_unused.h>
#include <86box/log.h>

#define ENABLE_LPT_BPCK_LOG 1
#ifdef ENABLE_LPT_BPCK_LOG
static void
bpck_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    pclog_ex(fmt, ap);
    va_end(ap);
}
#else
#    define bpck_log(fmt, ...)
#endif

/* Parallel port lines, named from the point of view of the host. */
#define LPT_CTRL_STROBE   0x01
#define LPT_CTRL_AUTOFD   0x02
#define LPT_CTRL_INIT     0x04
#define LPT_CTRL_SELECT   0x08
#define LPT_CTRL_LINES    0x0f

#define LPT_STAT_ERROR    0x08
#define LPT_STAT_SELECT   0x10
#define LPT_STAT_PAPEROUT 0x20
#define LPT_STAT_ACK      0x40
#define LPT_STAT_BUSY     0x80
#define LPT_STAT_MASK     0xf8
#define LPT_STAT_IDLE     0xd8

/*
 * Protocols the bridge can be programmed for. The host writes the bits into
 * register 0x04.
 */
#define BPCK_PROTO_SPP       0
#define BPCK_PROTO_PS2       1
#define BPCK_PROTO_EPP       2

#define BPCK_PROTO_BITS_PS2  0x10
#define BPCK_PROTO_BITS_EPP  0x18
#define BPCK_PROTO_BITS_MASK 0x18

typedef struct bpck_s {
    void   *lpt;

    /* The port as the host last left it. */
    uint8_t dat;
    uint8_t ctrl;
    uint8_t dat_out;
    uint8_t rd_out;
    uint8_t last_status;

    /* Which pod on the chain this is; the knock carries the address. */
    uint8_t unit;

    int     connected;
    int     knock;
    int     ident;
    int     latching;
    int     latch_commits;

    uint8_t cur_reg;
    uint8_t rd_val;
    int     rd_high;

    int     proto;

    /*
     * The register file. Phase one: every access is logged and a read gives
     * back what was written, so the probe sequence of a real driver
     * identifies the map before any of it is given behaviour.
     */
    uint8_t regs[256];
} bpck_t;

static const char *bpck_proto_name[] = { "SPP 4-bit", "PS/2 8-bit", "EPP" };

static const char *
bpck_state(const bpck_t *dev)
{
    static char buf[64];

    snprintf(buf, sizeof(buf), "%s reg=%02X knock=%d%s",
             dev->connected ? "CONN" : "----", dev->cur_reg, dev->knock,
             dev->latching ? " latch" : "");

    return buf;
}

/* --------------------------------------------------------------------- */
/* Layer 2: the register file                                            */
/* --------------------------------------------------------------------- */

static uint8_t
bpck_read_reg(bpck_t *dev, int reg)
{
    const uint8_t ret = dev->regs[reg & 0xff];

    bpck_log("BPCK:    RR %02X -> %02X\n", reg & 0xff, ret);

    return ret;
}

static void
bpck_write_reg(bpck_t *dev, int reg, uint8_t val)
{
    bpck_log("BPCK:    WR %02X <- %02X\n", reg & 0xff, val);

    dev->regs[reg & 0xff] = val;

    /*
     * Register 0x04 carries the protocol the bridge is to use. It is acted on
     * here rather than at the next connect: the host switches mode and then
     * talks, with no disconnect in between.
     */
    if ((reg & 0xff) == 0x04) {
        const uint8_t bits = val & BPCK_PROTO_BITS_MASK;
        const int     was  = dev->proto;

        if (bits == BPCK_PROTO_BITS_EPP)
            dev->proto = BPCK_PROTO_EPP;
        else if (bits == BPCK_PROTO_BITS_PS2)
            dev->proto = BPCK_PROTO_PS2;
        else
            dev->proto = BPCK_PROTO_SPP;

        if (dev->proto != was)
            bpck_log("BPCK: protocol now %s\n", bpck_proto_name[dev->proto]);
    }
}

/* --------------------------------------------------------------------- */
/* Layer 1: the BackPack wire protocol                                   */
/* --------------------------------------------------------------------- */

/*
 * A nibble across the five status bits the bridge owns: value bits 0-2 into
 * status 3-5, bit 3 into status 7. ACK is left clear so a nibble can never be
 * mistaken for the connect response.
 */
static uint8_t
bpck_encode_nibble(uint8_t val)
{
    return (uint8_t) (((val & 0x07) << 3) | ((val & 0x08) << 4));
}

static uint8_t
bpck_connect_response(const bpck_t *dev)
{
    return (uint8_t) ((dev->proto == BPCK_PROTO_EPP)
                          ? (LPT_STAT_ACK | LPT_STAT_BUSY)
                          : LPT_STAT_ACK);
}

/*
 * The address probe that follows every knock: the unit number, and the
 * complement of it when AUTOFD is low. The host requires the two readings to
 * be complements - an empty port cannot do that - and only then checks the
 * number.
 */
static uint8_t
bpck_ident_response(const bpck_t *dev)
{
    const uint8_t id = (dev->ctrl & LPT_CTRL_AUTOFD)
                           ? dev->unit
                           : (uint8_t) ~dev->unit;

    return (uint8_t) (bpck_connect_response(dev) | ((id & 0x07) << 3));
}

static void
bpck_connect(bpck_t *dev)
{
    dev->connected = 1;
    dev->knock     = 0;
    dev->ident     = 1;
    dev->rd_high   = 0;
    dev->rd_out    = bpck_connect_response(dev);

    bpck_log("BPCK: connected in %s mode\n", bpck_proto_name[dev->proto]);
}

static void
bpck_disconnect(bpck_t *dev)
{
    if (!dev->connected)
        return;

    dev->connected = 0;
    dev->knock     = 0;
    dev->ident     = 0;
    dev->latching  = 0;
    dev->rd_out    = 0x00;

    bpck_log("BPCK: disconnected\n");
}

static void
bpck_advance_read(bpck_t *dev)
{
    uint8_t val;

    if (!dev->connected)
        return;

    /*
     * In byte mode the bridge drives the data lines rather than the status
     * bits, so a whole byte comes back per toggle.
     */
    if (dev->proto == BPCK_PROTO_PS2) {
        val          = bpck_read_reg(dev, dev->cur_reg);
        dev->rd_out  = 0x00;
        dev->dat_out = val;
        if (dev->lpt != NULL)
            lpt_write_to_dat(dev->lpt, val);
        return;
    }

    if (dev->rd_high) {
        dev->rd_out  = bpck_encode_nibble(dev->rd_val >> 4);
        dev->rd_high = 0;
    } else {
        dev->rd_val  = bpck_read_reg(dev, dev->cur_reg);
        dev->rd_out  = bpck_encode_nibble(dev->rd_val & 0x0f);
        dev->rd_high = 1;
    }
}

static void
bpck_epp_write_data(uint8_t is_addr, uint8_t val, void *priv)
{
    bpck_t *dev = (bpck_t *) priv;

    if (is_addr) {
        bpck_log("BPCK: W3 %02X (EPP address)\n", val);
        dev->cur_reg = val;
        dev->rd_high = 0;
    } else {
        bpck_log("BPCK: W4 %02X (EPP data)\n", val);
        bpck_write_reg(dev, dev->cur_reg, val);
    }
}

static void
bpck_epp_request_read(uint8_t is_addr, void *priv)
{
    bpck_t *dev = (bpck_t *) priv;
    uint8_t val = 0xff;

    if (!is_addr)
        val = bpck_read_reg(dev, dev->cur_reg);

    bpck_log("BPCK: R%d -> %02X (EPP %s)\n", is_addr ? 3 : 4, val,
             is_addr ? "address" : "data");

    dev->dat_out = val;
    if (dev->lpt != NULL)
        lpt_write_to_dat(dev->lpt, val);
}

static void
bpck_write_data(uint8_t val, void *priv)
{
    bpck_t *dev = (bpck_t *) priv;

    bpck_log("BPCK: W0 %02X            [%s]\n", val, bpck_state(dev));

    dev->dat = val;

    /*
     * An address on the data lines starts a knock, so toggles counted before
     * it were not part of one. The port calls us for every write, changed or
     * not, so counting from the write is safe where arming on it is not.
     */
    dev->knock = 0;

    /*
     * The bridge drives the data lines only during a PS/2 or EPP read;
     * otherwise the host reads its own latch back, so keep the input register
     * of the port tracking what was written or it sees zeroes and misjudges
     * what the port can do.
     */
    if (dev->lpt != NULL)
        lpt_write_to_dat(dev->lpt, val);
}

static void
bpck_write_ctrl(uint8_t val, void *priv)
{
    bpck_t       *dev       = (bpck_t *) priv;
    const uint8_t old       = dev->ctrl;
    const uint8_t chg       = (uint8_t) ((old ^ val) & LPT_CTRL_LINES);
    const uint8_t lines     = (uint8_t) (val & LPT_CTRL_LINES);
    const uint8_t old_lines = (uint8_t) (old & LPT_CTRL_LINES);

    bpck_log("BPCK: W2 %02X (was %02X)  [%s]\n", val, old, bpck_state(dev));

    dev->ctrl = val;

    /* The bridge latches on edges. Rewriting the same value is nothing. */
    if (chg == 0x00)
        return;

    /*
     * An out-of-band register write, addressed off the data lines with SELECT
     * raised: SELECT latches the register number, each AUTOFD toggle commits
     * the data lines, STROBE dropping ends it. A disconnect opens identically
     * but commits nothing, so this must be tested before anything else looks
     * at SELECT.
     */
    if (dev->latching) {
        if (chg & LPT_CTRL_AUTOFD) {
            bpck_write_reg(dev, dev->cur_reg, dev->dat);
            dev->latch_commits++;
        }

        if (!(lines & LPT_CTRL_STROBE)) {
            dev->latching = 0;
            if (dev->latch_commits == 0)
                bpck_disconnect(dev);
        }

        return;
    }

    if (dev->connected && (lines == (LPT_CTRL_STROBE | LPT_CTRL_SELECT))) {
        dev->latching      = 1;
        dev->latch_commits = 0;
        dev->cur_reg       = dev->dat;
        dev->rd_high       = 0;
        return;
    }

    /*
     * Letting go: INIT dropped leaving AUTOFD alone, then INIT and SELECT
     * raised together. Only the pair in that order ends the link - a nibble
     * read passes through AUTOFD-alone twice a byte.
     */
    if (dev->connected && (old_lines == LPT_CTRL_AUTOFD) &&
        (lines & LPT_CTRL_SELECT)) {
        bpck_disconnect(dev);
        return;
    }

    /*
     * The address probe that closes the knock. Nothing else may look at these
     * two AUTOFD edges: once the link is up an AUTOFD edge latches a register
     * number off the data lines, which still carry the unit number.
     */
    if (dev->ident) {
        if (chg & LPT_CTRL_SELECT)
            dev->ident = 0;
        return;
    }

    /*
     * The connect knock: three toggles of SELECT while the other lines hold
     * INIT alone, with the unit address on the data lines. Taken as a level,
     * so stray counts are thrown out.
     */
    if ((lines & ~LPT_CTRL_SELECT) != LPT_CTRL_INIT)
        dev->knock = 0;
    else if (chg & LPT_CTRL_SELECT) {
        if (++dev->knock >= 3) {
            dev->knock = 0;

            /*
             * The knock is addressed: these chain, so only the pod named on
             * the data lines may take the link. Answering to every address is
             * not harmless - the host decides a unit is present by the status
             * changing across the knock, so a pod that answers everywhere is
             * found nowhere.
             */
            if (dev->dat == dev->unit)
                bpck_connect(dev);
            else
                bpck_log("BPCK: knock for unit %02X, not ours (%02X)\n",
                         dev->dat, dev->unit);
        }

        return;
    }

    if (!dev->connected)
        return;

    if (chg & LPT_CTRL_AUTOFD) {
        dev->cur_reg = dev->dat;
        dev->rd_high = 0;

        /*
         * Addressing a register also puts the connect answer back on the
         * status lines, and it has to be a level the bridge holds: a host that
         * reads the status without clocking anything must keep seeing it.
         */
        dev->rd_out = bpck_connect_response(dev);
        return;
    }

    if (chg & LPT_CTRL_INIT) {
        if (lines & LPT_CTRL_STROBE)
            bpck_write_reg(dev, dev->cur_reg, dev->dat);
        else
            bpck_advance_read(dev);
    }
}

static uint8_t
bpck_read_status(void *priv)
{
    bpck_t *dev = (bpck_t *) priv;
    uint8_t ret;

    if (dev->connected && dev->ident)
        ret = bpck_ident_response(dev);
    else if (dev->connected)
        ret = dev->rd_out;
    else
        /* Not connected: the bridge leaves the status lines alone. */
        ret = LPT_STAT_IDLE;

    ret &= LPT_STAT_MASK;

    /* Idle status reads are how a host spins; trace only what changes. */
    if ((ret != dev->last_status) || dev->connected) {
        bpck_log("BPCK: R1 -> %02X          [%s]\n", ret, bpck_state(dev));
        dev->last_status = ret;
    }

    return ret;
}

/* --------------------------------------------------------------------- */
/* 86Box device plumbing                                                 */
/* --------------------------------------------------------------------- */

static void *
bpck_init(UNUSED(const device_t *info))
{
    bpck_t *dev = calloc(1, sizeof(bpck_t));

    if (dev == NULL)
        return NULL;

    dev->unit  = (uint8_t) device_get_config_int("unit");
    dev->proto = BPCK_PROTO_SPP;

    dev->lpt = lpt_attach(bpck_write_data, bpck_write_ctrl, NULL,
                          bpck_read_status, NULL,
                          bpck_epp_write_data, bpck_epp_request_read, dev);
    if (dev->lpt == NULL) {
        /* Another device already has this port. */
        free(dev);
        return NULL;
    }

    bpck_log("BPCK: attached as unit %02X\n", dev->unit);

    return dev;
}

static void
bpck_close(void *priv)
{
    free(priv);
}

// clang-format off
static const device_config_t bpck_config[] = {
    {
        .name           = "unit",
        .description    = "Chain address",
        .type           = CONFIG_SELECTION,
        .default_string = NULL,
        .default_int    = 0,
        .file_filter    = NULL,
        .spinner        = { 0 },
        .selection      = {
            { .description = "Unit 0", .value = 0 },
            { .description = "Unit 1", .value = 1 },
            { .description = "Unit 2", .value = 2 },
            { .description = "Unit 3", .value = 3 },
            { .description = "" }
        },
        .bios           = { { 0 } }
    },
    { .name = "", .description = "", .type = CONFIG_END }
};
// clang-format on

const device_t lpt_bpck_device = {
    .name          = "Micro Solutions BackPack CD-ROM",
    .internal_name = "bpck",
    .flags         = DEVICE_LPT,
    .local         = 0,
    .init          = bpck_init,
    .close         = bpck_close,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = bpck_config
};
