#!/usr/bin/env python3
r"""REM out (or restore) a CONFIG.SYS / AUTOEXEC.BAT line inside a raw image.

  python tools/rem_config_line.py <image> <path-in-image> --rem <substring> [--note "why"]
  python tools/rem_config_line.py <image> <path-in-image> --unrem <substring>
  python tools/rem_config_line.py <image> <path-in-image> --show

Exists because the storage tests on this project keep coming down to "which
real-mode drivers are loaded" (technique 86 - IOS refuses to initialise a port
driver while an unsafe real-mode block driver holds the units), and editing
those lines by hand has twice produced a silent no-op:

  * a file written from bash is LF, and COMMAND.COM cannot parse it. Every write
    here is forced back to CRLF and the CR count is asserted against the line
    count before the image is touched (technique 75).
  * a substring that matches nothing produces a "successful" run that changed
    nothing. --rem and --unrem both refuse to write unless they matched
    (technique 28).

Always prints the resulting file so the change can be read, not assumed.
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
CRLF = chr(13) + chr(10)
LF = chr(10)


def run(*args):
    r = subprocess.run([sys.executable] + list(args), capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit((r.stdout or '') + (r.stderr or ''))
    return r.stdout


def main():
    img, path = sys.argv[1], sys.argv[2]
    mode = sys.argv[3] if len(sys.argv) > 3 else '--show'
    needle = sys.argv[4] if len(sys.argv) > 4 else None
    note = ''
    if '--note' in sys.argv:
        note = sys.argv[sys.argv.index('--note') + 1]

    tmp = os.path.join(tempfile.gettempdir(), 'cfg_edit.tmp')
    run(os.path.join(HERE, 'fatls.py'), img, '--get', path, tmp)
    raw = open(tmp, 'rb').read()
    text = raw.decode('latin1').replace(CRLF, LF)
    lines = text.split(LF)

    if mode == '--show':
        for i, l in enumerate(lines, 1):
            print('%3d  %s' % (i, l))
        return

    hits = 0
    out = []
    for l in lines:
        if needle.lower() in l.lower():
            stripped = l.lstrip()
            is_rem = stripped.upper().startswith('REM ')
            if mode == '--rem' and not is_rem:
                suffix = ('  REM ' + note) if note else ''
                l = 'REM ' + l + suffix
                hits += 1
            elif mode == '--unrem' and is_rem:
                l = stripped[4:]
                hits += 1
        out.append(l)

    if hits == 0:
        raise SystemExit('MATCHED NOTHING for %r - refusing to write (technique 28)' % needle)

    data = CRLF.join(out).encode('latin1')
    assert data.count(chr(13).encode('latin1')) == len(out), 'not fully CRLF'
    open(tmp, 'wb').write(data)
    print(run(os.path.join(HERE, 'fatcp.py'), img, path, tmp, '--yes'))
    print('--- %s now ---' % path)
    for i, l in enumerate(out, 1):
        print('%3d  %s' % (i, l))
    print('changed %d line(s)' % hits)


if __name__ == '__main__':
    main()
