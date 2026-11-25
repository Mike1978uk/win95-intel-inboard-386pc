# SBDS census, Doom 95 on the bed (2026-11-25)

Stage 1 of `docs/sbds_mixer_plan.md`. Bed `vm_5160_dbg` (387 fitted), SBDS trace build
(`drivers/sbds/build/SBDS.VXD`, md5 `5be78c6c`), stock DSOUND. Owner played Doom 95 with sound, quit,
`regedit /e C:\SBDS.REG HKEY_LOCAL_MACHINE\Software\SBDS`. Decode: `trace.txt`
(`python drivers/sbds/sbds_trace.py SBDS_bed_doom95.REG`). No hang, no Msgsrv32 fault.

- Doom 95 sets the primary format itself: **11,025 Hz 8-bit stereo** (call 11), half the bytes of
  DirectSound's 22 kHz default. Cursor advance confirms it: ~2,100 B per ~96 ms = 22 KB/s.
- After Play, **56 `Drv.GetCaps`** and nothing else until the polls: DSOUND asks our caps each time
  Doom creates or duplicates a sound buffer, to decide hardware or software placement. With no
  hardware channels offered, every effect goes to DSOUND's software mixer.
- Pan and volume are not visible at this level: secondaries never reach SBDS today. They will once
  SBDS offers channels (stage 3); the trace build will then show SetVolumePan.
- GetPosition polled every 25-100 ms (first 32 per Play only, by the trace design).
- The logged polls start ~2,867 s (48 min) after Play. The owner was away on calls with the bed running,
  so this may be real: DSOUND polls only while a sound plays, and the primary ran ~48 min with
  nothing polling it. Not proven; worth one timed DSTEST to confirm the trace's time base.

Compare M8DDB on the card (11-24, `../2026-11-24_sbds_dxdiag/SBDSM_card_m8ddb.REG`): one run left
22 kHz stereo, the other set 22 kHz mono. So programs set the primary format themselves; the mixer
must honour an app-set format and only choose mono/stereo where the app has not.
