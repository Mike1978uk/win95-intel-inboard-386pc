"""Stage the DOS agent for the 5160: files for C:\\V9XREMOT and edited CONFIG.SYS/AUTOEXEC.BAT."""
# How the 5160's DOSNET boot entry was staged (2026-11-18): reads the card's CONFIG.SYS and AUTOEXEC.BAT, adds the menu entry and the agent block, keeping CRLF. Card-specific anchors; read before reuse.
import os, shutil, sys

S = r'C:\Users\lycet\AppData\Local\Temp\claude\c5a7a5e50d1189a6\7981b946-94bb-48ea-9783-744c455efb65\scratchpad\dosnet'
P = r'C:\Users\lycet\RiderProjects\V9x-Remote-Agent\build\dos-install'
BOOT = os.path.join(os.environ['TEMP'], 'card_boot')
os.makedirs(S, exist_ok=True)

def crlf(text):
    return text.replace('\r\n', '\n').replace('\n', '\r\n').encode('ascii')

for f in ('V9XAGNT.EXE', 'DOS4GW.EXE', 'V9XLOOP.BAT'):
    shutil.copyfile(os.path.join(P, f), os.path.join(S, f))
shutil.copyfile(r'C:\Users\lycet\Downloads\3com-etherdisk-4-3\PKTDVR\3C5X9PD.COM', os.path.join(S, '3C5X9PD.COM'))
open(os.path.join(S, 'WATTCP.CFG'), 'wb').write(crlf('my_ip = dhcp\n'))
open(os.path.join(S, 'AGENT.INI'), 'wb').write(crlf('[agent]\nport=9869\nallowed_client=192.168.68.63\n'))
lp = open(os.path.join(S, 'V9XLOOP.BAT'), 'rb').read()
assert lp.count(b'\r\n') >= 10 and lp.count(b'\n') == lp.count(b'\r\n'), 'V9XLOOP.BAT not CRLF'

# CONFIG.SYS: one menu line after the SAFE item, and an empty [DOSNET] section.
c = open(os.path.join(BOOT, 'CONFIG.SYS'), 'rb').read()
nl = b'\r\n' if b'\r\n' in c else b'\n'
anchor = b'menuitem=SAFE, Windows 95 Safe Mode - INBRDPC.SYS loaded, LS-120 and Nero off' + nl
assert c.count(anchor) == 1, 'CONFIG anchor'
c2 = c.replace(anchor, anchor + b'menuitem=DOSNET, DOS only - network agent (no Windows)' + nl)
sec = b'[SAFE]' + nl
assert c2.count(sec) == 1, 'CONFIG section anchor'
c2 = c2.replace(sec, sec + nl + b'[DOSNET]' + nl)
open(os.path.join(S, 'CONFIG.SYS'), 'wb').write(c2)

# AUTOEXEC.BAT: branch before IVT68FIX; the branch runs IVT68FIX itself before Windows.
a = open(os.path.join(BOOT, 'AUTOEXEC.BAT'), 'rb').read()
nl = b'\r\n' if b'\r\n' in a else b'\n'
anchor = b'REM IRET patch from Win 95 build.'
assert a.count(anchor) == 1, 'AUTOEXEC anchor'
a2 = a.replace(anchor, b'IF "%CONFIG%"=="DOSNET" GOTO DOSNET' + nl + anchor)
tail = nl.join([
    b'GOTO END',
    b':DOSNET',
    b'REM DOS-only boot: packet driver, then the V9x DOS agent loop (C:\\V9XREMOT\\OFF stops it).',
    b'C:\\V9XREMOT\\3C5X9PD.COM 0x60',
    b'SET WATTCP.CFG=C:\\V9XREMOT',
    b'PATH C:\\V9XREMOT;%PATH%',
    b'CALL C:\\V9XREMOT\\V9XLOOP.BAT',
    b'REM Errorlevel 4 is Esc at the keyboard: a prompt, not Windows.',
    b'IF ERRORLEVEL 4 COMMAND /K ECHO Agent stopped. V9XLOOP starts it again, EXIT starts Windows.',
    b'REM Only an agent switched off on purpose stops here; a failed start goes on to Windows,',
    b'REM so the card can always be reached over the network.',
    b'IF EXIST C:\\V9XREMOT\\OFF COMMAND /K ECHO Agent off. Type EXIT to start Windows.',
    b'C:\\IVT68FIX.COM',
    b':END',
]) + nl
if not a2.endswith(nl):
    a2 += nl
a2 += tail
open(os.path.join(S, 'AUTOEXEC.BAT'), 'wb').write(a2)

for f in sorted(os.listdir(S)):
    d = open(os.path.join(S, f), 'rb').read()
    bare = d.count(b'\n') - d.count(b'\r\n')
    print(f'{f:14} {len(d):7}  bareLF={bare}')
