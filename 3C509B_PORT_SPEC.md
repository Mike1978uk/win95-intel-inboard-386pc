---
name: 3c509b_port_spec
description: Detailed 3C509B NIC port specification - for future implementation
metadata:
  type: project
---

# 3C509B Port Specification (Future Work)

**Priority**: Low (nice-to-have for mTCP networking)  
**Blocker**: No (Windows 3.11 boots without network)  
**Target Use**: mTCP stack with driver calls at DOS startup

## Real Hardware Config
- **I/O Base**: 0x320 (measured on real 5160)
- **IRQ**: 3 (real hardware)
- **ID Port**: 0x110 (3C509 protocol)
- **MAC Address**: 00:20:AF:6F:10:5E (real hardware measured)
- **Transceiver**: Twisted Pair (10BaseT)

## Reference Implementation
**QEMU 3C509B** (1126 lines, Antony T Curtis 2004)
- Location: `references/3c509b_qemu/qemu-3c509b.patch2`
- Complete, working, well-documented
- Ready to adapt to 86Box API

## Porting Requirements

### 1. State Structure
```c
typedef struct {
    uint16_t io_base;           /* 0x320 */
    uint8_t  irq;               /* 3 */
    uint8_t  mac[6];            /* 00:20:AF:6F:10:5E */
    uint16_t status;            /* Card status register */
    uint16_t command;           /* Command register */
    uint16_t net_status;        /* Network status */
    uint16_t rx_filter;         /* RX filter control */
    uint8_t  eeprom[64];        /* MAC in EEPROM[0:5] */
    /* TX/RX FIFO, buffers, counters... (see QEMU code) */
} nic_3c509b_state_t;
```

### 2. I/O Port Handlers
Replace QEMU's byte/word read/write with 86Box `io_sethandler()`:

```c
/* Port 0x320-0x321: Data/Command Port */
io_sethandler(0x0320, 2, 
              nic_3c509b_read,    /* Read handler */
              NULL, NULL,
              nic_3c509b_write,   /* Write handler */
              NULL, NULL,
              dev);

/* Port 0x110: ID Port (window select) */
io_sethandler(0x0110, 1,
              nic_3c509b_id_read,
              NULL, NULL,
              nic_3c509b_id_write,
              NULL, NULL,
              dev);
```

### 3. Network API Adapter
QEMU: `qemu_send_packet(nd, buf, size)`  
86Box: Use `network_send()` or equivalent

Map QEMU's `NetDriverState *nd` to 86Box's network interface.

### 4. IRQ Management
QEMU: `pic_set_irq(s->irq, state)`  
86Box: `picint_w(1 << 3)` for IRQ 3 (real hardware)

Update IRQ on:
- RX packet ready
- TX complete
- Status changes

### 5. EEPROM (MAC Address)
- Load real MAC (00:20:AF:6F:10:5E) into eeprom[0:5]
- Support EEPROM read/write commands
- Hardware: AT93C46 Serial EEPROM

## Implementation Phases

### Phase 1: Scaffold (3-4 hours)
- [ ] Adapt QEMU state structure to 86Box conventions
- [ ] Implement basic I/O port handlers (dummy responses)
- [ ] Wire up IRQ signaling
- [ ] Device init/close/reset callbacks

### Phase 2: Core Protocol (4-6 hours)
- [ ] Port command handler (tcm509_command)
- [ ] Implement RX filtering (padr_match, broadcast, multicast)
- [ ] Implement TX path (tcm509_transmit)
- [ ] EEPROM read/write

### Phase 3: Integration (2-3 hours)
- [ ] Network API integration
- [ ] Test with mTCP stack
- [ ] Verify MAC address persistence
- [ ] Test IRQ handling under load

### Phase 4: Validation (1-2 hours)
- [ ] Boot DOS with 3C509B driver
- [ ] mTCP initialization
- [ ] PING/basic network test
- [ ] Compare real hardware behavior

## QEMU Reference Structure (Adapt These)

From qemu-3c509b.patch2:

1. **Main state struct** (lines 86-123) - use as template
2. **I/O read/write handlers** (lines 141-142, ~200+ lines of implementation)
3. **Command handler** (tcm509_command, lines 245-334) - core logic
4. **RX packet filtering** (lines 159-191) - padr/broadcast/multicast
5. **TX handler** (tcm509_transmit, lines 336-349) - network send
6. **IRQ management** (tcm509_update_irq, lines 194-202) - key for driver
7. **Reset/init** (tcm509_reset, lines 204-231) - state initialization

## DOS/mTCP Requirements

For mTCP stack to work with 3C509B:
- [ ] DOS packet driver support
- [ ] INT 0x60 packet driver interface
- [ ] IRQ-driven packet delivery
- [ ] MAC address configuration
- [ ] ARP/ICMP/IP/UDP/TCP stack integration

3Com provides public packet driver interface - verify against QEMU code.

## Success Criteria
- [ ] DOS boots with 3C509B detected
- [ ] Driver loads without errors
- [ ] mTCP initializes
- [ ] PING works to real network
- [ ] File transfer works (if available)

## When to Start
After:
1. Windows 3.11 boots to Program Manager ✓
2. Mach8 self-test display issue resolved ✓
3. Sound Blaster DMA workaround documented ✓
4. Windows 3.11 stable for extended use ✓

Then dedicate focused session to full 3C509B port.

## Notes
- QEMU implementation is proven and complete
- No need to reverse-engineer hardware - QEMU did that
- Porting is translation work, not new research
- Real hardware testing available (Comrade + 5160)
- mTCP gives motivation to get it right

**Current status**: Stub in place (DEVICE_NOT_WORKING). Ready for proper port when prioritized.
