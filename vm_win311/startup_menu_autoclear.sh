#!/bin/bash
# Technique 23: auto-clear the Win95 Startup Menu (forced by repeated VM kills) before its
# ~13s countdown defaults to Safe Mode (known broken on this hardware). Watches only the
# LATEST vram_dump.txt snapshot (last 26 lines: 1 header + 25 rows) to avoid the stale-content
# false-positive Technique 23 already documented.
#
# 2026-08-02 fix: the first version of this script only sent the '1' digit (scancode 2) and
# never pressed Enter, so it typed "1" into the "Enter a choice:" prompt and then sat there
# forever re-typing the same digit every 2s without ever submitting it - the VM was
# discovered stuck at this exact menu for 60+ real seconds before being caught live. Digit
# then Enter (scancode 28), with a state flag so it only fires once per menu appearance
# (re-arms once the menu text is confirmed gone from the latest snapshot) instead of
# spamming keystrokes at a screen that already moved on.
cd "$(dirname "$0")"
handled=0
for i in $(seq 1 600); do
    if tail -n 26 vram_dump.txt 2>/dev/null | grep -q "Enter a choice"; then
        if [ "$handled" -eq 0 ]; then
            echo "1" > inject_key.txt
            sleep 1.5
            echo "28" > inject_key.txt
            echo "[autoclear] fired at iter $i (sent 1 + Enter)"
            handled=1
        fi
    else
        handled=0
    fi
    sleep 2
done
