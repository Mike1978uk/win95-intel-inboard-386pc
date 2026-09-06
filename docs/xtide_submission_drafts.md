# XTIDE Universal BIOS — submission drafts

Written 2026-09-06. **Nothing has been sent.** These are drafts for the owner to review, edit and
send himself; posting under his own account is what makes the attribution inherent.

Background and venue reasoning: `docs/next_session_2026_09_07.md`, "Open thread".

---

## Before sending — three things to confirm

1. **The thread URL.** The XUB front page names a Vintage Computer Forums thread as its discussion
   venue; that URL is not recorded here, so pick it up from the site rather than guessing. A second,
   arguably better target already exists and is recorded:
   [VCFed, "XTIDE and Windows 95 issues"](https://forum.vcfed.org/index.php?threads/xtide-and-windows-95-issues.52115/)
   — three pages of people asking this exact question and getting no answer. Posting the answer
   *there* is a reply to a question, not an announcement. Consider both: the XUB thread for reach,
   52115 for relevance.
2. **The addresses.** `aitotat@gmail.com` (Tomi Tilli) and `krille_n_@hotmail.com` (Krille), taken
   from the XUB front page. Re-read the page before sending in case they have changed.
3. **The download links resolve.** Paste them into a browser first — they are raw links into
   `master`, so they break if `dist/xtide_mpd/` moves.

Not a venue: `xtideuniversalbios.org` *View Tickets → Reports*. That is a Trac query against their
**BIOS** bug tracker; a Windows 95 driver does not belong in it.

---

## Draft 1 — forum post

> **Subject:** Windows 95 32-bit disk driver for XT-IDE / XT-CF — working here, and it needs
> stride-1 testers

Windows 95 has no 32-bit driver for an XT-IDE class controller, so it falls back to the real-mode
BIOS and every drive on the card runs in MS-DOS compatibility mode. I have written one, and it now
runs on real hardware. It is MIT licensed and I would like people with other XT-IDE cards to break
it.

**What it is.** `XTIDEMP.MPD`, a Windows 95 SCSI miniport — SCSIPORT owns the polling, the DCB
lifecycle and scatter/gather, and the driver supplies only the SRB-to-taskfile dispatch. It is
PIO only and never touches the 8237, because on an XT the free DMA channels are not free.

**What it is not.** It is not a change to the XTIDE Universal BIOS and it does not need one. XUB
still boots the machine, still owns the card's base address, and the driver simply takes the disk
over once Windows is in protected mode. It reads its I/O base from a string in the device node —
`PORT=0x300`, editable on the controller's Settings tab — and never probes for the card, which also
means it cannot walk into anything else's registers looking for it.

**Where it has run.** One machine and one card: an IBM 5160 with an Intel Inboard 386/PC, a Lo-tech
XT-CF rev 3 at `0x300`, register stride 2, 8-bit PIO, no high-byte latch, no IRQ. Windows 95 OSR1.
From `BOOTLOG.TXT`, read off the card afterwards rather than off the screen:

```
[001612B7] Initing xtidemp.mpd
[001612CE] Init Success xtidemp.mpd
[0016136F] INITCOMPLETESUCCESS = DiskTSD
[00161372] INITCOMPLETESUCCESS = SCSIPORT
           rmm.pdr  Dynamic load success ... never reaches INITCOMPLETE
```

That last line is the point: `RMM.PDR` is the real-mode mapper, and it finding nothing to claim is
the boot disk having been taken over. Desktop reached, `C:` navigable, no bang in Device Manager,
clean shutdown.

**What has not been tested, and this is the ask.** The shipped binary autodetects the register
stride and the data transport, so both code paths are live in the file you would download — but
only stride 2 with 8-bit PIO has ever been executed, by anyone. **Stride 1 (a stock XT-IDE card)
and the 16-bit high-byte-latch transport are written and unproven.** The stride probe is
deliberately read-only — it compares two ports that must hold the same register on a correct map
and writes nothing — because an earlier write-based probe would have asserted `SRST` on stride-1
hardware and hung somebody else's drive.

Also untested: sustained write load, and anything other than a CF card on the far end. Completion
is currently synchronous, so a heavy flush blocks the machine for its duration; that is a known
defect with a known fix, not a mystery.

If you have an XT-IDE, an XT-IDE rev 2, an XT-CFv3, a JR-IDE or anything else XUB drives, and a
Windows 95 install you do not mind reinstalling, I would like to know what happens. What is useful
back:

- the card and its jumpered base;
- `BOOTLOG.TXT` — specifically whether `Init Success xtidemp.mpd` appears, and what `rmm.pdr` does;
- whether `C:` still reads correctly afterwards.

Delete `BOOTLOG.TXT` before the run. A stale one names a driver from two images ago and reads
exactly like a result.

**Licensing.** MIT. It is hand-written MASM and contains no XUB source, so there is no GPL
entanglement in either direction — I mention it explicitly rather than leaving it to be assumed.
The register map was cross-checked against 86Box's `hdc_xtide.c` and against the XT-IDE port in
`patacd.asm`; both are by the same author, so that is a consistency check, not independent
confirmation. If it disagrees with the hardware you have, the hardware wins and I would like to
hear about it.

Driver, INF, full source and the install steps:
<https://github.com/Mike1978uk/win95-intel-inboard-386pc> — `drivers/xtide_mpd/`, download links in
`FIXES.md`. `XTIDEMP.MPD` is 10,752 bytes, md5 `561fb45b598ef5985e5a803016321f76`.

One prerequisite worth naming, because it will bite anyone whose `CONFIG.SYS` still loads a
real-mode driver IOS does not recognise: that driver has to appear in `[SafeList]` in
`WINDOWS\IOS.INI`, or IOS declines every miniport on the machine. On mine that is the Inboard's
`INBRDPC.SYS`.

---

## Draft 2 — email to the maintainers

> **To:** aitotat@gmail.com, krille_n_@hotmail.com
> **Subject:** Windows 95 32-bit miniport for XT-IDE cards — MIT, and a question about geometry

Tomi, Krille,

I have written a Windows 95 SCSI miniport that drives an XT-IDE class controller in protected mode,
so the disk leaves MS-DOS compatibility mode. It is confirmed on real hardware — an IBM 5160 with
an Intel Inboard 386/PC and a Lo-tech XT-CF rev 3 at `0x300`, stride 2, 8-bit PIO — and it is MIT
licensed.

It requires no change to XUB and no XUB source is used in it. XUB boots the machine and owns the
card's base; the driver reads that base from a string in the Windows device node and takes the disk
over once Windows is in protected mode. `RMM.PDR` loads and never reaches `INITCOMPLETE`, which is
the real-mode mapper finding the boot volume already claimed.

Three things I am writing to you about.

**1. A link, if you think it is worth one.** People turn up asking whether XT-IDE can run 32-bit
disk access under Windows 95, and until now the honest answer was no. A line on the wiki or the
site pointing at the repository would put the answer where they look. The licence is MIT, so the
only thing I ask in return is that the credit and the link stay attached.

**2. Testers for the paths I cannot reach.** The binary autodetects register stride and data
transport, and both paths ship live — but only stride 2 with 8-bit PIO has ever been executed. A
stock XT-IDE at stride 1, and any card with the 16-bit high-byte latch, are written and unproven. I
have exactly one card. If you know who to point at it, that is worth more to me than anything else
in this email. The stride probe is read-only by design: it compares two ports that must alias on a
correct map and writes nothing, precisely so that a wrong guess cannot assert `SRST` on someone
else's drive.

**3. A question I would rather ask than assume.** The driver addresses in LBA28 where the drive
reports it, and falls back to CHS built from the drive's own `IDENTIFY` geometry — it never reads
XUB's translated geometry. On my card that is consistent and the partition table lines up. Is there
a configuration where XUB's translation and the drive's own reported geometry diverge enough that a
protected-mode driver ignoring the translation would land in the wrong place? A CHS-only drive
behind a translating BIOS is the case I am unsure of.

Repository, with the source, the INF, the boot logs and the install sequence:
<https://github.com/Mike1978uk/win95-intel-inboard-386pc> — the driver is in `drivers/xtide_mpd/`.
`XTIDEMP.MPD`, 10,752 bytes, md5 `561fb45b598ef5985e5a803016321f76`.

I am happy to take corrections. The register map was cross-checked only against 86Box's
`hdc_xtide.c` and the XT-IDE port in `patacd.asm`, which share an author, so it has never been
checked against anything independent of that lineage. You would be the first.

Thank you for XUB — this machine boots from a CF card because of it.

Mike Lycett

---

## Notes on both drafts

- The pitch is a request, not an advert. The genuinely useful thing that community has and this
  project does not is **other cards**; asking for that is what makes it a collaboration.
- The GPL/MIT point is stated outright in both. XUB is GPL-2.0 and this driver is independent of
  it, but leaving that implicit invites the question at the worst moment.
- Both drafts name what is untested. Overclaiming to a maintainer costs the link and the testers.
- The `patacd.asm` / 86Box shared-author caveat is in both deliberately: it is recorded in
  `docs/resources_and_sources.md` as a known limit of the cross-check, and these are the people
  best placed to close it.
