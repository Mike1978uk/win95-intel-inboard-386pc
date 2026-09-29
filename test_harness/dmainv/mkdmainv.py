"""Build DMAINV.SCR / DMAINV.BAT: read-only 8237 and PIT channel 1 probe (#29 item 3, #33 step 1).

Run DMAINV.BAT from real DOS (Command prompt only), not a Windows DOS box: VDMAD and VTD
virtualise these ports there. DEBUG needs CRLF line endings or it never reaches `q` and hangs.
"""
import pathlib

here = pathlib.Path(__file__).parent

scr = []
# Port 0Ch resets the 8237 byte-pointer flip-flop so each address/count pair reads low, high.
# Channel 0 is read twice: a refresh channel that is running changes between the two reads.
for ch in (0, 0, 1, 2, 3):
    scr += ['o c 0', f'i {2 * ch:x}', f'i {2 * ch:x}', f'i {2 * ch + 1:x}', f'i {2 * ch + 1:x}']
scr.append('i 8')  # status: bits 4-7 request pending, bits 0-3 terminal count (cleared by the read)
# BIOS programs channel 1 LSB-only, mode 2: latch then one read per sample. The largest
# sample approaches the divisor.
for _ in range(32):
    scr += ['o 43 40', 'i 41']
scr.append('q')

bat = [
    '@ECHO OFF',
    'REM Read-only 8237 and PIT channel 1 probe. Real DOS only, not a Windows DOS box.',
    'DEBUG < C:\\DMAINV.SCR > C:\\DMAINV.TXT',
    'ECHO DMAINV done: C:\\DMAINV.TXT',
]

for name, lines in (('DMAINV.SCR', scr), ('DMAINV.BAT', bat)):
    (here / name).write_bytes(('\r\n'.join(lines) + '\r\n').encode('ascii'))
print('written', len(scr), 'script lines')
