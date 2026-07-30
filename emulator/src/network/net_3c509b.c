/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Emulation of the 3Com Etherlink III (3C509B) ISA NIC
 *          Ported from QEMU implementation (Antony T Curtis, 2004)
 *
 * Authors: Ported for Intel Inboard 386/PC project, 2026
 *
 * NOTE: This is a placeholder/stub implementation.
 *       Full port from QEMU requires substantial work (~1100+ lines)
 *       Priority: Get Windows 3.11 booting; NIC is secondary
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/io.h>
#include <86box/network.h>
#include <86box/plat_unused.h>

typedef struct {
    uint8_t  id_port_val;
    uint16_t io_base;
    uint8_t  irq;
    uint8_t  mac[6];
    int      enabled;
} nic_3c509b_t;

/* Stub device - real implementation pending full QEMU port */
static void *
nic_3c509b_init(const device_t *info)
{
    nic_3c509b_t *dev = malloc(sizeof(nic_3c509b_t));

    /* Real hardware configuration (from 2026-07-30 measurements) */
    dev->io_base = 0x320;  /* NOT 0x300 - real hardware measured */
    dev->irq = 3;
    dev->mac[0] = 0x00; dev->mac[1] = 0x20; dev->mac[2] = 0xAF;
    dev->mac[3] = 0x6F; dev->mac[4] = 0x10; dev->mac[5] = 0x5E;
    dev->enabled = 1;

    fprintf(stderr, "[3C509B] Stub initialized: I/O 0x%03x, IRQ %d, MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
            dev->io_base, dev->irq, dev->mac[0], dev->mac[1], dev->mac[2],
            dev->mac[3], dev->mac[4], dev->mac[5]);

    return dev;
}

static void
nic_3c509b_close(void *priv)
{
    free(priv);
}

const device_t nic_3c509b_device = {
    .name          = "3Com Etherlink III (3C509B)",
    .internal_name = "3c509b",
    .flags         = DEVICE_ISA | DEVICE_NOT_WORKING,  /* NOT_WORKING until full port done */
    .local         = 0,
    .init          = nic_3c509b_init,
    .close         = nic_3c509b_close,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};
