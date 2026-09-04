import sys, re, collections

# Which AEP function codes does a driver's AER actually test for?
# 32-bit VxD code compares the function code in a 16-bit register:
#   66 83 F8 nn   cmp ax, nn
#   66 83 FE nn   cmp si, nn
#   66 3D nn 00   cmp ax, nnnn
#   66 81 FE nn 00 cmp si, nnnn
NAMES = {0: 'INITIALIZE', 1: 'SYS_CRIT_SHUTDOWN', 2: 'BOOT_COMPLETE', 3: 'CONFIG_DCB',
         4: 'UNCONFIG_DCB', 5: 'IOP_TIMEOUT', 6: 'DEVICE_INQUIRY', 7: 'HALF_SEC',
         8: '1_SEC', 9: '2_SECS', 10: '4_SECS', 11: 'DBG_DOT_CMD', 12: 'ASSOCIATE_DCB',
         13: 'REAL_MODE_HANDOFF', 14: 'SYSTEM_SHUTDOWN', 15: 'UNINITIALIZE',
         16: 'DCB_LOCK', 17: 'MOUNT_NOTIFY', 18: 'CREATE_VRP', 19: 'DESTROY_VRP',
         20: 'REFRESH_DRIVE', 21: 'PEND_UNCONFIG_DCB', 22: '1E_VEC_UPDATE',
         23: 'CHANGE_RPM'}

PATS = [(re.compile(rb'\x66\x83[\xf8\xfe\xf9\xfa\xfb\xfd]([\x00-\x17])', re.S), 'cmp r16,imm8'),
        (re.compile(rb'\x66\x3d([\x00-\x17])\x00', re.S), 'cmp ax,imm16'),
        (re.compile(rb'\x66\x81[\xf8\xfe]([\x00-\x17])\x00', re.S), 'cmp r16,imm16')]

for path in sys.argv[1:]:
    d = open(path, 'rb').read()
    hits = collections.Counter()
    for rx, _ in PATS:
        for m in rx.finditer(d):
            hits[m.group(1)[0]] += 1
    name = path.split('/')[-1].split('\\')[-1]
    print('== %s' % name)
    codes = sorted(hits)
    print('   tests for: ' + ', '.join('%d=%s(x%d)' % (c, NAMES.get(c, '?'), hits[c])
                                       for c in codes))
