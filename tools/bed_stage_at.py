#!/usr/bin/env python3
r"""Stage a bed from the known-good Windows 95 AT image, with parallel-port drives.

  python tools/bed_stage_at.py <bed dir> [--lpt1 DEV] [--lpt2 DEV] [--dos]

DEV is one of:
  ls120-dos   LS-120 (SuperDisk 120), vendor DOS drivers: SD120PPD.SYS with the
              owner's switch line, then ASPIHDRM.SYS for the drive letter
  backpack    BackPack CD-ROM as a Toshiba XM-1502B, vendor DOS driver
              BPCDDRV.SYS + MSCDEX
--dos         boot to the DOS prompt instead of Windows (BootGUI=0)

The image is a fresh copy of vm_3c509b/at95/win95_at.img (past first-run
setup; win95_at_master.img is not). The config is at95's own with no SCSI card,
no CD-ROM or removable disk except those asked for, and no uuid (a copied uuid
makes 86Box stop on "moved or copied"). Media sit in the bed under names that
say what they are. Then launch with tools/bed_launch.ps1 -AllowMouse.
Inboard-hw-debug technique 132 lists the traps this replaces.
"""
import argparse, os, re, shutil, subprocess, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, 'vm_3c509b', 'at95')
BPCK_IMG = os.path.join(REPO, 'vm_bpck', 'dos.img')
LS_IMG = os.path.join(REPO, 'vm_ls120win', 'ls120win_clean.img')
LS_MEDIA = os.path.join(REPO, 'vm_ls120win', 'rd_verified_good.img')
DISC = os.path.join(REPO, 'vm_bpck', 'g4', 'A', 'win98se.iso')
SD_LINE = 'DEVICEHIGH=C:\\SD120PPD\\SD120PPD.SYS /port:{port} /IRQ:7 /de /db /ni /sf /dpc /dp /fp'
PORTS = {1: '378', 2: '278'}

ap = argparse.ArgumentParser()
ap.add_argument('bed')
ap.add_argument('--lpt1', choices=['ls120-dos', 'backpack'])
ap.add_argument('--lpt2', choices=['ls120-dos', 'backpack'])
ap.add_argument('--dos', action='store_true')
args = ap.parse_args()
devs = {n: d for n, d in ((1, args.lpt1), (2, args.lpt2)) if d}
if not devs or len(set(devs.values())) != len(devs):
    sys.exit('give --lpt1 and/or --lpt2, each device at most once')

bed = os.path.abspath(args.bed)
img = os.path.join(bed, 'win95_at.img')
tmp = os.path.join(bed, '_stage')
os.makedirs(tmp, exist_ok=True)
os.makedirs(os.path.join(bed, 'nvr'), exist_ok=True)


def run(*a):
    r = subprocess.run([sys.executable] + list(a), capture_output=True, text=True)
    if r.returncode:
        sys.exit('FAILED: %s\n%s%s' % (' '.join(a), r.stdout, r.stderr))
    return r.stdout


def get(image, path, name):
    out = os.path.join(tmp, name)
    run(os.path.join(REPO, 'tools', 'fatls.py'), image, '--get', path, out)
    return out


def put(path, local, *extra):
    print(run(os.path.join(REPO, 'tools', 'fatcp.py'), img, path, local, '--yes', *extra).strip().splitlines()[-1])


def crlf(b):
    return b.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')


def edit(path, name, fn, *extra):
    text = open(get(img, path, name), 'rb').read().decode('latin-1')
    text = fn(text)
    new = os.path.join(tmp, name + '.new')
    open(new, 'wb').write(crlf(text.encode('latin-1')))
    put(path.replace('\\', '/'), new, *extra)


shutil.copyfile(os.path.join(SRC, 'win95_at.img'), img)
for f in os.listdir(os.path.join(SRC, 'nvr')):
    shutil.copyfile(os.path.join(SRC, 'nvr', f), os.path.join(bed, 'nvr', f))

# 86box.cfg
cfg = open(os.path.join(SRC, '86box.cfg'), encoding='utf-8-sig').read().splitlines()
drop = re.compile(r'\s*(uuid|scsicard_|cdrom_0\d_|rdisk_0\d_|lpt\d_enabled)')
cd, rd = [], []
for port, d in devs.items():
    if d == 'backpack':
        shutil.copyfile(DISC, os.path.join(bed, 'backpack_disc.iso'))
        cd = ['cdrom_01_parameters = 1, lpt', 'cdrom_01_lpt_port = %d' % (port - 1),
              'cdrom_01_type = toshiba_1502b', 'cdrom_01_image_path = backpack_disc.iso',
              'cdrom_01_speed = 10']
    else:
        shutil.copyfile(LS_MEDIA, os.path.join(bed, 'ls120_media.img'))
        rd = ['rdisk_01_parameters = 7, lpt', 'rdisk_01_lpt_port = %d' % (port - 1),
              'rdisk_01_image_path = ls120_media.img']
out = []
for l in cfg:
    if drop.match(l):
        continue
    out.append(l)
    if l.strip() == '[Floppy and CD-ROM drives]':
        out += cd
    if l.strip() == '[Other removable devices]':
        out += rd
if 2 in devs:
    out += ['', '[Ports (COM & LPT)]', 'lpt2_enabled = 1']
assert not any('scsicard' in l for l in out)
assert sum(l.startswith('cdrom_01_') for l in out) == len(cd)
assert sum(l.startswith('rdisk_01_') for l in out) == len(rd)
open(os.path.join(bed, '86box.cfg'), 'w', encoding='utf-8').write('\n'.join(out) + '\n')

# Drivers, loaded the way a user loads them.
config_add, auto_add = [], []
for port, d in devs.items():
    if d == 'backpack':
        put('BPCDDRV.SYS', get(BPCK_IMG, 'BPCDDRV.SYS', 'BPCDDRV.SYS'))
        config_add.append('DEVICE=C:\\BPCDDRV.SYS /d:bpcddrv$')
        auto_add.append('C:\\WINDOWS\\COMMAND\\MSCDEX.EXE /D:BPCDDRV$')
    else:
        # The stock vendor package: SD120PPD.SYS (md5 cbb42e8e) is the transport,
        # ASPIHDRM.SYS gives the disk its DOS drive letter. Both, in that order.
        run(os.path.join(REPO, 'tools', 'fatcp.py'), img, '--mkdir', 'SD120PPD')
        for f in ('SD120PPD.SYS', 'ASPIHDRM.SYS'):
            put('SD120PPD/' + f, get(LS_IMG, 'SD120PPD\\' + f, f))
        config_add.append(SD_LINE.format(port=PORTS[port]))
        config_add.append('DEVICEHIGH=C:\\SD120PPD\\ASPIHDRM.SYS')

edit('CONFIG.SYS', 'CONFIG.SYS',
     lambda t: t.rstrip('\r\n\x1a') + '\n' + '\n'.join(config_add) + '\n')
if auto_add:
    edit('AUTOEXEC.BAT', 'AUTOEXEC.BAT',
         lambda t: t.rstrip('\r\n\x1a') + '\n' + '\n'.join(auto_add) + '\n')


def msdos(t):
    for key, val in (('AutoScan', '0'), ('BootGUI', '0' if args.dos else '1')):
        if re.search(r'(?im)^%s=' % key, t):
            t = re.sub(r'(?im)^%s=.*$' % key, lambda _: '%s=%s' % (key, val), t)
        else:
            t = re.sub(r'(?im)^\[Options\]\r?\n', lambda _: '[Options]\r\n%s=%s\r\n' % (key, val), t, count=1)
    assert re.search(r'(?im)^AutoScan=0', t)
    return t


edit('MSDOS.SYS', 'MSDOS.SYS', msdos, '--attr', 'rhs')
shutil.rmtree(tmp)
print('staged %s: %s%s' % (bed, ', '.join('LPT%d %s' % kv for kv in devs.items()), ' (boots to DOS)' if args.dos else ''))
