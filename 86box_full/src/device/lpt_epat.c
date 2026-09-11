/*
 * 86Box     A hypervisor and IBM PC system emulator.
 *
 *           Shuttle Technology EPAT / EPATRM parallel-port to ATAPI bridge.
 *
 *           This is the bridge used by the Imation SuperDisk (LS-120)
 *           parallel-port drive. 86Box already models the drive itself in
 *           rdisk.c ("IMATION / SUPERDISK 120 ATAPI"); what was missing was
 *           the bridge that carries ATAPI over a parallel port. RDISK_BUS_LPT
 *           exists in rdisk.h and is referenced by no source file.
 *
 *           Protocol source: reverse-engineered from Imation's own DOS driver
 *           and VERIFIED ON REAL HARDWARE (a Shuttle EPATRM bridge with a
 *           Matsushita LS-120 COSM 04, on an IBM 5160). Cross-checked against
 *           Linux's drivers/block/paride/epat.c. Every constant below has a
 *           captured hardware reading behind it; see the references in each
 *           comment.
 *
 * Authors:  Mike Lycett and contributors
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
#include <86box/scsi.h>
#include <86box/scsi_device.h>
#include <86box/hdc_ide.h>
#include <86box/rdisk.h>
#include <86box/plat_unused.h>
#include <86box/log.h>

/*
 * The whole reason for modelling this bridge is VISIBILITY. On real hardware a
 * driver bug surfaces as "36 zeros" or "media is not formatted" and costs a
 * boot to narrow. Here the bridge is on both sides of the conversation, so it
 * can say what the host asked for AND what the protocol expected - which turns
 * a day of bisecting into one log line.
 *
 * Every failure this project spent 2026-09-11 on is a one-liner from in here:
 * the CDB written as twelve register writes instead of a block write; a command
 * refused with a unit attention nobody had cleared; a host reading the length
 * it asked for rather than the count the device offered.
 */
#define ENABLE_EPAT_LOG 1
#ifdef ENABLE_EPAT_LOG
int epat_do_log = ENABLE_EPAT_LOG;

static void
epat_log(void *priv, const char *fmt, ...)
{
    if (epat_do_log) {
        va_list ap;
        va_start(ap, fmt);
        log_out(priv, fmt, ap);
        va_end(ap);
    }
}
#else
#    define epat_log(priv, fmt, ...)
#endif

/*
 * The unlock frame. Eight bytes to the data port, each written TWICE - the
 * second write is the I/O delay, not a retry - then committed by pulsing
 * nINIT. The eighth byte is the command: 0xE0 connect, 0x30 disconnect.
 *
 * TRANSPORT_SPEC.md section 3, replayed byte-for-byte on the real machine
 * with all three checkpoints matching, reproducibly, twice.
 */
static const uint8_t epat_unlock[7] = { 0x22, 0xAA, 0x55, 0x00, 0xFF, 0x87, 0x78 };

#define EPAT_CPP_CONNECT    0xE0
#define EPAT_CPP_DISCONNECT 0x30

/*
 * Checkpoint values the real bridge returns on the status port part-way
 * through the unlock frame. The driver masks these; the raw readings are what
 * the hardware gave, so returning them exactly makes a capture comparison
 * meaningful rather than approximate.
 *
 *   after 22 AA 55 00 FF : status & 0xF0 == 0xB0   hardware read 0xB8
 *   after 87             : status & 0xF0 == 0x50   hardware read 0x58
 *   after 78             : status & 0xB0 == 0xB0   hardware read 0xF0
 */
#define EPAT_CHK_1 0xB8
#define EPAT_CHK_2 0x58
#define EPAT_CHK_3 0xF0

/* Idle status when connected but not mid-transfer. */
#define EPAT_STAT_IDLE 0x38

typedef enum {
    EPAT_UNLOCK_IDLE = 0, /* not in an unlock frame           */
    EPAT_UNLOCK_RUN,      /* matching the seven-byte preamble */
    EPAT_UNLOCK_CMD       /* preamble matched, command byte next */
} epat_unlock_state_t;

typedef struct epat_s {
    void *lpt;
    void *log;

    /*
     * The drive this bridge fronts, or NULL when no removable disk is
     * assigned to this parallel port. With no drive the bridge still speaks
     * the protocol against its own regs[], which is useful for testing the
     * wire format on its own.
     */
    scsi_device_t *sd;
    ide_tf_t      *tf;
    uint8_t        port;
    uint8_t        no_drive; /* looked up once, and there was none */

    /*
     * Block mode. The CDB and all sector data move through register 7,
     * which is NOT the task file's data register - it is the bridge's own
     * streaming port, addressed without cont_map, and it has a protocol and
     * an alternating phase bit of its own. epat.c's read_block/write_block.
     */
    int     block;       /* EPAT_BLOCK_NONE / _READ / _WRITE */
    int     block_arm;   /* register 7 addressed, awaiting the w2 that starts it */
    int     block_half;  /* 0 = the next status read returns the low nibble */
    uint8_t block_latch; /* the byte being shifted out as two nibbles */

    /* Parallel-port pin state as the host last wrote it. */
    uint8_t data;    /* w0 */
    uint8_t ctrl;    /* w2 */
    uint8_t status;  /* what r1 will return */

    /* Unlock-frame recogniser. */
    epat_unlock_state_t ustate;
    int                 upos;     /* how much of the preamble has matched  */
    uint8_t             ulast;    /* last data byte, to fold the double-write */
    int                 udup;     /* 1 = the next identical byte is the delay */
    uint8_t             ucmd;     /* the command byte once the preamble matched */

    int connected;

    /*
     * Register access. The host addresses a register by writing its number to
     * the DATA port - directly, as regr + cont_map[cont]; there is no index/data
     * pair. A write tags the number with 0x60; a read sends it bare. The value
     * then comes back as two nibbles, each arriving in the TOP four bits of the
     * status port across two control-port strobes.
     *
     *   read : w0(r); w2(1); w2(3); a = r1(); w2(4); b = r1();
     *          value = ((a >> 4) & 0x0f) | (b & 0xf0)
     *   write: w0(0x60 + r); w2(1); w0(val); w2(4)
     */
    uint8_t reg_addr;   /* register the host last addressed */
    uint8_t reg_latch;  /* value fetched when the low nibble was asked for */
    int     reg_write;  /* the 0x60 tag was set - a value byte is coming */
    int     nibble_hi;  /* 0 = next status read returns the low nibble */

    /*
     * cont_map = { 0x18, 0x10, 0 }: the ATA task file lives at 0x18, device
     * control at 0x16, and the bridge's own registers at 0x00. One flat array
     * is simpler than three and the addresses do not overlap.
     */
    uint8_t regs[0x20];
} epat_t;

#define EPAT_REG_TASKFILE 0x18 /* cont 0 */
#define EPAT_REG_DEVCTL   0x16 /* cont 1 + 6 */
#define EPAT_WRITE_TAG    0x60

/* ATA/ATAPI task-file offsets, relative to EPAT_REG_TASKFILE. */
#define ATA_DATA     0
#define ATA_ERROR    1
#define ATA_IREASON  2
#define ATA_BCLO     4
#define ATA_BCHI     5
#define ATA_DRVHD    6
#define ATA_STATUS   7

/* The only ATA command an ATAPI drive behind this bridge is given. */
#define ATA_CMD_PACKET 0xA0

/*
 * epat.c addresses the streaming data port as a bare 7 to read and 0x67 to
 * write, with no cont_map offset applied. It never moves data through the
 * task file's own data register.
 */
#define EPAT_REG_BLOCK  0x07

#define EPAT_BLOCK_NONE  0
#define EPAT_BLOCK_READ  1
#define EPAT_BLOCK_WRITE 2
#define ATA_CDB_LEN    12

#define ATA_ST_DRDY 0x40
#define ATA_ST_DSC  0x10

/* The ATAPI signature a device reports after a reset: 14 EB in LBA mid/high. */
#define ATAPI_SIG_LO 0x14
#define ATAPI_SIG_HI 0xEB

/*
 * The preamble is written as pairs. Fold a repeat of the byte we just saw
 * into a single logical step, which is what the bridge does - it is an I/O
 * delay, not data.
 */
static int
epat_unlock_feed(epat_t *dev, uint8_t val)
{
    if (dev->udup && (val == dev->ulast)) {
        dev->udup = 0;
        return 0;
    }

    dev->ulast = val;
    dev->udup  = 1;

    if (dev->ustate == EPAT_UNLOCK_CMD) {
        dev->ucmd   = val;
        dev->ustate = EPAT_UNLOCK_IDLE;
        return 1; /* a complete frame is pending its nINIT commit */
    }

    if (val == epat_unlock[dev->upos]) {
        dev->upos++;
        dev->ustate = EPAT_UNLOCK_RUN;

        /* Publish the checkpoint the driver is about to read. */
        if (dev->upos == 5)
            dev->status = EPAT_CHK_1;
        else if (dev->upos == 6)
            dev->status = EPAT_CHK_2;
        else if (dev->upos == 7) {
            dev->status = EPAT_CHK_3;
            dev->ustate = EPAT_UNLOCK_CMD;
        }
        return 0;
    }

    /*
     * A mismatch restarts the match rather than aborting it - the first byte
     * of a new frame can arrive immediately after a failed one.
     */
    if (dev->ustate == EPAT_UNLOCK_RUN)
        epat_log(dev->log, "unlock frame broken at byte %i: got %02X, expected %02X\n",
                 dev->upos, val, epat_unlock[dev->upos]);
    dev->upos   = (val == epat_unlock[0]) ? 1 : 0;
    dev->ustate = dev->upos ? EPAT_UNLOCK_RUN : EPAT_UNLOCK_IDLE;
    return 0;
}

/*
 * Find the drive assigned to this port.
 *
 * This cannot be done at init: 86box.c runs lpt_devices_init() well before
 * rdisk_hard_reset(), so at the time this device is created no removable disk
 * exists yet. Moving rdisk_hard_reset() earlier is not an option either - it
 * attaches SCSI drives and so must follow scsi_card_init(), which is itself
 * after the parallel devices. So the lookup is done on first use and cached.
 *
 * With no drive assigned the bridge answers from its own regs[], which keeps
 * the wire protocol testable on its own.
 */
static int
epat_attach_drive(epat_t *dev)
{
    if (dev->sd != NULL)
        return 1;

    if (dev->no_drive)
        return 0;

    dev->sd = rdisk_get_lpt_device(dev->port);

    if (dev->sd == NULL) {
        dev->no_drive = 1;
        epat_log(dev->log, "no removable disk assigned to LPT%i - "
                           "answering from the bridge's own registers\n",
                 dev->port + 1);
        return 0;
    }

    dev->tf = ((rdisk_t *) dev->sd->sc)->tf;
    epat_log(dev->log, "LPT%i drive attached\n", dev->port + 1);

    return 1;
}

/*
 * The packet phase engine, transliterated from ide_atapi_callback() and
 * ide_atapi_pio_request() in hdc_ide.c. The drive is an ordinary ATAPI device;
 * all that differs is that its task file and data register are reached a nibble
 * at a time down a parallel cable instead of over the ISA bus.
 *
 * Two deliberate departures from the IDE path:
 *
 *   - No interrupts. The Shuttle bridges here run polled - the real machine
 *     measures dmaEn = 0 - and the driver polls BSY and DRQ.
 *   - No callback timing. The IDE controller arms a timer for the delay the
 *     drive asks for; the bridge completes immediately, because one byte over
 *     this link costs far more than any seek that delay models. This does mean
 *     a spin-up wait is not reproduced, which is worth remembering when using
 *     the bridge to test a driver's timeouts.
 */
static void epat_pio_request(epat_t *dev, int out);

static void
epat_atapi_callback(epat_t *dev)
{
    scsi_common_t *sc = dev->sd->sc;

    switch (sc->packet_status) {
        default:
            break;

        case PHASE_IDLE:
            dev->tf->pos     = 0;
            dev->tf->phase   = 1;
            dev->tf->atastat = READY_STAT | DRQ_STAT | (dev->tf->atastat & ERR_STAT);
            break;

        case PHASE_COMMAND:
            dev->tf->atastat = BUSY_STAT | (dev->tf->atastat & ERR_STAT);
            dev->sd->command(sc, sc->atapi_cdb);
            /*
             * Whatever delay the drive asked for is discarded, and the phase it
             * moved to is acted on now. rdisk_set_callback() is a no-op for an
             * LPT drive, so nothing else would ever run this.
             */
            sc->callback = 0.0;
            if (sc->packet_status != PHASE_COMMAND)
                epat_atapi_callback(dev);
            break;

        case PHASE_COMPLETE:
        case PHASE_ERROR:
            dev->tf->atastat = READY_STAT;
            if (sc->packet_status == PHASE_ERROR)
                dev->tf->atastat |= ERR_STAT;
            dev->tf->phase    = 3;
            sc->packet_status = PHASE_NONE;
            epat_log(dev->log, "command done, status %02X\n", dev->tf->atastat);
            break;

        case PHASE_DATA_IN:
        case PHASE_DATA_OUT:
            dev->tf->atastat = READY_STAT | DRQ_STAT | (dev->tf->atastat & ERR_STAT);
            /* PHASE_DATA_IN gives ireason 2 (I/O set), PHASE_DATA_OUT gives 0. */
            dev->tf->phase   = !(sc->packet_status & 0x01) << 1;
            epat_log(dev->log, "data phase %s, %u bytes, request length %u\n",
                     (sc->packet_status == PHASE_DATA_IN) ? "in" : "out",
                     sc->packet_len, dev->tf->request_length);
            break;
    }
}

/* A transfer has reached the end of a block, or of the whole command. */
static void
epat_pio_request(epat_t *dev, const int out)
{
    scsi_common_t *sc = dev->sd->sc;

    dev->tf->atastat = BUSY_STAT;

    if (dev->tf->pos >= sc->packet_len) {
        dev->tf->pos    = 0;
        sc->request_pos = 0;

        if (out)
            dev->sd->phase_data_out(sc);
        else
            dev->sd->command_stop(sc);

        sc->callback = 0.0;
        if (sc->packet_status == PHASE_COMPLETE)
            epat_atapi_callback(dev);
    } else {
        /* Short tail: tell the host how much is actually left. */
        if ((sc->packet_len - dev->tf->pos) < sc->max_transfer_len) {
            sc->max_transfer_len    = (uint16_t) (sc->packet_len - dev->tf->pos);
            dev->tf->request_length = sc->max_transfer_len;
        }

        sc->packet_status = PHASE_DATA_IN | out;
        epat_atapi_callback(dev);
        sc->request_pos = 0;
    }
}

/*
 * The data register. The bridge is a byte-wide link, so unlike the IDE path
 * this moves one byte per access rather than a word.
 */
static uint8_t
epat_data_read(epat_t *dev)
{
    scsi_common_t *sc = dev->sd->sc;
    uint8_t        ret;

    if ((sc->temp_buffer == NULL) || (sc->packet_status != PHASE_DATA_IN))
        return 0;

    /*
     * Reading past the buffer returns zero rather than reading off the end:
     * a command with an allocation length below one sector leaves the host
     * asking for more than the drive prepared.
     */
    ret = (dev->tf->pos < sc->packet_len) ? sc->temp_buffer[dev->tf->pos] : 0;
    dev->tf->pos++;
    sc->request_pos++;

    if ((sc->request_pos >= sc->max_transfer_len) || (dev->tf->pos >= sc->packet_len))
        epat_pio_request(dev, 0);

    return ret;
}

static void
epat_data_write(epat_t *dev, const uint8_t val)
{
    scsi_common_t *sc  = dev->sd->sc;
    uint8_t       *buf = NULL;

    /* Before a command is assembled the data register carries the CDB. */
    if (sc->packet_status == PHASE_IDLE)
        buf = sc->atapi_cdb;
    else if (sc->packet_status == PHASE_DATA_OUT)
        buf = sc->temp_buffer;

    if (buf == NULL)
        return;

    buf[dev->tf->pos] = val;
    dev->tf->pos++;
    sc->request_pos++;

    if (sc->packet_status == PHASE_DATA_OUT) {
        if ((sc->request_pos >= sc->max_transfer_len) ||
            (dev->tf->pos >= sc->packet_len))
            epat_pio_request(dev, 1);
    } else if (dev->tf->pos >= ATA_CDB_LEN) {
        epat_log(dev->log, "CDB %02X %02X %02X %02X %02X %02X "
                           "%02X %02X %02X %02X %02X %02X\n",
                 buf[0], buf[1], buf[2],  buf[3],  buf[4],  buf[5],
                 buf[6], buf[7], buf[8],  buf[9],  buf[10], buf[11]);

        dev->tf->pos      = 0;
        dev->tf->atastat  = BUSY_STAT;
        sc->packet_status = PHASE_COMMAND;
        epat_atapi_callback(dev);
    }
}

/* A write to the command register. PACKET is the only one that means anything. */
static void
epat_command(epat_t *dev, const uint8_t cmd)
{
    scsi_common_t *sc = dev->sd->sc;

    if (cmd != ATA_CMD_PACKET) {
        epat_log(dev->log, "command %02X is not PACKET, aborting\n", cmd);
        dev->tf->atastat = READY_STAT | ERR_STAT | ATA_ST_DSC;
        dev->tf->error   = ABRT_ERR;
        return;
    }

    epat_log(dev->log, "PACKET, byte count %u\n", dev->tf->request_length);

    dev->tf->pos      = 0;
    sc->packet_status = PHASE_IDLE;
    dev->tf->phase    = 1; /* ireason 1: the drive wants the CDB */
    dev->tf->atastat  = READY_STAT | DRQ_STAT;
}

/*
 * The ATA task file belongs to the drive, not to the bridge. These map the
 * eight task-file offsets onto ide_tf_t, which is the same structure the IDE
 * controller drives the drive through. Offsets outside the task file (device
 * control, and the bridge's own registers) stay in regs[].
 */
static uint8_t
epat_reg_read(epat_t *dev, const uint8_t addr)
{
    epat_attach_drive(dev);

    if ((dev->tf != NULL) && (addr == EPAT_REG_BLOCK))
        return epat_data_read(dev);

    if ((dev->tf == NULL) || (addr < EPAT_REG_TASKFILE) ||
        (addr > (EPAT_REG_TASKFILE + ATA_STATUS)))
        return dev->regs[addr];

    switch (addr - EPAT_REG_TASKFILE) {
        case ATA_DATA:
            return epat_data_read(dev);
        case ATA_ERROR:
            return dev->tf->error;
        case ATA_IREASON:
            return dev->tf->phase;
        case ATA_BCLO:
            return dev->tf->request_length & 0xff;
        case ATA_BCHI:
            return (dev->tf->request_length >> 8) & 0xff;
        case ATA_DRVHD:
            return dev->tf->drvsel;
        case ATA_STATUS:
            return dev->tf->atastat;
        default:
            return dev->regs[addr];
    }
}

static void
epat_reg_write(epat_t *dev, const uint8_t addr, const uint8_t val)
{
    dev->regs[addr] = val;

    epat_attach_drive(dev);

    if ((dev->tf == NULL) || (addr < EPAT_REG_TASKFILE) ||
        (addr > (EPAT_REG_TASKFILE + ATA_STATUS)))
        return;

    switch (addr - EPAT_REG_TASKFILE) {
        case ATA_DATA:
            epat_data_write(dev, val);
            break;
        case ATA_ERROR:
            dev->tf->features = val;
            break;
        case ATA_IREASON:
            dev->tf->phase = val;
            break;
        case ATA_BCLO:
            dev->tf->request_length = (dev->tf->request_length & 0xff00) | val;
            break;
        case ATA_BCHI:
            dev->tf->request_length = (dev->tf->request_length & 0x00ff) |
                                      ((uint16_t) val << 8);
            break;
        case ATA_DRVHD:
            dev->tf->drvsel = val;
            break;
        case ATA_STATUS: /* the command register, on a write */
            epat_command(dev, val);
            break;
        default:
            break;
    }
}

/* Present the drive as it is immediately after a reset. */
static void
epat_device_reset(epat_t *dev)
{
    memset(dev->regs, 0x00, sizeof(dev->regs));
    dev->regs[EPAT_REG_TASKFILE + ATA_STATUS] = ATA_ST_DRDY | ATA_ST_DSC;
    dev->regs[EPAT_REG_TASKFILE + ATA_BCLO]   = ATAPI_SIG_LO;
    dev->regs[EPAT_REG_TASKFILE + ATA_BCHI]   = ATAPI_SIG_HI;

    /*
     * A real drive sets its own signature here. rdisk_reset() writes
     * request_length = 0xEB14, which is that signature, so the values above
     * only apply when the bridge is running without a drive.
     */
    if (epat_attach_drive(dev)) {
        dev->sd->reset(dev->sd->sc);

        /*
         * A real drive raises a unit attention when it is reset, and the
         * physical LS-120 demonstrably does: after this same SRST it answers
         * the next READ with sense key 6. rdisk_reset() clears the flag, so
         * without this the emulated drive is more forgiving than the real one
         * and a driver that mishandles the condition would pass here and fail
         * on hardware. rdisk already implements the rest, ALLOW_UA included.
         */
        ((rdisk_t *) dev->sd->sc)->unit_attention = 1;
    }

    epat_log(dev->log, "device reset: status %02X, signature %02X %02X\n",
             dev->regs[EPAT_REG_TASKFILE + ATA_STATUS], ATAPI_SIG_LO, ATAPI_SIG_HI);
}

static void
epat_write_data(uint8_t val, void *priv)
{
    epat_t *dev = (epat_t *) priv;

    dev->data = val;

    epat_unlock_feed(dev, val);

    /*
     * While an unlock frame is being matched, or one is committed but waiting
     * for its nINIT pulse, these bytes are frame content and not register
     * addresses. A register number that happens to equal 0x22 would start the
     * recogniser spuriously; epat_write_ctrl cancels a partial match the moment
     * it sees w2(1), which an unlock frame never issues.
     */
    if ((dev->ustate != EPAT_UNLOCK_IDLE) || dev->ucmd)
        return;

    if (!dev->connected)
        return;

    /*
     * Mid-block every data byte belongs to the stream. On the read side the
     * host writes 0xFF to turn the bus around and 0xFD to flag the last byte;
     * neither is data, and 0x00 ends the block.
     */
    if (dev->block == EPAT_BLOCK_WRITE) {
        epat_data_write(dev, val);
        return;
    }

    if (dev->block == EPAT_BLOCK_READ) {
        if (val == 0x00) {
            epat_log(dev->log, "block read done\n");
            dev->block = EPAT_BLOCK_NONE;
        }
        return;
    }

    if (dev->reg_write) {
        /* The value for the register addressed by the previous write. */
        dev->reg_write = 0;
        if (dev->reg_addr < sizeof(dev->regs)) {
            epat_reg_write(dev, dev->reg_addr, val);
            epat_log(dev->log, "W reg %02X = %02X\n", dev->reg_addr, val);

            /* SRST asserted then released is how a cold drive is brought up. */
            if ((dev->reg_addr == EPAT_REG_DEVCTL) && !(val & 0x04))
                epat_device_reset(dev);
        } else
            epat_log(dev->log, "W reg %02X out of range\n", dev->reg_addr);
        return;
    }

    /* Register 7, tagged or bare, arms a block transfer rather than a register. */
    if (val == (EPAT_WRITE_TAG | EPAT_REG_BLOCK)) {
        dev->block_arm = EPAT_BLOCK_WRITE;
        return;
    }

    if (val == EPAT_REG_BLOCK) {
        dev->block_arm = EPAT_BLOCK_READ;
        return;
    }

    if (val & EPAT_WRITE_TAG) {
        dev->reg_addr  = val & ~EPAT_WRITE_TAG;
        dev->reg_write = 1;
    } else {
        dev->reg_addr  = val;
        dev->nibble_hi = 0;
    }
}

static void
epat_write_ctrl(uint8_t val, void *priv)
{
    epat_t *dev = (epat_t *) priv;

    /*
     * The frame is committed by pulsing nINIT: 0x04 -> 0x05 -> 0x04. Act on
     * the rising edge of bit 0 while a command byte is pending.
     */
    if (!(dev->ctrl & 0x01) && (val & 0x01) && (dev->ustate == EPAT_UNLOCK_IDLE) && dev->ucmd) {
        if (dev->ucmd == EPAT_CPP_CONNECT) {
            dev->connected = 1;
            dev->status    = EPAT_STAT_IDLE;
            epat_log(dev->log, "CONNECT\n");
        } else if (dev->ucmd == EPAT_CPP_DISCONNECT) {
            dev->connected = 0;
            dev->status    = EPAT_STAT_IDLE;
            epat_log(dev->log, "DISCONNECT\n");
        } else
            epat_log(dev->log, "unlock frame committed with unknown command %02X\n",
                     dev->ucmd);
        dev->ucmd = 0;
        dev->upos = 0;
    }

    /*
     * A register read strobes w2(1) then w2(3), then w2(4) for the second
     * nibble. An unlock frame never writes 0x01, so seeing it both cancels any
     * partial frame match and starts the nibble sequence.
     */
    /*
     * w0(0x67); w2(1); w2(5)  starts a write block.
     * w0(7);    w2(1); w2(3)  starts a read block.
     * A write block ends on w2(7), a read block on w0(0) above.
     */
    if (dev->connected && (dev->block_arm != EPAT_BLOCK_NONE)) {
        if ((dev->block_arm == EPAT_BLOCK_WRITE) && (val == 0x05)) {
            dev->block     = EPAT_BLOCK_WRITE;
            dev->block_arm = EPAT_BLOCK_NONE;
            epat_log(dev->log, "block write start\n");
            dev->ctrl = val;
            return;
        }

        if ((dev->block_arm == EPAT_BLOCK_READ) && (val == 0x03)) {
            dev->block      = EPAT_BLOCK_READ;
            dev->block_arm  = EPAT_BLOCK_NONE;
            dev->block_half = 0;
            epat_log(dev->log, "block read start\n");
            dev->ctrl = val;
            return;
        }
    }

    if ((dev->block == EPAT_BLOCK_WRITE) && (val == 0x07)) {
        epat_log(dev->log, "block write done\n");
        dev->block = EPAT_BLOCK_NONE;
        dev->ctrl  = val;
        return;
    }

    if (dev->block != EPAT_BLOCK_NONE) {
        /* Mid-block the control port only carries the handshake phase. */
        dev->ctrl = val;
        return;
    }

    if (dev->connected && (val == 0x01)) {
        dev->ustate    = EPAT_UNLOCK_IDLE;
        dev->upos      = 0;
        dev->nibble_hi = 0;
    } else if (dev->connected && (val == 0x03))
        dev->nibble_hi = 0; /* first read returns the LOW nibble */
    else if (dev->connected && (val == 0x04) && !dev->ucmd)
        dev->nibble_hi = 1; /* second read returns the HIGH nibble */

    dev->ctrl = val;
}

static uint8_t
epat_read_status(void *priv)
{
    epat_t *dev = (epat_t *) priv;
    uint8_t val;
    uint8_t ret;

    /* Mid-handshake the checkpoints take priority over any register value. */
    if ((dev->ustate != EPAT_UNLOCK_IDLE) || !dev->connected)
        return dev->status;

    if (dev->block == EPAT_BLOCK_READ) {
        if (dev->block_half) {
            dev->block_half = 0;
            /* j44 takes the high nibble from the second read, unshifted. */
            return dev->block_latch & 0xF0;
        }

        dev->block_latch = epat_data_read(dev);
        dev->block_half  = 1;

        /*
         * The low nibble is returned in the TOP four bits, and bit 3 MUST be
         * clear: epat_read_block treats a set bit 3 as "that read carried the
         * whole byte" and skips the second one, which would corrupt every
         * byte read. EPAT_STAT_IDLE has bit 3 set, so it cannot be used here.
         */
        return (uint8_t) ((dev->block_latch & 0x0F) << 4);
    }

    if (dev->reg_addr >= sizeof(dev->regs))
        return dev->status;

    /*
     * A byte is fetched once and shifted out as two nibbles. Fetching it again
     * for the high nibble would advance the data register twice per byte.
     */
    if (!dev->nibble_hi)
        dev->reg_latch = epat_reg_read(dev, dev->reg_addr);

    val = dev->reg_latch;

    /*
     * j44(a, b) = ((a >> 4) & 0x0f) | (b & 0xf0), so each nibble must arrive in
     * the TOP four bits. The low four are not used by the combine; the bridge
     * drives them from its own state and the driver ignores them.
     */
    if (dev->nibble_hi)
        ret = (val & 0xF0) | (EPAT_STAT_IDLE & 0x0F);
    else
        ret = (uint8_t) ((val & 0x0F) << 4) | (EPAT_STAT_IDLE & 0x0F);

    epat_log(dev->log, "R reg %02X %s nibble -> %02X (value %02X)\n",
             dev->reg_addr, dev->nibble_hi ? "high" : "low", ret, val);

    return ret;
}

static uint8_t
epat_read_ctrl(void *priv)
{
    const epat_t *dev = (epat_t *) priv;

    return dev->ctrl;
}

static void *
epat_init(UNUSED(const device_t *info))
{
    epat_t *dev = (epat_t *) calloc(1, sizeof(epat_t));

    if (dev == NULL)
        return NULL;

    dev->status = EPAT_STAT_IDLE;
    dev->log    = log_open("EPAT");
    dev->port   = (uint8_t) device_get_config_int("port");

    /* The drive does not exist yet - see epat_attach_drive(). */

    dev->lpt = lpt_attach_ex(device_get_config_int("port"),
                             epat_write_data, epat_write_ctrl, NULL,
                             epat_read_status, epat_read_ctrl,
                             NULL, NULL, dev);

    return dev;
}

static void
epat_close(void *priv)
{
    epat_t *dev = (epat_t *) priv;

    if (dev != NULL) {
        if (dev->log != NULL)
            log_close(dev->log);
        free(dev);
    }
}

static const device_config_t epat_config[] = {
    {
        .name           = "port",
        .description    = "Parallel Port",
        .type           = CONFIG_SELECTION,
        .default_string = NULL,
        .default_int    = 0,
        .file_filter    = NULL,
        .spinner        = { 0 },
        .selection      = {
            { .description = "LPT1", .value = 0 },
            { .description = "LPT2", .value = 1 },
            { .description = "LPT3", .value = 2 },
            { .description = "LPT4", .value = 3 },
            { .description = ""                 }
        },
        .bios           = { { 0 } }
    },
    { .name = "", .description = "", .type = CONFIG_END }
};

const device_t lpt_epat_device = {
    .name          = "Shuttle EPAT parallel-port ATAPI bridge",
    .internal_name = "lpt_epat",
    .flags         = DEVICE_LPT,
    .local         = 0,
    .init          = epat_init,
    .close         = epat_close,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = epat_config
};
