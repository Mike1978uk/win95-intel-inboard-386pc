"""Build boot-menu drafts of CONFIG.SYS and AUTOEXEC.BAT from the CF's current files."""
BS = chr(92)
NL = chr(10)
IOSUB = BS + 'WINDOWS' + BS + 'SYSTEM' + BS + 'IOSUBSYS'
NETDRV = 'C:' + BS + 'WINDOWS' + BS + 'SYSTEM' + BS

cfg = open('D:/CONFIG.SYS', 'rb').read().decode('latin1').replace('\r\n', NL).rstrip(NL)
aut = open('D:/AUTOEXEC.BAT', 'rb').read().decode('latin1').replace('\r\n', NL).rstrip(NL)

menu = NL.join([
    '[menu]',
    'menuitem=FULL, Windows 95 - LS-120 and Nero loaded',
    'menuitem=LEAN, Windows 95 - LS-120 and Nero off (less locked RAM)',
    'menuitem=OFFLINE, Windows 95 - network and LS-120 off, Nero on (CD or tape writing)',
    'menuitem=SAFE, Windows 95 Safe Mode - INBRDPC.SYS loaded, LS-120 and Nero off',
    'menudefault=FULL,5',
    '',
    '[common]',
    cfg,
    '',
    '[FULL]',
    '',
    '[LEAN]',
    '',
    '[OFFLINE]',
    '',
    '[SAFE]',
    '',
    '[common]',
])


def on(stem, ext):
    return f'IF EXIST {stem}.OFF IF NOT EXIST {stem}.{ext} REN {stem}.OFF {stem}.{ext}'


def off(stem, ext):
    return f'IF EXIST {stem}.{ext} REN {stem}.{ext} {stem}.OFF'


block = NL.join([
    'REM Boot-menu choice from CONFIG.SYS. A driver renamed to .OFF does not load.',
    'C:',
    'CD ' + IOSUB,
    'REM Test, not GOTO %CONFIG%: an unset CONFIG would end the batch before IVT68FIX.',
    'IF "%CONFIG%"=="LEAN" GOTO LEAN',
    'IF "%CONFIG%"=="SAFE" GOTO LEAN',
    'IF "%CONFIG%"=="OFFLINE" GOTO OFFLINE',
    ':FULL',
    on('SD120PPD', 'MPD'),
    on('NEROCD95', 'VXD'),
    'GOTO NETON',
    ':LEAN',
    off('SD120PPD', 'MPD'),
    off('NEROCD95', 'VXD'),
    'GOTO NETON',
    ':OFFLINE',
    off('SD120PPD', 'MPD'),
    on('NEROCD95', 'VXD'),
    'REM Without the card driver Windows loads none of TCP/IP or NetBIOS over TCP: ~200 KB locked.',
    'IF EXIST ' + NETDRV + 'ELNK3.VXD REN ' + NETDRV + 'ELNK3.VXD ELNK3.OFF',
    'GOTO MENUDONE',
    ':NETON',
    'IF EXIST ' + NETDRV + 'ELNK3.OFF IF NOT EXIST ' + NETDRV + 'ELNK3.VXD REN ' + NETDRV + 'ELNK3.OFF ELNK3.VXD',
    ':MENUDONE',
    'CD ' + BS,
    '',
    '',
])

anchor = 'REM INT 15h AH=86h wait'
assert aut.count(anchor) == 1
aut2 = aut.replace(anchor, block + anchor)

# F5 Safe Mode skips CONFIG.SYS, so INBRDPC.SYS never loads and Windows has no
# extended memory. Starting WIN /D:M ourselves keeps CONFIG.SYS and gets Safe Mode.
last = 'C:' + BS + 'IVT68FIX.COM'
assert aut2.count(last) == 1
old_rem = 'REM IRET patch from Win 95 build. Must stay the LAST line - technique 38.'
assert aut2.count(old_rem) == 1
aut2 = aut2.replace(old_rem, 'REM IRET patch from Win 95 build. Must be the last command before Windows - technique 38.')
aut2 = aut2.replace(last, NL.join([
    last,
    'REM Safe Mode after a normal CONFIG.SYS; WIN must follow IVT68FIX directly.',
    'IF "%CONFIG%"=="SAFE" WIN /D:M',
]))

for name, text in (('CONFIG.SYS', menu), ('AUTOEXEC.BAT', aut2)):
    with open('tools/bootmenu/' + name, 'wb') as f:
        f.write((text.rstrip(NL) + NL).replace(NL, '\r\n').encode('latin1'))
print('written')
