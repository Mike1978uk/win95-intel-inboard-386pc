# XTIDE Universal BIOS — submission drafts

Rewritten 2026-09-07. The 2026-09-06 drafts described the untested path as "stride 1, i.e. a stock
XT-IDE card". That was wrong in a way worth correcting before anything went out — see
[`xtide_register_maps.md`](xtide_register_maps.md). There are **three** maps, our driver expresses
two of them, and the one it cannot express is the **default on every XT-IDE Rev 2/3/4**.

Venue reasoning: [`next_session_2026_09_07.md`](next_session_2026_09_07.md), "Open thread".

---

## Pre-send checks — all three closed 2026-09-07

1. ✅ **Venue.** The XUB front page's own link
   (`vcfed.org/forum/showthread.php?17986`) **404s** since VCFed's move to XenForo. The live thread
   is [XTIDE Universal BIOS](https://forum.vcfed.org/index.php?threads/xtide-universal-bios.18240/)
   — 39 pages, started by aitotat in 2009, and **Krille was posting in it in February 2026**.
   Secondary venue, agreed with the owner: the older
   [XTIDE and Windows 95 issues](https://forum.vcfed.org/index.php?threads/xtide-and-windows-95-issues.52115/)
   thread — last reply 2018, but it is what people find by search, and it is how the owner found it.
2. ✅ **Addresses re-read from the XUB front page**, unchanged: `aitotat@gmail.com` (Tomi Tilli) and
   `krille_n_@hotmail.com` (Krille). Wording there: *"you can also send email to aitotat (at)
   gmail.com or krille_n_ (at) hotmail.com."*
3. ✅ **Download link resolves and serves the tested bytes.** The public raw link returns md5
   `561fb45b598ef5985e5a803016321f76`, matching `dist/xtide_mpd/XTIDEMP.MPD` and the build copy.
   10,752 bytes. Checked by fetching it, not by loading the page.

Not a venue: `xtideuniversalbios.org` *View Tickets → Reports*. Trac queries against their **BIOS**
tracker; a Windows 95 driver is a category error there.

---

## Draft 1 — the XUB thread (18240)

> **Windows 95 32-bit disk driver for XT-IDE class cards — working here, and I need testers for the
> other register maps**

Windows 95 has no 32-bit driver for an XT-IDE class controller, so it falls back to the real-mode
BIOS and every drive on the card runs in MS-DOS compatibility mode. I have written one and it now
runs on real hardware. It is MIT licensed, and I would like people with other XT-IDE cards to break
it.

**What it is.** `XTIDEMP.MPD`, a Windows 95 SCSI miniport. SCSIPORT owns the polling, the DCB
lifecycle and scatter/gather; the driver supplies only the SRB-to-taskfile dispatch. PIO only — it
never touches the 8237, because on an XT the free DMA channels are not free.

**What it is not.** It is not a change to XUB and does not need one. XUB still boots the machine and
owns the card's base address. The driver reads that base from a string in the Windows device node
(`PORT=0x300`, editable on the controller's Settings tab), never probes for the card, and takes the
disk over once Windows is in protected mode.

**Where it has run.** One machine, one card: an IBM 5160 with an Intel Inboard 386/PC, a Lo-tech
XT-CF rev 3 at `0x300`, 8-bit PIO, no IRQ, Windows 95 OSR1. From `BOOTLOG.TXT`, read off the card
afterwards rather than off the screen:

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

**Now the part I had wrong until yesterday, which is why I am posting rather than just announcing.**

My driver computes a register's port as `base + index * stride`, stride 1 or 2. Stride 2 is what my
card needs — A0 is not decoded on it, so register N sits at `base + 2N`. I measured that by
write/readback rather than assuming it.

Reading the [register map on minuszerodegrees](https://minuszerodegrees.net/xtide/XT-IDE%20-%20Register%20map.jpg),
that is **not** the map an XT-IDE Rev 2/3/4 presents in Hi-Speed mode. That map comes from swapping
A3 and A0, which *permutes* the registers rather than scaling them — sector count `302`, LBA mid
`304`, drive/head `306`, error/features `308`, LBA low `30A`, LBA high `30C`, status `30E`. It
agrees with mine on data-low and status and on nothing else. A permutation is not a stride, so my
driver cannot express it at all.

So, plainly:

- **Compatibility mode** — Rev 1, and Rev 2/3/4 switched into it, XUB device type `XTIDE rev1` — is
  my stride 1, with the high data byte at `base+8`. It is in the shipped binary and **has never been
  executed by anyone**.
- **Hi-Speed mode is not implemented.** If you run my driver on a Hi-Speed card I expect it to
  *decline* the card rather than misdrive it. I settle the map by reading Status and Alternate
  Status — the same register on any correct ATA map — and comparing them. Nothing is written, so a
  wrong guess cannot assert `SRST` and hang your drive. On Hi-Speed, stride 1 should mismatch, and
  stride 2 should read `base+1Ch` outside the decode and get `FFh`, which I reject.

The case I cannot rule out from here is a card whose decode is partial enough that `base+1Ch`
aliases back inside the window. This machine's own 8259 answers across `0x20-0x3F` for exactly that
reason, so I do not treat partial decode as unlikely — I just have no card to try it on.

**What I am asking for.**

1. **Compatibility mode on any Rev 2/3/4 card.** A jumper and a BIOS device-type setting, on
   hardware plenty of people here have, exercising a path that has never run.
2. **Whether my reading of the Hi-Speed map is right.** If it is, supporting it is a lookup table
   instead of a multiply and I will do it — I just cannot test it.
3. **Anything that is not a CF card on the far end.** Also untested: sustained write load.
   Completion is currently synchronous, so a heavy flush blocks the machine for its duration. Known
   defect, known fix.

If you try it, what is useful back is the card and its jumpered base, `BOOTLOG.TXT` — specifically
whether `Init Success xtidemp.mpd` appears and what `rmm.pdr` does — and whether `C:` still reads
correctly afterwards. **Delete `BOOTLOG.TXT` before the run.** A stale one names a driver from two
images ago and reads exactly like a result.

**How you install it**, because it is not where you would look. It presents as a SCSI
controller, not an IDE one — SCSIPORT is the framework, so that is the class it lives in.

1. Put `XTIDEMP.MPD` and `XTIDEMP.INF` in a directory on the hard disk. Not on a floppy: on a
   machine with no floppy controller installed, typing `A:\` into the Have Disk dialog hangs it.
2. Control Panel → Add New Hardware → **No**, do not let it autodetect → **SCSI controllers** →
   Have Disk, and type the path by hand.
3. Pick *Lo-tech XT-CF / XT-IDE 8-bit disk controller (polled)*.
4. If a resource conflict is offered, **do not force the configuration** — let Windows assign it.
5. Device Manager → the controller → **Settings** must read `PORT=0x300`, **or whatever base your
   own card is jumpered to and XUB is configured for.** This is the one field you have to get
   right: the driver takes its base from that string and never probes, so a wrong value means it
   simply finds nothing.
6. Reboot. Then check `BOOTLOG.TXT` for `Init Success xtidemp.mpd` — that it installed and that it
   *ran* are different things.

If you already have an older XT-IDE **port driver** (`PORT.PDR`) installed, remove its node and
delete the file first. It installs under class `hdc`, claims the same I/O range, and two nodes on
`300-31F` conflict.

**Licensing.** MIT. Hand-written MASM, no XUB source, so no GPL entanglement in either direction — I
would rather say that outright than leave it assumed. The register map was cross-checked against
86Box's `hdc_xtide.c` and the XT-IDE port in `patacd.asm`, which share an author, so that is a
consistency check and not independent confirmation.

Driver, INF, full source, boot logs and install steps:
https://github.com/Mike1978uk/win95-intel-inboard-386pc — `drivers/xtide_mpd/`, downloads in
`FIXES.md`. `XTIDEMP.MPD` is 10,752 bytes, md5 `561fb45b598ef5985e5a803016321f76`.

One prerequisite that will bite anyone whose `CONFIG.SYS` still loads a real-mode driver IOS does
not recognise: that driver has to appear in `[SafeList]` in `WINDOWS\IOS.INI`, or IOS declines every
miniport on the machine. On mine that is the Inboard's `INBRDPC.SYS`.

---

## Draft 2 — reply on thread 52115, "XTIDE and Windows 95 issues"

Short on purpose. It is an eight-year-old thread; the job is to answer the question and point at the
live one, not to repeat the whole post.

> Reviving this because it was never actually answered, and I have an answer now.
>
> The first post here asks why the disk runs in "dos compatibility mode" with XTIDE. The reason is
> that Windows 95 ships no 32-bit driver for an XT-IDE class controller, so it falls back to the
> real-mode BIOS — nothing is misconfigured, the driver simply did not exist.
>
> I have written one. `XTIDEMP.MPD` is a Windows 95 SCSI miniport that takes the disk over in
> protected mode; on my IBM 5160 with a Lo-tech XT-CF rev 3 the boot disk now leaves compatibility
> mode, with `RMM.PDR` loading and never reaching `INITCOMPLETE`. MIT licensed, source and boot logs
> in the repo: https://github.com/Mike1978uk/win95-intel-inboard-386pc (`drivers/xtide_mpd/`).
>
> It installs as a SCSI controller via Add New Hardware → Have Disk, not as an IDE one, and the
> `PORT=` value on its Settings tab has to match your card's base.
>
> It has only ever run on one card and one register map. It does **not** implement the Hi-Speed map
> that Rev 2/3/4 cards use by default, though it should decline such a card rather than misdrive it.
> Details and the request for testers are in the main XTIDE Universal BIOS thread:
> https://forum.vcfed.org/index.php?threads/xtide-universal-bios.18240/
>
> The other problem in the first post — the 3C509 driver reading the XT-IDE option ROM as if it were
> a network boot ROM — I have not solved and am not claiming to.

---

## Draft 3 — email to Tomi Tilli and Krille

> **To:** aitotat@gmail.com, krille_n_@hotmail.com
> **Subject:** Windows 95 32-bit miniport for XT-IDE cards — MIT, and a register-map question

Tomi, Krille,

I have written a Windows 95 SCSI miniport that drives an XT-IDE class controller in protected mode,
so the disk leaves MS-DOS compatibility mode. It is confirmed on real hardware — an IBM 5160 with an
Intel Inboard 386/PC and a Lo-tech XT-CF rev 3 at `0x300` — and it is MIT licensed.

It requires no change to XUB and uses no XUB source. XUB boots the machine and owns the card's base;
the driver reads that base from a string in the Windows device node and takes the disk over once
Windows is in protected mode. `RMM.PDR` loads and never reaches `INITCOMPLETE`, which is the
real-mode mapper finding the boot volume already claimed.

Three things, and the third is the one I would most like an answer to.

**1. A link, if you think it is worth one.** People turn up asking whether XT-IDE can do 32-bit disk
access under Windows 95, and until now the honest answer was no. A line on the wiki or the site
pointing at the repository would put the answer where they look. The licence is MIT, so the only
thing I ask back is that the credit and the link stay attached.

**2. Testers for the maps I cannot reach.** I have exactly one card. If you know who to point at
this, that is worth more to me than anything else in this email.

**3. A register-map question I would rather ask than assume.** My driver computes a register's port
as `base + index * stride`, with stride 1 or 2. Stride 2 is what my card needs — A0 is not decoded
on it, so register N is at `base + 2N`, measured by write/readback.

Reading minuszerodegrees' XT-IDE register map, that is not the map a Rev 2/3/4 presents in Hi-Speed
mode. That one comes from swapping A3 and A0, which permutes the registers rather than scaling them:
sector count `302`, LBA mid `304`, drive/head `306`, error/features `308`, LBA low `30A`, LBA high
`30C`, status `30E`. It agrees with mine on data-low and status and nothing else. A permutation is
not a stride, so my driver cannot express it — which means:

- Compatibility mode (Rev 1, or Rev 2/3/4 switched into it, device type `XTIDE rev1`) is my stride
  1, with the high data byte at `base+8`. Shipped, never executed by anyone.
- Hi-Speed is **not implemented**, and I would rather say so than have someone find out on their own
  disk.

What I expect on a Hi-Speed card is that my probe declines it: I settle the map by reading Status
and Alternate Status, which must be the same register, and comparing them — `base+7` against
`base+0Eh` for stride 1, `base+0Eh` against `base+1Ch` for stride 2. Nothing is written, deliberately,
so a wrong guess cannot assert `SRST` on someone's drive. On a Hi-Speed card stride 1 should
mismatch and stride 2 should read `base+1Ch` outside the decode and get `FFh`, which I reject.

So: **is that reading of the Hi-Speed map right, and do you know of any XT-IDE variant that decodes
narrowly enough for `base+1Ch` to alias back inside the window?** That last one is the only way I
can see for the probe to pass on a map I would then drive wrongly, and this machine's 8259 aliases
across `0x20-0x3F`, so I do not assume partial decode is rare.

If the map reading is right, adding Hi-Speed is a lookup table instead of a multiply. I will do it;
I just cannot test it.

One smaller question while I am asking: the driver addresses in LBA28 where the drive reports it and
otherwise builds CHS from the drive's own `IDENTIFY` — it never reads XUB's translated geometry. Is
there a configuration where those diverge enough to matter? A CHS-only drive behind a translating
BIOS is the case I am unsure of.

It installs through Add New Hardware as a **SCSI controller** — not an IDE one; SCSIPORT is the
framework, so that is the class it lives in — with Have Disk and a path typed by hand. The only
field that has to be right afterwards is the `PORT=` string on the controller's Settings tab, which
must match the base your card is jumpered to and XUB is configured for. Full steps are in the repo.

Repository, with source, INF, boot logs and the install sequence:
https://github.com/Mike1978uk/win95-intel-inboard-386pc — the driver is in `drivers/xtide_mpd/`.
`XTIDEMP.MPD`, 10,752 bytes, md5 `561fb45b598ef5985e5a803016321f76`.

I am happy to take corrections. The register map was cross-checked only against 86Box's
`hdc_xtide.c` and the XT-IDE port in `patacd.asm`, which share an author, so it has never been
checked against anything independent of that lineage. You would be the first.

Thank you for XUB — this machine boots from a CF card because of it.

Mike Lycett

---

## Notes

- The ask is a request, not an advert. What that community has and this project does not is **other
  cards and other register maps**.
- All three drafts state the Hi-Speed gap. Discovering it after someone else's disk was involved
  would be considerably worse than saying it now.
- The GPL/MIT point is explicit in all three. XUB is GPL-2.0, this driver is independent of it, and
  leaving that implicit invites the question at the worst moment.
- The `patacd.asm` / 86Box shared-author caveat stays: it is a known limit of the cross-check,
  recorded in [`resources_and_sources.md`](resources_and_sources.md), and these are the people best
  placed to close it.
